/**
 * @file Matcher.hpp
 * @brief 模板匹配 C++ 封装，基于 C API，便于在 C++ 中组合使用
 */
#ifndef TEMPLATEMATCH_MATCHER_HPP
#define TEMPLATEMATCH_MATCHER_HPP

#include "tm_api.h"
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <vector>
#include <string>
#include <memory>
#include <cstring>

namespace templatematch {

/** C++ 匹配结果 */
struct MatchResult {
  double left_top_x = 0, left_top_y = 0;
  double left_bottom_x = 0, left_bottom_y = 0;
  double right_top_x = 0, right_top_y = 0;
  double right_bottom_x = 0, right_bottom_y = 0;
  double center_x = 0, center_y = 0;
  double angle = 0;
  double score = 0;

  static MatchResult from_c(const TM_MatchResult& c) {
    MatchResult r;
    r.left_top_x = c.left_top_x;
    r.left_top_y = c.left_top_y;
    r.left_bottom_x = c.left_bottom_x;
    r.left_bottom_y = c.left_bottom_y;
    r.right_top_x = c.right_top_x;
    r.right_top_y = c.right_top_y;
    r.right_bottom_x = c.right_bottom_x;
    r.right_bottom_y = c.right_bottom_y;
    r.center_x = c.center_x;
    r.center_y = c.center_y;
    r.angle = c.angle;
    r.score = c.score;
    return r;
  }
};

/** 参数 */
struct Params {
  int max_count = 200;
  double score_threshold = 0.5;
  double iou_threshold = 0.0;
  double angle = 0.0;
  double min_area = 256.0;
};

/**
 * 模板匹配器（RAII，内部使用 C API）
 */
class Matcher {
public:
  explicit Matcher(const Params& params = Params()) {
    TM_Params p;
    p.max_count = params.max_count;
    p.score_threshold = params.score_threshold;
    p.iou_threshold = params.iou_threshold;
    p.angle = params.angle;
    p.min_area = params.min_area;
    handle_ = tm_create(&p);
  }

  ~Matcher() {
    if (handle_) {
      tm_destroy(handle_);
      handle_ = nullptr;
    }
  }

  Matcher(const Matcher&) = delete;
  Matcher& operator=(const Matcher&) = delete;

  bool valid() const { return handle_ != nullptr; }

  /** 从 cv::Mat 设置模板（自动转灰度） */
  bool setTemplate(const cv::Mat& templateImage) {
    if (!handle_ || templateImage.empty()) return false;
    cv::Mat gray;
    if (templateImage.channels() == 1)
      gray = templateImage;
    else
      cv::cvtColor(templateImage, gray, cv::COLOR_BGR2GRAY);
    return tm_set_template_from_memory(
      handle_, gray.data, gray.cols, gray.rows, 1) == 0;
  }

  /** 从文件设置模板 */
  bool setTemplateFromFile(const std::string& path) {
    return handle_ && tm_set_template_from_file(handle_, path.c_str()) == 0;
  }

  /** 在图像中匹配，返回结果列表 */
  std::vector<MatchResult> match(const cv::Mat& image) {
    std::vector<MatchResult> out;
    if (!handle_ || image.empty()) return out;
    cv::Mat gray;
    if (image.channels() == 1)
      gray = image;
    else
      cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
    const int maxCount = 512;
    TM_MatchResult results[maxCount];
    int n = tm_match(handle_, gray.data, gray.cols, gray.rows, 1, results, maxCount);
    if (n < 0) return out;
    out.reserve(static_cast<size_t>(n));
    for (int i = 0; i < n; i++)
      out.push_back(MatchResult::from_c(results[i]));
    return out;
  }

  TM_Handle nativeHandle() const { return handle_; }

private:
  TM_Handle handle_ = nullptr;
};

} // namespace templatematch

#endif /* TEMPLATEMATCH_MATCHER_HPP */
