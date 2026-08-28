#include "dumbbell_curl.h"
#include <algorithm>
#include <cmath>

namespace rehab {
namespace {
constexpr float kStartJointMax = 32.0f;
constexpr float kStartUpperArmMax = 18.0f;
constexpr float kStartTorsoMax = 18.0f;
constexpr float kUpperWarn = 28.0f;
constexpr float kUpperHard = 35.0f;
constexpr float kPlaneWarn = 25.0f;
constexpr float kPlaneHard = 35.0f;
constexpr float kTorsoHard = 30.0f;
float score01(float value,float full){ return 1.0f-std::clamp(value/std::max(full,1.0f),0.0f,1.0f); }
}

DumbbellCurlDetector::DumbbellCurlDetector():RuleBasedDetector(ExerciseId::DumbbellCurl){}

bool DumbbellCurlDetector::startPosePlausible(const MotionFrame& f) const {
  return std::fabs(f.primaryJointAngleDeg)<=kStartJointMax &&
         f.upperArmDeviationDeg<=kStartUpperArmMax &&
         f.torsoTiltDeg<=kStartTorsoMax &&
         f.stabilityScore>=0.50f;
}

QualityCode DumbbellCurlDetector::evaluateQuality(const MotionFrame& f,float peak) const {
  if(peak+3.0f<profile_.targetDeg) return QualityCode::RomLow;
  if(f.upperArmDeviationDeg>kUpperHard) return QualityCode::UpperArmCompensation;
  if(f.planeDeviationDeg>kPlaneHard) return QualityCode::PlaneDeviation;
  if(f.torsoTiltDeg>kTorsoHard) return QualityCode::TorsoCompensation;
  if(std::fabs(f.angularSpeedDegS)>profile_.maxSpeedDegS) return QualityCode::TooFast;
  if(f.stabilityScore<profile_.minStability) return QualityCode::Unstable;
  return QualityCode::Good;
}

uint32_t DumbbellCurlDetector::collectQualityFlags(const MotionFrame& f,float peak,const RepMetrics& m) const {
  uint32_t q=RuleBasedDetector::collectQualityFlags(f,peak,m);
  if(m.maxUpperArmDeviationDeg>kUpperHard) q|=QF_UPPER_COMPENSATION;
  if(m.maxPlaneDeviationDeg>kPlaneHard) q|=QF_PLANE_DEVIATION;
  if(m.maxTorsoTiltDeg>kTorsoHard) q|=QF_TORSO_COMPENSATION;
  if(m.meanStability<0.55f) q|=QF_UNSTABLE;
  return q;
}

float DumbbellCurlDetector::scoreRep(const MotionFrame&,float peak,const RepMetrics& m,uint32_t flags) const {
  const float rom=std::clamp(peak/std::max(profile_.targetDeg,1.0f),0.0f,1.0f);
  const float stable=std::clamp(m.meanStability,0.0f,1.0f);
  const float upper=score01(std::max(0.0f,m.maxUpperArmDeviationDeg-kUpperWarn),30.0f);
  const float plane=score01(std::max(0.0f,m.maxPlaneDeviationDeg-kPlaneWarn),35.0f);
  const float torso=score01(m.maxTorsoTiltDeg,45.0f);
  const float tempo=score01(std::max(0.0f,m.maxAbsSpeedDegS-120.0f),140.0f);
  float s=100.0f*(0.38f*rom+0.17f*stable+0.16f*upper+0.12f*plane+0.10f*torso+0.07f*tempo);
  if(flags&QF_ROM_LOW) s-=10.0f;
  if(flags&QF_UPPER_COMPENSATION) s-=10.0f;
  if(flags&QF_PLANE_DEVIATION) s-=8.0f;
  if(flags&QF_TORSO_COMPENSATION) s-=8.0f;
  return std::clamp(s,0.0f,100.0f);
}
} // namespace rehab
