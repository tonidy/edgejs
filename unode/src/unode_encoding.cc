#include "unode_encoding.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "simdutf.h"

namespace {

bool IsBase64Char(char c, bool url_mode) {
  if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
    return true;
  }
  if (c == '=') return true;
  if (c == '+' || c == '/') return true;
  if (url_mode && (c == '-' || c == '_')) return true;
  return false;
}

std::string NormalizeForgivingBase64(const std::string& input, bool url_mode) {
  std::string compact;
  compact.reserve(input.size());
  bool saw_padding = false;

  for (char c : input) {
    if (c == '=') {
      saw_padding = true;
      break;
    }
    if (!IsBase64Char(c, url_mode)) {
      // WHATWG forgiving behavior: ignore out-of-alphabet bytes.
      continue;
    }
    if (url_mode) {
      if (c == '-') c = '+';
      else if (c == '_') c = '/';
    }
    compact.push_back(c);
  }

  const size_t rem = compact.size() % 4;
  if (rem == 1) {
    // WHATWG forgiving decode rejects this shape.
    return "";
  }
  if (rem == 2) compact.append("==");
  else if (rem == 3) compact.push_back('=');
  (void)saw_padding;
  return compact;
}

bool ExtractBytesFromValue(napi_env env, napi_value value, const char** data, size_t* len) {
  if (value == nullptr || data == nullptr || len == nullptr) return false;
  bool is_buffer = false;
  if (napi_is_buffer(env, value, &is_buffer) != napi_ok) return false;
  if (is_buffer) {
    void* ptr = nullptr;
    if (napi_get_buffer_info(env, value, &ptr, len) != napi_ok || ptr == nullptr) return false;
    *data = static_cast<const char*>(ptr);
    return true;
  }

  bool is_typed = false;
  if (napi_is_typedarray(env, value, &is_typed) != napi_ok || !is_typed) return false;
  napi_typedarray_type type = napi_uint8_array;
  size_t element_len = 0;
  void* ptr = nullptr;
  napi_value arraybuffer = nullptr;
  size_t byte_offset = 0;
  if (napi_get_typedarray_info(
          env, value, &type, &element_len, &ptr, &arraybuffer, &byte_offset) != napi_ok ||
      ptr == nullptr) {
    return false;
  }

  size_t bytes_per_element = 1;
  switch (type) {
    case napi_int16_array:
    case napi_uint16_array:
      bytes_per_element = 2;
      break;
    case napi_int32_array:
    case napi_uint32_array:
    case napi_float32_array:
      bytes_per_element = 4;
      break;
    case napi_float64_array:
      bytes_per_element = 8;
      break;
    default:
      bytes_per_element = 1;
      break;
  }
  *data = static_cast<const char*>(ptr);
  *len = element_len * bytes_per_element;
  return true;
}

napi_value MakeUint8Array(napi_env env, const char* data, size_t len) {
  napi_value arraybuffer = nullptr;
  void* out = nullptr;
  if (napi_create_arraybuffer(env, len, &out, &arraybuffer) != napi_ok || arraybuffer == nullptr) {
    return nullptr;
  }
  if (len > 0 && out != nullptr && data != nullptr) {
    std::memcpy(out, data, len);
  }
  napi_value typed = nullptr;
  if (napi_create_typedarray(env, napi_uint8_array, len, arraybuffer, 0, &typed) != napi_ok) {
    return nullptr;
  }
  return typed;
}

napi_value BindingEncodeUtf8(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value argv[1] = {nullptr};
  if (napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr) != napi_ok || argc < 1) {
    return nullptr;
  }

  size_t utf16_len = 0;
  if (napi_get_value_string_utf16(env, argv[0], nullptr, 0, &utf16_len) != napi_ok) {
    return nullptr;
  }
  std::vector<char16_t> input(utf16_len + 1);
  size_t copied = 0;
  if (napi_get_value_string_utf16(env, argv[0], input.data(), input.size(), &copied) != napi_ok) {
    return nullptr;
  }
  const size_t out_len = simdutf::utf8_length_from_utf16(input.data(), copied);
  std::vector<char> out(out_len);
  const size_t written = simdutf::convert_utf16_to_utf8(input.data(), copied, out.data());
  if (written != out_len) {
    return nullptr;
  }
  return MakeUint8Array(env, out.data(), out.size());
}

napi_value BindingUtf8ByteLength(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value argv[1] = {nullptr};
  if (napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr) != napi_ok || argc < 1) {
    return nullptr;
  }
  size_t utf16_len = 0;
  if (napi_get_value_string_utf16(env, argv[0], nullptr, 0, &utf16_len) != napi_ok) {
    return nullptr;
  }
  std::vector<char16_t> input(utf16_len + 1);
  size_t copied = 0;
  if (napi_get_value_string_utf16(env, argv[0], input.data(), input.size(), &copied) != napi_ok) {
    return nullptr;
  }
  const size_t out_len = simdutf::utf8_length_from_utf16(input.data(), copied);
  napi_value out = nullptr;
  if (napi_create_uint32(env, static_cast<uint32_t>(out_len), &out) != napi_ok) return nullptr;
  return out;
}

napi_value BindingValidateUtf8(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value argv[1] = {nullptr};
  if (napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr) != napi_ok || argc < 1) {
    return nullptr;
  }
  const char* data = nullptr;
  size_t len = 0;
  if (!ExtractBytesFromValue(env, argv[0], &data, &len)) return nullptr;
  napi_value out = nullptr;
  if (napi_get_boolean(env, simdutf::validate_utf8(data, len), &out) != napi_ok) return nullptr;
  return out;
}

napi_value BindingDecodeUtf8(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value argv[1] = {nullptr};
  if (napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr) != napi_ok || argc < 1) {
    return nullptr;
  }
  const char* data = nullptr;
  size_t len = 0;
  if (!ExtractBytesFromValue(env, argv[0], &data, &len)) return nullptr;

  const size_t utf16_len = simdutf::utf16_length_from_utf8(data, len);
  std::vector<char16_t> out(utf16_len);
  const size_t written = simdutf::convert_utf8_to_utf16(data, len, out.data());
  if (written != utf16_len) {
    return nullptr;
  }

  napi_value str = nullptr;
  if (napi_create_string_utf16(env, out.data(), out.size(), &str) != napi_ok) return nullptr;
  return str;
}

napi_value BindingEncodeBase64(napi_env env, napi_callback_info info) {
  size_t argc = 2;
  napi_value argv[2] = {nullptr, nullptr};
  if (napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr) != napi_ok || argc < 1) {
    return nullptr;
  }
  const char* data = nullptr;
  size_t len = 0;
  if (!ExtractBytesFromValue(env, argv[0], &data, &len)) return nullptr;
  bool is_url = false;
  if (argc >= 2 && argv[1] != nullptr) {
    napi_get_value_bool(env, argv[1], &is_url);
  }

  const size_t out_len = is_url
                             ? simdutf::base64_length_from_binary(len, simdutf::base64_url)
                             : simdutf::base64_length_from_binary(len);
  std::string out(out_len, '\0');
  const size_t written = is_url
                             ? simdutf::binary_to_base64(data, len, out.data(), simdutf::base64_url)
                             : simdutf::binary_to_base64(data, len, out.data());
  if (written != out_len) return nullptr;
  napi_value str = nullptr;
  if (napi_create_string_utf8(env, out.data(), written, &str) != napi_ok) return nullptr;
  return str;
}

napi_value BindingDecodeBase64(napi_env env, napi_callback_info info) {
  size_t argc = 2;
  napi_value argv[2] = {nullptr, nullptr};
  if (napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr) != napi_ok || argc < 1) {
    return nullptr;
  }

  size_t in_len = 0;
  if (napi_get_value_string_utf8(env, argv[0], nullptr, 0, &in_len) != napi_ok) return nullptr;
  std::string input(in_len + 1, '\0');
  size_t copied = 0;
  if (napi_get_value_string_utf8(env, argv[0], input.data(), input.size(), &copied) != napi_ok) return nullptr;
  input.resize(copied);

  bool is_url = false;
  if (argc >= 2 && argv[1] != nullptr) {
    napi_get_value_bool(env, argv[1], &is_url);
  }

  const size_t max_len = simdutf::maximal_binary_length_from_base64(input.data(), input.size());
  std::vector<char> out(max_len);
  size_t out_len = max_len;
  simdutf::result result = is_url
                               ? simdutf::base64_to_binary_safe(
                                     input.data(), input.size(), out.data(), out_len, simdutf::base64_url)
                               : simdutf::base64_to_binary_safe(
                                     input.data(), input.size(), out.data(), out_len);
  if (result.error != simdutf::error_code::SUCCESS) {
    const std::string forgiving = NormalizeForgivingBase64(input, is_url);
    if (forgiving.empty()) return MakeUint8Array(env, nullptr, 0);
    const size_t fallback_max = simdutf::maximal_binary_length_from_base64(forgiving.data(), forgiving.size());
    std::vector<char> fallback_out(fallback_max);
    size_t fallback_len = fallback_max;
    simdutf::result fallback_result = simdutf::base64_to_binary_safe(
        forgiving.data(), forgiving.size(), fallback_out.data(), fallback_len);
    if (fallback_result.error != simdutf::error_code::SUCCESS) {
      return MakeUint8Array(env, nullptr, 0);
    }
    return MakeUint8Array(env, fallback_out.data(), fallback_len);
  }
  return MakeUint8Array(env, out.data(), out_len);
}

void SetMethod(napi_env env, napi_value obj, const char* name, napi_callback cb) {
  napi_value fn = nullptr;
  if (napi_create_function(env, name, NAPI_AUTO_LENGTH, cb, nullptr, &fn) == napi_ok && fn != nullptr) {
    napi_set_named_property(env, obj, name, fn);
  }
}

}  // namespace

void UnodeInstallEncodingBinding(napi_env env) {
  napi_value binding = nullptr;
  if (napi_create_object(env, &binding) != napi_ok || binding == nullptr) return;
  SetMethod(env, binding, "encodeUtf8", BindingEncodeUtf8);
  SetMethod(env, binding, "decodeUtf8", BindingDecodeUtf8);
  SetMethod(env, binding, "utf8ByteLength", BindingUtf8ByteLength);
  SetMethod(env, binding, "validateUtf8", BindingValidateUtf8);
  SetMethod(env, binding, "encodeBase64", BindingEncodeBase64);
  SetMethod(env, binding, "decodeBase64", BindingDecodeBase64);

  napi_value global = nullptr;
  if (napi_get_global(env, &global) != napi_ok || global == nullptr) return;
  napi_set_named_property(env, global, "__unode_encoding", binding);
}
