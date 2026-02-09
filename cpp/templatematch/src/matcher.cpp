#include "base_matcher/base_matcher.h"
#include "Pattern_Matching/PatternMatching.h"

namespace template_matching {

Matcher* GetMatcher(const MatcherParam& param) {
  MatcherParam paramCopy = param;
  BaseMatcher* matcher = nullptr;
  switch (paramCopy.matcherType) {
    case MatcherType::PATTERN:
      matcher = new PatternMatcher(paramCopy);
      break;
    default:
      break;
  }
  if (matcher && !matcher->isInited()) {
    delete matcher;
    matcher = nullptr;
  }
  return matcher;
}

} // namespace template_matching
