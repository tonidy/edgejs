#include "internal_binding/dispatch.h"

#include "internal_binding/helpers.h"
#include "ubi_encoding.h"

namespace internal_binding {

napi_value ResolveEncodingBinding(napi_env env, const ResolveOptions& /*options*/) {
  napi_value binding = UbiInstallEncodingBinding(env);
  return binding != nullptr ? binding : Undefined(env);
}

}  // namespace internal_binding
