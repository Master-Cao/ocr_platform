/**
 * @file ConfigLoader.hpp
 * @brief 从配置文件加载 OCR 检测参数
 */
#ifndef OCRDETECT_CONFIG_LOADER_HPP
#define OCRDETECT_CONFIG_LOADER_HPP

#include <fstream>
#include <sstream>
#include <map>
#include <vector>
#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace ocrdetect {

/** OCR 检测选项（对应 OCR_Options + num_threads） */
struct OcrDetectOptions {
  int padding;
  int short_side_len;
  int crop_short_side_len;
  float box_score_thresh;
  float box_thresh;
  float un_clip_ratio;
  int do_angle;
  int most_angle;
  int num_threads;
};

/** 从 key=value 配置文件加载 OcrDetectOptions，无默认值，缺失则抛出 std::runtime_error */
inline OcrDetectOptions loadOcrDetectConfigFromFile(const std::string& path) {
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
  auto getFloat = [&kv, &path](const char* k) {
    auto it = kv.find(k);
    if (it == kv.end())
      throw std::runtime_error(std::string("配置缺失: ") + k + " (文件: " + path + ")");
    return std::stof(it->second);
  };

  OcrDetectOptions opt{};
  opt.padding = getInt("padding");
  opt.short_side_len = getInt("short_side_len");
  opt.crop_short_side_len = getInt("crop_short_side_len");
  opt.box_score_thresh = getFloat("box_score_thresh");
  opt.box_thresh = getFloat("box_thresh");
  opt.un_clip_ratio = getFloat("un_clip_ratio");
  opt.do_angle = getInt("do_angle");
  opt.most_angle = getInt("most_angle");
  opt.num_threads = getInt("num_threads");
  return opt;
}

/** 尝试多个路径加载配置，全部失败则抛出 std::runtime_error */
inline OcrDetectOptions loadOcrDetectConfigFromFileWithFallback(const std::vector<std::string>& paths,
    std::string* loaded_path = nullptr) {
  std::string lastErr;
  for (const auto& path : paths) {
    try {
      OcrDetectOptions opt = loadOcrDetectConfigFromFile(path);
      if (loaded_path) *loaded_path = path;
      return opt;
    } catch (const std::exception& e) {
      lastErr = e.what();
    }
  }
  std::string pathList;
  for (size_t i = 0; i < paths.size(); i++) {
    if (i > 0) pathList += ", ";
    pathList += paths[i];
  }
  throw std::runtime_error("未找到有效 ocrdetect 配置。尝试路径: [" + pathList + "]，最后错误: " + lastErr);
}

} // namespace ocrdetect

#endif /* OCRDETECT_CONFIG_LOADER_HPP */
