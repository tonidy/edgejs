#include "unode_fs.h"

#include <cerrno>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#endif
#include <sys/stat.h>

#include "js_native_api.h"
#include "uv.h"

namespace {

std::string PathFromValue(napi_env env, napi_value value) {
  size_t length = 0;
  if (napi_get_value_string_utf8(env, value, nullptr, 0, &length) != napi_ok) {
    return "";
  }
  std::string out(length + 1, '\0');
  size_t copied = 0;
  if (napi_get_value_string_utf8(env, value, out.data(), out.size(), &copied) !=
      napi_ok) {
    return "";
  }
  out.resize(copied);
  return out;
}

void ThrowUVException(napi_env env, int errorno, const char* syscall,
                      const char* path) {
  const char* code = uv_err_name(errorno);
  const char* msg = uv_strerror(errorno);
  if (code == nullptr) code = "UNKNOWN";
  if (msg == nullptr) msg = "unknown error";

  std::string message = std::string(code) + ": " + msg + ", " + syscall;
  if (path != nullptr && path[0] != '\0') {
    message += " '";
    message += path;
    message += "'";
  }

  napi_value msg_val = nullptr;
  if (napi_create_string_utf8(env, message.c_str(), NAPI_AUTO_LENGTH,
                               &msg_val) != napi_ok ||
      msg_val == nullptr) {
    return;
  }
  napi_value error = nullptr;
  if (napi_create_error(env, nullptr, msg_val, &error) != napi_ok ||
      error == nullptr) {
    return;
  }
  napi_value code_val = nullptr;
  if (napi_create_string_utf8(env, code, NAPI_AUTO_LENGTH, &code_val) ==
      napi_ok) {
    napi_set_named_property(env, error, "code", code_val);
  }
  napi_value errno_val = nullptr;
  if (napi_create_int32(env, errorno, &errno_val) == napi_ok) {
    napi_set_named_property(env, error, "errno", errno_val);
  }
  napi_value syscall_val = nullptr;
  if (napi_create_string_utf8(env, syscall, NAPI_AUTO_LENGTH, &syscall_val) ==
      napi_ok) {
    napi_set_named_property(env, error, "syscall", syscall_val);
  }
  if (path != nullptr && path[0] != '\0') {
    napi_value path_val = nullptr;
    if (napi_create_string_utf8(env, path, NAPI_AUTO_LENGTH, &path_val) ==
        napi_ok) {
      napi_set_named_property(env, error, "path", path_val);
    }
  }
  napi_throw(env, error);
}

void ThrowErrnoException(napi_env env, int errorno, const char* syscall,
                         const char* message, const char* path) {
  std::string msg = message != nullptr ? message : strerror(errorno);
  if (path != nullptr && path[0] != '\0') {
    msg += " '";
    msg += path;
    msg += "'";
  }
  napi_value msg_val = nullptr;
  if (napi_create_string_utf8(env, msg.c_str(), NAPI_AUTO_LENGTH, &msg_val) !=
          napi_ok ||
      msg_val == nullptr) {
    return;
  }
  napi_value error = nullptr;
  if (napi_create_error(env, nullptr, msg_val, &error) != napi_ok ||
      error == nullptr) {
    return;
  }
  const char* code =
      uv_err_name(errorno < 0 ? errorno : -errorno);
  if (code == nullptr) code = "ERR_UNKNOWN_ERRNO";
  napi_value code_val = nullptr;
  if (napi_create_string_utf8(env, code, NAPI_AUTO_LENGTH, &code_val) ==
      napi_ok) {
    napi_set_named_property(env, error, "code", code_val);
  }
  napi_value errno_val = nullptr;
  if (napi_create_int32(env, errorno, &errno_val) == napi_ok) {
    napi_set_named_property(env, error, "errno", errno_val);
  }
  napi_value syscall_val = nullptr;
  if (napi_create_string_utf8(env, syscall, NAPI_AUTO_LENGTH, &syscall_val) ==
      napi_ok) {
    napi_set_named_property(env, error, "syscall", syscall_val);
  }
  if (path != nullptr && path[0] != '\0') {
    napi_value path_val = nullptr;
    if (napi_create_string_utf8(env, path, NAPI_AUTO_LENGTH, &path_val) ==
        napi_ok) {
      napi_set_named_property(env, error, "path", path_val);
    }
  }
  napi_throw(env, error);
}

napi_value BindingReadFileUtf8(napi_env env, napi_callback_info info) {
  size_t argc = 2;
  napi_value argv[2] = {nullptr};
  if (napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr) != napi_ok ||
      argc < 2) {
    return nullptr;
  }
  std::string path = PathFromValue(env, argv[0]);
  if (path.empty()) {
    return nullptr;
  }
  int32_t flags = 0;
  if (napi_get_value_int32(env, argv[1], &flags) != napi_ok) {
    return nullptr;
  }

  uv_fs_t req;
  uv_file file = uv_fs_open(nullptr, &req, path.c_str(), flags, 0666, nullptr);
  uv_fs_req_cleanup(&req);
  if (file < 0) {
    ThrowUVException(env, static_cast<int>(file), "open", path.c_str());
    return nullptr;
  }

  std::string result;
  char buffer[8192];
  uv_buf_t buf = uv_buf_init(buffer, sizeof(buffer));
  while (true) {
    uv_fs_read(nullptr, &req, file, &buf, 1, -1, nullptr);
    ssize_t r = req.result;
    uv_fs_req_cleanup(&req);
    if (r < 0) {
      uv_fs_close(nullptr, &req, file, nullptr);
      uv_fs_req_cleanup(&req);
      ThrowUVException(env, static_cast<int>(r), "read", nullptr);
      return nullptr;
    }
    if (r <= 0) break;
    result.append(buf.base, static_cast<size_t>(r));
  }

  uv_fs_close(nullptr, &req, file, nullptr);
  uv_fs_req_cleanup(&req);

  napi_value out = nullptr;
  if (napi_create_string_utf8(env, result.c_str(), result.size(), &out) !=
      napi_ok) {
    return nullptr;
  }
  return out;
}

napi_value BindingWriteFileUtf8(napi_env env, napi_callback_info info) {
  size_t argc = 4;
  napi_value argv[4] = {nullptr};
  if (napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr) != napi_ok ||
      argc < 4) {
    return nullptr;
  }
  std::string path = PathFromValue(env, argv[0]);
  if (path.empty()) {
    return nullptr;
  }
  size_t data_len = 0;
  if (napi_get_value_string_utf8(env, argv[1], nullptr, 0, &data_len) !=
      napi_ok) {
    return nullptr;
  }
  std::string data(data_len + 1, '\0');
  size_t copied = 0;
  if (napi_get_value_string_utf8(env, argv[1], data.data(), data.size(),
                                 &copied) != napi_ok) {
    return nullptr;
  }
  data.resize(copied);
  int32_t flags = 0;
  int32_t mode = 0;
  if (napi_get_value_int32(env, argv[2], &flags) != napi_ok ||
      napi_get_value_int32(env, argv[3], &mode) != napi_ok) {
    return nullptr;
  }

  uv_fs_t req;
  uv_file file =
      uv_fs_open(nullptr, &req, path.c_str(), flags, mode, nullptr);
  uv_fs_req_cleanup(&req);
  if (file < 0) {
    ThrowUVException(env, static_cast<int>(file), "open", path.c_str());
    return nullptr;
  }

  size_t offset = 0;
  const size_t length = data.size();
  while (offset < length) {
    uv_buf_t uvbuf =
        uv_buf_init(const_cast<char*>(data.data() + offset),
                    static_cast<unsigned int>(length - offset));
    uv_fs_write(nullptr, &req, file, &uvbuf, 1, -1, nullptr);
    ssize_t bytes_written = req.result;
    uv_fs_req_cleanup(&req);
    if (bytes_written < 0) {
      uv_fs_close(nullptr, &req, file, nullptr);
      uv_fs_req_cleanup(&req);
      ThrowUVException(env, static_cast<int>(bytes_written), "write",
                       path.c_str());
      return nullptr;
    }
    offset += static_cast<size_t>(bytes_written);
  }

  uv_fs_close(nullptr, &req, file, nullptr);
  uv_fs_req_cleanup(&req);
  return nullptr;
}

const char kPathSeparator = '/';

bool MkdirpSync(std::string path, int mode, std::string* first_path,
                int* out_err) {
  std::vector<std::string> stack;
  stack.push_back(std::move(path));
  uv_fs_t req;

  while (!stack.empty()) {
    std::string next_path = std::move(stack.back());
    stack.pop_back();

    int err = uv_fs_mkdir(nullptr, &req, next_path.c_str(), mode, nullptr);
    uv_fs_req_cleanup(&req);

    switch (err) {
      case 0:
        if (first_path && first_path->empty()) {
          *first_path = next_path;
        }
        break;
      case UV_EACCES:
      case UV_ENOSPC:
      case UV_ENOTDIR:
      case UV_EPERM:
        *out_err = err;
        return false;
      case UV_ENOENT: {
        size_t sep = next_path.find_last_of(kPathSeparator);
        std::string dirname =
            (sep != std::string::npos) ? next_path.substr(0, sep) : next_path;
        if (dirname != next_path) {
          stack.push_back(std::move(next_path));
          stack.push_back(std::move(dirname));
        } else {
          *out_err = UV_EEXIST;
          return false;
        }
        break;
      }
      default: {
        int stat_err = uv_fs_stat(nullptr, &req, next_path.c_str(), nullptr);
        uv_fs_req_cleanup(&req);
        if (stat_err == 0 && (req.statbuf.st_mode & S_IFMT) != S_IFDIR) {
          if (err == UV_EEXIST && !stack.empty()) {
            *out_err = UV_ENOTDIR;
          } else {
            *out_err = UV_EEXIST;
          }
          return false;
        }
        if (stat_err < 0) {
          *out_err = stat_err;
          return false;
        }
        break;
      }
    }
  }
  *out_err = 0;
  return true;
}

napi_value BindingMkdir(napi_env env, napi_callback_info info) {
  size_t argc = 3;
  napi_value argv[3] = {nullptr};
  if (napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr) != napi_ok ||
      argc < 3) {
    return nullptr;
  }
  std::string path = PathFromValue(env, argv[0]);
  if (path.empty()) {
    return nullptr;
  }
  int32_t mode = 0;
  bool recursive = false;
  if (napi_get_value_int32(env, argv[1], &mode) != napi_ok) {
    return nullptr;
  }
  if (napi_get_value_bool(env, argv[2], &recursive) != napi_ok) {
    return nullptr;
  }

  if (recursive) {
    std::string first_path;
    int err = 0;
    if (!MkdirpSync(path, mode, &first_path, &err)) {
      ThrowUVException(env, err, "mkdir", path.c_str());
      return nullptr;
    }
    if (!first_path.empty()) {
      napi_value ret = nullptr;
      if (napi_create_string_utf8(env, first_path.c_str(), NAPI_AUTO_LENGTH,
                                  &ret) == napi_ok) {
        return ret;
      }
    }
    return nullptr;
  }

  uv_fs_t req;
  int err = uv_fs_mkdir(nullptr, &req, path.c_str(), mode, nullptr);
  uv_fs_req_cleanup(&req);
  if (err < 0) {
    ThrowUVException(env, err, "mkdir", path.c_str());
    return nullptr;
  }
  return nullptr;
}

napi_value BindingRmSync(napi_env env, napi_callback_info info) {
  size_t argc = 4;
  napi_value argv[4] = {nullptr};
  if (napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr) != napi_ok ||
      argc < 4) {
    return nullptr;
  }
  std::string path = PathFromValue(env, argv[0]);
  if (path.empty()) {
    return nullptr;
  }
  int32_t max_retries = 0;
  bool recursive = false;
  int32_t retry_delay = 0;
  if (napi_get_value_int32(env, argv[1], &max_retries) != napi_ok ||
      napi_get_value_bool(env, argv[2], &recursive) != napi_ok ||
      napi_get_value_int32(env, argv[3], &retry_delay) != napi_ok) {
    return nullptr;
  }

  namespace fs = std::filesystem;
  std::error_code error;
  auto file_status = fs::symlink_status(path, error);
  if (file_status.type() == fs::file_type::not_found) {
    return nullptr;
  }
  if (file_status.type() == fs::file_type::directory && !recursive) {
    ThrowErrnoException(env, EISDIR, "rm", "Path is a directory", path.c_str());
    return nullptr;
  }

  auto can_retry = [](const std::error_code& ec) {
    return ec == std::errc::device_or_resource_busy ||
           ec == std::errc::too_many_files_open ||
           ec == std::errc::too_many_files_open_in_system ||
           ec == std::errc::directory_not_empty ||
           ec == std::errc::operation_not_permitted;
  };

  int i = 1;
  while (max_retries >= 0) {
    error.clear();
    if (recursive) {
      fs::remove_all(path, error);
    } else {
      fs::remove(path, error);
    }
    if (!error || error == std::errc::no_such_file_or_directory) {
      return nullptr;
    }
    if (!can_retry(error)) {
      break;
    }
    if (retry_delay > 0) {
#ifdef _WIN32
      Sleep(static_cast<DWORD>(i * retry_delay / 1000));
#else
      sleep(static_cast<unsigned>(i * retry_delay / 1000));
#endif
    }
    max_retries--;
    i++;
  }

  int errno_val = error.value();
#ifdef _WIN32
  int permission_denied_errno = EPERM;
#else
  int permission_denied_errno = EACCES;
#endif
  if (error == std::errc::operation_not_permitted) {
    ThrowErrnoException(env, EPERM, "rm", "Operation not permitted",
                        path.c_str());
  } else if (error == std::errc::directory_not_empty) {
    ThrowErrnoException(env, ENOTEMPTY, "rm", "Directory not empty",
                        path.c_str());
  } else if (error == std::errc::not_a_directory) {
    ThrowErrnoException(env, ENOTDIR, "rm", "Not a directory", path.c_str());
  } else if (error == std::errc::permission_denied) {
    ThrowErrnoException(env, permission_denied_errno, "rm", "Permission denied",
                        path.c_str());
  } else {
    ThrowErrnoException(env, UV_UNKNOWN, "rm",
                       ("Unknown error: " + error.message()).c_str(),
                       path.c_str());
  }
  return nullptr;
}

napi_value BindingReaddir(napi_env env, napi_callback_info info) {
  size_t argc = 2;
  napi_value argv[2] = {nullptr};
  if (napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr) != napi_ok ||
      argc < 2) {
    return nullptr;
  }
  std::string path = PathFromValue(env, argv[0]);
  if (path.empty()) {
    return nullptr;
  }
  bool with_file_types = false;
  if (napi_get_value_bool(env, argv[1], &with_file_types) != napi_ok) {
    return nullptr;
  }

  uv_fs_t req;
  int err = uv_fs_scandir(nullptr, &req, path.c_str(), 0, nullptr);
  if (err < 0) {
    uv_fs_req_cleanup(&req);
    ThrowUVException(env, err, "scandir", path.c_str());
    return nullptr;
  }

  std::vector<std::string> names;
  std::vector<int> types;
  uv_dirent_t ent;
  while (true) {
    int r = uv_fs_scandir_next(&req, &ent);
    if (r == UV_EOF) break;
    if (r < 0) {
      uv_fs_req_cleanup(&req);
      ThrowUVException(env, r, "scandir", path.c_str());
      return nullptr;
    }
    names.emplace_back(ent.name);
    if (with_file_types) {
      types.push_back(static_cast<int>(ent.type));
    }
  }
  uv_fs_req_cleanup(&req);

  napi_value names_array = nullptr;
  if (napi_create_array_with_length(env, names.size(), &names_array) !=
      napi_ok) {
    return nullptr;
  }
  for (size_t i = 0; i < names.size(); i++) {
    napi_value name_val = nullptr;
    if (napi_create_string_utf8(env, names[i].c_str(), NAPI_AUTO_LENGTH,
                                &name_val) == napi_ok) {
      napi_set_element(env, names_array, static_cast<uint32_t>(i), name_val);
    }
  }

  if (!with_file_types) {
    return names_array;
  }

  napi_value types_array = nullptr;
  if (napi_create_array_with_length(env, types.size(), &types_array) !=
      napi_ok) {
    return names_array;
  }
  for (size_t i = 0; i < types.size(); i++) {
    napi_value type_val = nullptr;
    if (napi_create_int32(env, types[i], &type_val) == napi_ok) {
      napi_set_element(env, types_array, static_cast<uint32_t>(i), type_val);
    }
  }

  napi_value result = nullptr;
  if (napi_create_array_with_length(env, 2, &result) != napi_ok) {
    return names_array;
  }
  napi_set_element(env, result, 0, names_array);
  napi_set_element(env, result, 1, types_array);
  return result;
}

napi_value BindingRealpath(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value argv[1] = {nullptr};
  if (napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr) != napi_ok ||
      argc < 1) {
    return nullptr;
  }
  std::string path = PathFromValue(env, argv[0]);
  if (path.empty()) {
    return nullptr;
  }

  uv_fs_t req;
  int err = uv_fs_realpath(nullptr, &req, path.c_str(), nullptr);
  if (err < 0) {
    uv_fs_req_cleanup(&req);
    ThrowUVException(env, err, "realpath", path.c_str());
    return nullptr;
  }
  const char* resolved = static_cast<const char*>(req.ptr);
  napi_value out = nullptr;
  if (napi_create_string_utf8(env, resolved, NAPI_AUTO_LENGTH, &out) !=
      napi_ok) {
    uv_fs_req_cleanup(&req);
    return nullptr;
  }
  uv_fs_req_cleanup(&req);
  return out;
}

void SetMethod(napi_env env, napi_value obj, const char* name,
               napi_callback cb) {
  napi_value fn = nullptr;
  if (napi_create_function(env, name, NAPI_AUTO_LENGTH, cb, nullptr, &fn) !=
      napi_ok) {
    return;
  }
  napi_set_named_property(env, obj, name, fn);
}

void SetInt32Constant(napi_env env, napi_value obj, const char* name,
                      int32_t value) {
  napi_value val = nullptr;
  if (napi_create_int32(env, value, &val) != napi_ok) return;
  napi_set_named_property(env, obj, name, val);
}

}  // namespace

void UnodeInstallFsBinding(napi_env env) {
  napi_value binding = nullptr;
  if (napi_create_object(env, &binding) != napi_ok || binding == nullptr) {
    return;
  }

  SetMethod(env, binding, "readFileUtf8", BindingReadFileUtf8);
  SetMethod(env, binding, "writeFileUtf8", BindingWriteFileUtf8);
  SetMethod(env, binding, "mkdir", BindingMkdir);
  SetMethod(env, binding, "rmSync", BindingRmSync);
  SetMethod(env, binding, "readdir", BindingReaddir);
  SetMethod(env, binding, "realpath", BindingRealpath);

  SetInt32Constant(env, binding, "O_RDONLY", UV_FS_O_RDONLY);
  SetInt32Constant(env, binding, "O_WRONLY", UV_FS_O_WRONLY);
  SetInt32Constant(env, binding, "O_RDWR", UV_FS_O_RDWR);
  SetInt32Constant(env, binding, "O_CREAT", UV_FS_O_CREAT);
  SetInt32Constant(env, binding, "O_TRUNC", UV_FS_O_TRUNC);
  SetInt32Constant(env, binding, "O_APPEND", UV_FS_O_APPEND);
  SetInt32Constant(env, binding, "O_EXCL", UV_FS_O_EXCL);
  SetInt32Constant(env, binding, "O_SYNC", UV_FS_O_SYNC);

  SetInt32Constant(env, binding, "UV_DIRENT_UNKNOWN", UV_DIRENT_UNKNOWN);
  SetInt32Constant(env, binding, "UV_DIRENT_FILE", UV_DIRENT_FILE);
  SetInt32Constant(env, binding, "UV_DIRENT_DIR", UV_DIRENT_DIR);
  SetInt32Constant(env, binding, "UV_DIRENT_LINK", UV_DIRENT_LINK);
  SetInt32Constant(env, binding, "UV_DIRENT_FIFO", UV_DIRENT_FIFO);
  SetInt32Constant(env, binding, "UV_DIRENT_SOCKET", UV_DIRENT_SOCKET);
  SetInt32Constant(env, binding, "UV_DIRENT_CHAR", UV_DIRENT_CHAR);
  SetInt32Constant(env, binding, "UV_DIRENT_BLOCK", UV_DIRENT_BLOCK);

  napi_value global = nullptr;
  if (napi_get_global(env, &global) != napi_ok || global == nullptr) {
    return;
  }
  napi_set_named_property(env, global, "__unode_fs", binding);
}
