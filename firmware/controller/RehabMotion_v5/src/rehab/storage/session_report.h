#pragma once
#include "../motion_engine/core/motion_types.h"
#include <cstdint>
#include <string>

namespace rehab {
struct ReportSummary {
  uint32_t totalReps{0};
  uint32_t acceptedReps{0};
  uint32_t rejectedReps{0};
  float maxRomDeg{0};
  float meanRomDeg{0};
  float meanQualityScore{0};
  float passRatePct{0};
};
class SessionAccumulator {
public:
  void reset();
  void add(const ExerciseFeedback& f);
  ReportSummary summary() const;
  std::string summaryJson() const;
private:
  uint32_t total_{0}, accepted_{0}, rejected_{0};
  float romSum_{0}, maxRom_{0}, qualitySum_{0};
};
}
