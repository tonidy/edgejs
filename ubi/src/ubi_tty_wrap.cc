#include "ubi_tty_wrap.h"

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_map>

#include <unistd.h>
#include <uv.h>

#include "ubi_async_wrap.h"
#include "ubi_stream_base.h"

namespace {

struct TtyWrap {
  napi_env env = nullptr;
  UbiStreamBase base{};
  uv_tty_t handle{};
  bool initialized = false;
  int init_err = 0;
  int32_t fd = -1;
};

struct TtyBindingState {
  napi_ref tty_ctor_ref = nullptr;
  napi_ref binding_ref = nullptr;
};

std::unordered_map<napi_env, TtyBindingState> g_tty_states;

TtyBindingState& EnsureBindingState(napi_env env) { return g_tty_states[env]; }

TtyWrap* FromBase(UbiStreamBase* base) {
  if (base == nullptr) return nullptr;
  return reinterpret_cast<TtyWrap*>(reinterpret_cast<char*>(base) - offsetof(TtyWrap, base));
}

uv_handle_t* TtyGetHandle(UbiStreamBase* base) {
  auto* wrap = FromBase(base);
  if (wrap == nullptr || !wrap->initialized) return nullptr;
  return reinterpret_cast<uv_handle_t*>(&wrap->handle);
}

uv_stream_t* TtyGetStream(UbiStreamBase* base) {
  auto* wrap = FromBase(base);
  if (wrap == nullptr || !wrap->initialized) return nullptr;
  return reinterpret_cast<uv_stream_t*>(&wrap->handle);
}

void TtyDestroy(UbiStreamBase* base) {
  delete FromBase(base);
}

void TtyAfterClose(uv_handle_t* handle) {
  auto* wrap = handle != nullptr ? static_cast<TtyWrap*>(handle->data) : nullptr;
  if (wrap == nullptr) return;
  UbiStreamBaseOnClosed(&wrap->base);
}

const UbiStreamBaseOps kTtyOps = {
    TtyGetHandle,
    TtyGetStream,
    TtyAfterClose,
    TtyDestroy,
    nullptr,
};

int SyncTtyWrite(TtyWrap* wrap, const char* data, size_t len) {
  if (wrap == nullptr || !wrap->initialized || wrap->fd < 0) return UV_EBADF;
  if (len == 0) return 0;
  ssize_t rc = -1;
  do {
    rc = write(wrap->fd, data, len);
  } while (rc < 0 && errno == EINTR);
  if (rc < 0) return -errno;
  wrap->base.bytes_written += static_cast<uint64_t>(rc);
  return 0;
}

napi_value FinishSyncTtyWrite(TtyWrap* wrap, napi_value req_obj, int status) {
  if (wrap == nullptr || wrap->env == nullptr) return nullptr;
  if (req_obj != nullptr) {
    napi_value stream_obj = UbiStreamBaseGetWrapper(&wrap->base);
    napi_value argv[3] = {
        UbiStreamBaseMakeInt32(wrap->env, status),
        stream_obj != nullptr ? stream_obj : UbiStreamBaseUndefined(wrap->env),
        status < 0 ? nullptr : UbiStreamBaseUndefined(wrap->env),
    };
    UbiStreamBaseInvokeReqOnComplete(wrap->env, req_obj, status, argv, 3);
  }
  return UbiStreamBaseMakeInt32(wrap->env, status);
}

napi_value GetThis(napi_env env,
                   napi_callback_info info,
                   size_t* argc_out,
                   napi_value* argv,
                   TtyWrap** wrap_out) {
  size_t argc = argc_out != nullptr ? *argc_out : 0;
  napi_value self = nullptr;
  napi_get_cb_info(env, info, &argc, argv, &self, nullptr);
  if (argc_out != nullptr) *argc_out = argc;
  if (wrap_out != nullptr) {
    *wrap_out = nullptr;
    if (self != nullptr) {
      napi_unwrap(env, self, reinterpret_cast<void**>(wrap_out));
    }
  }
  return self;
}

void SetInitErrorContext(napi_env env, napi_value maybe_ctx, int err) {
  if (env == nullptr || maybe_ctx == nullptr || err == 0) return;
  napi_valuetype ctx_type = napi_undefined;
  if (napi_typeof(env, maybe_ctx, &ctx_type) != napi_ok || ctx_type != napi_object) return;

  napi_value errno_v = nullptr;
  napi_value code_v = nullptr;
  napi_value message_v = nullptr;
  napi_value syscall_v = nullptr;
  napi_create_int32(env, err, &errno_v);
  napi_create_string_utf8(env,
                          uv_err_name(err) != nullptr ? uv_err_name(err) : "UV_ERROR",
                          NAPI_AUTO_LENGTH,
                          &code_v);
  napi_create_string_utf8(env,
                          uv_strerror(err) != nullptr ? uv_strerror(err) : "unknown error",
                          NAPI_AUTO_LENGTH,
                          &message_v);
  napi_create_string_utf8(env, "uv_tty_init", NAPI_AUTO_LENGTH, &syscall_v);

  if (errno_v != nullptr) napi_set_named_property(env, maybe_ctx, "errno", errno_v);
  if (code_v != nullptr) napi_set_named_property(env, maybe_ctx, "code", code_v);
  if (message_v != nullptr) napi_set_named_property(env, maybe_ctx, "message", message_v);
  if (syscall_v != nullptr) napi_set_named_property(env, maybe_ctx, "syscall", syscall_v);
}

void OnAlloc(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
  auto* wrap = handle != nullptr ? static_cast<TtyWrap*>(handle->data) : nullptr;
  UbiStreamBaseOnUvAlloc(wrap != nullptr ? &wrap->base : nullptr, suggested_size, buf);
}

void OnRead(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) {
  auto* wrap = stream != nullptr ? static_cast<TtyWrap*>(stream->data) : nullptr;
  UbiStreamBaseOnUvRead(wrap != nullptr ? &wrap->base : nullptr, nread, buf);
}

napi_value TtyCtor(napi_env env, napi_callback_info info) {
  size_t argc = 2;
  napi_value argv[2] = {nullptr, nullptr};
  napi_value self = nullptr;
  napi_get_cb_info(env, info, &argc, argv, &self, nullptr);
  if (self == nullptr) return nullptr;

  int32_t fd = -1;
  if (argc >= 1 && argv[0] != nullptr) {
    napi_get_value_int32(env, argv[0], &fd);
  }

  auto* wrap = new TtyWrap();
  wrap->env = env;
  wrap->fd = fd;
  UbiStreamBaseInit(&wrap->base, env, &kTtyOps, kUbiProviderTtyWrap);

  if (fd >= 0) {
    wrap->init_err = uv_tty_init(uv_default_loop(), &wrap->handle, fd, 0);
  } else {
    wrap->init_err = UV_EINVAL;
  }
  wrap->initialized = (wrap->init_err == 0);
  if (wrap->initialized) {
    wrap->handle.data = wrap;
  }

  napi_wrap(env,
            self,
            wrap,
            [](napi_env finalize_env, void* data, void*) {
              auto* tty_wrap = static_cast<TtyWrap*>(data);
              if (tty_wrap == nullptr) return;
              UbiStreamBaseFinalize(&tty_wrap->base);
            },
            nullptr,
            &wrap->base.wrapper_ref);

  if (wrap->initialized) {
    UbiStreamBaseSetWrapperRef(&wrap->base, wrap->base.wrapper_ref);
  }
  UbiStreamBaseSetInitialStreamProperties(&wrap->base, true, false);
  SetInitErrorContext(env, argc >= 2 ? argv[1] : nullptr, wrap->init_err);
  return self;
}

napi_value TtyIsTTY(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value argv[1] = {nullptr};
  napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);

  int32_t fd = -1;
  if (argc < 1 || argv[0] == nullptr || napi_get_value_int32(env, argv[0], &fd) != napi_ok || fd < 0) {
    return UbiStreamBaseMakeBool(env, false);
  }
  return UbiStreamBaseMakeBool(env, uv_guess_handle(fd) == UV_TTY);
}

napi_value TtyGetWindowSize(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value argv[1] = {nullptr};
  TtyWrap* wrap = nullptr;
  GetThis(env, info, &argc, argv, &wrap);
  if (wrap == nullptr || !wrap->initialized) {
    return UbiStreamBaseMakeInt32(env, UV_EBADF);
  }

  int width = 0;
  int height = 0;
  const int rc = uv_tty_get_winsize(&wrap->handle, &width, &height);
  if (rc == 0 && argc >= 1 && argv[0] != nullptr) {
    napi_set_element(env, argv[0], 0, UbiStreamBaseMakeInt32(env, width));
    napi_set_element(env, argv[0], 1, UbiStreamBaseMakeInt32(env, height));
  }
  return UbiStreamBaseMakeInt32(env, rc);
}

napi_value TtySetRawMode(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value argv[1] = {nullptr};
  TtyWrap* wrap = nullptr;
  GetThis(env, info, &argc, argv, &wrap);
  if (wrap == nullptr || !wrap->initialized) {
    return UbiStreamBaseMakeInt32(env, UV_EBADF);
  }

  bool flag = false;
  if (argc >= 1 && argv[0] != nullptr) {
    napi_get_value_bool(env, argv[0], &flag);
  }
#ifdef UV_TTY_MODE_RAW_VT
  const uv_tty_mode_t mode = flag ? UV_TTY_MODE_RAW_VT : UV_TTY_MODE_NORMAL;
#else
  const uv_tty_mode_t mode = flag ? UV_TTY_MODE_RAW : UV_TTY_MODE_NORMAL;
#endif
  return UbiStreamBaseMakeInt32(env, uv_tty_set_mode(&wrap->handle, mode));
}

napi_value TtySetBlocking(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value argv[1] = {nullptr};
  TtyWrap* wrap = nullptr;
  GetThis(env, info, &argc, argv, &wrap);
  if (wrap == nullptr || !wrap->initialized) {
    return UbiStreamBaseMakeInt32(env, UV_EBADF);
  }

  bool enable = true;
  if (argc >= 1 && argv[0] != nullptr) {
    napi_get_value_bool(env, argv[0], &enable);
  }
  return UbiStreamBaseMakeInt32(
      env,
      uv_stream_set_blocking(reinterpret_cast<uv_stream_t*>(&wrap->handle), enable ? 1 : 0));
}

napi_value TtyReadStart(napi_env env, napi_callback_info info) {
  TtyWrap* wrap = nullptr;
  GetThis(env, info, nullptr, nullptr, &wrap);
  if (wrap == nullptr || !wrap->initialized) {
    return UbiStreamBaseMakeInt32(env, UV_EBADF);
  }

  const int rc = uv_read_start(reinterpret_cast<uv_stream_t*>(&wrap->handle), OnAlloc, OnRead);
  UbiStreamBaseSetReading(&wrap->base, rc == 0);
  return UbiStreamBaseMakeInt32(env, rc);
}

napi_value TtyReadStop(napi_env env, napi_callback_info info) {
  TtyWrap* wrap = nullptr;
  GetThis(env, info, nullptr, nullptr, &wrap);
  if (wrap == nullptr || !wrap->initialized) {
    return UbiStreamBaseMakeInt32(env, UV_EBADF);
  }

  const int rc = uv_read_stop(reinterpret_cast<uv_stream_t*>(&wrap->handle));
  UbiStreamBaseSetReading(&wrap->base, false);
  return UbiStreamBaseMakeInt32(env, rc);
}

napi_value TtyWriteBuffer(napi_env env, napi_callback_info info) {
  size_t argc = 2;
  napi_value argv[2] = {nullptr, nullptr};
  TtyWrap* wrap = nullptr;
  GetThis(env, info, &argc, argv, &wrap);
  if (wrap == nullptr || !wrap->initialized || argc < 2) {
    return UbiStreamBaseMakeInt32(env, UV_EINVAL);
  }
#if defined(__wasi__)
  const uint8_t* data = nullptr;
  size_t len = 0;
  bool refable = false;
  std::string temp_utf8;
  UbiStreamBaseExtractByteSpan(env, argv[1], &data, &len, &refable, &temp_utf8);
  if (data == nullptr && len > 0) return UbiStreamBaseMakeInt32(env, UV_EINVAL);
  return FinishSyncTtyWrite(wrap, argv[0], SyncTtyWrite(wrap, reinterpret_cast<const char*>(data), len));
#else
  return UbiLibuvStreamWriteBuffer(&wrap->base, argv[0], argv[1], nullptr, nullptr);
#endif
}

napi_value TtyWriteString(napi_env env, napi_callback_info info) {
  size_t argc = 2;
  napi_value argv[2] = {nullptr, nullptr};
  napi_value self = nullptr;
  void* data = nullptr;
  napi_get_cb_info(env, info, &argc, argv, &self, &data);
  TtyWrap* wrap = nullptr;
  if (self != nullptr) {
    napi_unwrap(env, self, reinterpret_cast<void**>(&wrap));
  }
  if (wrap == nullptr || !wrap->initialized || argc < 2) {
    return UbiStreamBaseMakeInt32(env, UV_EINVAL);
  }
#if defined(__wasi__)
  napi_value encoding = nullptr;
  const char* encoding_name = static_cast<const char*>(data);
  if (encoding_name != nullptr) {
    napi_create_string_utf8(env, encoding_name, NAPI_AUTO_LENGTH, &encoding);
  }
  napi_value buffer = UbiStreamBufferFromWithEncoding(env, argv[1], encoding);
  const uint8_t* bytes = nullptr;
  size_t len = 0;
  bool refable = false;
  std::string temp_utf8;
  UbiStreamBaseExtractByteSpan(env, buffer, &bytes, &len, &refable, &temp_utf8);
  if (bytes == nullptr && len > 0) return UbiStreamBaseMakeInt32(env, UV_EINVAL);
  return FinishSyncTtyWrite(wrap, argv[0], SyncTtyWrite(wrap, reinterpret_cast<const char*>(bytes), len));
#else
  return UbiLibuvStreamWriteString(&wrap->base,
                                   argv[0],
                                   argv[1],
                                   static_cast<const char*>(data),
                                   nullptr,
                                   nullptr);
#endif
}

napi_value TtyWriteV(napi_env env, napi_callback_info info) {
  size_t argc = 3;
  napi_value argv[3] = {nullptr, nullptr, nullptr};
  TtyWrap* wrap = nullptr;
  GetThis(env, info, &argc, argv, &wrap);
  if (wrap == nullptr || !wrap->initialized || argc < 2) return UbiStreamBaseMakeInt32(env, UV_EINVAL);

  bool all_buffers = false;
  if (argc > 2 && argv[2] != nullptr) {
    napi_get_value_bool(env, argv[2], &all_buffers);
  }
  return UbiLibuvStreamWriteV(&wrap->base, argv[0], argv[1], all_buffers, nullptr, nullptr);
}

napi_value TtyShutdown(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value argv[1] = {nullptr};
  TtyWrap* wrap = nullptr;
  GetThis(env, info, &argc, argv, &wrap);
  if (wrap == nullptr || !wrap->initialized || argc < 1 || argv[0] == nullptr) {
    return UbiStreamBaseMakeInt32(env, UV_EINVAL);
  }
  return UbiLibuvStreamShutdown(&wrap->base, argv[0]);
}

napi_value TtyClose(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value argv[1] = {nullptr};
  TtyWrap* wrap = nullptr;
  GetThis(env, info, &argc, argv, &wrap);
  if (wrap == nullptr) return UbiStreamBaseUndefined(env);
  napi_value close_callback = (argc > 0) ? argv[0] : nullptr;
  return UbiStreamBaseClose(&wrap->base, close_callback);
}

napi_value TtyRef(napi_env env, napi_callback_info info) {
  TtyWrap* wrap = nullptr;
  GetThis(env, info, nullptr, nullptr, &wrap);
  if (wrap != nullptr) UbiStreamBaseRef(&wrap->base);
  return UbiStreamBaseUndefined(env);
}

napi_value TtyUnref(napi_env env, napi_callback_info info) {
  TtyWrap* wrap = nullptr;
  GetThis(env, info, nullptr, nullptr, &wrap);
  if (wrap != nullptr) UbiStreamBaseUnref(&wrap->base);
  return UbiStreamBaseUndefined(env);
}

napi_value TtyHasRef(napi_env env, napi_callback_info info) {
  TtyWrap* wrap = nullptr;
  GetThis(env, info, nullptr, nullptr, &wrap);
  return UbiStreamBaseMakeBool(env, wrap != nullptr && UbiStreamBaseHasRef(&wrap->base));
}

napi_value TtyGetAsyncId(napi_env env, napi_callback_info info) {
  TtyWrap* wrap = nullptr;
  GetThis(env, info, nullptr, nullptr, &wrap);
  return UbiStreamBaseGetAsyncId(wrap != nullptr ? &wrap->base : nullptr);
}

napi_value TtyGetProviderType(napi_env env, napi_callback_info info) {
  TtyWrap* wrap = nullptr;
  GetThis(env, info, nullptr, nullptr, &wrap);
  return UbiStreamBaseGetProviderType(wrap != nullptr ? &wrap->base : nullptr);
}

napi_value TtyAsyncReset(napi_env env, napi_callback_info info) {
  TtyWrap* wrap = nullptr;
  GetThis(env, info, nullptr, nullptr, &wrap);
  return UbiStreamBaseAsyncReset(wrap != nullptr ? &wrap->base : nullptr);
}

napi_value TtyUseUserBuffer(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value argv[1] = {nullptr};
  TtyWrap* wrap = nullptr;
  GetThis(env, info, &argc, argv, &wrap);
  if (wrap == nullptr || argc < 1) {
    return UbiStreamBaseMakeInt32(env, UV_EINVAL);
  }
  return UbiStreamBaseUseUserBuffer(&wrap->base, argv[0]);
}

napi_value TtyGetOnRead(napi_env env, napi_callback_info info) {
  TtyWrap* wrap = nullptr;
  GetThis(env, info, nullptr, nullptr, &wrap);
  return UbiStreamBaseGetOnRead(wrap != nullptr ? &wrap->base : nullptr);
}

napi_value TtySetOnRead(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value argv[1] = {nullptr};
  TtyWrap* wrap = nullptr;
  GetThis(env, info, &argc, argv, &wrap);
  return UbiStreamBaseSetOnRead(wrap != nullptr ? &wrap->base : nullptr, argc > 0 ? argv[0] : nullptr);
}

napi_value TtyBytesReadGetter(napi_env env, napi_callback_info info) {
  TtyWrap* wrap = nullptr;
  GetThis(env, info, nullptr, nullptr, &wrap);
  return UbiStreamBaseGetBytesRead(wrap != nullptr ? &wrap->base : nullptr);
}

napi_value TtyBytesWrittenGetter(napi_env env, napi_callback_info info) {
  TtyWrap* wrap = nullptr;
  GetThis(env, info, nullptr, nullptr, &wrap);
  return UbiStreamBaseGetBytesWritten(wrap != nullptr ? &wrap->base : nullptr);
}

napi_value TtyFdGetter(napi_env env, napi_callback_info info) {
  TtyWrap* wrap = nullptr;
  GetThis(env, info, nullptr, nullptr, &wrap);
  return UbiStreamBaseMakeInt32(env, wrap != nullptr ? wrap->fd : -1);
}

napi_value TtyExternalStreamGetter(napi_env env, napi_callback_info info) {
  TtyWrap* wrap = nullptr;
  GetThis(env, info, nullptr, nullptr, &wrap);
  return UbiStreamBaseGetExternal(wrap != nullptr ? &wrap->base : nullptr);
}

napi_value TtyWriteQueueSizeGetter(napi_env env, napi_callback_info info) {
  TtyWrap* wrap = nullptr;
  GetThis(env, info, nullptr, nullptr, &wrap);
  return UbiStreamBaseGetWriteQueueSize(wrap != nullptr ? &wrap->base : nullptr);
}

}  // namespace

napi_value UbiInstallTtyWrapBinding(napi_env env) {
  TtyBindingState& state = EnsureBindingState(env);
  napi_value cached = nullptr;
  if (state.binding_ref != nullptr) {
    napi_get_reference_value(env, state.binding_ref, &cached);
  }
  if (cached != nullptr) return cached;

  napi_value binding = nullptr;
  if (napi_create_object(env, &binding) != napi_ok || binding == nullptr) return nullptr;

  constexpr napi_property_attributes kMethodAttrs =
      static_cast<napi_property_attributes>(napi_writable | napi_configurable);
  napi_property_descriptor tty_props[] = {
      {"getWindowSize", nullptr, TtyGetWindowSize, nullptr, nullptr, nullptr, kMethodAttrs, nullptr},
      {"setRawMode", nullptr, TtySetRawMode, nullptr, nullptr, nullptr, kMethodAttrs, nullptr},
      {"setBlocking", nullptr, TtySetBlocking, nullptr, nullptr, nullptr, kMethodAttrs, nullptr},
      {"readStart", nullptr, TtyReadStart, nullptr, nullptr, nullptr, kMethodAttrs, nullptr},
      {"readStop", nullptr, TtyReadStop, nullptr, nullptr, nullptr, kMethodAttrs, nullptr},
      {"writeBuffer", nullptr, TtyWriteBuffer, nullptr, nullptr, nullptr, kMethodAttrs, nullptr},
      {"writev", nullptr, TtyWriteV, nullptr, nullptr, nullptr, kMethodAttrs, nullptr},
      {"shutdown", nullptr, TtyShutdown, nullptr, nullptr, nullptr, kMethodAttrs, nullptr},
      {"writeLatin1String", nullptr, TtyWriteString, nullptr, nullptr, nullptr, kMethodAttrs,
       const_cast<char*>("latin1")},
      {"writeUtf8String", nullptr, TtyWriteString, nullptr, nullptr, nullptr, kMethodAttrs,
       const_cast<char*>("utf8")},
      {"writeAsciiString", nullptr, TtyWriteString, nullptr, nullptr, nullptr, kMethodAttrs,
       const_cast<char*>("ascii")},
      {"writeUcs2String", nullptr, TtyWriteString, nullptr, nullptr, nullptr, kMethodAttrs,
       const_cast<char*>("ucs2")},
      {"close", nullptr, TtyClose, nullptr, nullptr, nullptr, kMethodAttrs, nullptr},
      {"ref", nullptr, TtyRef, nullptr, nullptr, nullptr, kMethodAttrs, nullptr},
      {"unref", nullptr, TtyUnref, nullptr, nullptr, nullptr, kMethodAttrs, nullptr},
      {"hasRef", nullptr, TtyHasRef, nullptr, nullptr, nullptr, kMethodAttrs, nullptr},
      {"getAsyncId", nullptr, TtyGetAsyncId, nullptr, nullptr, nullptr, kMethodAttrs, nullptr},
      {"getProviderType", nullptr, TtyGetProviderType, nullptr, nullptr, nullptr, kMethodAttrs, nullptr},
      {"asyncReset", nullptr, TtyAsyncReset, nullptr, nullptr, nullptr, kMethodAttrs, nullptr},
      {"useUserBuffer", nullptr, TtyUseUserBuffer, nullptr, nullptr, nullptr, kMethodAttrs, nullptr},
      {"onread", nullptr, nullptr, TtyGetOnRead, TtySetOnRead, nullptr, napi_default, nullptr},
      {"bytesRead", nullptr, nullptr, TtyBytesReadGetter, nullptr, nullptr, napi_default, nullptr},
      {"bytesWritten", nullptr, nullptr, TtyBytesWrittenGetter, nullptr, nullptr, napi_default, nullptr},
      {"fd", nullptr, nullptr, TtyFdGetter, nullptr, nullptr, napi_default, nullptr},
      {"_externalStream", nullptr, nullptr, TtyExternalStreamGetter, nullptr, nullptr, napi_default, nullptr},
      {"writeQueueSize", nullptr, nullptr, TtyWriteQueueSizeGetter, nullptr, nullptr, napi_default, nullptr},
  };

  napi_value tty_ctor = nullptr;
  if (napi_define_class(env,
                        "TTY",
                        NAPI_AUTO_LENGTH,
                        TtyCtor,
                        nullptr,
                        sizeof(tty_props) / sizeof(tty_props[0]),
                        tty_props,
                        &tty_ctor) != napi_ok ||
      tty_ctor == nullptr) {
    return nullptr;
  }
  if (state.tty_ctor_ref != nullptr) napi_delete_reference(env, state.tty_ctor_ref);
  napi_create_reference(env, tty_ctor, 1, &state.tty_ctor_ref);

  napi_set_named_property(env, binding, "TTY", tty_ctor);
  napi_property_descriptor is_tty_desc = {
      "isTTY", nullptr, TtyIsTTY, nullptr, nullptr, nullptr, napi_default, nullptr,
  };
  napi_define_properties(env, binding, 1, &is_tty_desc);

  if (state.binding_ref != nullptr) napi_delete_reference(env, state.binding_ref);
  napi_create_reference(env, binding, 1, &state.binding_ref);
  return binding;
}
