#include "base_matcher.h"

namespace template_matching {

BaseMatcher::BaseMatcher() = default;

BaseMatcher::~BaseMatcher() = default;

bool BaseMatcher::initMatcher(const MatcherParam& param) {
  matchParam_ = param;
  return true;
}

void BaseMatcher::drawResult(const cv::Mat& /*frame*/, const std::vector<MatchResult>& /*matchResults*/) {
  /* optional: not used by C API */
}

} // namespace template_matching
