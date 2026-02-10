#ifndef TEMPLATEMATCH_TEMPLATE_MATCHING_H
#define TEMPLATEMATCH_TEMPLATE_MATCHING_H

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

namespace template_matching {

enum MatcherType { PATTERN = 0 };

struct MatcherParam {
  MatcherType matcherType = PATTERN;
  int maxCount = 1;
  double scoreThreshold = 0.5;
  double iouThreshold = 0.0;
  double angle = 0;
  double minArea = 256;
  double topAngleStep = 5.0;  /**< 顶层角度步长（度） */
};

struct MatchResult {
  cv::Point2d LeftTop, LeftBottom, RightTop, RightBottom, Center;
  double Angle = 0;
  double Score = 0;
};

} // namespace template_matching

#endif
