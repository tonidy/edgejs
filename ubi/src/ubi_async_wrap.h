#ifndef UBI_ASYNC_WRAP_H_
#define UBI_ASYNC_WRAP_H_

#include <cstdint>

#include "node_api.h"

enum UbiAsyncProviderType : int32_t {
  kUbiProviderNone = 0,
  kUbiProviderJsStream = 20,
  kUbiProviderPipeConnectWrap = 23,
  kUbiProviderPipeServerWrap = 24,
  kUbiProviderPipeWrap = 25,
  kUbiProviderShutdownWrap = 35,
  kUbiProviderTcpConnectWrap = 39,
  kUbiProviderTcpServerWrap = 40,
  kUbiProviderTcpWrap = 41,
  kUbiProviderTtyWrap = 42,
  kUbiProviderWriteWrap = 53,
};

int64_t UbiAsyncWrapNextId(napi_env env);
void UbiAsyncWrapQueueDestroyId(napi_env env, int64_t async_id);
void UbiAsyncWrapReset(napi_env env, int64_t* async_id);

#endif  // UBI_ASYNC_WRAP_H_
