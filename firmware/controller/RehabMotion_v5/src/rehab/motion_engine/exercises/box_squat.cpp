#include "box_squat.h"
#include <algorithm>
#include <cmath>

namespace rehab {
namespace {
constexpr float kStartJointMax=30.0f;
constexpr float kLeanHard=40.0f;
constexpr float kTorsoHard=25.0f;
constexpr float kAsymHard=18.0f;
constexpr float kPlaneHard=30.0f;
float inv(float v,float maxv){ return 1.0f-std::clamp(v/std::max(maxv,1.0f),0.0f,1.0f); }
}

BoxSquatDetector::BoxSquatDetector():RuleBasedDetector(ExerciseId::BoxSquat){}
float BoxSquatDetector::primarySignal(const MotionFrame& f) const { return std::max(std::fabs(f.primaryJointAngleDeg),std::fabs(f.verticalExcursionDeg)); }

bool BoxSquatDetector::startPosePlausible(const MotionFrame& f) const {
  return std::fabs(f.primaryJointAngleDeg)<=kStartJointMax && f.stabilityScore>=0.55f && f.torsoTiltDeg<=18.0f;
}

QualityCode BoxSquatDetector::evaluateQuality(const MotionFrame& f,float peak) const {
  if(peak+3.0f<profile_.targetDeg) return QualityCode::RomLow;
  if(f.forwardLeanDeg>kLeanHard) return QualityCode::ExcessForwardLean;
  if(f.torsoTiltDeg>kTorsoHard) return QualityCode::TorsoCompensation;
  if(std::fabs(f.leftRightDifferenceDeg)>kAsymHard) return QualityCode::Asymmetry;
  if(f.planeDeviationDeg>kPlaneHard) return QualityCode::PlaneDeviation;
  if(f.stabilityScore<0.60f) return QualityCode::Unstable;
  return QualityCode::Good;
}

uint32_t BoxSquatDetector::collectQualityFlags(const MotionFrame& f,float peak,const RepMetrics& m) const {
  uint32_t q=RuleBasedDetector::collectQualityFlags(f,peak,m);
  q&=~QF_UPPER_COMPENSATION;
  if(m.maxForwardLeanDeg>kLeanHard) q|=QF_FORWARD_LEAN;
  if(m.maxAsymmetryDeg>kAsymHard) q|=QF_ASYMMETRY;
  if(m.maxPlaneDeviationDeg>kPlaneHard) q|=QF_PLANE_DEVIATION;
  if(m.maxTorsoTiltDeg>kTorsoHard) q|=QF_TORSO_COMPENSATION;
  return q;
}

float BoxSquatDetector::scoreRep(const MotionFrame&,float peak,const RepMetrics& m,uint32_t flags) const {
  const float depth=std::clamp(peak/std::max(profile_.targetDeg,1.0f),0.0f,1.0f);
  const float symmetry=inv(m.maxAsymmetryDeg,35.0f);
  const float lean=inv(m.maxForwardLeanDeg,55.0f);
  const float plane=inv(m.maxPlaneDeviationDeg,50.0f);
  const float torso=inv(m.maxTorsoTiltDeg,42.0f);
  float s=100.0f*(0.36f*depth+0.18f*m.meanStability+0.15f*symmetry+0.13f*lean+0.10f*plane+0.08f*torso);
  if(flags&QF_TOO_FAST) s-=7.0f;
  if(flags&QF_FORWARD_LEAN) s-=7.0f;
  return std::clamp(s,0.0f,100.0f);
}
} // namespace rehab
