#include "exercise_calibration.h"
#include "exercise_profile.h"
#include <algorithm>
#include <cmath>

namespace rehab {

static const CalibrationProfile kCalibrationProfiles[] = {
  {ExerciseId::DumbbellCurl, CalibrationKind::HingeAxis, 1200, 900, 7, 50, 150, 22, 18, 35, 1,
   "上臂贴近躯干并保持稳定，缓慢屈肘至舒适高位，再回到自然下垂"},
  {ExerciseId::TricepsExtension, CalibrationKind::HingeAxis, 1200, 900, 7, 45, 145, 25, 22, 35, 1,
   "固定上臂位置，缓慢完成一次肘关节伸展与返回"},
  {ExerciseId::ScaptionRaise, CalibrationKind::RaisePlane, 1200, 1000, 7, 55, 130, 22, 18, 25, 1,
   "肩部放松，沿肩胛平面缓慢抬手至约肩高，再回到体侧"},
  {ExerciseId::WallCrawl, CalibrationKind::RaisePlane, 1000, 1000, 7, 55, 100, 24, 18, 28, 1,
   "面向墙面，手指沿墙缓慢向上移动并返回起点"},
  {ExerciseId::KneeFlexExtend, CalibrationKind::HingeAxis, 1200, 900, 7, 50, 150, 24, 20, 30, 1,
   "保持大腿稳定，缓慢完成一次膝关节屈伸并回到起始位"},
  {ExerciseId::SitToStand, CalibrationKind::FunctionalTransition, 1400, 1200, 8, 40, 130, 25, 28, 35, 1,
   "坐稳后自然起立至完全站稳，再受控坐回起始位置"},
  {ExerciseId::BoxSquat, CalibrationKind::FunctionalTransition, 1400, 1100, 8, 45, 120, 25, 28, 35, 1,
   "站稳后缓慢后坐下蹲至箱面附近，再平稳站起"},
  {ExerciseId::StepUp, CalibrationKind::StepCycle, 1200, 1100, 8, 35, 150, 25, 25, 35, 1,
   "面向低台阶，完成一次上台阶与回到地面的完整循环"}
};

const CalibrationProfile& exerciseCalibrationProfile(ExerciseId id) {
  auto i = static_cast<uint8_t>(id);
  if (i >= static_cast<uint8_t>(ExerciseId::Count)) i = 0;
  return kCalibrationProfiles[i];
}

const char* calibrationKindName(CalibrationKind kind) {
  switch (kind) {
    case CalibrationKind::HingeAxis: return "HINGE_AXIS";
    case CalibrationKind::RaisePlane: return "RAISE_PLANE";
    case CalibrationKind::FunctionalTransition: return "FUNCTIONAL_TRANSITION";
    case CalibrationKind::StepCycle: return "STEP_CYCLE";
  }
  return "UNKNOWN";
}

const char* calibrationStateName(CalibrationState state) {
  switch (state) {
    case CalibrationState::Idle: return "IDLE";
    case CalibrationState::StartStill: return "START_STILL";
    case CalibrationState::LearnMotion: return "LEARN_MOTION";
    case CalibrationState::ReturnStill: return "RETURN_STILL";
    case CalibrationState::Completed: return "COMPLETED";
    case CalibrationState::Failed: return "FAILED";
  }
  return "UNKNOWN";
}

ExerciseCalibrator::ExerciseCalibrator() { select(ExerciseId::DumbbellCurl); }
ExerciseCalibrator::ExerciseCalibrator(ExerciseId id) { select(id); }

void ExerciseCalibrator::select(ExerciseId id) {
  profile_ = exerciseCalibrationProfile(id);
  reset();
}

void ExerciseCalibrator::reset() {
  status_ = {};
  status_.state = CalibrationState::StartStill;
  status_.message = "hold_start_pose_still";
  phaseStartMs_ = 0;
  peak_ = 0;
  lastSignal_ = 0;
}

float ExerciseCalibrator::signal(const MotionFrame& f) const {
  switch (profile_.kind) {
    case CalibrationKind::RaisePlane: return std::fabs(f.secondaryJointAngleDeg);
    case CalibrationKind::FunctionalTransition:
      return std::max(std::fabs(f.primaryJointAngleDeg), std::fabs(f.verticalExcursionDeg));
    case CalibrationKind::StepCycle:
      return std::max(std::fabs(f.verticalExcursionDeg), std::fabs(f.primaryJointAngleDeg));
    case CalibrationKind::HingeAxis:
    default: return std::fabs(f.primaryJointAngleDeg);
  }
}

bool ExerciseCalibrator::isStill(const MotionFrame& f) const {
  return std::fabs(f.angularSpeedDegS) <= profile_.stillGyroDegS && f.stabilityScore >= 0.55f;
}

CalibrationStatus ExerciseCalibrator::update(const MotionFrame& f) {
  if (status_.state == CalibrationState::Completed || status_.state == CalibrationState::Failed) return status_;
  if (phaseStartMs_ == 0) phaseStartMs_ = f.timestampMs;
  const float s = signal(f);

  switch (status_.state) {
    case CalibrationState::StartStill: {
      if (!isStill(f)) {
        phaseStartMs_ = f.timestampMs;
        status_.progress01 = 0;
        status_.message = "keep_still";
        break;
      }
      const uint32_t elapsed = f.timestampMs - phaseStartMs_;
      status_.progress01 = std::min(1.0f, elapsed / static_cast<float>(profile_.startStillMs));
      if (elapsed >= profile_.startStillMs) {
        status_.state = CalibrationState::LearnMotion;
        status_.message = "perform_standard_motion_slowly";
        status_.progress01 = 0;
        phaseStartMs_ = f.timestampMs;
        peak_ = 0;
      }
      break;
    }
    case CalibrationState::LearnMotion: {
      peak_ = std::max(peak_, s);
      status_.learnedPeakDeg = peak_;
      status_.progress01 = std::min(1.0f, peak_ / std::max(profile_.minLearnRomDeg, 1.0f));
      if (std::fabs(f.angularSpeedDegS) > profile_.maxLearnSpeedDegS) {
        status_.message = "move_more_slowly";
      }
      if (f.torsoTiltDeg > profile_.maxTorsoTiltDeg) {
        status_.message = "keep_torso_stable";
      }
      if (peak_ >= profile_.minLearnRomDeg && s < peak_ - 10.0f) {
        status_.state = CalibrationState::ReturnStill;
        status_.message = "return_to_start_and_hold";
        phaseStartMs_ = f.timestampMs;
        status_.progress01 = 0;
      }
      break;
    }
    case CalibrationState::ReturnStill: {
      if (s > profile_.returnToleranceDeg || !isStill(f)) {
        phaseStartMs_ = f.timestampMs;
        status_.progress01 = 0;
        status_.message = "return_to_start_and_hold";
        break;
      }
      const uint32_t elapsed = f.timestampMs - phaseStartMs_;
      status_.progress01 = std::min(1.0f, elapsed / static_cast<float>(profile_.returnStillMs));
      if (elapsed >= profile_.returnStillMs) {
        status_.completedCycles++;
        status_.learnedPrimaryAxisScore = std::min(1.0f, peak_ / std::max(profile_.minLearnRomDeg, 1.0f));
        if (status_.completedCycles >= profile_.requiredCycles) {
          status_.state = CalibrationState::Completed;
          status_.ready = true;
          status_.progress01 = 1.0f;
          status_.message = "calibration_ready";
        } else {
          status_.state = CalibrationState::LearnMotion;
          status_.message = "repeat_standard_motion";
          phaseStartMs_ = f.timestampMs;
          peak_ = 0;
        }
      }
      break;
    }
    default: break;
  }
  lastSignal_ = s;
  return status_;
}

} // namespace rehab
