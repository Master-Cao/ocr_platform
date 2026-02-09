#ifndef TEMPLATEMATCH_BASE_MATCHER_H
#define TEMPLATEMATCH_BASE_MATCHER_H

#include "../matcher.h"

namespace template_matching {

class BaseMatcher : public Matcher {
public:
  BaseMatcher();
  ~BaseMatcher() override;
  bool isInited() const { return initFinishedFlag_; }
  void setMetricsTime(bool enabled) override { metricsTime_ = enabled; }
  bool getMetricsTime() const override { return metricsTime_; }
  void drawResult(const cv::Mat& frame, const std::vector<MatchResult>& matchResults) override;

protected:
  bool initMatcher(const MatcherParam& param);
  bool initFinishedFlag_ = false;
  MatcherParam matchParam_;
  cv::Mat templateImage_;
  bool metricsTime_ = false;
};

} // namespace template_matching

#endif
