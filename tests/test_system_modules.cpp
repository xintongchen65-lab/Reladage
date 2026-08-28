#include "../firmware/controller/RehabMotion_v5/src/rehab/application/rehab_runtime.h"
#include "../firmware/controller/RehabMotion_v5/src/rehab/protocol/rehab_protocol.h"
#include "../firmware/controller/RehabMotion_v5/src/rehab/storage/session_report.h"
#include "../firmware/controller/RehabMotion_v5/src/rehab/games/game_mapping.h"
#include "../firmware/controller/RehabMotion_v5/src/rehab/digital_twin/motionframe_packet.h"
#include "../firmware/controller/RehabMotion_v5/src/rehab/motion_engine/core/exercise_profile.h"
#include "../firmware/controller/RehabMotion_v5/src/rehab/integration/v5_motion_adapter.h"
#include "../firmware/controller/RehabMotion_v5/src/rehab/application/rehab_system_orchestrator.h"
#include <cassert>
#include <iostream>
using namespace rehab;

static ExerciseFeedback runOneRep(RehabRuntime& rt, ExerciseId id){
  rt.selectExercise(id); rt.start(); const auto& p=exerciseProfile(id);
  MotionFrame f{}; f.stabilityScore=.95f;
  f.timestampMs=100; f.primaryJointAngleDeg=p.activateDeg+4; f.secondaryJointAngleDeg=p.activateDeg+4; f.verticalExcursionDeg=p.activateDeg+4; rt.update(f);
  f.timestampMs=900; f.primaryJointAngleDeg=p.targetDeg+5; f.secondaryJointAngleDeg=p.targetDeg+5; f.verticalExcursionDeg=p.targetDeg+5; rt.update(f);
  f.timestampMs=1800; f.primaryJointAngleDeg=p.returnDeg; f.secondaryJointAngleDeg=p.returnDeg; f.verticalExcursionDeg=p.returnDeg;
  return rt.update(f).feedback;
}

int main(){
  auto c=parseCommandLine("CMD|SELECT_EXERCISE|4"); assert(c.type==CommandType::SelectExercise && c.value==4);
  assert(makeDeviceStateMessage(true,0x1F,82,true,true).find("imu=1F")!=std::string::npos);

  RehabRuntime rt; rt.setSdReady(true); rt.setWiFiConnected(false);
  auto f=runOneRep(rt,ExerciseId::DumbbellCurl); assert(f.repCompleted);
  SessionAccumulator acc; acc.add(f); auto sum=acc.summary(); assert(sum.totalReps==1);
  auto ge=mapFeedbackToGame(ExerciseId::DumbbellCurl,f); assert(ge.game==GameId::Rowing);

  V5MotionSnapshot snap{}; snap.timestampMs=42; snap.primaryAngleDeg=61; snap.torsoTiltDeg=3; snap.imuMask=0x1F;
  MotionFrame mf=makeMotionFrameFromV5(snap); assert(mf.imu[4].online && mf.primaryJointAngleDeg==61);
  auto tp=makeTwinPacket(mf); assert(tp.timestampMs==42); assert(twinPacketToJson(tp).find("primary_angle_deg")!=std::string::npos);

  RehabSystemOrchestrator sys;
  sys.setConnectivity(false,true);
  sys.selectExercise(ExerciseId::DumbbellCurl);
  sys.start();
  V5MotionSnapshot s1{}; s1.imuMask=0x1F; s1.stabilityScore=.95f; s1.timestampMs=100; s1.primaryAngleDeg=45;
  sys.update(s1);
  s1.timestampMs=900; s1.primaryAngleDeg=86; sys.update(s1);
  s1.timestampMs=1800; s1.primaryAngleDeg=20; auto so=sys.update(s1);
  assert(so.runtime.feedback.repCompleted);
  assert(so.persistRepRecord);
  assert(sys.pendingSyncCount()==1);
  sys.setConnectivity(true,true);
  s1.timestampMs=1900; auto flush=sys.update(s1);
  assert(flush.cloudPayloadReady);
  std::cout << "system modules ok\n";
  return 0;
}
