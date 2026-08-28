#include "exercise_detector.h"
#include <algorithm>
#include <cmath>

namespace rehab {

RuleBasedDetector::RuleBasedDetector(ExerciseId id) : profile_(exerciseProfile(id)) { reset(); }

void RuleBasedDetector::reset() {
  phase_=Phase::Ready; reps_=0; peak_=0; activeStartMs_=0; repMetrics_={}; speedSum_=0; stabilitySum_=0;
}

float RuleBasedDetector::primarySignal(const MotionFrame& f) const { return std::fabs(f.primaryJointAngleDeg); }

bool RuleBasedDetector::sensorSetPlausible(const MotionFrame& frame) const {
  bool anyTimestamp = false;
  for (const auto& s : frame.imu) anyTimestamp = anyTimestamp || s.timestampMs != 0 || s.online;
  if (!anyTimestamp) return true; // host/simulation frame: leave transport validation to integration layer.
  for (uint8_t i=0; i<profile_.preferredSensorCount; ++i) {
    const auto idx = static_cast<uint8_t>(profile_.preferredSensors[i]);
    if (idx >= frame.imu.size() || !frame.imu[idx].online) return false;
  }
  return true;
}

bool RuleBasedDetector::startPosePlausible(const MotionFrame& f) const {
  return primarySignal(f) <= std::max(profile_.returnDeg + 12.0f, profile_.activateDeg - 5.0f)
      && f.torsoTiltDeg <= profile_.torsoCompensationDeg + 8.0f;
}

void RuleBasedDetector::beginRep(const MotionFrame& f, float signal) {
  activeStartMs_ = f.timestampMs;
  peak_ = signal;
  repMetrics_ = {};
  repMetrics_.startMs = f.timestampMs;
  repMetrics_.minStability = 1.0f;
  speedSum_ = 0;
  stabilitySum_ = 0;
  accumulate(f, signal);
}

void RuleBasedDetector::accumulate(const MotionFrame& f, float signal) {
  ++repMetrics_.sampleCount;
  repMetrics_.peakRomDeg = std::max(repMetrics_.peakRomDeg, signal);
  repMetrics_.maxAbsSpeedDegS = std::max(repMetrics_.maxAbsSpeedDegS, std::fabs(f.angularSpeedDegS));
  repMetrics_.minStability = std::min(repMetrics_.minStability, std::clamp(f.stabilityScore,0.0f,1.0f));
  repMetrics_.maxUpperArmDeviationDeg = std::max(repMetrics_.maxUpperArmDeviationDeg, std::fabs(f.upperArmDeviationDeg));
  repMetrics_.maxPlaneDeviationDeg = std::max(repMetrics_.maxPlaneDeviationDeg, std::fabs(f.planeDeviationDeg));
  repMetrics_.maxTorsoTiltDeg = std::max(repMetrics_.maxTorsoTiltDeg, std::fabs(f.torsoTiltDeg));
  repMetrics_.maxAsymmetryDeg = std::max(repMetrics_.maxAsymmetryDeg, std::fabs(f.leftRightDifferenceDeg));
  repMetrics_.maxForwardLeanDeg = std::max(repMetrics_.maxForwardLeanDeg, std::fabs(f.forwardLeanDeg));
  repMetrics_.maxVerticalExcursionDeg = std::max(repMetrics_.maxVerticalExcursionDeg, std::fabs(f.verticalExcursionDeg));
  repMetrics_.peakCadenceRpm = std::max(repMetrics_.peakCadenceRpm, std::fabs(f.cadenceRpm));
  speedSum_ += std::fabs(f.angularSpeedDegS);
  stabilitySum_ += std::clamp(f.stabilityScore,0.0f,1.0f);
}

MotionFrame RuleBasedDetector::aggregateFrameForEvaluation(const MotionFrame& terminal) const {
  MotionFrame f = terminal;
  f.upperArmDeviationDeg = repMetrics_.maxUpperArmDeviationDeg;
  f.planeDeviationDeg = repMetrics_.maxPlaneDeviationDeg;
  f.torsoTiltDeg = repMetrics_.maxTorsoTiltDeg;
  f.angularSpeedDegS = repMetrics_.maxAbsSpeedDegS;
  f.stabilityScore = repMetrics_.minStability;
  f.leftRightDifferenceDeg = repMetrics_.maxAsymmetryDeg;
  f.forwardLeanDeg = repMetrics_.maxForwardLeanDeg;
  f.verticalExcursionDeg = repMetrics_.maxVerticalExcursionDeg;
  f.cadenceRpm = repMetrics_.peakCadenceRpm;
  return f;
}

QualityCode RuleBasedDetector::evaluateQuality(const MotionFrame& f, float peak) const {
  if (peak + 3.0f < profile_.targetDeg) return QualityCode::RomLow;
  if (f.upperArmDeviationDeg > profile_.upperCompensationDeg) return QualityCode::UpperArmCompensation;
  if (f.planeDeviationDeg > profile_.planeDeviationDeg) return QualityCode::PlaneDeviation;
  if (f.torsoTiltDeg > profile_.torsoCompensationDeg) return QualityCode::TorsoCompensation;
  if (std::fabs(f.angularSpeedDegS) > profile_.maxSpeedDegS) return QualityCode::TooFast;
  if (f.stabilityScore < profile_.minStability) return QualityCode::Unstable;
  return QualityCode::Good;
}

uint32_t RuleBasedDetector::collectQualityFlags(const MotionFrame& f, float peak, const RepMetrics& m) const {
  uint32_t flags = QF_NONE;
  if (peak + 3.0f < profile_.targetDeg) flags |= QF_ROM_LOW;
  if (f.upperArmDeviationDeg > profile_.upperCompensationDeg) flags |= QF_UPPER_COMPENSATION;
  if (f.planeDeviationDeg > profile_.planeDeviationDeg) flags |= QF_PLANE_DEVIATION;
  if (f.torsoTiltDeg > profile_.torsoCompensationDeg) flags |= QF_TORSO_COMPENSATION;
  if (m.maxAbsSpeedDegS > profile_.maxSpeedDegS || m.durationMs < minimumRepDurationMs()) flags |= QF_TOO_FAST;
  if (m.durationMs > maximumRepDurationMs()) flags |= QF_TOO_SLOW;
  if (m.minStability < profile_.minStability) flags |= QF_UNSTABLE;
  if (m.maxAsymmetryDeg > 20.0f) flags |= QF_ASYMMETRY;
  if (m.maxForwardLeanDeg > 45.0f) flags |= QF_FORWARD_LEAN;
  return flags;
}

QualityCode RuleBasedDetector::dominantQualityFromFlags(uint32_t flags) const {
  if (flags & QF_ROM_LOW) return QualityCode::RomLow;
  if (flags & QF_UPPER_COMPENSATION) return QualityCode::UpperArmCompensation;
  if (flags & QF_PLANE_DEVIATION) return QualityCode::PlaneDeviation;
  if (flags & QF_TORSO_COMPENSATION) return QualityCode::TorsoCompensation;
  if (flags & QF_FORWARD_LEAN) return QualityCode::ExcessForwardLean;
  if (flags & QF_ASYMMETRY) return QualityCode::Asymmetry;
  if (flags & QF_TOO_FAST) return QualityCode::TooFast;
  if (flags & QF_TOO_SLOW) return QualityCode::TooSlow;
  if (flags & QF_UNSTABLE) return QualityCode::Unstable;
  return QualityCode::Good;
}

float RuleBasedDetector::scoreRep(const MotionFrame&, float peak, const RepMetrics& m, uint32_t flags) const {
  const float rom = std::clamp(peak / std::max(profile_.targetDeg,1.0f),0.0f,1.0f);
  const float stability = std::clamp(m.meanStability,0.0f,1.0f);
  const float speedPenalty = (flags & QF_TOO_FAST) ? 0.18f : ((flags & QF_TOO_SLOW) ? 0.10f : 0.0f);
  const float techniquePenalty = (flags & (QF_UPPER_COMPENSATION|QF_PLANE_DEVIATION|QF_TORSO_COMPENSATION|QF_ASYMMETRY|QF_FORWARD_LEAN)) ? 0.28f : 0.0f;
  const float raw = 0.60f*rom + 0.40f*stability - speedPenalty - techniquePenalty;
  return 100.0f * std::clamp(raw,0.0f,1.0f);
}

const char* RuleBasedDetector::qualityWarning(QualityCode q) const {
  switch(q) {
    case QualityCode::Good: return "none";
    case QualityCode::RomLow: return "range_of_motion_below_target";
    case QualityCode::UpperArmCompensation: return "proximal_segment_compensation";
    case QualityCode::PlaneDeviation: return "movement_plane_deviation";
    case QualityCode::TorsoCompensation: return "torso_compensation";
    case QualityCode::TooFast: return "movement_too_fast";
    case QualityCode::TooSlow: return "movement_too_slow";
    case QualityCode::Unstable: return "movement_unstable";
    case QualityCode::Asymmetry: return "left_right_asymmetry";
    case QualityCode::StartPoseInvalid: return "start_pose_invalid";
    case QualityCode::SensorUnavailable: return "required_sensor_unavailable";
    case QualityCode::IncompleteReturn: return "return_to_start_incomplete";
    case QualityCode::ExcessForwardLean: return "forward_lean_excessive";
    case QualityCode::CadenceAbnormal: return "cadence_out_of_range";
  }
  return "unknown";
}

ExerciseFeedback RuleBasedDetector::update(const MotionFrame& frame) {
  ExerciseFeedback out{};
  out.exercise = profile_.id;
  out.phase = phase_;
  out.repCount = reps_;
  out.currentAngleDeg = primarySignal(frame);
  const float signal = out.currentAngleDeg;

  if (!sensorSetPlausible(frame)) {
    out.quality = QualityCode::SensorUnavailable;
    out.warning = qualityWarning(out.quality);
    out.qualityScore = 0;
    return out;
  }

  if (phase_ == Phase::Ready && signal >= profile_.activateDeg) {
    phase_ = Phase::Active;
    beginRep(frame, signal);
  } else if (phase_ == Phase::Active || phase_ == Phase::Peak || phase_ == Phase::Returning) {
    accumulate(frame, signal);
    peak_ = std::max(peak_, signal);
    if (signal >= profile_.targetDeg) phase_ = Phase::Peak;
    if (peak_ >= profile_.activateDeg && signal < peak_ - 8.0f) phase_ = Phase::Returning;
    if (phase_ == Phase::Returning && signal <= profile_.returnDeg) {
      repMetrics_.endMs = frame.timestampMs;
      repMetrics_.durationMs = frame.timestampMs - repMetrics_.startMs;
      if (repMetrics_.sampleCount) {
        repMetrics_.meanAbsSpeedDegS = speedSum_ / repMetrics_.sampleCount;
        repMetrics_.meanStability = stabilitySum_ / repMetrics_.sampleCount;
      }
      repMetrics_.peakRomDeg = peak_;
      MotionFrame eval = aggregateFrameForEvaluation(frame);
      uint32_t flags = collectQualityFlags(eval, peak_, repMetrics_);
      const QualityCode custom = evaluateQuality(eval, peak_);
      if (custom != QualityCode::Good) {
        switch (custom) {
          case QualityCode::RomLow: flags |= QF_ROM_LOW; break;
          case QualityCode::UpperArmCompensation: flags |= QF_UPPER_COMPENSATION; break;
          case QualityCode::PlaneDeviation: flags |= QF_PLANE_DEVIATION; break;
          case QualityCode::TorsoCompensation: flags |= QF_TORSO_COMPENSATION; break;
          case QualityCode::TooFast: flags |= QF_TOO_FAST; break;
          case QualityCode::TooSlow: flags |= QF_TOO_SLOW; break;
          case QualityCode::Unstable: flags |= QF_UNSTABLE; break;
          case QualityCode::Asymmetry: flags |= QF_ASYMMETRY; break;
          case QualityCode::ExcessForwardLean: flags |= QF_FORWARD_LEAN; break;
          default: break;
        }
      }
      ++reps_;
      out.repCompleted = true;
      out.qualityFlags = flags;
      out.quality = dominantQualityFromFlags(flags);
      out.repAccepted = flags == QF_NONE;
      out.warning = qualityWarning(out.quality);
      out.peakRomDeg = peak_;
      out.repCount = reps_;
      out.metrics = repMetrics_;
      out.qualityScore = scoreRep(eval, peak_, repMetrics_, flags);
      peak_ = 0;
      phase_ = Phase::Ready;
      repMetrics_ = {};
      speedSum_=0; stabilitySum_=0;
    }
  }

  out.phase = phase_;
  out.peakRomDeg = std::max(out.peakRomDeg, peak_);
  if (!out.repCompleted) {
    out.quality = QualityCode::Good;
    out.qualityFlags = QF_NONE;
    out.warning = "none";
    out.metrics = repMetrics_;
    const float romScore = std::min(1.0f, out.peakRomDeg / std::max(profile_.targetDeg, 1.0f));
    out.qualityScore = 100.0f * (0.7f * romScore + 0.3f * std::clamp(frame.stabilityScore,0.0f,1.0f));
  }
  return out;
}

} // namespace rehab
