#include "unode_url.h"

#include <string>
#include <string_view>
#include <vector>

#include "ada.h"

namespace {

constexpr char kHexDigits[] = "0123456789ABCDEF";

std::string ValueToUtf8(napi_env env, napi_value value) {
  size_t len = 0;
  if (napi_get_value_string_utf8(env, value, nullptr, 0, &len) != napi_ok) return {};
  std::string out(len + 1, '\0');
  size_t copied = 0;
  if (napi_get_value_string_utf8(env, value, out.data(), out.size(), &copied) != napi_ok) return {};
  out.resize(copied);
  return out;
}

void SetMethod(napi_env env, napi_value obj, const char* name, napi_callback cb) {
  napi_value fn = nullptr;
  if (napi_create_function(env, name, NAPI_AUTO_LENGTH, cb, nullptr, &fn) == napi_ok && fn != nullptr) {
    napi_set_named_property(env, obj, name, fn);
  }
}

bool IsUnreserved(unsigned char c) {
  switch (c) {
    case '\0':
    case '\t':
    case '\n':
    case '\r':
    case ' ':
    case '"':
    case '#':
    case '%':
    case '?':
    case '[':
    case '\\':
    case ']':
    case '^':
    case '|':
    case '~':
      return false;
    default:
      return true;
  }
}

std::string PercentEncodePath(std::string_view input, bool windows) {
  std::string out;
  out.reserve(input.size() + 16);
  for (unsigned char ch : input) {
    if (windows && ch == '\\') {
      out.push_back('/');
      continue;
    }
    if (IsUnreserved(ch)) {
      out.push_back(static_cast<char>(ch));
      continue;
    }
    out.push_back('%');
    out.push_back(kHexDigits[(ch >> 4) & 0xF]);
    out.push_back(kHexDigits[ch & 0xF]);
  }
  return out;
}

napi_value BindingDomainToASCII(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value argv[1] = {nullptr};
  if (napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr) != napi_ok || argc < 1) return nullptr;
  std::string input = ValueToUtf8(env, argv[0]);
  napi_value ret = nullptr;
  if (input.empty()) {
    napi_create_string_utf8(env, "", 0, &ret);
    return ret;
  }
  auto out = ada::parse<ada::url>("ws://x");
  std::string result;
  if (out && out->set_hostname(input)) result = out->get_hostname();
  napi_create_string_utf8(env, result.c_str(), result.size(), &ret);
  return ret;
}

napi_value BindingDomainToUnicode(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value argv[1] = {nullptr};
  if (napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr) != napi_ok || argc < 1) return nullptr;
  std::string input = ValueToUtf8(env, argv[0]);
  napi_value ret = nullptr;
  if (input.empty()) {
    napi_create_string_utf8(env, "", 0, &ret);
    return ret;
  }
  auto out = ada::parse<ada::url>("ws://x");
  std::string result;
  if (out && out->set_hostname(input)) result = ada::idna::to_unicode(out->get_hostname());
  napi_create_string_utf8(env, result.c_str(), result.size(), &ret);
  return ret;
}

napi_value BindingPathToFileURL(napi_env env, napi_callback_info info) {
  size_t argc = 3;
  napi_value argv[3] = {nullptr, nullptr, nullptr};
  if (napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr) != napi_ok || argc < 2) return nullptr;
  std::string input = ValueToUtf8(env, argv[0]);
  bool windows = false;
  napi_get_value_bool(env, argv[1], &windows);
  std::string encoded = "file://" + PercentEncodePath(input, windows);
  auto out = ada::parse<ada::url_aggregator>(encoded, nullptr);
  napi_value ret = nullptr;
  if (!out) {
    napi_create_string_utf8(env, encoded.c_str(), encoded.size(), &ret);
    return ret;
  }
  if (windows && argc > 2 && argv[2] != nullptr) {
    std::string host = ValueToUtf8(env, argv[2]);
    if (!host.empty()) out->set_hostname(host);
  }
  const std::string href(out->get_href());
  napi_create_string_utf8(env, href.c_str(), href.size(), &ret);
  return ret;
}

napi_value BindingFormat(napi_env env, napi_callback_info info) {
  size_t argc = 5;
  napi_value argv[5] = {nullptr, nullptr, nullptr, nullptr, nullptr};
  if (napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr) != napi_ok || argc < 1) return nullptr;
  std::string href = ValueToUtf8(env, argv[0]);
  bool fragment = true;
  bool unicode = false;
  bool search = true;
  bool auth = true;
  if (argc > 1 && argv[1] != nullptr) napi_get_value_bool(env, argv[1], &fragment);
  if (argc > 2 && argv[2] != nullptr) napi_get_value_bool(env, argv[2], &unicode);
  if (argc > 3 && argv[3] != nullptr) napi_get_value_bool(env, argv[3], &search);
  if (argc > 4 && argv[4] != nullptr) napi_get_value_bool(env, argv[4], &auth);

  auto parsed = ada::parse<ada::url_aggregator>(href, nullptr);
  napi_value ret = nullptr;
  if (!parsed) {
    napi_create_string_utf8(env, href.c_str(), href.size(), &ret);
    return ret;
  }
  if (!fragment) parsed->set_hash("");
  if (!search) parsed->set_search("");
  if (!auth) {
    parsed->set_username("");
    parsed->set_password("");
  }
  if (unicode) {
    const std::string host(parsed->get_hostname());
    parsed->set_hostname(ada::idna::to_unicode(host));
  }
  const std::string result(parsed->get_href());
  napi_create_string_utf8(env, result.c_str(), result.size(), &ret);
  return ret;
}

napi_value BindingParse(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value argv[1] = {nullptr};
  if (napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr) != napi_ok || argc < 1) return nullptr;
  std::string href = ValueToUtf8(env, argv[0]);
  napi_value out = nullptr;
  if (napi_create_object(env, &out) != napi_ok || out == nullptr) return nullptr;
  auto parsed = ada::parse<ada::url_aggregator>(href, nullptr);
  if (!parsed) return out;

  auto set_string = [&](const char* key, std::string_view v) {
    napi_value val = nullptr;
    if (napi_create_string_utf8(env, std::string(v).c_str(), v.size(), &val) == napi_ok && val != nullptr) {
      napi_set_named_property(env, out, key, val);
    }
  };

  set_string("protocol", parsed->get_protocol());
  set_string("hostname", parsed->get_hostname());
  set_string("pathname", parsed->get_pathname());
  set_string("search", parsed->get_search());
  set_string("hash", parsed->get_hash());
  set_string("username", parsed->get_username());
  set_string("password", parsed->get_password());
  set_string("href", parsed->get_href());
  if (parsed->has_port()) {
    napi_value port = nullptr;
    const std::string port_str(parsed->get_port());
    napi_create_string_utf8(env, port_str.c_str(), NAPI_AUTO_LENGTH, &port);
    if (port != nullptr) napi_set_named_property(env, out, "port", port);
  } else {
    napi_value empty = nullptr;
    napi_create_string_utf8(env, "", 0, &empty);
    if (empty != nullptr) napi_set_named_property(env, out, "port", empty);
  }
  return out;
}

}  // namespace

void UnodeInstallUrlBinding(napi_env env) {
  napi_value binding = nullptr;
  if (napi_create_object(env, &binding) != napi_ok || binding == nullptr) return;

  SetMethod(env, binding, "format", BindingFormat);
  SetMethod(env, binding, "domainToASCII", BindingDomainToASCII);
  SetMethod(env, binding, "domainToUnicode", BindingDomainToUnicode);
  SetMethod(env, binding, "pathToFileURL", BindingPathToFileURL);
  SetMethod(env, binding, "parse", BindingParse);

  napi_value global = nullptr;
  if (napi_get_global(env, &global) != napi_ok || global == nullptr) return;
  napi_set_named_property(env, global, "__unode_url", binding);
}
