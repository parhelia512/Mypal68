/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* SpiderMonkey initialization and shutdown APIs. */

#ifndef js_Initialization_h
#define js_Initialization_h

#include "mozilla/Span.h"

#include "jstypes.h"

struct JS_PUBLIC_API JSContext;

namespace JS {
namespace detail {

enum class InitState { Uninitialized = 0, Initializing, Running, ShutDown };

/**
 * SpiderMonkey's initialization status is tracked here, and it controls things
 * that should happen only once across all runtimes.  It's an API requirement
 * that JS_Init (and JS_ShutDown, if called) be called in a thread-aware
 * manner, so this (internal -- embedders, don't use!) variable doesn't need to
 * be atomic.
 */
extern JS_PUBLIC_DATA InitState libraryInitState;

extern JS_PUBLIC_API const char* InitWithFailureDiagnostic(bool isDebugBuild);

}  // namespace detail
}  // namespace JS

// These are equivalent to ICU's |UMemAllocFn|, |UMemReallocFn|, and
// |UMemFreeFn| types.  The first argument (called |context| in the ICU docs)
// will always be nullptr and should be ignored.
typedef void* (*JS_ICUAllocFn)(const void*, size_t size);
typedef void* (*JS_ICUReallocFn)(const void*, void* p, size_t size);
typedef void (*JS_ICUFreeFn)(const void*, void* p);

/**
 * This function can be used to track memory used by ICU.  If it is called, it
 * *must* be called before JS_Init.  Don't use it unless you know what you're
 * doing!
 */
extern JS_PUBLIC_API bool JS_SetICUMemoryFunctions(JS_ICUAllocFn allocFn,
                                                   JS_ICUReallocFn reallocFn,
                                                   JS_ICUFreeFn freeFn);

/**
 * Initialize SpiderMonkey, returning true only if initialization succeeded.
 * Once this method has succeeded, it is safe to call JS_NewContext and other
 * JSAPI methods.
 *
 * This method must be called before any other JSAPI method is used on any
 * thread.  Once it has been used, it is safe to call any JSAPI method, and it
 * remains safe to do so until JS_ShutDown is correctly called.
 *
 * It is currently not possible to initialize SpiderMonkey multiple times (that
 * is, calling JS_Init/JSAPI methods/JS_ShutDown in that order, then doing so
 * again).  This restriction may eventually be lifted.
 */
inline bool JS_Init(void) {
#ifdef DEBUG
  return !JS::detail::InitWithFailureDiagnostic(true);
#else
  return !JS::detail::InitWithFailureDiagnostic(false);
#endif
}

/**
 * A variant of JS_Init. On success it returns nullptr. On failure it returns a
 * pointer to a string literal that describes how initialization failed, which
 * can be useful for debugging purposes.
 */
inline const char* JS_InitWithFailureDiagnostic(void) {
#ifdef DEBUG
  return JS::detail::InitWithFailureDiagnostic(true);
#else
  return JS::detail::InitWithFailureDiagnostic(false);
#endif
}

/*
 * Returns true if SpiderMonkey has been initialized successfully, even if it
 * has possibly been shut down.
 *
 * Note that it is the responsibility of the embedder to call JS_Init() and
 * JS_ShutDown() at the correct times, and therefore this API should ideally not
 * be necessary to use.  This is only intended to be used in cases where the
 * embedder isn't in full control of deciding whether to initialize SpiderMonkey
 * or hand off the task to another consumer.
 */
inline bool JS_IsInitialized(void) {
  return JS::detail::libraryInitState >= JS::detail::InitState::Running;
}

namespace JS {

// Reference to a sequence of bytes.
// TODO: This type should be Span<cont uint8_t> (Bug 1709135)
using SelfHostedCache = mozilla::Span<uint8_t>;

// Callback function used to copy the SelfHosted content to memory or to disk.
using SelfHostedWriter = bool (*)(JSContext*, SelfHostedCache);

/*
 * Initialize the runtime's self-hosted code. Embeddings should call this
 * exactly once per runtime/context, before the first JS_NewGlobalObject
 * call.
 *
 * This function parses the self-hosted code, except if the provided cache span
 * is not empty, in which case the self-hosted content is decoded from the span.
 *
 * The cached content provided as argument, when non-empty, should come from the
 * a previous execution of JS::InitSelfHostedCode where a writer was registered.
 * The content should come from the same version of the binary, otherwise this
 * would cause an error.
 *
 * The cached content provided with the Span should remain alive until
 * JS_Shutdown is called.
 *
 * The writer callback given as argument would be called by when the result of
 * the parser is ready to be cached. The writer is in charge of saving the
 * content in memory or on disk. The span given as argument of the writer only
 * last for the time of the call, and contains the content to be saved.
 *
 * The writer is not called if the cached content given as argument of
 * InitSelfHostedCode is non-empty.
 *
 * Errors returned by the writer callback would bubble up through
 * JS::InitSelfHostedCode.
 *
 * The cached content provided by the writer callback is safe to reuse across
 * threads, and even across multiple executions as long as the executable is
 * identical.
 *
 * NOTE: This may not set a pending exception in the case of OOM since this
 *       runs very early in startup.
 */
JS_PUBLIC_API bool InitSelfHostedCode(JSContext* cx,
                                      SelfHostedCache cache = nullptr,
                                      SelfHostedWriter writer = nullptr);

}  // namespace JS

/**
 * Destroy free-standing resources allocated by SpiderMonkey, not associated
 * with any runtime, context, or other structure.
 *
 * This method should be called after all other JSAPI data has been properly
 * cleaned up: every new runtime must have been destroyed, every new context
 * must have been destroyed, and so on.  Calling this method before all other
 * resources have been destroyed has undefined behavior.
 *
 * Failure to call this method, at present, has no adverse effects other than
 * leaking memory.  This may not always be the case; it's recommended that all
 * embedders call this method when all other JSAPI operations have completed.
 *
 * It is currently not possible to initialize SpiderMonkey multiple times (that
 * is, calling JS_Init/JSAPI methods/JS_ShutDown in that order, then doing so
 * again).  This restriction may eventually be lifted.
 */
extern JS_PUBLIC_API void JS_ShutDown(void);

#endif /* js_Initialization_h */
