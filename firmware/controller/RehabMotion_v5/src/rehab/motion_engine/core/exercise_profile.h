#pragma once
#include "motion_types.h"
#include <array>

namespace rehab {

struct ExerciseProfile {
  ExerciseId id;
  const char* code;
  const char* nameZh;
  const char* jointFamily;
  const char* startPose;
  const char* primaryMetric;
  std::array<SensorId,5> preferredSensors;
  uint8_t preferredSensorCount;
  float activateDeg;
  float targetDeg;
  float returnDeg;
  float upperCompensationDeg;
  float planeDeviationDeg;
  float torsoCompensationDeg;
  float maxSpeedDegS;
  float minStability;
};

const ExerciseProfile& exerciseProfile(ExerciseId id);
const char* exerciseName(ExerciseId id);
const char* exerciseCode(ExerciseId id);

} // namespace rehab
