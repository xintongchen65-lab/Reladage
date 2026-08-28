#pragma once
#include "../core/exercise_detector.h"
namespace rehab {
class KneeFlexExtendDetector final : public RuleBasedDetector {
public:
  KneeFlexExtendDetector();
protected:
  bool startPosePlausible(const MotionFrame& f) const override;
  QualityCode evaluateQuality(const MotionFrame& f,float peak) const override;
  uint32_t collectQualityFlags(const MotionFrame& f,float peak,const RepMetrics& m) const override;
  float scoreRep(const MotionFrame& f,float peak,const RepMetrics& m,uint32_t flags) const override;
  uint32_t minimumRepDurationMs() const override { return 700; }
  uint32_t maximumRepDurationMs() const override { return 10000; }
};
}
