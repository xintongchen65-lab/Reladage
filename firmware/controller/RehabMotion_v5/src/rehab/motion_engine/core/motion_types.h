#pragma once
#include <cstdint>
#include <array>
#include <string>

namespace rehab {

enum class SensorId : uint8_t { A=0, B=1, C=2, D=3, E=4 };
enum class ExerciseId : uint8_t {
  DumbbellCurl = 0,
  TricepsExtension,
  ScaptionRaise,
  WallCrawl,
  KneeFlexExtend,
  SitToStand,
  BoxSquat,
  StepUp,
  Count
};

enum class Phase : uint8_t { Idle, Ready, Active, Peak, Returning, Completed, Paused };
enum class QualityCode : uint8_t {
  Good,
  RomLow,
  UpperArmCompensation,
  PlaneDeviation,
  TorsoCompensation,
  TooFast,
  TooSlow,
  Unstable,
  Asymmetry,
  StartPoseInvalid,
  SensorUnavailable,
  IncompleteReturn,
  ExcessForwardLean,
  CadenceAbnormal
};

enum QualityFlag : uint32_t {
  QF_NONE                = 0,
  QF_ROM_LOW             = 1u << 0,
  QF_UPPER_COMPENSATION  = 1u << 1,
  QF_PLANE_DEVIATION     = 1u << 2,
  QF_TORSO_COMPENSATION  = 1u << 3,
  QF_TOO_FAST            = 1u << 4,
  QF_TOO_SLOW            = 1u << 5,
  QF_UNSTABLE            = 1u << 6,
  QF_ASYMMETRY           = 1u << 7,
  QF_INCOMPLETE_RETURN   = 1u << 8,
  QF_FORWARD_LEAN        = 1u << 9,
  QF_CADENCE             = 1u << 10
};

struct Vec3 { float x{0}, y{0}, z{0}; };

struct SensorFrame {
  bool online{false};
  uint32_t timestampMs{0};
  Vec3 accel{};
  Vec3 gyro{};
  std::array<float,4> quat{{1,0,0,0}};
};

struct MotionFrame {
  uint32_t timestampMs{0};
  std::array<SensorFrame,5> imu{};
  float primaryJointAngleDeg{0};
  float secondaryJointAngleDeg{0};
  float upperArmDeviationDeg{0};
  float planeDeviationDeg{0};
  float torsoTiltDeg{0};
  float torsoYawRelativeDeg{0};
  float angularSpeedDegS{0};
  float stabilityScore{1.0f};
  float leftRightDifferenceDeg{0};
  float verticalExcursionDeg{0};
  float forwardLeanDeg{0};
  float cadenceRpm{0};
};

struct RepMetrics {
  uint32_t startMs{0};
  uint32_t endMs{0};
  uint32_t durationMs{0};
  uint16_t sampleCount{0};
  float peakRomDeg{0};
  float maxAbsSpeedDegS{0};
  float meanAbsSpeedDegS{0};
  float minStability{1.0f};
  float meanStability{1.0f};
  float maxUpperArmDeviationDeg{0};
  float maxPlaneDeviationDeg{0};
  float maxTorsoTiltDeg{0};
  float maxAsymmetryDeg{0};
  float maxForwardLeanDeg{0};
  float maxVerticalExcursionDeg{0};
  float peakCadenceRpm{0};
};

struct ExerciseFeedback {
  ExerciseId exercise{ExerciseId::DumbbellCurl};
  Phase phase{Phase::Idle};
  QualityCode quality{QualityCode::Good};
  uint32_t qualityFlags{QF_NONE};
  bool repCompleted{false};
  bool repAccepted{false};
  uint16_t repCount{0};
  float peakRomDeg{0};
  float currentAngleDeg{0};
  float qualityScore{0};
  RepMetrics metrics{};
  std::string warning;
};

} // namespace rehab
