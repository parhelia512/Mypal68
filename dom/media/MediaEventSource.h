/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MediaEventSource_h_
#define MediaEventSource_h_

#include <type_traits>
#include <utility>

#include "mozilla/AbstractThread.h"
#include "mozilla/Atomics.h"
#include "mozilla/Mutex.h"
#include "mozilla/Tuple.h"
#include "mozilla/TypeTraits.h"
#include "mozilla/Unused.h"

#include "nsISupportsImpl.h"
#include "nsTArray.h"
#include "nsThreadUtils.h"

namespace mozilla {

/**
 * A thread-safe tool to communicate "revocation" across threads. It is used to
 * disconnect a listener from the event source to prevent future notifications
 * from coming. Revoke() can be called on any thread. However, it is recommended
 * to be called on the target thread to avoid race condition.
 *
 * RevocableToken is not exposed to the client code directly.
 * Use MediaEventListener below to do the job.
 */
class RevocableToken {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(RevocableToken);

 public:
  RevocableToken() = default;

  void Revoke() { mRevoked = true; }

  bool IsRevoked() const { return mRevoked; }

 protected:
  // Virtual destructor is required since we might delete a Listener object
  // through its base type pointer.
  virtual ~RevocableToken() = default;

 private:
  Atomic<bool> mRevoked{false};
};

enum class ListenerPolicy : int8_t {
  // Allow at most one listener. Move will be used when possible
  // to pass the event data to save copy.
  Exclusive,
  // Allow multiple listeners. Event data will always be copied when passed
  // to the listeners.
  NonExclusive
};

namespace detail {

/**
 * Define how an event type is passed internally in MediaEventSource and to the
 * listeners. Specialized for the void type to pass a dummy bool instead.
 */
template <typename T>
struct EventTypeTraits {
  typedef T ArgType;
};

template <>
struct EventTypeTraits<void> {
  typedef bool ArgType;
};

/**
 * Test if a method function or lambda accepts one or more arguments.
 */
template <typename T>
class TakeArgsHelper {
  template <typename C>
  static std::false_type test(void (C::*)(), int);
  template <typename C>
  static std::false_type test(void (C::*)() const, int);
  template <typename C>
  static std::false_type test(void (C::*)() volatile, int);
  template <typename C>
  static std::false_type test(void (C::*)() const volatile, int);
  template <typename F>
  static std::false_type test(F&&, decltype(std::declval<F>()(), 0));
  static std::true_type test(...);

 public:
  typedef decltype(test(std::declval<T>(), 0)) type;
};

template <typename T>
struct TakeArgs : public TakeArgsHelper<T>::type {};

template <typename T>
struct EventTarget;

template <>
struct EventTarget<nsIEventTarget> {
  static void Dispatch(nsIEventTarget* aTarget,
                       already_AddRefed<nsIRunnable> aTask) {
    aTarget->Dispatch(std::move(aTask), NS_DISPATCH_NORMAL);
  }
};

template <>
struct EventTarget<AbstractThread> {
  static void Dispatch(AbstractThread* aTarget,
                       already_AddRefed<nsIRunnable> aTask) {
    Unused << aTarget->Dispatch(std::move(aTask));
  }
};

/**
 * Encapsulate a raw pointer to be captured by a lambda without causing
 * static-analysis errors.
 */
template <typename T>
class RawPtr {
 public:
  explicit RawPtr(T* aPtr) : mPtr(aPtr) {}
  T* get() const { return mPtr; }

 private:
  T* const mPtr;
};

template <typename... As>
class Listener : public RevocableToken {
 public:
  template <typename... Ts>
  void Dispatch(Ts&&... aEvents) {
    if (CanTakeArgs()) {
      DispatchTask(NewRunnableMethod<std::decay_t<Ts>&&...>(
          "detail::Listener::ApplyWithArgs", this, &Listener::ApplyWithArgs,
          std::forward<Ts>(aEvents)...));
    } else {
      DispatchTask(NewRunnableMethod("detail::Listener::ApplyWithNoArgs", this,
                                     &Listener::ApplyWithNoArgs));
    }
  }

 protected:
  virtual ~Listener() {
    MOZ_ASSERT(IsRevoked(), "Must disconnect the listener.");
  }

 private:
  virtual void DispatchTask(already_AddRefed<nsIRunnable> aTask) = 0;

  // True if the underlying listener function takes non-zero arguments.
  virtual bool CanTakeArgs() const = 0;
  // Pass the event data to the underlying listener function. Should be called
  // only when CanTakeArgs() returns true.
  virtual void ApplyWithArgs(As&&... aEvents) = 0;
  // Invoke the underlying listener function. Should be called only when
  // CanTakeArgs() returns false.
  virtual void ApplyWithNoArgs() = 0;
};

/**
 * Store the registered target thread and function so it knows where and to
 * whom to send the event data.
 */
template <typename Target, typename Function, typename... As>
class ListenerImpl : public Listener<As...> {
  // Strip CV and reference from Function.
  using FunctionStorage = std::decay_t<Function>;

 public:
  template <typename F>
  ListenerImpl(Target* aTarget, F&& aFunction)
      : mTarget(aTarget), mFunction(std::forward<F>(aFunction)) {}

 private:
  void DispatchTask(already_AddRefed<nsIRunnable> aTask) override {
    EventTarget<Target>::Dispatch(mTarget.get(), std::move(aTask));
  }

  bool CanTakeArgs() const override { return TakeArgs<FunctionStorage>::value; }

  // |F| takes one or more arguments.
  template <typename F>
  std::enable_if_t<TakeArgs<F>::value, void> ApplyWithArgsImpl(
      const F& aFunc, As&&... aEvents) {
    aFunc(std::move(aEvents)...);
  }

  // |F| takes no arguments.
  template <typename F>
  std::enable_if_t<!TakeArgs<F>::value, void> ApplyWithArgsImpl(
      const F& aFunc, As&&... aEvents) {
    MOZ_CRASH("Call ApplyWithNoArgs instead.");
  }

  void ApplyWithArgs(As&&... aEvents) override {
    MOZ_RELEASE_ASSERT(TakeArgs<Function>::value);
    // Don't call the listener if it is disconnected.
    if (!RevocableToken::IsRevoked()) {
      ApplyWithArgsImpl(mFunction, std::move(aEvents)...);
    }
  }

  // |F| takes one or more arguments.
  template <typename F>
  std::enable_if_t<TakeArgs<F>::value, void> ApplyWithNoArgsImpl(
      const F& aFunc) {
    MOZ_CRASH("Call ApplyWithArgs instead.");
  }

  // |F| takes no arguments.
  template <typename F>
  std::enable_if_t<!TakeArgs<F>::value, void> ApplyWithNoArgsImpl(
      const F& aFunc) {
    aFunc();
  }

  virtual void ApplyWithNoArgs() override {
    MOZ_RELEASE_ASSERT(!TakeArgs<Function>::value);
    // Don't call the listener if it is disconnected.
    if (!RevocableToken::IsRevoked()) {
      ApplyWithNoArgsImpl(mFunction);
    }
  }

  const RefPtr<Target> mTarget;
  FunctionStorage mFunction;
};

/**
 * Return true if any type is a reference type.
 */
template <typename Head, typename... Tails>
struct IsAnyReference {
  static const bool value =
      std::is_reference_v<Head> || IsAnyReference<Tails...>::value;
};

template <typename T>
struct IsAnyReference<T> {
  static const bool value = std::is_reference_v<T>;
};

}  // namespace detail

template <ListenerPolicy, typename... Ts>
class MediaEventSourceImpl;

/**
 * Not thread-safe since this is not meant to be shared and therefore only
 * move constructor is provided. Used to hold the result of
 * MediaEventSource<T>::Connect() and call Disconnect() to disconnect the
 * listener from an event source.
 */
class MediaEventListener {
  template <ListenerPolicy, typename... Ts>
  friend class MediaEventSourceImpl;

 public:
  MediaEventListener() = default;

  MediaEventListener(MediaEventListener&& aOther)
      : mToken(std::move(aOther.mToken)) {}

  MediaEventListener& operator=(MediaEventListener&& aOther) {
    MOZ_ASSERT(!mToken, "Must disconnect the listener.");
    mToken = std::move(aOther.mToken);
    return *this;
  }

  ~MediaEventListener() {
    MOZ_ASSERT(!mToken, "Must disconnect the listener.");
  }

  void Disconnect() {
    mToken->Revoke();
    mToken = nullptr;
  }

  void DisconnectIfExists() {
    if (mToken) {
      Disconnect();
    }
  }

 private:
  // Avoid exposing RevocableToken directly to the client code so that
  // listeners can be disconnected in a controlled manner.
  explicit MediaEventListener(RevocableToken* aToken) : mToken(aToken) {}
  RefPtr<RevocableToken> mToken;
};

/**
 * A generic and thread-safe class to implement the observer pattern.
 */
template <ListenerPolicy Lp, typename... Es>
class MediaEventSourceImpl {
  static_assert(!detail::IsAnyReference<Es...>::value,
                "Ref-type not supported!");

  template <typename T>
  using ArgType = typename detail::EventTypeTraits<T>::ArgType;

  typedef detail::Listener<ArgType<Es>...> Listener;

  template <typename Target, typename Func>
  using ListenerImpl = detail::ListenerImpl<Target, Func, ArgType<Es>...>;

  template <typename Method>
  using TakeArgs = detail::TakeArgs<Method>;

  void PruneListeners() {
    mListeners.RemoveElementsBy(
        [](const auto& listener) { return listener->IsRevoked(); });
  }

  template <typename Target, typename Function>
  MediaEventListener ConnectInternal(Target* aTarget, Function&& aFunction) {
    MutexAutoLock lock(mMutex);
    PruneListeners();
    MOZ_ASSERT(Lp == ListenerPolicy::NonExclusive || mListeners.IsEmpty());
    auto l = mListeners.AppendElement();
    *l = new ListenerImpl<Target, Function>(aTarget,
                                            std::forward<Function>(aFunction));
    return MediaEventListener(*l);
  }

  // |Method| takes one or more arguments.
  template <typename Target, typename This, typename Method>
  std::enable_if_t<TakeArgs<Method>::value, MediaEventListener> ConnectInternal(
      Target* aTarget, This* aThis, Method aMethod) {
    detail::RawPtr<This> thiz(aThis);
    return ConnectInternal(aTarget, [=](ArgType<Es>&&... aEvents) {
      (thiz.get()->*aMethod)(std::move(aEvents)...);
    });
  }

  // |Method| takes no arguments. Don't bother passing the event data.
  template <typename Target, typename This, typename Method>
  std::enable_if_t<!TakeArgs<Method>::value, MediaEventListener>
  ConnectInternal(Target* aTarget, This* aThis, Method aMethod) {
    detail::RawPtr<This> thiz(aThis);
    return ConnectInternal(aTarget, [=]() { (thiz.get()->*aMethod)(); });
  }

 public:
  /**
   * Register a function to receive notifications from the event source.
   *
   * @param aTarget The target thread on which the function will run.
   * @param aFunction A function to be called on the target thread. The function
   *                  parameter must be convertible from |EventType|.
   * @return An object used to disconnect from the event source.
   */
  template <typename Function>
  MediaEventListener Connect(AbstractThread* aTarget, Function&& aFunction) {
    return ConnectInternal(aTarget, std::forward<Function>(aFunction));
  }

  template <typename Function>
  MediaEventListener Connect(nsIEventTarget* aTarget, Function&& aFunction) {
    return ConnectInternal(aTarget, std::forward<Function>(aFunction));
  }

  /**
   * As above.
   *
   * Note we deliberately keep a weak reference to |aThis| in order not to
   * change its lifetime. This is because notifications are dispatched
   * asynchronously and removing a listener doesn't always break the reference
   * cycle for the pending event could still hold a reference to |aThis|.
   *
   * The caller must call MediaEventListener::Disconnect() to avoid dangling
   * pointers.
   */
  template <typename This, typename Method>
  MediaEventListener Connect(AbstractThread* aTarget, This* aThis,
                             Method aMethod) {
    return ConnectInternal(aTarget, aThis, aMethod);
  }

  template <typename This, typename Method>
  MediaEventListener Connect(nsIEventTarget* aTarget, This* aThis,
                             Method aMethod) {
    return ConnectInternal(aTarget, aThis, aMethod);
  }

 protected:
  MediaEventSourceImpl() : mMutex("MediaEventSourceImpl::mMutex") {}

  template <typename... Ts>
  void NotifyInternal(Ts&&... aEvents) {
    MutexAutoLock lock(mMutex);
    int32_t last = static_cast<int32_t>(mListeners.Length()) - 1;
    for (int32_t i = last; i >= 0; --i) {
      auto&& l = mListeners[i];
      // Remove disconnected listeners.
      // It is not optimal but is simple and works well.
      if (l->IsRevoked()) {
        mListeners.RemoveElementAt(i);
        continue;
      }
      l->Dispatch(std::forward<Ts>(aEvents)...);
    }
  }

 private:
  Mutex mMutex;
  nsTArray<RefPtr<Listener>> mListeners;
};

template <typename... Es>
using MediaEventSource =
    MediaEventSourceImpl<ListenerPolicy::NonExclusive, Es...>;

template <typename... Es>
using MediaEventSourceExc =
    MediaEventSourceImpl<ListenerPolicy::Exclusive, Es...>;

/**
 * A class to separate the interface of event subject (MediaEventSource)
 * and event publisher. Mostly used as a member variable to publish events
 * to the listeners.
 */
template <typename... Es>
class MediaEventProducer : public MediaEventSource<Es...> {
 public:
  template <typename... Ts>
  void Notify(Ts&&... aEvents) {
    // Pass lvalues to prevent move in NonExclusive mode.
    this->NotifyInternal(aEvents...);
  }
};

/**
 * Specialization for void type. A dummy bool is passed to NotifyInternal
 * since there is no way to pass a void value.
 */
template <>
class MediaEventProducer<void> : public MediaEventSource<void> {
 public:
  void Notify() { this->NotifyInternal(true /* dummy */); }
};

/**
 * A producer allowing at most one listener.
 */
template <typename... Es>
class MediaEventProducerExc : public MediaEventSourceExc<Es...> {
 public:
  template <typename... Ts>
  void Notify(Ts&&... aEvents) {
    this->NotifyInternal(std::forward<Ts>(aEvents)...);
  }
};

}  // namespace mozilla

#endif  // MediaEventSource_h_
