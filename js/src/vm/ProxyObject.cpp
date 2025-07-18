/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/ProxyObject.h"

#include "gc/Allocator.h"
#include "gc/GCProbes.h"
#include "proxy/DeadObjectProxy.h"
#include "vm/Realm.h"

#include "gc/ObjectKind-inl.h"
#include "gc/WeakMap-inl.h"
#include "vm/JSObject-inl.h"

using namespace js;

static gc::AllocKind GetProxyGCObjectKind(const JSClass* clasp,
                                          const BaseProxyHandler* handler,
                                          const Value& priv) {
  MOZ_ASSERT(clasp->isProxyObject());

  uint32_t nreserved = JSCLASS_RESERVED_SLOTS(clasp);

  // For now assert each Proxy Class has at least 1 reserved slot. This is
  // not a hard requirement, but helps catch Classes that need an explicit
  // JSCLASS_HAS_RESERVED_SLOTS since bug 1360523.
  MOZ_ASSERT(nreserved > 0);

  MOZ_ASSERT(
      js::detail::ProxyValueArray::sizeOf(nreserved) % sizeof(Value) == 0,
      "ProxyValueArray must be a multiple of Value");

  uint32_t nslots =
      js::detail::ProxyValueArray::sizeOf(nreserved) / sizeof(Value);
  MOZ_ASSERT(nslots <= NativeObject::MAX_FIXED_SLOTS);

  gc::AllocKind kind = gc::GetGCObjectKind(nslots);
  if (handler->finalizeInBackground(priv)) {
    kind = ForegroundToBackgroundAllocKind(kind);
  }

  return kind;
}

void ProxyObject::init(const BaseProxyHandler* handler, HandleValue priv,
                       JSContext* cx) {
  setInlineValueArray();

  detail::ProxyValueArray* values = detail::GetProxyDataLayout(this)->values();
  values->init(numReservedSlots());

  data.handler = handler;

  if (IsCrossCompartmentWrapper(this)) {
    MOZ_ASSERT(cx->global() == &cx->compartment()->globalForNewCCW());
    setCrossCompartmentPrivate(priv);
  } else {
    setSameCompartmentPrivate(priv);
  }

  // The expando slot is nullptr until required by the installation of
  // a private field.
  setExpando(nullptr);
}

/* static */
ProxyObject* ProxyObject::New(JSContext* cx, const BaseProxyHandler* handler,
                              HandleValue priv, TaggedProto proto_,
                              const JSClass* clasp) {
  Rooted<TaggedProto> proto(cx, proto_);

  MOZ_ASSERT(!clasp->isNativeObject());
  MOZ_ASSERT(clasp->isProxyObject());
  MOZ_ASSERT(isValidProxyClass(clasp));
  MOZ_ASSERT(clasp->shouldDelayMetadataBuilder());
  MOZ_ASSERT_IF(proto.isObject(),
                cx->compartment() == proto.toObject()->compartment());
  MOZ_ASSERT(clasp->hasFinalize());

#ifdef DEBUG
  if (priv.isGCThing()) {
    JS::AssertCellIsNotGray(priv.toGCThing());
  }
#endif

  gc::AllocKind allocKind = GetProxyGCObjectKind(clasp, handler, priv);

  Realm* realm = cx->realm();

  AutoSetNewObjectMetadata metadata(cx);
  // Try to look up the shape in the NewProxyCache.
  RootedShape shape(cx);
  if (!realm->newProxyCache.lookup(clasp, proto, shape.address())) {
    shape = SharedShape::getInitialShape(cx, clasp, realm, proto,
                                         /* nfixed = */ 0);
    if (!shape) {
      return nullptr;
    }

    realm->newProxyCache.add(shape);
  }

  MOZ_ASSERT(shape->realm() == realm);
  MOZ_ASSERT(!IsAboutToBeFinalizedUnbarriered(shape.address()));

  // Ensure that the wrapper has the same lifetime assumptions as the
  // wrappee. Prefer to allocate in the nursery, when possible.
  gc::InitialHeap heap;
  if ((priv.isGCThing() && priv.toGCThing()->isTenured()) ||
      !handler->canNurseryAllocate()) {
    heap = gc::TenuredHeap;
  } else {
    heap = gc::DefaultHeap;
  }

  debugCheckNewObject(shape, allocKind, heap);

  JSObject* obj =
      AllocateObject(cx, allocKind, /* nDynamicSlots = */ 0, heap, clasp);
  if (!obj) {
    return nullptr;
  }

  ProxyObject* proxy = static_cast<ProxyObject*>(obj);
  proxy->initShape(shape);

  MOZ_ASSERT(clasp->shouldDelayMetadataBuilder());
  realm->setObjectPendingMetadata(cx, proxy);

  gc::gcprobes::CreateObject(proxy);

  proxy->init(handler, priv, cx);

  return proxy;
}

gc::AllocKind ProxyObject::allocKindForTenure() const {
  MOZ_ASSERT(usingInlineValueArray());
  Value priv = private_();
  return GetProxyGCObjectKind(getClass(), data.handler, priv);
}

void ProxyObject::setCrossCompartmentPrivate(const Value& priv) {
  setPrivate(priv);
}

void ProxyObject::setSameCompartmentPrivate(const Value& priv) {
  MOZ_ASSERT(IsObjectValueInCompartment(priv, compartment()));
  setPrivate(priv);
}

inline void ProxyObject::setPrivate(const Value& priv) {
  MOZ_ASSERT_IF(IsMarkedBlack(this) && priv.isGCThing(),
                !JS::GCThingIsMarkedGray(priv.toGCCellPtr()));
  *slotOfPrivate() = priv;
}

void ProxyObject::setExpando(JSObject* expando) {
  // Ensure that we don't accidentally end up pointing to a
  // grey object, which would violate GC invariants.
  MOZ_ASSERT_IF(IsMarkedBlack(this) && expando,
                !JS::GCThingIsMarkedGray(JS::GCCellPtr(expando)));

  // Ensure we're in the same compartment as the proxy object: Don't want the
  // expando to end up as a CCW.
  MOZ_ASSERT_IF(expando, expando->compartment() == compartment());
  *slotOfExpando() = ObjectOrNullValue(expando);
}

void ProxyObject::nuke() {
  // Notify the zone that a delegate is no longer a delegate. Be careful not to
  // expose this pointer, because it has already been removed from the wrapper
  // map yet we have assertions during tracing that will verify that it is
  // still present.
  JSObject* delegate = UncheckedUnwrapWithoutExpose(this);
  if (delegate != this) {
    delegate->zone()->beforeClearDelegate(this, delegate);
  }

  // Clear the target reference and replaced it with a value that encodes
  // various information about the original target.
  setSameCompartmentPrivate(DeadProxyTargetValue(this));

  // Clear out the expando
  setExpando(nullptr);

  // Update the handler to make this a DeadObjectProxy.
  setHandler(&DeadObjectProxy::singleton);

  // The proxy's reserved slots are not cleared and will continue to be
  // traced. This avoids the possibility of triggering write barriers while
  // nuking proxies in dead compartments which could otherwise cause those
  // compartments to be kept alive. Note that these are slots cannot hold
  // cross compartment pointers, so this cannot cause the target compartment
  // to leak.
}

JS_PUBLIC_API void js::detail::SetValueInProxy(Value* slot,
                                               const Value& value) {
  // Slots in proxies are not GCPtrValues, so do a cast whenever assigning
  // values to them which might trigger a barrier.
  *reinterpret_cast<GCPtrValue*>(slot) = value;
}
