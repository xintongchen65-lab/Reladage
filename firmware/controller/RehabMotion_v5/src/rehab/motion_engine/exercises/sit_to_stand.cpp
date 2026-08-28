#include "sit_to_stand.h"
#include <algorithm>
#include <cmath>

namespace rehab {
namespace {
constexpr float kStartTorsoMax=35.0f;
constexpr float kLeanHard=45.0f;
constexpr float kTorsoHard=28.0f;
constexpr float kAsymHard=20.0f;
float inv(float v,float maxv){ return 1.0f-std::clamp(v/std::max(maxv,1.0f),0.0f,1.0f); }
}

SitToStandDetector::SitToStandDetector():RuleBasedDetector(ExerciseId::SitToStand){}
float SitToStandDetector::primarySignal(const MotionFrame& f) const { return std::max(std::fabs(f.primaryJointAngleDeg),std::fabs(f.verticalExcursionDeg)); }

bool SitToStandDetector::startPosePlausible(const MotionFrame& f) const {
  return f.stabilityScore>=0.50f && f.torsoTiltDeg<=kStartTorsoMax && std::fabs(f.leftRightDifferenceDeg)<=25.0f;
}

QualityCode SitToStandDetector::evaluateQuality(const MotionFrame& f,float peak) const {
  if(peak+3.0f<profile_.targetDeg) return QualityCode::RomLow;
  if(f.forwardLeanDeg>kLeanHard) return QualityCode::ExcessForwardLean;
  if(f.torsoTiltDeg>kTorsoHard) return QualityCode::TorsoCompensation;
  if(std::fabs(f.leftRightDifferenceDeg)>kAsymHard) return QualityCode::Asymmetry;
  if(f.stabilityScore<0.58f) return QualityCode::Unstable;
  if(f.cadenceRpm>35.0f) return QualityCode::CadenceAbnormal;
  return QualityCode::Good;
}

uint32_t SitToStandDetector::collectQualityFlags(const MotionFrame& f,float peak,const RepMetrics& m) const {
  uint32_t q=RuleBasedDetector::collectQualityFlags(f,peak,m);
  q&=~QF_UPPER_COMPENSATION;
  if(m.maxForwardLeanDeg>kLeanHard) q|=QF_FORWARD_LEAN;
  if(m.maxAsymmetryDeg>kAsymHard) q|=QF_ASYMMETRY;
  if(m.maxTorsoTiltDeg>kTorsoHard) q|=QF_TORSO_COMPENSATION;
  if(m.peakCadenceRpm>35.0f) q|=QF_CADENCE;
  return q;
}

float SitToStandDetector::scoreRep(const MotionFrame&,float peak,const RepMetrics& m,uint32_t flags) const {
  const float completion=std::clamp(peak/std::max(profile_.targetDeg,1.0f),0.0f,1.0f);
  const float symmetry=inv(m.maxAsymmetryDeg,40.0f);
  const float lean=inv(std::max(0.0f,m.maxForwardLeanDeg-15.0f),45.0f);
  const float torso=inv(m.maxTorsoTiltDeg,45.0f);
  const float cadence=m.peakCadenceRpm<=35.0f?1.0f:inv(m.peakCadenceRpm-35.0f,30.0f);
  float s=100.0f*(0.36f*completion+0.20f*m.meanStability+0.17f*symmetry+0.12f*lean+0.09f*torso+0.06f*cadence);
  if(flags&QF_FORWARD_LEAN) s-=7.0f;
  if(flags&QF_ASYMMETRY) s-=7.0f;
  return std::clamp(s,0.0f,100.0f);
}
} // namespace rehab
