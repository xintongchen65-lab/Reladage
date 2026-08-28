#include "motion_feature_pipeline.h"
#include <algorithm>
#include <cmath>
namespace rehab {
void MotionFeaturePipeline::reset(){head_=0;count_=0;ring_={};}
FeatureSummary MotionFeaturePipeline::push(const FeatureSample& s){ ring_[head_]=s; head_=(head_+1)%WindowSize; count_=std::min(count_+1,WindowSize); return summary(); }
FeatureSummary MotionFeaturePipeline::summary() const {
  FeatureSummary o{}; o.samples=static_cast<uint16_t>(count_); if(!count_)return o;
  float minA=1e9f,maxA=-1e9f,sumS=0,sumS2=0,sumT=0,sumP=0; int turning=0; float lastVel=0;
  for(std::size_t j=0;j<count_;++j){
    const std::size_t idx=(head_+WindowSize-count_+j)%WindowSize; const auto&s=ring_[idx];
    minA=std::min(minA,s.primaryAngleDeg);maxA=std::max(maxA,s.primaryAngleDeg);
    const float v=std::fabs(s.angularSpeedDegS);sumS+=v;sumS2+=v*v;sumT+=std::fabs(s.torsoTiltDeg);sumP+=std::fabs(s.planeDeviationDeg);
    if(j>0 && ((lastVel<0&&s.angularSpeedDegS>=0)||(lastVel>0&&s.angularSpeedDegS<=0)))turning++;
    lastVel=s.angularSpeedDegS;
  }
  const float n=static_cast<float>(count_); o.angleRangeDeg=maxA-minA;o.meanSpeedDegS=sumS/n;
  o.speedStdDegS=std::sqrt(std::max(0.0f,sumS2/n-o.meanSpeedDegS*o.meanSpeedDegS));o.meanTorsoTiltDeg=sumT/n;o.meanPlaneDeviationDeg=sumP/n;
  const float variability=std::clamp(o.speedStdDegS/120.0f,0.0f,1.0f);const float torso=std::clamp(o.meanTorsoTiltDeg/40.0f,0.0f,1.0f);const float plane=std::clamp(o.meanPlaneDeviationDeg/50.0f,0.0f,1.0f);
  o.stabilityScore=std::clamp(1.0f-(0.5f*variability+0.25f*torso+0.25f*plane),0.0f,1.0f);
  if(count_>=2){const std::size_t first=(head_+WindowSize-count_)%WindowSize;const std::size_t last=(head_+WindowSize-1)%WindowSize;const uint32_t dt=ring_[last].timestampMs-ring_[first].timestampMs;if(dt>0)o.estimatedCadenceRpm=(turning*0.5f)*60000.0f/dt;}
  o.ready=count_>=8;return o;
}
MotionFrame MotionFeaturePipeline::enrich(const MotionFrame& in) const {MotionFrame f=in;auto s=summary();if(s.ready){f.stabilityScore=s.stabilityScore;if(f.cadenceRpm<=0)f.cadenceRpm=s.estimatedCadenceRpm;}return f;}
}
