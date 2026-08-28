#pragma once
#include "../motion_engine/core/motion_types.h"
#include <cstdint>

namespace rehab {

// Thin adapter between the established RehabMotion_v5 telemetry vocabulary and
// the unified MotionFrame consumed by all eight action detectors.
struct V5MotionSnapshot {
  uint32_t timestampMs{0};
  float primaryAngleDeg{0};
  float secondaryAngleDeg{0};
  float upperDeviationDeg{0};
  float planeDeviationDeg{0};
  float torsoTiltDeg{0};
  float torsoYawRelativeDeg{0};
  float angularSpeedDegS{0};
  float stabilityScore{1.0f};
  float leftRightDifferenceDeg{0};
  float verticalExcursionDeg{0};
  float forwardLeanDeg{0};
  float cadenceRpm{0};
  uint8_t imuMask{0};
};

MotionFrame makeMotionFrameFromV5(const V5MotionSnapshot& in);

} // namespace rehab
