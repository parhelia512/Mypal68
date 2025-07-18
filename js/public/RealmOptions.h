/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Options specified when creating a realm to determine its behavior, immutable
 * options determining the behavior of an existing realm, and mutable options on
 * an existing realm that may be changed when desired.
 */

#ifndef js_RealmOptions_h
#define js_RealmOptions_h

#include "mozilla/Assertions.h"  // MOZ_ASSERT

#include "jstypes.h"  // JS_PUBLIC_API

#include "js/Class.h"  // JSTraceOp

struct JS_PUBLIC_API JSContext;
class JS_PUBLIC_API JSObject;

namespace JS {

class JS_PUBLIC_API Compartment;
class JS_PUBLIC_API Realm;
class JS_PUBLIC_API Zone;

}  // namespace JS

namespace JS {

/**
 * Specification for which compartment/zone a newly created realm should use.
 */
enum class CompartmentSpecifier {
  // Create a new realm and compartment in the single runtime wide system
  // zone. The meaning of this zone is left to the embedder.
  NewCompartmentInSystemZone,

  // Create a new realm and compartment in a particular existing zone.
  NewCompartmentInExistingZone,

  // Create a new zone/compartment.
  NewCompartmentAndZone,

  // Create a new realm in an existing compartment.
  ExistingCompartment,
};

/**
 * Specification for whether weak refs should be enabled and if so whether the
 * FinalizationRegistry.cleanupSome method should be present.
 */
enum class WeakRefSpecifier {
  Disabled,
  EnabledWithCleanupSome,
  EnabledWithoutCleanupSome
};

/**
 * RealmCreationOptions specifies options relevant to creating a new realm, that
 * are either immutable characteristics of that realm or that are discarded
 * after the realm has been created.
 *
 * Access to these options on an existing realm is read-only: if you need
 * particular selections, you must make them before you create the realm.
 */
class JS_PUBLIC_API RealmCreationOptions {
 public:
  RealmCreationOptions() : comp_(nullptr) {}

  JSTraceOp getTrace() const { return traceGlobal_; }
  RealmCreationOptions& setTrace(JSTraceOp op) {
    traceGlobal_ = op;
    return *this;
  }

  Zone* zone() const {
    MOZ_ASSERT(compSpec_ == CompartmentSpecifier::NewCompartmentInExistingZone);
    return zone_;
  }
  Compartment* compartment() const {
    MOZ_ASSERT(compSpec_ == CompartmentSpecifier::ExistingCompartment);
    return comp_;
  }
  CompartmentSpecifier compartmentSpecifier() const { return compSpec_; }

  // Set the compartment/zone to use for the realm. See CompartmentSpecifier
  // above.
  RealmCreationOptions& setNewCompartmentInSystemZone();
  RealmCreationOptions& setNewCompartmentInExistingZone(JSObject* obj);
  RealmCreationOptions& setNewCompartmentAndZone();
  RealmCreationOptions& setExistingCompartment(JSObject* obj);
  RealmCreationOptions& setExistingCompartment(Compartment* compartment);

  // Certain compartments are implementation details of the embedding, and
  // references to them should never leak out to script. This flag causes this
  // realm to skip firing onNewGlobalObject and makes addDebuggee a no-op for
  // this global.
  //
  // Debugger visibility is per-compartment, not per-realm (it's only practical
  // to enforce visibility on compartment boundaries), so if a realm is being
  // created in an extant compartment, its requested visibility must match that
  // of the compartment.
  bool invisibleToDebugger() const { return invisibleToDebugger_; }
  RealmCreationOptions& setInvisibleToDebugger(bool flag) {
    invisibleToDebugger_ = flag;
    return *this;
  }

  // Realms used for off-thread compilation have their contents merged into a
  // target realm when the compilation is finished. This is only allowed if
  // this flag is set. The invisibleToDebugger flag must also be set for such
  // realms.
  bool mergeable() const { return mergeable_; }
  RealmCreationOptions& setMergeable(bool flag) {
    mergeable_ = flag;
    return *this;
  }

  // Determines whether this realm should preserve JIT code on non-shrinking
  // GCs.
  bool preserveJitCode() const { return preserveJitCode_; }
  RealmCreationOptions& setPreserveJitCode(bool flag) {
    preserveJitCode_ = flag;
    return *this;
  }

  // Determines whether 1) the global Atomic property is defined and atomic
  // operations are supported, and 2) whether shared-memory operations are
  // supported.
  bool getSharedMemoryAndAtomicsEnabled() const;
  RealmCreationOptions& setSharedMemoryAndAtomicsEnabled(bool flag);

  // Determines (if getSharedMemoryAndAtomicsEnabled() is true) whether the
  // global SharedArrayBuffer property is defined.  If the property is not
  // defined, shared array buffer functionality can only be invoked if the
  // host/embedding specifically acts to expose it.
  //
  // This option defaults to true: embeddings unable to tolerate a global
  // SharedAraryBuffer property must opt out of it.
  bool defineSharedArrayBufferConstructor() const {
    return defineSharedArrayBufferConstructor_;
  }
  RealmCreationOptions& setDefineSharedArrayBufferConstructor(bool flag) {
    defineSharedArrayBufferConstructor_ = flag;
    return *this;
  }

  bool getStreamsEnabled() const { return streams_; }
  RealmCreationOptions& setStreamsEnabled(bool flag) {
    streams_ = flag;
    return *this;
  }

  bool getReadableByteStreamsEnabled() const { return readableByteStreams_; }
  RealmCreationOptions& setReadableByteStreamsEnabled(bool flag) {
    readableByteStreams_ = flag;
    return *this;
  }

  bool getBYOBStreamReadersEnabled() const { return byobStreamReaders_; }
  RealmCreationOptions& setBYOBStreamReadersEnabled(bool enabled) {
    byobStreamReaders_ = enabled;
    return *this;
  }

  bool getWritableStreamsEnabled() const { return writableStreams_; }
  RealmCreationOptions& setWritableStreamsEnabled(bool enabled) {
    writableStreams_ = enabled;
    return *this;
  }

  bool getReadableStreamPipeToEnabled() const { return readableStreamPipeTo_; }
  RealmCreationOptions& setReadableStreamPipeToEnabled(bool enabled) {
    readableStreamPipeTo_ = enabled;
    return *this;
  }

  WeakRefSpecifier getWeakRefsEnabled() const { return weakRefs_; }
  RealmCreationOptions& setWeakRefsEnabled(WeakRefSpecifier spec) {
    weakRefs_ = spec;
    return *this;
  }

  bool getToSourceEnabled() const { return toSource_; }
  RealmCreationOptions& setToSourceEnabled(bool flag) {
    toSource_ = flag;
    return *this;
  }

  bool getPropertyErrorMessageFixEnabled() const {
    return propertyErrorMessageFix_;
  }
  RealmCreationOptions& setPropertyErrorMessageFixEnabled(bool flag) {
    propertyErrorMessageFix_ = flag;
    return *this;
  }

  bool getIteratorHelpersEnabled() const { return iteratorHelpers_; }
  RealmCreationOptions& setIteratorHelpersEnabled(bool flag) {
    iteratorHelpers_ = flag;
    return *this;
  }

  // This flag doesn't affect JS engine behavior.  It is used by Gecko to
  // mark whether content windows and workers are "Secure Context"s. See
  // https://w3c.github.io/webappsec-secure-contexts/
  // https://bugzilla.mozilla.org/show_bug.cgi?id=1162772#c34
  bool secureContext() const { return secureContext_; }
  RealmCreationOptions& setSecureContext(bool flag) {
    secureContext_ = flag;
    return *this;
  }

  uint64_t profilerRealmID() const { return profilerRealmID_; }
  RealmCreationOptions& setProfilerRealmID(uint64_t id) {
    profilerRealmID_ = id;
    return *this;
  }

 private:
  JSTraceOp traceGlobal_ = nullptr;
  CompartmentSpecifier compSpec_ = CompartmentSpecifier::NewCompartmentAndZone;
  union {
    Compartment* comp_;
    Zone* zone_;
  };
  uint64_t profilerRealmID_ = 0;
  WeakRefSpecifier weakRefs_ = WeakRefSpecifier::Disabled;
  bool invisibleToDebugger_ = false;
  bool mergeable_ = false;
  bool preserveJitCode_ = false;
  bool sharedMemoryAndAtomics_ = false;
  bool defineSharedArrayBufferConstructor_ = true;
  bool streams_ = false;
  bool readableByteStreams_ = false;
  bool byobStreamReaders_ = false;
  bool writableStreams_ = false;
  bool readableStreamPipeTo_ = false;
  bool toSource_ = false;
  bool propertyErrorMessageFix_ = false;
  bool iteratorHelpers_ = false;
  bool secureContext_ = false;
};

/**
 * RealmBehaviors specifies behaviors of a realm that can be changed after the
 * realm's been created.
 */
class JS_PUBLIC_API RealmBehaviors {
 public:
  RealmBehaviors() = default;

  // For certain globals, we know enough about the code that will run in them
  // that we can discard script source entirely.
  bool discardSource() const { return discardSource_; }
  RealmBehaviors& setDiscardSource(bool flag) {
    discardSource_ = flag;
    return *this;
  }

  bool clampAndJitterTime() const { return clampAndJitterTime_; }
  RealmBehaviors& setClampAndJitterTime(bool flag) {
    clampAndJitterTime_ = flag;
    return *this;
  }

  class Override {
   public:
    Override() : mode_(Default) {}

    bool get(bool defaultValue) const {
      if (mode_ == Default) {
        return defaultValue;
      }
      return mode_ == ForceTrue;
    }

    void set(bool overrideValue) {
      mode_ = overrideValue ? ForceTrue : ForceFalse;
    }

    void reset() { mode_ = Default; }

   private:
    enum Mode { Default, ForceTrue, ForceFalse };

    Mode mode_;
  };

  // A Realm can stop being "live" in all the ways that matter before its global
  // is actually GCed.  Consumers that tear down parts of a Realm or its global
  // before that point should set isNonLive accordingly.
  bool isNonLive() const { return isNonLive_; }
  RealmBehaviors& setNonLive() {
    isNonLive_ = true;
    return *this;
  }

 private:
  bool discardSource_ = false;
  bool clampAndJitterTime_ = true;
  bool isNonLive_ = false;
};

/**
 * RealmOptions specifies realm characteristics: both those that can't be
 * changed on a realm once it's been created (RealmCreationOptions), and those
 * that can be changed on an existing realm (RealmBehaviors).
 */
class JS_PUBLIC_API RealmOptions {
 public:
  explicit RealmOptions() : creationOptions_(), behaviors_() {}

  RealmOptions(const RealmCreationOptions& realmCreation,
               const RealmBehaviors& realmBehaviors)
      : creationOptions_(realmCreation), behaviors_(realmBehaviors) {}

  // RealmCreationOptions specify fundamental realm characteristics that must
  // be specified when the realm is created, that can't be changed after the
  // realm is created.
  RealmCreationOptions& creationOptions() { return creationOptions_; }
  const RealmCreationOptions& creationOptions() const {
    return creationOptions_;
  }

  // RealmBehaviors specify realm characteristics that can be changed after
  // the realm is created.
  RealmBehaviors& behaviors() { return behaviors_; }
  const RealmBehaviors& behaviors() const { return behaviors_; }

 private:
  RealmCreationOptions creationOptions_;
  RealmBehaviors behaviors_;
};

extern JS_PUBLIC_API const RealmCreationOptions& RealmCreationOptionsRef(
    Realm* realm);

extern JS_PUBLIC_API const RealmCreationOptions& RealmCreationOptionsRef(
    JSContext* cx);

extern JS_PUBLIC_API const RealmBehaviors& RealmBehaviorsRef(Realm* realm);

extern JS_PUBLIC_API const RealmBehaviors& RealmBehaviorsRef(JSContext* cx);

extern JS_PUBLIC_API void SetRealmNonLive(Realm* realm);

}  // namespace JS

#endif  // js_RealmOptions_h
