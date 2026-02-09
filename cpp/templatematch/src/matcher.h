#ifndef TEMPLATEMATCH_MATCHER_H
#define TEMPLATEMATCH_MATCHER_H

#include "template_matching.h"
#include <vector>

namespace template_matching {

class Matcher {
public:
  virtual ~Matcher() = default;
  virtual int match(const cv::Mat& frame, std::vector<MatchResult>& matchResults) = 0;
  virtual int setTemplate(const cv::Mat& templateImage) = 0;
  virtual void drawResult(const cv::Mat& frame, const std::vector<MatchResult>& matchResults) {}
  virtual void setMetricsTime(bool enabled) = 0;
  virtual bool getMetricsTime() const = 0;
};

Matcher* GetMatcher(const MatcherParam& param);

} // namespace template_matching

#endif
