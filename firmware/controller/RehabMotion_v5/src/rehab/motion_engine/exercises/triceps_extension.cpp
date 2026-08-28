#include "triceps_extension.h"
#include <algorithm>
#include <cmath>

namespace rehab {
namespace {
constexpr float kStartJointMax=35.0f;
constexpr float kStartTorsoMax=22.0f;
constexpr float kUpperHard=30.0f;
constexpr float kPlaneHard=35.0f;
constexpr float kTorsoHard=28.0f;
constexpr float kForwardLeanHard=35.0f;
float inv(float v,float maxv){ return 1.0f-std::clamp(v/std::max(maxv,1.0f),0.0f,1.0f); }
}

TricepsExtensionDetector::TricepsExtensionDetector():RuleBasedDetector(ExerciseId::TricepsExtension){}
float TricepsExtensionDetector::primarySignal(const MotionFrame& f) const { return std::fabs(f.primaryJointAngleDeg); }

bool TricepsExtensionDetector::startPosePlausible(const MotionFrame& f) const {
  return std::fabs(f.primaryJointAngleDeg)<=kStartJointMax && f.torsoTiltDeg<=kStartTorsoMax && f.stabilityScore>=0.50f;
}

QualityCode TricepsExtensionDetector::evaluateQuality(const MotionFrame& f,float peak) const {
  if(peak+3.0f<profile_.targetDeg) return QualityCode::RomLow;
  if(f.upperArmDeviationDeg>kUpperHard) return QualityCode::UpperArmCompensation;
  if(f.planeDeviationDeg>kPlaneHard) return QualityCode::PlaneDeviation;
  if(f.torsoTiltDeg>kTorsoHard) return QualityCode::TorsoCompensation;
  if(f.forwardLeanDeg>kForwardLeanHard) return QualityCode::ExcessForwardLean;
  if(std::fabs(f.angularSpeedDegS)>170.0f) return QualityCode::TooFast;
  if(f.stabilityScore<0.55f) return QualityCode::Unstable;
  return QualityCode::Good;
}

uint32_t TricepsExtensionDetector::collectQualityFlags(const MotionFrame& f,float peak,const RepMetrics& m) const {
  uint32_t q=RuleBasedDetector::collectQualityFlags(f,peak,m);
  if(m.maxUpperArmDeviationDeg>kUpperHard) q|=QF_UPPER_COMPENSATION;
  if(m.maxPlaneDeviationDeg>kPlaneHard) q|=QF_PLANE_DEVIATION;
  if(m.maxTorsoTiltDeg>kTorsoHard) q|=QF_TORSO_COMPENSATION;
  if(m.maxForwardLeanDeg>kForwardLeanHard) q|=QF_FORWARD_LEAN;
  return q;
}

float TricepsExtensionDetector::scoreRep(const MotionFrame&,float peak,const RepMetrics& m,uint32_t flags) const {
  const float rom=std::clamp(peak/std::max(profile_.targetDeg,1.0f),0.0f,1.0f);
  const float arm=inv(m.maxUpperArmDeviationDeg,50.0f);
  const float plane=inv(m.maxPlaneDeviationDeg,55.0f);
  const float torso=inv(m.maxTorsoTiltDeg,42.0f);
  const float speed=inv(std::max(0.0f,m.maxAbsSpeedDegS-110.0f),150.0f);
  float s=100.0f*(0.42f*rom+0.16f*m.meanStability+0.14f*arm+0.11f*plane+0.10f*torso+0.07f*speed);
  if(flags&QF_FORWARD_LEAN) s-=8.0f;
  if(flags&QF_TOO_FAST) s-=6.0f;
  return std::clamp(s,0.0f,100.0f);
}
} // namespace rehab
