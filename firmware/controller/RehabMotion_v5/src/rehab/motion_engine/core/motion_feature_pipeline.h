#pragma once
#include "motion_types.h"
#include <array>
#include <cstddef>
#include <cstdint>
namespace rehab {
struct FeatureSample {
  uint32_t timestampMs{0};
  float primaryAngleDeg{0};
  float secondaryAngleDeg{0};
  float angularSpeedDegS{0};
  float torsoTiltDeg{0};
  float planeDeviationDeg{0};
};
struct FeatureSummary {
  bool ready{false};
  uint16_t samples{0};
  float angleRangeDeg{0};
  float meanSpeedDegS{0};
  float speedStdDegS{0};
  float meanTorsoTiltDeg{0};
  float meanPlaneDeviationDeg{0};
  float stabilityScore{1};
  float estimatedCadenceRpm{0};
};
class MotionFeaturePipeline {
public:
  static constexpr std::size_t WindowSize=32;
  void reset();
  FeatureSummary push(const FeatureSample& sample);
  FeatureSummary summary() const;
  MotionFrame enrich(const MotionFrame& frame) const;
private:
  std::array<FeatureSample,WindowSize> ring_{};
  std::size_t head_{0};
  std::size_t count_{0};
};
}
