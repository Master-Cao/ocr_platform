/**
 * @file server_config.h
 * @brief 服务端 INI 风格配置加载（server/config/server.conf）
 */
#ifndef OCR_SERVER_SERVER_CONFIG_H
#define OCR_SERVER_SERVER_CONFIG_H

#include <string>
#include <map>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace server {

/** 简单 INI：section -> key -> value */
using ConfigMap = std::map<std::string, std::map<std::string, std::string>>;

/** 从路径列表加载，第一个存在的文件生效；支持 # 注释与 [section] */
inline bool load_server_config(const std::vector<std::string>& paths, ConfigMap& out) {
  std::ifstream f;
  for (const auto& p : paths) {
    f.open(p);
    if (f.is_open()) break;
  }
  if (!f.is_open()) return false;

  std::string section;
  std::string line;
  while (std::getline(f, line)) {
    // 去掉行尾 \r
    if (!line.empty() && line.back() == '\r') line.pop_back();
    // 去掉首尾空白
    auto trim = [](std::string& s) {
      s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char c) { return !std::isspace(c); }));
      s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char c) { return !std::isspace(c); }).base(), s.end());
    };
    trim(line);
    if (line.empty() || line[0] == '#') continue;
    if (line[0] == '[') {
      auto end = line.find(']');
      if (end != std::string::npos)
        section = line.substr(1, end - 1);
      continue;
    }
    auto eq = line.find('=');
    if (eq == std::string::npos) continue;
    std::string key = line.substr(0, eq);
    std::string val = line.substr(eq + 1);
    trim(key);
    trim(val);
    if (!section.empty() && !key.empty())
      out[section][key] = val;
  }
  return true;
}

/** 获取配置项，不存在返回 default_val */
inline std::string config_get(const ConfigMap& cfg, const std::string& section, const std::string& key, const std::string& default_val) {
  auto it = cfg.find(section);
  if (it == cfg.end()) return default_val;
  auto kit = it->second.find(key);
  if (kit == it->second.end()) return default_val;
  return kit->second;
}

inline int config_get_int(const ConfigMap& cfg, const std::string& section, const std::string& key, int default_val) {
  std::string s = config_get(cfg, section, key, "");
  if (s.empty()) return default_val;
  try {
    return std::stoi(s);
  } catch (...) {
    return default_val;
  }
}

}  // namespace server

#endif
