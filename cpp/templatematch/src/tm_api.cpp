/**
 * 模板匹配 C API 实现
 */
#include "../include/templatematch/tm_api.h"
#include "matcher.h"
#include "template_matching.h"
#include <opencv2/opencv.hpp>
#include <cstring>
#include <vector>

#include "../include/templatematch/export.h"

static void to_param(const TM_Params* p, template_matching::MatcherParam& out) {
  if (p) {
    out.maxCount = p->max_count;
    out.scoreThreshold = p->score_threshold;
    out.iouThreshold = p->iou_threshold;
    out.angle = p->angle;
    out.minArea = p->min_area;
    out.topAngleStep = p->top_angle_step;
  }
}

static void to_c(const template_matching::MatchResult& r, TM_MatchResult* c) {
  c->left_top_x = r.LeftTop.x;
  c->left_top_y = r.LeftTop.y;
  c->left_bottom_x = r.LeftBottom.x;
  c->left_bottom_y = r.LeftBottom.y;
  c->right_top_x = r.RightTop.x;
  c->right_top_y = r.RightTop.y;
  c->right_bottom_x = r.RightBottom.x;
  c->right_bottom_y = r.RightBottom.y;
  c->center_x = r.Center.x;
  c->center_y = r.Center.y;
  c->angle = r.Angle;
  c->score = r.Score;
}

TM_Handle TEMPLATEMATCH_CALL tm_create(const TM_Params* params) {
  template_matching::MatcherParam p;
  to_param(params, p);
  p.matcherType = template_matching::PATTERN;
  template_matching::Matcher* m = template_matching::GetMatcher(p);
  if (!m) return nullptr;
  m->setMetricsTime(false);
  return static_cast<void*>(m);
}

void TEMPLATEMATCH_CALL tm_destroy(TM_Handle h) {
  if (h) {
    delete static_cast<template_matching::Matcher*>(h);
  }
}

int TEMPLATEMATCH_CALL tm_set_template_from_memory(
    TM_Handle h, const unsigned char* data, int width, int height, int channels) {
  if (!h || !data || width <= 0 || height <= 0) return -1;
  if (channels != 1) return -2;
  cv::Mat mat(height, width, CV_8UC1);
  size_t len = static_cast<size_t>(width * height);
  std::memcpy(mat.data, data, len);
  int ret = static_cast<template_matching::Matcher*>(h)->setTemplate(mat);
  return ret;
}

int TEMPLATEMATCH_CALL tm_set_template_from_file(TM_Handle h, const char* file_path) {
  if (!h || !file_path) return -1;
  cv::Mat img = cv::imread(file_path);
  if (img.empty()) return -2;
  cv::Mat gray;
  if (img.channels() == 1)
    gray = img;
  else
    cv::cvtColor(img, gray, cv::COLOR_BGR2GRAY);
  return static_cast<template_matching::Matcher*>(h)->setTemplate(gray);
}

int TEMPLATEMATCH_CALL tm_match(TM_Handle h,
    const unsigned char* image_data, int width, int height, int channels,
    TM_MatchResult* results, int max_results) {
  if (!h || !image_data || !results || max_results <= 0) return -1;
  if (channels != 1) return -2;
  cv::Mat mat(height, width, CV_8UC1);
  std::memcpy(mat.data, image_data, static_cast<size_t>(width * height));
  std::vector<template_matching::MatchResult> vec;
  int n = static_cast<template_matching::Matcher*>(h)->match(mat, vec);
  if (n < 0) return n;
  int out = (n <= max_results) ? n : max_results;
  for (int i = 0; i < out; i++)
    to_c(vec[i], results + i);
  return out;
}
