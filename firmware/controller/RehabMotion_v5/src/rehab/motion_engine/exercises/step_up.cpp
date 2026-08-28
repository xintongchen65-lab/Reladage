#include "step_up.h"
#include <algorithm>
#include <cmath>

namespace rehab {
namespace {
constexpr float kStartSignalMax=30.0f;
constexpr float kTorsoHard=25.0f;
constexpr float kLeanHard=38.0f;
constexpr float kAsymHard=22.0f;
constexpr float kCadenceHard=45.0f;
float inv(float v,float maxv){ return 1.0f-std::clamp(v/std::max(maxv,1.0f),0.0f,1.0f); }
}

StepUpDetector::StepUpDetector():RuleBasedDetector(ExerciseId::StepUp){}
float StepUpDetector::primarySignal(const MotionFrame& f) const { return std::max(std::fabs(f.verticalExcursionDeg),std::fabs(f.primaryJointAngleDeg)); }

bool StepUpDetector::startPosePlausible(const MotionFrame& f) const {
  return primarySignal(f)<=kStartSignalMax && f.stabilityScore>=0.55f && f.torsoTiltDeg<=20.0f && std::fabs(f.leftRightDifferenceDeg)<=25.0f;
}

QualityCode StepUpDetector::evaluateQuality(const MotionFrame& f,float peak) const {
  if(peak+3.0f<profile_.targetDeg) return QualityCode::RomLow;
  if(f.torsoTiltDeg>kTorsoHard) return QualityCode::TorsoCompensation;
  if(f.forwardLeanDeg>kLeanHard) return QualityCode::ExcessForwardLean;
  if(std::fabs(f.leftRightDifferenceDeg)>kAsymHard) return QualityCode::Asymmetry;
  if(f.stabilityScore<0.58f) return QualityCode::Unstable;
  if(f.cadenceRpm>kCadenceHard) return QualityCode::CadenceAbnormal;
  return QualityCode::Good;
}

uint32_t StepUpDetector::collectQualityFlags(const MotionFrame& f,float peak,const RepMetrics& m) const {
  uint32_t q=RuleBasedDetector::collectQualityFlags(f,peak,m);
  q&=~QF_UPPER_COMPENSATION;
  if(m.maxAsymmetryDeg>kAsymHard) q|=QF_ASYMMETRY;
  if(m.maxForwardLeanDeg>kLeanHard) q|=QF_FORWARD_LEAN;
  if(m.maxTorsoTiltDeg>kTorsoHard) q|=QF_TORSO_COMPENSATION;
  if(m.peakCadenceRpm>kCadenceHard) q|=QF_CADENCE;
  return q;
}

float StepUpDetector::scoreRep(const MotionFrame&,float peak,const RepMetrics& m,uint32_t flags) const {
  const float height=std::clamp(peak/std::max(profile_.targetDeg,1.0f),0.0f,1.0f);
  const float symmetry=inv(m.maxAsymmetryDeg,45.0f);
  const float torso=inv(m.maxTorsoTiltDeg,40.0f);
  const float lean=inv(m.maxForwardLeanDeg,55.0f);
  const float cadence=m.peakCadenceRpm<=kCadenceHard?1.0f:inv(m.peakCadenceRpm-kCadenceHard,30.0f);
  float s=100.0f*(0.36f*height+0.18f*m.meanStability+0.14f*symmetry+0.12f*torso+0.10f*lean+0.10f*cadence);
  if(flags&QF_FORWARD_LEAN) s-=6.0f;
  if(flags&QF_CADENCE) s-=6.0f;
  return std::clamp(s,0.0f,100.0f);
}
} // namespace rehab
