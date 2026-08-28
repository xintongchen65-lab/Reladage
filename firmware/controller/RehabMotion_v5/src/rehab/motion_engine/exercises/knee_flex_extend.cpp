#include "knee_flex_extend.h"
#include <algorithm>
#include <cmath>

namespace rehab {
namespace {
constexpr float kStartJointMax=35.0f;
constexpr float kStartTorsoMax=20.0f;
constexpr float kPlaneHard=30.0f;
constexpr float kTorsoHard=25.0f;
constexpr float kAsymHard=18.0f;
float inv(float v,float maxv){ return 1.0f-std::clamp(v/std::max(maxv,1.0f),0.0f,1.0f); }
}

KneeFlexExtendDetector::KneeFlexExtendDetector():RuleBasedDetector(ExerciseId::KneeFlexExtend){}

bool KneeFlexExtendDetector::startPosePlausible(const MotionFrame& f) const {
  return std::fabs(f.primaryJointAngleDeg)<=kStartJointMax && f.torsoTiltDeg<=kStartTorsoMax && f.stabilityScore>=0.50f;
}

QualityCode KneeFlexExtendDetector::evaluateQuality(const MotionFrame& f,float peak) const {
  if(peak+3.0f<profile_.targetDeg) return QualityCode::RomLow;
  if(f.planeDeviationDeg>kPlaneHard) return QualityCode::PlaneDeviation;
  if(f.torsoTiltDeg>kTorsoHard) return QualityCode::TorsoCompensation;
  if(std::fabs(f.leftRightDifferenceDeg)>kAsymHard) return QualityCode::Asymmetry;
  if(std::fabs(f.angularSpeedDegS)>180.0f) return QualityCode::TooFast;
  if(f.stabilityScore<0.55f) return QualityCode::Unstable;
  return QualityCode::Good;
}

uint32_t KneeFlexExtendDetector::collectQualityFlags(const MotionFrame& f,float peak,const RepMetrics& m) const {
  uint32_t q=RuleBasedDetector::collectQualityFlags(f,peak,m);
  q&=~QF_UPPER_COMPENSATION;
  if(m.maxAsymmetryDeg>kAsymHard) q|=QF_ASYMMETRY;
  if(m.maxPlaneDeviationDeg>kPlaneHard) q|=QF_PLANE_DEVIATION;
  if(m.maxTorsoTiltDeg>kTorsoHard) q|=QF_TORSO_COMPENSATION;
  return q;
}

float KneeFlexExtendDetector::scoreRep(const MotionFrame&,float peak,const RepMetrics& m,uint32_t flags) const {
  const float rom=std::clamp(peak/std::max(profile_.targetDeg,1.0f),0.0f,1.0f);
  const float plane=inv(m.maxPlaneDeviationDeg,50.0f);
  const float symmetry=inv(m.maxAsymmetryDeg,35.0f);
  const float torso=inv(m.maxTorsoTiltDeg,40.0f);
  float s=100.0f*(0.45f*rom+0.18f*m.meanStability+0.15f*plane+0.13f*symmetry+0.09f*torso);
  if(flags&QF_TOO_FAST) s-=6.0f;
  if(flags&QF_TOO_SLOW) s-=4.0f;
  return std::clamp(s,0.0f,100.0f);
}
} // namespace rehab
