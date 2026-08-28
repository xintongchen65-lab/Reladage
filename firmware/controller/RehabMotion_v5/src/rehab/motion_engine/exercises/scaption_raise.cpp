#include "scaption_raise.h"
#include <algorithm>
#include <cmath>

namespace rehab {
namespace {
constexpr float kStartRaiseMax=28.0f;
constexpr float kStartTorsoMax=15.0f;
constexpr float kPlaneHard=25.0f;
constexpr float kTorsoHard=20.0f;
constexpr float kAsymHard=20.0f;
float inv(float v,float maxv){ return 1.0f-std::clamp(v/std::max(maxv,1.0f),0.0f,1.0f); }
}

ScaptionRaiseDetector::ScaptionRaiseDetector():RuleBasedDetector(ExerciseId::ScaptionRaise){}
float ScaptionRaiseDetector::primarySignal(const MotionFrame& f) const { return std::fabs(f.secondaryJointAngleDeg); }

bool ScaptionRaiseDetector::startPosePlausible(const MotionFrame& f) const {
  return std::fabs(f.secondaryJointAngleDeg)<=kStartRaiseMax && f.torsoTiltDeg<=kStartTorsoMax && f.stabilityScore>=0.55f;
}

QualityCode ScaptionRaiseDetector::evaluateQuality(const MotionFrame& f,float peak) const {
  if(peak+3.0f<profile_.targetDeg) return QualityCode::RomLow;
  if(f.planeDeviationDeg>kPlaneHard) return QualityCode::PlaneDeviation;
  if(f.torsoTiltDeg>kTorsoHard) return QualityCode::TorsoCompensation;
  if(std::fabs(f.leftRightDifferenceDeg)>kAsymHard) return QualityCode::Asymmetry;
  if(std::fabs(f.angularSpeedDegS)>150.0f) return QualityCode::TooFast;
  if(f.stabilityScore<0.60f) return QualityCode::Unstable;
  return QualityCode::Good;
}

uint32_t ScaptionRaiseDetector::collectQualityFlags(const MotionFrame& f,float peak,const RepMetrics& m) const {
  uint32_t q=RuleBasedDetector::collectQualityFlags(f,peak,m);
  // Upper-arm motion is the intended movement in this exercise, therefore the
  // generic proximal compensation flag is replaced by plane/torso checks.
  q&=~QF_UPPER_COMPENSATION;
  if(m.maxPlaneDeviationDeg>kPlaneHard) q|=QF_PLANE_DEVIATION;
  if(m.maxTorsoTiltDeg>kTorsoHard) q|=QF_TORSO_COMPENSATION;
  if(m.maxAsymmetryDeg>kAsymHard) q|=QF_ASYMMETRY;
  return q;
}

float ScaptionRaiseDetector::scoreRep(const MotionFrame&,float peak,const RepMetrics& m,uint32_t flags) const {
  const float rom=std::clamp(peak/std::max(profile_.targetDeg,1.0f),0.0f,1.0f);
  const float plane=inv(m.maxPlaneDeviationDeg,45.0f);
  const float torso=inv(m.maxTorsoTiltDeg,35.0f);
  const float symmetry=inv(m.maxAsymmetryDeg,35.0f);
  float s=100.0f*(0.42f*rom+0.22f*plane+0.14f*torso+0.12f*m.meanStability+0.10f*symmetry);
  if(flags&QF_TOO_FAST) s-=8.0f;
  if(flags&QF_UNSTABLE) s-=6.0f;
  return std::clamp(s,0.0f,100.0f);
}
} // namespace rehab
