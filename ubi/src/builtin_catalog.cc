#include "builtin_catalog.h"

#include <algorithm>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

namespace builtin_catalog {

namespace fs = std::filesystem;

namespace {

#include "generated_builtin_ids.inc"

constexpr const char* kNodePrefix = "node:";
constexpr const char* kInternalDepsPrefix = "internal/deps/";

const std::vector<std::string> kNodeDepsBuiltinRoots = {
    "undici",
    "acorn",
    "minimatch",
    "cjs-module-lexer",
    "amaro",
    "v8/tools",
};

bool PathExistsRegularFile(const fs::path& path) {
  std::error_code ec;
  return fs::exists(path, ec) && fs::is_regular_file(path, ec);
}

bool PathExistsDirectory(const fs::path& path) {
  std::error_code ec;
  return fs::exists(path, ec) && fs::is_directory(path, ec);
}

bool ResolveAsFile(const fs::path& candidate, fs::path* out) {
  if (out == nullptr) return false;
  if (PathExistsRegularFile(candidate)) {
    *out = fs::absolute(candidate).lexically_normal();
    return true;
  }
  if (candidate.has_extension()) return false;
  const fs::path js_candidate = candidate.string() + ".js";
  if (PathExistsRegularFile(js_candidate)) {
    *out = fs::absolute(js_candidate).lexically_normal();
    return true;
  }
  const fs::path mjs_candidate = candidate.string() + ".mjs";
  if (PathExistsRegularFile(mjs_candidate)) {
    *out = fs::absolute(mjs_candidate).lexically_normal();
    return true;
  }
  return false;
}

bool IsInsideRoot(const fs::path& path, const fs::path& root) {
  const fs::path normalized_path = fs::absolute(path).lexically_normal();
  const fs::path normalized_root = fs::absolute(root).lexically_normal();
  const fs::path rel = normalized_path.lexically_relative(normalized_root);
  const std::string rel_text = rel.generic_string();
  return !rel_text.empty() && rel_text != "." && rel_text.rfind("..", 0) != 0;
}

std::string StripBuiltinExtension(const fs::path& relative_path) {
  std::string text = relative_path.generic_string();
  if (text.size() >= 4 && text.compare(text.size() - 3, 3, ".js") == 0) {
    text.resize(text.size() - 3);
    return text;
  }
  if (text.size() >= 5 && text.compare(text.size() - 4, 4, ".mjs") == 0) {
    text.resize(text.size() - 4);
    return text;
  }
  return "";
}

bool IsAllowedNodeDepsRelativePath(const fs::path& rel) {
  const std::string rel_text = rel.generic_string();
  for (const std::string& root : kNodeDepsBuiltinRoots) {
    if (rel_text == root || rel_text.rfind(root + "/", 0) == 0) return true;
  }
  return false;
}

}  // namespace

const fs::path& NodeLibRoot() {
  static const fs::path root = fs::path("/node-lib");
  return root;
}

const fs::path& NodeDepsRoot() {
  static const fs::path root = fs::path("/node/deps");
  return root;
}

bool ResolveBuiltinId(const std::string& specifier, fs::path* out_path) {
  if (out_path == nullptr) return false;
  std::string id = specifier;
  if (id.size() > 5 && id.compare(0, 5, kNodePrefix) == 0) {
    id = id.substr(5);
  }
  if (id.empty() || id.rfind(".", 0) == 0) return false;

  fs::path resolved;
  if (id.rfind(kInternalDepsPrefix, 0) == 0) {
    const std::string rel = id.substr(std::string(kInternalDepsPrefix).size());
    if (rel.empty()) return false;
    const fs::path candidate = NodeDepsRoot() / rel;
    if (ResolveAsFile(candidate, &resolved)) {
      const fs::path relative = resolved.lexically_relative(NodeDepsRoot());
      if (!IsAllowedNodeDepsRelativePath(relative)) return false;
      *out_path = resolved;
      return true;
    }
    return false;
  }

  const fs::path candidate = NodeLibRoot() / id;
  if (!ResolveAsFile(candidate, &resolved)) return false;
  if (resolved.extension() != ".js") return false;
  *out_path = resolved;
  return true;
}

bool TryGetBuiltinIdForPath(const fs::path& resolved_path, std::string* out_id) {
  if (out_id == nullptr) return false;
  const fs::path normalized = fs::absolute(resolved_path).lexically_normal();
  if (normalized.extension() != ".js" && normalized.extension() != ".mjs") return false;

  if (IsInsideRoot(normalized, NodeLibRoot())) {
    const fs::path rel = normalized.lexically_relative(NodeLibRoot());
    const std::string id = StripBuiltinExtension(rel);
    if (id.empty()) return false;
    *out_id = id;
    return true;
  }

  if (IsInsideRoot(normalized, NodeDepsRoot())) {
    const fs::path rel = normalized.lexically_relative(NodeDepsRoot());
    if (!IsAllowedNodeDepsRelativePath(rel)) return false;
    const std::string dep_id = StripBuiltinExtension(rel);
    if (dep_id.empty()) return false;
    *out_id = std::string(kInternalDepsPrefix) + dep_id;
    return true;
  }

  return false;
}

const std::vector<std::string>& AllBuiltinIds() {
  static const std::vector<std::string> ids = []() {
    std::vector<std::string> out;
    out.reserve(sizeof(kGeneratedBuiltinIds) / sizeof(kGeneratedBuiltinIds[0]));
    for (const char* builtin_id : kGeneratedBuiltinIds) {
      out.emplace_back(builtin_id);
    }
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
  }();
  return ids;
}

}  // namespace builtin_catalog
