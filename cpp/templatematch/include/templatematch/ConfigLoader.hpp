/**
 * @file ConfigLoader.hpp
 * @brief 从配置文件加载模板匹配参数
 */
#ifndef TEMPLATEMATCH_CONFIG_LOADER_HPP
#define TEMPLATEMATCH_CONFIG_LOADER_HPP

#include "Matcher.hpp"
#include <fstream>
#include <sstream>
#include <map>
#include <vector>
#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace templatematch {

/** 从 key=value 配置文件加载 Params，无默认值，缺失则抛出 std::runtime_error */
inline Params loadParamsFromFile(const std::string& path) {
  std::ifstream f(path);
  if (!f.is_open())
    throw std::runtime_error("无法打开配置文件: " + path);

  std::map<std::string, std::string> kv;
  std::string line;
  while (std::getline(f, line)) {
    size_t pos = line.find('#');
    if (pos != std::string::npos) line.resize(pos);
    pos = line.find('=');
    if (pos == std::string::npos) continue;
    std::string key = line.substr(0, pos);
    std::string val = line.substr(pos + 1);
    auto trim = [](std::string& s) {
      s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char c) { return !std::isspace(c); }));
      s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char c) { return !std::isspace(c); }).base(), s.end());
    };
    trim(key);
    trim(val);
    if (!key.empty()) kv[key] = val;
  }

  auto getInt = [&kv, &path](const char* k) {
    auto it = kv.find(k);
    if (it == kv.end())
      throw std::runtime_error(std::string("配置缺失: ") + k + " (文件: " + path + ")");
    return std::stoi(it->second);
  };
  auto getDouble = [&kv, &path](const char* k) {
    auto it = kv.find(k);
    if (it == kv.end())
      throw std::runtime_error(std::string("配置缺失: ") + k + " (文件: " + path + ")");
    return std::stod(it->second);
  };

  Params p;
  p.max_count = getInt("max_count");
  p.score_threshold = getDouble("score_threshold");
  p.iou_threshold = getDouble("iou_threshold");
  p.angle = getDouble("angle");
  p.min_area = getDouble("min_area");
  p.top_angle_step = getDouble("top_angle_step");
  return p;
}

/** 尝试多个路径加载配置，全部失败则抛出 std::runtime_error */
inline Params loadParamsFromFileWithFallback(const std::vector<std::string>& paths,
    std::string* loaded_path = nullptr) {
  std::string lastErr;
  for (const auto& path : paths) {
    try {
      Params p = loadParamsFromFile(path);
      if (loaded_path) *loaded_path = path;
      return p;
    } catch (const std::exception& e) {
      lastErr = e.what();
    }
  }
  std::string pathList;
  for (size_t i = 0; i < paths.size(); i++) {
    if (i > 0) pathList += ", ";
    pathList += paths[i];
  }
  throw std::runtime_error("未找到有效 templatematch 配置。尝试路径: [" + pathList + "]，最后错误: " + lastErr);
}

} // namespace templatematch

#endif /* TEMPLATEMATCH_CONFIG_LOADER_HPP */
