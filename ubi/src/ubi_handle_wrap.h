#ifndef UBI_HANDLE_WRAP_H_
#define UBI_HANDLE_WRAP_H_

#include <cstdint>

#include <uv.h>

#include "node_api.h"

enum UbiHandleState : uint8_t {
  kUbiHandleUninitialized = 0,
  kUbiHandleInitialized,
  kUbiHandleClosing,
  kUbiHandleClosed,
};

struct UbiHandleWrap {
  napi_env env = nullptr;
  napi_ref wrapper_ref = nullptr;
  void* active_handle_token = nullptr;
  bool finalized = false;
  bool delete_on_close = false;
  bool wrapper_ref_held = false;
  UbiHandleState state = kUbiHandleUninitialized;
};

void UbiHandleWrapInit(UbiHandleWrap* wrap, napi_env env);
napi_value UbiHandleWrapGetRefValue(napi_env env, napi_ref ref);
void UbiHandleWrapDeleteRefIfPresent(napi_env env, napi_ref* ref);
void UbiHandleWrapHoldWrapperRef(UbiHandleWrap* wrap);
void UbiHandleWrapReleaseWrapperRef(UbiHandleWrap* wrap);
napi_value UbiHandleWrapGetActiveOwner(napi_env env, napi_ref wrapper_ref);
void UbiHandleWrapSetOnCloseCallback(napi_env env, napi_value wrapper, napi_value callback);
void UbiHandleWrapMaybeCallOnClose(UbiHandleWrap* wrap);
bool UbiHandleWrapHasRef(const UbiHandleWrap* wrap, const uv_handle_t* handle);

#endif  // UBI_HANDLE_WRAP_H_
