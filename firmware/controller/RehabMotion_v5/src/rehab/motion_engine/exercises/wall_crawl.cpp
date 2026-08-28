#include "wall_crawl.h"
#include <algorithm>
#include <cmath>

namespace rehab {
namespace {
constexpr float kStartHeightMax=30.0f;
constexpr float kStartLeanMax=25.0f;
constexpr float kStartTorsoMax=15.0f;
constexpr float kPlaneHard=30.0f;
constexpr float kLeanHard=35.0f;
constexpr float kTorsoHard=20.0f;
float inv(float v,float maxv){ return 1.0f-std::clamp(v/std::max(maxv,1.0f),0.0f,1.0f); }
}

WallCrawlDetector::WallCrawlDetector():RuleBasedDetector(ExerciseId::WallCrawl){}
float WallCrawlDetector::primarySignal(const MotionFrame& f) const { return std::max(std::fabs(f.secondaryJointAngleDeg),std::fabs(f.verticalExcursionDeg)); }

bool WallCrawlDetector::startPosePlausible(const MotionFrame& f) const {
  return primarySignal(f)<=kStartHeightMax && f.forwardLeanDeg<=kStartLeanMax && f.torsoTiltDeg<=kStartTorsoMax && f.stabilityScore>=0.55f;
}

QualityCode WallCrawlDetector::evaluateQuality(const MotionFrame& f,float peak) const {
  if(peak+3.0f<profile_.targetDeg) return QualityCode::RomLow;
  if(f.planeDeviationDeg>kPlaneHard) return QualityCode::PlaneDeviation;
  if(f.forwardLeanDeg>kLeanHard) return QualityCode::ExcessForwardLean;
  if(f.torsoTiltDeg>kTorsoHard) return QualityCode::TorsoCompensation;
  if(std::fabs(f.angularSpeedDegS)>120.0f) return QualityCode::TooFast;
  if(f.stabilityScore<0.65f) return QualityCode::Unstable;
  return QualityCode::Good;
}

uint32_t WallCrawlDetector::collectQualityFlags(const MotionFrame& f,float peak,const RepMetrics& m) const {
  uint32_t q=RuleBasedDetector::collectQualityFlags(f,peak,m);
  q&=~QF_UPPER_COMPENSATION;
  if(m.maxForwardLeanDeg>kLeanHard) q|=QF_FORWARD_LEAN;
  if(m.maxPlaneDeviationDeg>kPlaneHard) q|=QF_PLANE_DEVIATION;
  if(m.maxTorsoTiltDeg>kTorsoHard) q|=QF_TORSO_COMPENSATION;
  return q;
}

float WallCrawlDetector::scoreRep(const MotionFrame&,float peak,const RepMetrics& m,uint32_t flags) const {
  const float height=std::clamp(peak/std::max(profile_.targetDeg,1.0f),0.0f,1.0f);
  const float lean=inv(m.maxForwardLeanDeg,50.0f);
  const float plane=inv(m.maxPlaneDeviationDeg,50.0f);
  const float torso=inv(m.maxTorsoTiltDeg,35.0f);
  const float control=inv(std::max(0.0f,m.maxAbsSpeedDegS-90.0f),100.0f);
  float s=100.0f*(0.42f*height+0.18f*m.meanStability+0.14f*lean+0.12f*plane+0.08f*torso+0.06f*control);
  if(flags&QF_TOO_FAST) s-=8.0f;
  return std::clamp(s,0.0f,100.0f);
}
} // namespace rehab
