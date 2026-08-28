#include "v5_motion_adapter.h"

namespace rehab {
MotionFrame makeMotionFrameFromV5(const V5MotionSnapshot& in){
  MotionFrame f{};
  f.timestampMs=in.timestampMs;
  for(uint8_t i=0;i<5;i++) f.imu[i].online=(in.imuMask & (1u<<i))!=0;
  f.primaryJointAngleDeg=in.primaryAngleDeg;
  f.secondaryJointAngleDeg=in.secondaryAngleDeg;
  f.upperArmDeviationDeg=in.upperDeviationDeg;
  f.planeDeviationDeg=in.planeDeviationDeg;
  f.torsoTiltDeg=in.torsoTiltDeg;
  f.torsoYawRelativeDeg=in.torsoYawRelativeDeg;
  f.angularSpeedDegS=in.angularSpeedDegS;
  f.stabilityScore=in.stabilityScore;
  f.leftRightDifferenceDeg=in.leftRightDifferenceDeg;
  f.verticalExcursionDeg=in.verticalExcursionDeg;
  f.forwardLeanDeg=in.forwardLeanDeg;
  f.cadenceRpm=in.cadenceRpm;
  return f;
}
}
