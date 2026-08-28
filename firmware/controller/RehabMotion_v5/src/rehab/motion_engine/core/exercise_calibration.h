#pragma once
#include "motion_types.h"
#include <cstdint>

namespace rehab {

enum class CalibrationKind : uint8_t {
  HingeAxis,
  RaisePlane,
  FunctionalTransition,
  StepCycle
};

enum class CalibrationState : uint8_t {
  Idle,
  StartStill,
  LearnMotion,
  ReturnStill,
  Completed,
  Failed
};

struct CalibrationProfile {
  ExerciseId id{ExerciseId::DumbbellCurl};
  CalibrationKind kind{CalibrationKind::HingeAxis};
  uint16_t startStillMs{1200};
  uint16_t returnStillMs{1000};
  float stillGyroDegS{7.0f};
  float minLearnRomDeg{45.0f};
  float maxLearnSpeedDegS{160.0f};
  float returnToleranceDeg{22.0f};
  float maxTorsoTiltDeg{20.0f};
  float maxPlaneDeviationDeg{35.0f};
  uint8_t requiredCycles{1};
  const char* instructionZh{"保持起始姿势，随后完成一次缓慢标准动作"};
};

struct CalibrationStatus {
  CalibrationState state{CalibrationState::Idle};
  bool ready{false};
  bool failed{false};
  float progress01{0.0f};
  float learnedPeakDeg{0.0f};
  float learnedPrimaryAxisScore{0.0f};
  uint8_t completedCycles{0};
  const char* message{"idle"};
};

const CalibrationProfile& exerciseCalibrationProfile(ExerciseId id);
const char* calibrationKindName(CalibrationKind kind);
const char* calibrationStateName(CalibrationState state);

class ExerciseCalibrator {
public:
  ExerciseCalibrator();
  explicit ExerciseCalibrator(ExerciseId id);
  void select(ExerciseId id);
  void reset();
  CalibrationStatus update(const MotionFrame& frame);
  const CalibrationStatus& status() const { return status_; }
  const CalibrationProfile& profile() const { return profile_; }

private:
  float signal(const MotionFrame& frame) const;
  bool isStill(const MotionFrame& frame) const;
  CalibrationProfile profile_{};
  CalibrationStatus status_{};
  uint32_t phaseStartMs_{0};
  float peak_{0.0f};
  float lastSignal_{0.0f};
};

} // namespace rehab
