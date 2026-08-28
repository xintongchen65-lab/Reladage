#include "../firmware/controller/RehabMotion_v5/src/rehab/motion_engine/core/exercise_engine.h"
#include "../firmware/controller/RehabMotion_v5/src/rehab/motion_engine/core/exercise_profile.h"
#include "../firmware/controller/RehabMotion_v5/src/rehab/motion_engine/core/exercise_calibration.h"
#include "../firmware/controller/RehabMotion_v5/src/rehab/motion_engine/core/exercise_semantics.h"
#include "../firmware/controller/RehabMotion_v5/src/rehab/motion_engine/core/telemetry_schema.h"
#include "../firmware/controller/RehabMotion_v5/src/rehab/motion_engine/core/motion_feature_pipeline.h"
#include <cassert>
#include <iostream>
using namespace rehab;

static ExerciseFeedback goodRep(ExerciseId id){
  const auto& p=exerciseProfile(id);
  ExerciseEngine e; e.select(id);
  MotionFrame f{}; f.stabilityScore=.92f; f.angularSpeedDegS=40;
  f.timestampMs=100; f.primaryJointAngleDeg=p.activateDeg+5; f.secondaryJointAngleDeg=p.activateDeg+5; f.verticalExcursionDeg=p.activateDeg+5; e.update(f);
  f.timestampMs=900; f.primaryJointAngleDeg=p.targetDeg+6; f.secondaryJointAngleDeg=p.targetDeg+6; f.verticalExcursionDeg=p.targetDeg+6; e.update(f);
  f.timestampMs=1800; f.angularSpeedDegS=30; f.primaryJointAngleDeg=p.returnDeg; f.secondaryJointAngleDeg=p.returnDeg; f.verticalExcursionDeg=p.returnDeg;
  return e.update(f);
}

static void exerciseCalibrationSmoke(ExerciseId id){
  const auto& cp=exerciseCalibrationProfile(id);
  ExerciseCalibrator cal(id); MotionFrame f{}; f.stabilityScore=.95f; f.angularSpeedDegS=0;
  f.timestampMs=100; cal.update(f);
  f.timestampMs=100+cp.startStillMs+20; auto s=cal.update(f); assert(s.state==CalibrationState::LearnMotion);
  f.angularSpeedDegS=35; f.primaryJointAngleDeg=cp.minLearnRomDeg+8; f.secondaryJointAngleDeg=cp.minLearnRomDeg+8; f.verticalExcursionDeg=cp.minLearnRomDeg+8;
  f.timestampMs+=600; cal.update(f);
  f.primaryJointAngleDeg=10; f.secondaryJointAngleDeg=10; f.verticalExcursionDeg=10; f.angularSpeedDegS=25; f.timestampMs+=500; s=cal.update(f); assert(s.state==CalibrationState::ReturnStill);
  f.angularSpeedDegS=0; f.timestampMs+=10; cal.update(f);
  f.timestampMs+=cp.returnStillMs+20; s=cal.update(f); assert(s.ready && s.state==CalibrationState::Completed);
}

int main(){
  for(int i=0;i<8;i++){
    auto id=static_cast<ExerciseId>(i);
    const auto& p=exerciseProfile(id); const auto& sem=exerciseSemantics(id); const auto& cp=exerciseCalibrationProfile(id);
    assert(p.code && p.nameZh && sem.sensorLayoutZh && cp.instructionZh);
    auto r=goodRep(id); assert(r.repCompleted); assert(r.repAccepted); assert(r.metrics.durationMs>=650); assert(r.metrics.sampleCount>=3);
    auto js=feedbackToJson(r); assert(js.find("quality_flags")!=std::string::npos); assert(js.find("metrics")!=std::string::npos);
    std::cout << i+1 << " " << exerciseCode(id) << " " << js << "\n";
    exerciseCalibrationSmoke(id);
  }
  MotionFeaturePipeline fp;
  for(int i=0;i<16;i++){
    FeatureSample x{}; x.timestampMs=100*i; x.primaryAngleDeg=(i<8?i:15-i)*10.0f; x.angularSpeedDegS=(i<8?55.0f:-55.0f); x.torsoTiltDeg=2; x.planeDeviationDeg=3; fp.push(x);
  }
  auto fs=fp.summary(); assert(fs.ready && fs.angleRangeDeg>=70 && fs.stabilityScore>0.5f);

  // Persistent deviation must be retained across the whole rep rather than lost at return pose.
  ExerciseEngine e; e.select(ExerciseId::DumbbellCurl); const auto& p=exerciseProfile(ExerciseId::DumbbellCurl); MotionFrame f{}; f.stabilityScore=.9f;
  f.timestampMs=100; f.primaryJointAngleDeg=p.activateDeg+5; e.update(f);
  f.timestampMs=900; f.primaryJointAngleDeg=p.targetDeg+5; f.planeDeviationDeg=55; e.update(f);
  f.timestampMs=1800; f.primaryJointAngleDeg=p.returnDeg; f.planeDeviationDeg=0; auto bad=e.update(f);
  assert(bad.repCompleted && !bad.repAccepted && (bad.qualityFlags & QF_PLANE_DEVIATION));
  return 0;
}
