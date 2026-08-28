#include "telemetry_schema.h"
#include "exercise_profile.h"
#include <cstdio>
namespace rehab {
static const char* qualityName(QualityCode q){
  switch(q){
    case QualityCode::Good:return "GOOD"; case QualityCode::RomLow:return "ROM_LOW";
    case QualityCode::UpperArmCompensation:return "UPPER_ARM_COMPENSATION";
    case QualityCode::PlaneDeviation:return "PLANE_DEVIATION";
    case QualityCode::TorsoCompensation:return "TORSO_COMPENSATION";
    case QualityCode::TooFast:return "TOO_FAST"; case QualityCode::TooSlow:return "TOO_SLOW";
    case QualityCode::Unstable:return "UNSTABLE"; case QualityCode::Asymmetry:return "ASYMMETRY";
    case QualityCode::StartPoseInvalid:return "START_POSE_INVALID";
    case QualityCode::SensorUnavailable:return "SENSOR_UNAVAILABLE";
    case QualityCode::IncompleteReturn:return "INCOMPLETE_RETURN";
    case QualityCode::ExcessForwardLean:return "EXCESS_FORWARD_LEAN";
    case QualityCode::CadenceAbnormal:return "CADENCE_ABNORMAL";
  } return "UNKNOWN";
}
static const char* phaseName(Phase p){
  switch(p){case Phase::Idle:return "IDLE";case Phase::Ready:return "READY";case Phase::Active:return "ACTIVE";case Phase::Peak:return "PEAK";case Phase::Returning:return "RETURNING";case Phase::Completed:return "COMPLETED";case Phase::Paused:return "PAUSED";} return "UNKNOWN";
}
std::string feedbackToJson(const ExerciseFeedback& f){
  char buf[1600];
  const auto& m=f.metrics;
  std::snprintf(buf,sizeof(buf),
    "{\"exercise\":\"%s\",\"exercise_name\":\"%s\",\"phase\":\"%s\",\"rep_count\":%u,\"rep_completed\":%s,\"rep_accepted\":%s,\"current_angle_deg\":%.1f,\"peak_rom_deg\":%.1f,\"quality\":\"%s\",\"quality_flags\":%u,\"quality_score\":%.1f,\"warning\":\"%s\",\"metrics\":{\"duration_ms\":%u,\"sample_count\":%u,\"max_speed_deg_s\":%.1f,\"mean_speed_deg_s\":%.1f,\"min_stability\":%.3f,\"mean_stability\":%.3f,\"upper_comp_deg\":%.1f,\"plane_dev_deg\":%.1f,\"torso_tilt_deg\":%.1f,\"asymmetry_deg\":%.1f,\"forward_lean_deg\":%.1f,\"vertical_excursion_deg\":%.1f,\"cadence_rpm\":%.1f}}",
    exerciseCode(f.exercise),exerciseName(f.exercise),phaseName(f.phase),f.repCount,
    f.repCompleted?"true":"false",f.repAccepted?"true":"false",f.currentAngleDeg,f.peakRomDeg,
    qualityName(f.quality),static_cast<unsigned>(f.qualityFlags),f.qualityScore,f.warning.c_str(),
    static_cast<unsigned>(m.durationMs),m.sampleCount,m.maxAbsSpeedDegS,m.meanAbsSpeedDegS,m.minStability,m.meanStability,
    m.maxUpperArmDeviationDeg,m.maxPlaneDeviationDeg,m.maxTorsoTiltDeg,m.maxAsymmetryDeg,m.maxForwardLeanDeg,m.maxVerticalExcursionDeg,m.peakCadenceRpm);
  return std::string(buf);
}
}
