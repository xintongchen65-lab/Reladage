#pragma once
#include <Arduino.h>
#include <math.h>
#include <string.h>

// RehabMotion V4.4.16 — continuous body-frame quality reference.
//
// E never enters elbow ROM. It continuously supplies torso tilt T and the BODY reference
// for arm-vs-torso rotation. V4.4.16 deliberately avoids cross-IMU absolute yaw/quaternion
// heading: each IMU's gyro is projected onto its OWN gravity-defined vertical axis and
// BODY_AZ integrates verticalGyro(A)-verticalGyro(E). Whole-body turning therefore cancels.
//
// Sensor roles: A=left upper, B=left lower, C=right upper, D=right lower, E=waist/abdomen torso reference.
// Formal rules:
//   U = A sagittal/in-plane participation.
//   P = max(A side tilt, K11 semantic hinge deviation, gross A-E BODY_AZIMUTH >=90deg). Lower BODY_AZ remains diagnostic only.
//   T = filtered E gravity tilt.
// B forearm quaternion plane is formal after deliberate E-gyro BODY-turn compensation;
// WT901 Euler yaw and raw-acceleration side remain diagnostic only.
//
// Calibration happens once: neutral/down -> declared BODY FRONT -> one K11 motion.
// Normal training is continuous. Breathing and ordinary small torso motion never gate repetitions,
// and there is no per-repetition hold/recalibration state.

static constexpr int TORSO_REF_UPPER_SLOT = 0; // A
static constexpr int TORSO_REF_FORE_SLOT  = 1; // B
static constexpr int TORSO_REF_SLOT       = 4; // E waist/abdomen

enum BodyFrameCalPhase : uint8_t {
  BF_OFF = 0,
  BF_DOWN_STILL,
  BF_FRONT_RAISES,
  BF_SIDE_RAISES, // compatibility only
  BF_READY,
  BF_FAILED
};

struct BFSemanticVec {
  float front = 0.0f;
  float side  = 0.0f;
  float down  = 1.0f;
  bool valid = false;
};

struct BodyFrameSideCal {
  BodyFrameCalPhase phase = BF_OFF;
  unsigned long stillStartMs = 0;
  unsigned long unstableStartMs = 0;
  unsigned long poseUnstableStartMs = 0;
  unsigned long lastDiagMs = 0;
  float pairAlignmentDeg = 999.0f;
  const char *failReason = "none";

  // V4.4.16 keeps the proven V4.4.7 front-stage state machine.
  bool frontArmed = false;
  bool frontRaiseSeen = false;
  unsigned long frontNeutralConfirmStartMs = 0;

  // Candidate keeps both local gravity (for pose validation) and each sensor's own
  // orientation quaternion.  The quaternion is never mixed with E; it is only used
  // inside that same sensor's heading frame to distinguish BODY-FRONT from SIDE.
  WT901Vec3 frontCandidateAGravityLocal = {0,0,1};
  WT901Vec3 frontCandidateBGravityLocal = {0,0,1};
  WT901Quat frontCandidateAQ = {1,0,0,0};
  WT901Quat frontCandidateBQ = {1,0,0,0};
  bool frontCandidateReady = false;
  unsigned long frontCandidateMs = 0;
};

static BodyFrameSideCal bodyFrameCal[2];
static int bodyFrameActiveSide = 0;

struct TorsoReferenceState {
  bool neutralReady = false;
  bool frontReady = false;
  bool upperBaselineReady = false;

  // Compatibility names retained. In V4.4.16 these are the neutral local gravity
  // vectors of A/B and therefore also the neutral limb-long proxies.
  WT901Vec3 upperLongLocal = {0,0,1};
  WT901Vec3 foreLongLocal  = {0,0,1};

  // A/B-only arm references.
  WT901Vec3 upperNeutralGravityLocal = {0,0,1};
  WT901Vec3 foreNeutralGravityLocal  = {0,0,1};
  WT901Vec3 upperFrontGravityLocal   = {1,0,0};
  WT901Vec3 foreFrontGravityLocal    = {1,0,0};

  // V4.4.16 TRUE-PLANE reference.  Each sensor gets its OWN yaw-offset-safe
  // pseudo-world frame.  We rotate the fixed limb-long axis (captured at neutral)
  // by that SAME sensor's quaternion.  Unknown constant yaw offset cancels because
  // BODY-FRONT and formal motion are compared in the same per-sensor frame.
  WT901Quat upperNeutralQ = {1,0,0,0};
  WT901Quat foreNeutralQ  = {1,0,0,0};
  WT901Quat upperFrontQ   = {1,0,0,0};
  WT901Quat foreFrontQ    = {1,0,0,0};
  WT901Vec3 upperDownAxisWorld  = {0,0,1};
  WT901Vec3 upperFrontAxisWorld = {1,0,0};
  WT901Vec3 upperSideAxisWorld  = {0,1,0};
  WT901Vec3 foreDownAxisWorld   = {0,0,1};
  WT901Vec3 foreFrontAxisWorld  = {1,0,0};
  WT901Vec3 foreSideAxisWorld   = {0,1,0};

  // Compatibility aliases retained so older UI/JSON code still builds.
  WT901Vec3 upperDownAxisLocal  = {0,0,1};
  WT901Vec3 upperFrontAxisLocal = {1,0,0};
  WT901Vec3 upperSideAxisLocal  = {0,1,0};
  WT901Vec3 foreDownAxisLocal   = {0,0,1};
  WT901Vec3 foreFrontAxisLocal  = {1,0,0};
  WT901Vec3 foreSideAxisLocal   = {0,1,0};

  // V4.4.16 upper-arm compensation baseline: A local gravity only.
  WT901Vec3 upperBaselineGravityLocal = {0,0,1};

  // E torso references (tilt T + continuous differential vertical-gyro BODY_AZ diagnostic).
  WT901Quat torsoNeutralQ = {1,0,0,0};
  WT901Quat torsoExerciseBaselineQ = {1,0,0,0};
  bool torsoExerciseBaselineReady = false;
  WT901Vec3 torsoNeutralGravityLocal = {0,0,1};
  WT901Vec3 torsoExerciseBaselineGravityLocal = {0,0,1};
  bool torsoExerciseBaselineGravityReady = false;

  // Compatibility fields kept because JSON/other code accesses them directly.
  // They are NOT valid E-local anatomical front/side axes in V4.4.16.
  WT901Vec3 downAxisTorso  = {0,0,1};
  WT901Vec3 frontAxisTorso = {0,0,0};
  WT901Vec3 sideAxisTorso  = {0,0,0};
  WT901Vec3 tangent1Torso  = {0,0,0};
  WT901Vec3 tangent2Torso  = {0,0,0};
  WT901Vec3 upperBaselineTorso = {0,0,1}; // compatibility only

  float upperRelTorsoDeg = 0.0f;      // API name retained; now A-only baseline deviation
  float torsoDeviationDeg = 0.0f;     // full 3D E quaternion diagnostic only
  float torsoTiltDeviationDeg = 0.0f; // E gravity tilt; formal torso metric
  float torsoTiltFilteredDeg = 0.0f;
  bool torsoTiltFilterReady = false;
  unsigned long torsoTiltLastSampleMs = 0;

  float upperNeutralElevationDeg = 0.0f;
  float foreNeutralElevationDeg = 0.0f;
  float elbowProxyDeg = 0.0f; // semantic diagnostic only; K11 remains authoritative
  float foreSagittalPlaneDeviationDeg = 0.0f;
  float foreSagittalPlaneDeviationRawDeg = 0.0f; // fixed BODY-FRONT X-Z plane, no E-yaw compensation; calibration gate only
  float upperSagittalPlaneDeviationDeg = 0.0f;
  float foreFrontComponent = 0.0f;
  float foreSideComponent = 0.0f;

  // V4.4.24 BODY-relative forearm plane.
  // Save E yaw at BODY-FRONT calibration, then rotate the accepted X/Z plane
  // by E's current yaw delta so the plane follows whole-body turning.
  bool torsoFrontYawReferenceReady = false;
  float torsoFrontYawReferenceDeg = 0.0f;
  // Diagnostic only: WT901 fused Euler yaw can jump/drift with magnetic heading and
  // MUST NOT rotate the formal exercise plane.
  float torsoYawFromFrontDeg = 0.0f;

  // V4.4.29 robust BODY-turn tracker. The formal X-Z plane follows only a
  // physically observed waist rotation about gravity, measured from E gyro.
  // Low-rate bias and magnetometer/Euler-yaw jumps are ignored.
  float torsoBodyTurnYawDeg = 0.0f;
  float torsoBodyTurnYawRateFilteredDegS = 0.0f;
  float torsoBodyTurnYawBiasDegS = 0.0f;
  unsigned long torsoBodyTurnYawLastSampleMs = 0;
  bool torsoBodyTurnYawReady = false;

  // V4.4.30 formal arm-plane heading: accumulated physical rotation of B about
  // world/body Z minus the same E torso rotation.  It uses callback-time gyro
  // integration with roll/pitch only, so magnetometer/Euler-yaw drift cannot move it.
  float upperarmRelativeBodyYawDeg = 0.0f;
  float forearmRelativeBodyYawDeg = 0.0f;

  // V4.4.16 local semantic bases.  These are built from each sensor's OWN neutral/front
  // gravity vectors, so they are mounting-independent and never compare A-local to B-local.
  bool upperLocalSemanticReady = false;
  bool foreLocalSemanticReady = false;
  WT901Vec3 upperSemanticDownLocal  = {0,0,1};
  WT901Vec3 upperSemanticFrontLocal = {1,0,0};
  WT901Vec3 upperSemanticSideLocal  = {0,1,0};
  WT901Vec3 foreSemanticDownLocal   = {0,0,1};
  WT901Vec3 foreSemanticFrontLocal  = {1,0,0};
  WT901Vec3 foreSemanticSideLocal   = {0,1,0};
  float upperBaselineInPlaneDeg = 0.0f;
  float upperBaselineSideTiltDeg = 0.0f;
  float foreBaselineSideTiltDeg = 0.0f;
  float upperInPlaneParticipationDeg = 0.0f; // U: sagittal shoulder participation only
  float upperSideTiltDeg = 0.0f;             // P component: upper-arm ab/adduction
  float foreSideTiltDeg = 0.0f;              // diagnostic only: raw/local forearm side indicator

  // V4.4.16 body-relative azimuth: heading-independent differential vertical gyro.
  // Gravity gives the same physical vertical axis in EACH sensor's local frame.
  // Therefore yaw-rate-about-vertical is coordinate invariant:
  //   wA_vertical = dot(gyroA_local, gravityA_local)
  //   wE_vertical = dot(gyroE_local, gravityE_local)
  // BODY_AZ integrates their difference. Whole-body turns appear in both and cancel;
  // arm rotation relative to torso remains. No absolute quaternion/magnetometer heading
  // is required, and normal breathing is never used as a training gate.
  WT901Quat upperExerciseBaselineQ = {1,0,0,0};
  bool upperExerciseBaselineQReady = false;
  bool bodyPlaneReferenceValid = false;
  float armVerticalTwistDeltaDeg = 0.0f;
  float torsoVerticalTwistDeltaDeg = 0.0f;
  float bodyRelativeAzimuthDeg = 0.0f;

  // Continuous vertical-yaw integration state. Legacy field names are retained for JSON/API.
  float armVerticalYawIntegralDeg = 0.0f;
  float torsoVerticalYawIntegralDeg = 0.0f;
  float armVerticalYawRateDegS = 0.0f;
  float torsoVerticalYawRateDegS = 0.0f;
  float armVerticalYawBiasDegS = 0.0f;
  float torsoVerticalYawBiasDegS = 0.0f;
  float armVerticalYawPrevRateDegS = 0.0f;
  float torsoVerticalYawPrevRateDegS = 0.0f;
  bool armVerticalYawPrevReady = false;
  bool torsoVerticalYawPrevReady = false;
  unsigned long armVerticalYawLastSampleMs = 0;
  unsigned long torsoVerticalYawLastSampleMs = 0;
};

static TorsoReferenceState torsoRef;

static constexpr unsigned long TR_SAMPLE_FRESH_MS = 600UL;
static constexpr unsigned long TR_NEUTRAL_HOLD_MS = 1500UL;
static constexpr unsigned long TR_FRONT_ARM_NEUTRAL_CONFIRM_MS = 350UL;
static constexpr unsigned long TR_FRONT_HOLD_MS = 650UL;
static constexpr float TR_ARM_STILL_GYRO_MAX_DEG_S = 6.0f;
static constexpr float TR_TORSO_STILL_GYRO_MAX_DEG_S = 20.0f; // E breathing never has to be "still"; only gross torso motion is rejected during one-time calibration
static constexpr float TR_STILL_GROSS_MOTION_DEG_S = 20.0f;
static constexpr unsigned long TR_STILL_GLITCH_GRACE_MS = 180UL;
static constexpr unsigned long TR_FRONT_POSE_GLITCH_GRACE_MS = 220UL;
static constexpr unsigned long TR_FRONT_MAX_SAMPLE_SKEW_MS = 180UL;
static constexpr float TR_ACCEL_MIN_G = 0.72f;
static constexpr float TR_ACCEL_MAX_G = 1.28f;
static constexpr float TR_FRONT_ARMED_MAX_DELTA_DEG = 15.0f;
static constexpr float TR_FRONT_RAISE_DETECT_MIN_DEG = 30.0f;
static constexpr float TR_FRONT_MIN_ELEV_DEG = 40.0f;
static constexpr float TR_FRONT_MAX_ELEV_DEG = 110.0f;
static constexpr float TR_FRONT_AB_DELTA_DIFF_MAX_DEG = 20.0f;
static constexpr float TR_FRONT_TORSO_TILT_MAX_DEG = 18.0f;
static constexpr unsigned long TR_DIAG_MS = 700UL;
// V4.4.16 continuous body-relative vertical-yaw tracker.
static constexpr float TR_VERTICAL_YAW_DEADZONE_DEG_S = 0.8f;
static constexpr float TR_VERTICAL_YAW_BIAS_CAPTURE_MAX_DEG_S = 3.0f;
static constexpr unsigned long TR_VERTICAL_YAW_MAX_DT_MS = 900UL;
static constexpr float TR_VERTICAL_YAW_ZERO_SNAP_MAX_DEG = 15.0f;
static constexpr float TR_VERTICAL_YAW_ZERO_SNAP_RATE_DEG_S = 2.5f;
static constexpr float TR_VERTICAL_YAW_ZERO_SNAP_UPPER_DEG = 10.0f;
static constexpr float TR_VERTICAL_YAW_ZERO_SNAP_TORSO_DEG = 12.0f;
static constexpr float TR_VERTICAL_YAW_ZERO_SNAP_ALPHA = 0.12f;
// V4.4.29: BODY turn used by the formal plane is intentionally conservative.
// A real whole-body turn is a clear waist angular motion; slow 1-3 deg/s gyro bias
// and magnetometer/Euler yaw jumps must never rotate the accepted X-Z plane.
static constexpr float TR_BODY_TURN_RATE_GATE_DEG_S = 7.0f;
static constexpr float TR_BODY_TURN_RATE_FILTER_ALPHA = 0.35f;
static constexpr float TR_BODY_TURN_BIAS_ADAPT_MAX_DEG_S = 3.0f;
static constexpr float TR_BODY_TURN_MAX_DT_S = 0.25f;

static WT901Vec3 trVec(float x,float y,float z){ WT901Vec3 v={x,y,z}; return v; }
static float trDot(const WT901Vec3&a,const WT901Vec3&b){return a.x*b.x+a.y*b.y+a.z*b.z;}
static float trNorm(const WT901Vec3&a){return sqrtf(trDot(a,a));}
static bool trNormalize(WT901Vec3 &v){float n=trNorm(v); if(!isfinite(n)||n<1e-5f)return false;v.x/=n;v.y/=n;v.z/=n;return true;}
static WT901Vec3 trAdd(const WT901Vec3&a,const WT901Vec3&b){return trVec(a.x+b.x,a.y+b.y,a.z+b.z);}
static WT901Vec3 trScale(const WT901Vec3&a,float s){return trVec(a.x*s,a.y*s,a.z*s);}
static WT901Vec3 trSub(const WT901Vec3&a,const WT901Vec3&b){return trVec(a.x-b.x,a.y-b.y,a.z-b.z);}
static WT901Vec3 trCross(const WT901Vec3&a,const WT901Vec3&b){return trVec(a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x);}
static float trClamp1(float v){if(v>1.0f)return 1.0f;if(v<-1.0f)return -1.0f;return v;}
static float trAngleDeg(const WT901Vec3&a,const WT901Vec3&b){
  float na=trNorm(a),nb=trNorm(b); if(na<1e-5f||nb<1e-5f)return 999.0f;
  return acosf(trClamp1(trDot(a,b)/(na*nb)))*57.2957795131f;
}
static float trAbsMaxGyro(const WT901EulerData &d){
  float m=fabsf(d.gx); if(fabsf(d.gy)>m)m=fabsf(d.gy); if(fabsf(d.gz)>m)m=fabsf(d.gz); return m;
}
static float trAccelNorm(const WT901EulerData &d){return sqrtf(d.ax*d.ax+d.ay*d.ay+d.az*d.az);}
static bool trGravityFromData(const WT901EulerData &d,WT901Vec3 &g){
  if(!d.valid)return false; g=trVec(d.ax,d.ay,d.az); return trNormalize(g);
}
static bool trLocalGravity(int slot,WT901Vec3 &g){
  if(slot<0||slot>=WT901_ABCD_COUNT)return false;
  return trGravityFromData(wt901Slots[slot].latest,g);
}
static WT901Quat trCurrentQuat(int slot){
  const WT901EulerData &d=wt901Slots[slot].latest;
  return wt901EulerDegToQuat(d.roll,d.pitch,d.yaw);
}
static float trSlotGyroAbsMax(int slot){
  if(slot<0||slot>=WT901_ABCD_COUNT)return 999.0f;
  const WT901EulerData &d=wt901Slots[slot].latest;
  if(!d.valid)return 999.0f;
  return trAbsMaxGyro(d);
}
static bool trSlotFreshStill(int slot){
  if(slot<0||slot>=WT901_ABCD_COUNT)return false;
  const WT901Slot &s=wt901Slots[slot]; const WT901EulerData &d=s.latest;
  if(!wt901SlotReceiving(slot)||!d.valid||d.sampleMs==0)return false;
  if(millis()-d.sampleMs>TR_SAMPLE_FRESH_MS)return false;
  float an=trAccelNorm(d); if(!isfinite(an)||an<TR_ACCEL_MIN_G||an>TR_ACCEL_MAX_G)return false;
  const float limit=(slot==TORSO_REF_SLOT)?TR_TORSO_STILL_GYRO_MAX_DEG_S:TR_ARM_STILL_GYRO_MAX_DEG_S;
  return trAbsMaxGyro(d)<=limit;
}
static bool trCombinedGrossMotion(){
  return trSlotGyroAbsMax(TORSO_REF_UPPER_SLOT)>TR_STILL_GROSS_MOTION_DEG_S ||
         trSlotGyroAbsMax(TORSO_REF_FORE_SLOT)>TR_STILL_GROSS_MOTION_DEG_S ||
         trSlotGyroAbsMax(TORSO_REF_SLOT)>TR_STILL_GROSS_MOTION_DEG_S;
}
static void trPrintStillDiag(const char *tag,bool rawStill){
  Serial.printf("%s: still=%d gyroMax[A/B/E]=%.2f/%.2f/%.2f deg/s limits=%.1f/%.1f/%.1f\n",
                tag,rawStill?1:0,
                trSlotGyroAbsMax(TORSO_REF_UPPER_SLOT),
                trSlotGyroAbsMax(TORSO_REF_FORE_SLOT),
                trSlotGyroAbsMax(TORSO_REF_SLOT),
                TR_ARM_STILL_GYRO_MAX_DEG_S,TR_ARM_STILL_GYRO_MAX_DEG_S,TR_TORSO_STILL_GYRO_MAX_DEG_S);
}

struct TRFrontSnapshot {
  WT901EulerData a;
  WT901EulerData b;
  WT901EulerData e;
  bool valid = false;
  unsigned long minSampleMs = 0;
  unsigned long maxSampleMs = 0;
  unsigned long skewMs = 999999UL;
};
static unsigned long trMin3ul(unsigned long a,unsigned long b,unsigned long c){unsigned long m=a<b?a:b;return m<c?m:c;}
static unsigned long trMax3ul(unsigned long a,unsigned long b,unsigned long c){unsigned long m=a>b?a:b;return m>c?m:c;}
static bool trTakeFrontSnapshot(TRFrontSnapshot &s){
  s=TRFrontSnapshot();
  if(!wt901SnapshotLatestABE(s.a,s.b,s.e))return false;
  s.minSampleMs=trMin3ul(s.a.sampleMs,s.b.sampleMs,s.e.sampleMs);
  s.maxSampleMs=trMax3ul(s.a.sampleMs,s.b.sampleMs,s.e.sampleMs);
  s.skewMs=s.maxSampleMs-s.minSampleMs;
  unsigned long now=millis();
  if(s.minSampleMs==0 || now-s.minSampleMs>TR_SAMPLE_FRESH_MS)return false;
  s.valid=true;
  return true;
}
static bool trSnapshotAccelPlausible(const WT901EulerData &d){
  float an=trAccelNorm(d); return isfinite(an)&&an>=TR_ACCEL_MIN_G&&an<=TR_ACCEL_MAX_G;
}
static bool trFrontSnapshotStill(const TRFrontSnapshot &s){
  if(!s.valid||s.skewMs>TR_FRONT_MAX_SAMPLE_SKEW_MS)return false;
  if(!trSnapshotAccelPlausible(s.a)||!trSnapshotAccelPlausible(s.b)||!trSnapshotAccelPlausible(s.e))return false;
  return trAbsMaxGyro(s.a)<=TR_ARM_STILL_GYRO_MAX_DEG_S &&
         trAbsMaxGyro(s.b)<=TR_ARM_STILL_GYRO_MAX_DEG_S &&
         trAbsMaxGyro(s.e)<=TR_TORSO_STILL_GYRO_MAX_DEG_S;
}
static bool trFrontSnapshotGrossMotion(const TRFrontSnapshot &s){
  if(!s.valid)return true;
  return trAbsMaxGyro(s.a)>TR_STILL_GROSS_MOTION_DEG_S ||
         trAbsMaxGyro(s.b)>TR_STILL_GROSS_MOTION_DEG_S ||
         trAbsMaxGyro(s.e)>TR_STILL_GROSS_MOTION_DEG_S;
}
static void trPrintFrontSnapshotDiag(const char *tag,const TRFrontSnapshot&s,bool still){
  Serial.printf("%s: still=%d skew=%lums gyroMax[A/B/E]=%.2f/%.2f/%.2f deg/s\n",
    tag,still?1:0,(unsigned long)s.skewMs,
    s.valid?trAbsMaxGyro(s.a):999.0f,s.valid?trAbsMaxGyro(s.b):999.0f,s.valid?trAbsMaxGyro(s.e):999.0f);
}

static bool trBuildSemanticBasis(const WT901Vec3 &neutralG,const WT901Vec3 &frontG,
                                 WT901Vec3 &downAxis,WT901Vec3 &frontAxis,WT901Vec3 &sideAxis){
  downAxis=neutralG; if(!trNormalize(downAxis))return false;
  WT901Vec3 tangent=trSub(frontG,trScale(downAxis,trDot(frontG,downAxis)));
  if(!trNormalize(tangent))return false;
  frontAxis=tangent;
  sideAxis=trCross(downAxis,frontAxis);
  if(!trNormalize(sideAxis))return false;
  // Re-orthogonalize front to suppress numerical drift.
  frontAxis=trCross(sideAxis,downAxis);
  if(!trNormalize(frontAxis))return false;
  // Ensure the calibration vector has a positive FRONT component.
  if(trDot(frontG,frontAxis)<0.0f){frontAxis=trScale(frontAxis,-1.0f);sideAxis=trScale(sideAxis,-1.0f);}
  return true;
}

static bool trSemanticFromGravity(const WT901Vec3 &g,const WT901Vec3 &downAxis,
                                  const WT901Vec3 &frontAxis,const WT901Vec3 &sideAxis,
                                  BFSemanticVec &out){
  WT901Vec3 gn=g; if(!trNormalize(gn))return false;
  out.down=trDot(gn,downAxis);
  out.front=trDot(gn,frontAxis);
  out.side=trDot(gn,sideAxis);
  float n=sqrtf(out.down*out.down+out.front*out.front+out.side*out.side);
  if(n<1e-5f)return false;
  out.down/=n;out.front/=n;out.side/=n;out.valid=true;return true;
}

static float trPlaneDeviationFromSemantic(const BFSemanticVec &v){
  if(!v.valid)return 999.0f;
  return asinf(fabsf(trClamp1(v.side)))*57.2957795131f;
}

static WT901Vec3 trRotateLocalVecByQuat(const WT901Quat &q,const WT901Vec3 &v){
  return wt901RotateLocalVectorByQuat(q,v.x,v.y,v.z);
}

static float trWrap180(float a){
  while(a>180.0f)a-=360.0f;
  while(a<-180.0f)a+=360.0f;
  return a;
}

static WT901Vec3 trRotateAroundAxisDeg(const WT901Vec3 &v,const WT901Vec3 &axisIn,float deg){
  WT901Vec3 axis=axisIn;
  if(!trNormalize(axis)) return v;
  const float r=deg*0.01745329251994329577f;
  const float c=cosf(r), s=sinf(r);
  WT901Vec3 term1=trScale(v,c);
  WT901Vec3 term2=trScale(trCross(axis,v),s);
  WT901Vec3 term3=trScale(axis,trDot(axis,v)*(1.0f-c));
  return trAdd(trAdd(term1,term2),term3);
}

static float trTorsoYawFromFrontNowDeg(){
  if(!torsoRef.torsoFrontYawReferenceReady || !wt901SlotReceiving(TORSO_REF_SLOT)) return 0.0f;
  const WT901EulerData &e=wt901Slots[TORSO_REF_SLOT].latest;
  if(!e.valid || e.sampleMs==0) return 0.0f;
  return trWrap180(e.yaw-torsoRef.torsoFrontYawReferenceDeg);
}

static bool trLocalSemanticAngles(const WT901Vec3 &g,
                                  const WT901Vec3 &downAxis,const WT901Vec3 &frontAxis,const WT901Vec3 &sideAxis,
                                  float &inPlaneDeg,float &sideTiltDeg,BFSemanticVec *semanticOut=nullptr){
  BFSemanticVec v;
  if(!trSemanticFromGravity(g,downAxis,frontAxis,sideAxis,v))return false;
  // In-plane shoulder participation is the rotation in FRONT/DOWN only. SIDE is deliberately
  // excluded so abducting the arm cannot masquerade as upper-arm participation.
  inPlaneDeg=atan2f(v.front,v.down)*57.2957795131f;
  sideTiltDeg=asinf(trClamp1(v.side))*57.2957795131f;
  if(semanticOut)*semanticOut=v;
  return isfinite(inPlaneDeg)&&isfinite(sideTiltDeg);
}

static float trVerticalYawRateFromData(const WT901EulerData &d){
  WT901Vec3 g;
  if(!trGravityFromData(d,g))return 0.0f;
  WT901Vec3 gyro=trVec(d.gx,d.gy,d.gz);
  float r=trDot(gyro,g);
  return isfinite(r)?r:0.0f;
}

static float trCaptureSmallVerticalYawBias(const WT901EulerData &d){
  float r=trVerticalYawRateFromData(d);
  return (isfinite(r)&&fabsf(r)<=TR_VERTICAL_YAW_BIAS_CAPTURE_MAX_DEG_S)?r:0.0f;
}

// V4.4.29: track deliberate whole-body yaw from E's gyro projected onto gravity.
// This is mounting-axis independent. It deliberately ignores low-rate motion so
// stationary gyro bias cannot accumulate into a fake 30-90 degree body turn.
static void trResetTorsoBodyTurnYawFromLatest(){
  const WT901EulerData &e=wt901Slots[TORSO_REF_SLOT].latest;
  torsoRef.torsoBodyTurnYawDeg=0.0f;
  torsoRef.torsoBodyTurnYawRateFilteredDegS=0.0f;
  torsoRef.torsoBodyTurnYawBiasDegS=(e.valid&&e.sampleMs!=0)?trCaptureSmallVerticalYawBias(e):0.0f;
  torsoRef.torsoBodyTurnYawLastSampleMs=(e.valid?e.sampleMs:0);
  torsoRef.torsoBodyTurnYawReady=e.valid&&e.sampleMs!=0;
}

static void trUpdateTorsoBodyTurnYaw(){
  const WT901EulerData &e=wt901Slots[TORSO_REF_SLOT].latest;
  if(!wt901SlotReceiving(TORSO_REF_SLOT)||!e.valid||e.sampleMs==0)return;
  if(!torsoRef.torsoBodyTurnYawReady){
    trResetTorsoBodyTurnYawFromLatest();
    return;
  }
  if(e.sampleMs==torsoRef.torsoBodyTurnYawLastSampleMs)return;
  unsigned long dtMs=e.sampleMs-torsoRef.torsoBodyTurnYawLastSampleMs;
  torsoRef.torsoBodyTurnYawLastSampleMs=e.sampleMs;
  if(dtMs==0)return;
  float dt=(float)dtMs*0.001f;
  if(dt>TR_BODY_TURN_MAX_DT_S){
    torsoRef.torsoBodyTurnYawRateFilteredDegS=0.0f;
    return;
  }

  float raw=trVerticalYawRateFromData(e);
  if(!isfinite(raw))return;
  // Adapt only around genuine rest. Deliberate turns are far above this range.
  if(fabsf(raw)<=TR_BODY_TURN_BIAS_ADAPT_MAX_DEG_S){
    torsoRef.torsoBodyTurnYawBiasDegS=0.995f*torsoRef.torsoBodyTurnYawBiasDegS+0.005f*raw;
  }
  float rate=raw-torsoRef.torsoBodyTurnYawBiasDegS;
  torsoRef.torsoBodyTurnYawRateFilteredDegS =
    (1.0f-TR_BODY_TURN_RATE_FILTER_ALPHA)*torsoRef.torsoBodyTurnYawRateFilteredDegS +
    TR_BODY_TURN_RATE_FILTER_ALPHA*rate;

  if(fabsf(torsoRef.torsoBodyTurnYawRateFilteredDegS)>=TR_BODY_TURN_RATE_GATE_DEG_S){
    torsoRef.torsoBodyTurnYawDeg=trWrap180(
      torsoRef.torsoBodyTurnYawDeg + torsoRef.torsoBodyTurnYawRateFilteredDegS*dt);
  }
}

static void trResetBodyRelativeAzimuthFromLatest(){
  const WT901EulerData &a=wt901Slots[TORSO_REF_UPPER_SLOT].latest;
  const WT901EulerData &e=wt901Slots[TORSO_REF_SLOT].latest;
  torsoRef.bodyPlaneReferenceValid=torsoRef.frontReady&&a.valid&&e.valid&&a.sampleMs!=0&&e.sampleMs!=0;
  torsoRef.armVerticalYawIntegralDeg=0.0f;
  torsoRef.torsoVerticalYawIntegralDeg=0.0f;
  torsoRef.armVerticalTwistDeltaDeg=0.0f;
  torsoRef.torsoVerticalTwistDeltaDeg=0.0f;
  torsoRef.bodyRelativeAzimuthDeg=0.0f;
  torsoRef.armVerticalYawRateDegS=0.0f;
  torsoRef.torsoVerticalYawRateDegS=0.0f;
  torsoRef.armVerticalYawPrevRateDegS=0.0f;
  torsoRef.torsoVerticalYawPrevRateDegS=0.0f;
  torsoRef.armVerticalYawPrevReady=false;
  torsoRef.torsoVerticalYawPrevReady=false;
  torsoRef.armVerticalYawLastSampleMs=a.sampleMs;
  torsoRef.torsoVerticalYawLastSampleMs=e.sampleMs;
  torsoRef.armVerticalYawBiasDegS=torsoRef.bodyPlaneReferenceValid?trCaptureSmallVerticalYawBias(a):0.0f;
  torsoRef.torsoVerticalYawBiasDegS=torsoRef.bodyPlaneReferenceValid?trCaptureSmallVerticalYawBias(e):0.0f;
  trResetTorsoBodyTurnYawFromLatest();
}

static void trRefreshBodyRelativeAzimuthBiasKeepAngle(){
  if(!torsoRef.bodyPlaneReferenceValid)return;
  const WT901EulerData &a=wt901Slots[TORSO_REF_UPPER_SLOT].latest;
  const WT901EulerData &e=wt901Slots[TORSO_REF_SLOT].latest;
  float ar=trVerticalYawRateFromData(a), er=trVerticalYawRateFromData(e);
  if(fabsf(ar)<=TR_VERTICAL_YAW_BIAS_CAPTURE_MAX_DEG_S)
    torsoRef.armVerticalYawBiasDegS=0.9f*torsoRef.armVerticalYawBiasDegS+0.1f*ar;
  if(fabsf(er)<=TR_VERTICAL_YAW_BIAS_CAPTURE_MAX_DEG_S)
    torsoRef.torsoVerticalYawBiasDegS=0.9f*torsoRef.torsoVerticalYawBiasDegS+0.1f*er;
}

static bool trIntegrateVerticalYawSample(const WT901EulerData &d,float bias,
                                         unsigned long &lastMs,float &integralDeg,float &rateOut,
                                         float &prevRate,bool &prevReady){
  if(!d.valid||d.sampleMs==0||d.sampleMs==lastMs)return false;
  float rate=trVerticalYawRateFromData(d)-bias;
  if(!isfinite(rate))rate=0.0f;
  if(fabsf(rate)<TR_VERTICAL_YAW_DEADZONE_DEG_S)rate=0.0f;
  unsigned long dtMs=(lastMs==0)?0:(d.sampleMs-lastMs);
  lastMs=d.sampleMs;
  if(dtMs>0&&dtMs<=TR_VERTICAL_YAW_MAX_DT_MS){
    float useRate=prevReady?0.5f*(prevRate+rate):rate;
    integralDeg += useRate*((float)dtMs*0.001f);
  }
  prevRate=rate; prevReady=true; rateOut=rate;
  return true;
}

static void trUpdateBodyRelativeAzimuth(){
  if(!torsoRef.frontReady)return;
  const WT901EulerData &a=wt901Slots[TORSO_REF_UPPER_SLOT].latest;
  const WT901EulerData &e=wt901Slots[TORSO_REF_SLOT].latest;
  unsigned long now=millis();
  const bool fresh=wt901SlotReceiving(TORSO_REF_UPPER_SLOT)&&wt901SlotReceiving(TORSO_REF_SLOT)&&
     a.valid&&e.valid&&a.sampleMs!=0&&e.sampleMs!=0&&
     now-a.sampleMs<=TR_SAMPLE_FRESH_MS&&now-e.sampleMs<=TR_SAMPLE_FRESH_MS;
  if(!fresh){
    torsoRef.bodyPlaneReferenceValid=false;
    torsoRef.armVerticalYawPrevReady=false; torsoRef.torsoVerticalYawPrevReady=false;
    return;
  }
  // A transient BLE gap must not permanently kill E's body reference. Re-arm on the
  // first fresh pair without bridging the unknown gap and without changing the accumulated
  // relative body angle. This is recovery, not recalibration.
  if(!torsoRef.bodyPlaneReferenceValid){
    torsoRef.bodyPlaneReferenceValid=true;
    torsoRef.armVerticalYawLastSampleMs=a.sampleMs; torsoRef.torsoVerticalYawLastSampleMs=e.sampleMs;
    torsoRef.armVerticalYawPrevReady=false; torsoRef.torsoVerticalYawPrevReady=false;
    torsoRef.armVerticalYawRateDegS=0.0f; torsoRef.torsoVerticalYawRateDegS=0.0f;
    return;
  }

  bool ua=trIntegrateVerticalYawSample(a,torsoRef.armVerticalYawBiasDegS,
    torsoRef.armVerticalYawLastSampleMs,torsoRef.armVerticalYawIntegralDeg,torsoRef.armVerticalYawRateDegS,
    torsoRef.armVerticalYawPrevRateDegS,torsoRef.armVerticalYawPrevReady);
  bool ue=trIntegrateVerticalYawSample(e,torsoRef.torsoVerticalYawBiasDegS,
    torsoRef.torsoVerticalYawLastSampleMs,torsoRef.torsoVerticalYawIntegralDeg,torsoRef.torsoVerticalYawRateDegS,
    torsoRef.torsoVerticalYawPrevRateDegS,torsoRef.torsoVerticalYawPrevReady);

  float rel=trWrap180(torsoRef.armVerticalYawIntegralDeg-torsoRef.torsoVerticalYawIntegralDeg);

  // Invisible drift correction only near the natural hanging start posture. It never pauses
  // training and never asks the user to hold still. Large genuine relative rotations are kept.
  if(ua||ue){
    WT901Vec3 ga,ge;
    float upperDev=999.0f,torsoDev=999.0f;
    if(trLocalGravity(TORSO_REF_UPPER_SLOT,ga)&&torsoRef.upperBaselineReady)
      upperDev=trAngleDeg(ga,torsoRef.upperBaselineGravityLocal);
    if(trLocalGravity(TORSO_REF_SLOT,ge)&&torsoRef.torsoExerciseBaselineGravityReady)
      torsoDev=trAngleDeg(ge,torsoRef.torsoExerciseBaselineGravityLocal);
    float rateDiff=torsoRef.armVerticalYawRateDegS-torsoRef.torsoVerticalYawRateDegS;
    if(upperDev<=TR_VERTICAL_YAW_ZERO_SNAP_UPPER_DEG&&torsoDev<=TR_VERTICAL_YAW_ZERO_SNAP_TORSO_DEG&&
       fabsf(rel)<=TR_VERTICAL_YAW_ZERO_SNAP_MAX_DEG&&fabsf(rateDiff)<=TR_VERTICAL_YAW_ZERO_SNAP_RATE_DEG_S){
      float corr=rel*TR_VERTICAL_YAW_ZERO_SNAP_ALPHA;
      torsoRef.armVerticalYawIntegralDeg-=corr;
      rel=trWrap180(torsoRef.armVerticalYawIntegralDeg-torsoRef.torsoVerticalYawIntegralDeg);
      // Slow bias adaptation at natural rest absorbs sensor zero drift and breathing-scale motion.
      float ar=trVerticalYawRateFromData(a),er=trVerticalYawRateFromData(e);
      if(fabsf(ar)<=TR_VERTICAL_YAW_BIAS_CAPTURE_MAX_DEG_S)
        torsoRef.armVerticalYawBiasDegS=0.98f*torsoRef.armVerticalYawBiasDegS+0.02f*ar;
      if(fabsf(er)<=TR_VERTICAL_YAW_BIAS_CAPTURE_MAX_DEG_S)
        torsoRef.torsoVerticalYawBiasDegS=0.98f*torsoRef.torsoVerticalYawBiasDegS+0.02f*er;
    }
  }

  torsoRef.armVerticalTwistDeltaDeg=torsoRef.armVerticalYawIntegralDeg;
  torsoRef.torsoVerticalTwistDeltaDeg=torsoRef.torsoVerticalYawIntegralDeg;
  torsoRef.bodyRelativeAzimuthDeg=fabsf(rel);
}

static bool trSemanticSegmentWorld(int slot,const WT901Vec3 &segmentAxisLocal,
                                   const WT901Vec3 &downWorld,const WT901Vec3 &frontWorld,const WT901Vec3 &sideWorld,
                                   BFSemanticVec &out){
  if(!wt901SlotReceiving(slot))return false;
  WT901Quat q=trCurrentQuat(slot);
  WT901Vec3 segWorld=trRotateLocalVecByQuat(q,segmentAxisLocal);
  return trSemanticFromGravity(segWorld,downWorld,frontWorld,sideWorld,out);
}

static void torsoRefUpdateMetrics(){
  if(!torsoRef.neutralReady)return;

  WT901Vec3 ga,gb;
  bool okA=trLocalGravity(TORSO_REF_UPPER_SLOT,ga);
  bool okB=trLocalGravity(TORSO_REF_FORE_SLOT,gb);
  if(okA){
    torsoRef.upperNeutralElevationDeg=trAngleDeg(ga,torsoRef.upperNeutralGravityLocal);
    // Compatibility total-tilt value remains available, but formal U no longer uses it.
    torsoRef.upperRelTorsoDeg=torsoRef.upperBaselineReady?trAngleDeg(ga,torsoRef.upperBaselineGravityLocal):0.0f;
  }
  if(okB) torsoRef.foreNeutralElevationDeg=trAngleDeg(gb,torsoRef.foreNeutralGravityLocal);

  if(torsoRef.frontReady){
    trUpdateBodyRelativeAzimuth();
    trUpdateTorsoBodyTurnYaw();
    // V4.4.30: direct arm-vs-body Z rotation.  Because B and E are integrated
    // independently from the exact BLE packets, a whole-body turn appears in both
    // and cancels, while rotating only the arm/hand plane remains.
    const float eYaw = wt901FusedVerticalYawIntegralDeg(TORSO_REF_SLOT);
    float aeYaw = wt901FusedVerticalYawIntegralDeg(TORSO_REF_UPPER_SLOT) - eYaw;
    float beYaw = wt901FusedVerticalYawIntegralDeg(TORSO_REF_FORE_SLOT) - eYaw;
    torsoRef.upperarmRelativeBodyYawDeg = trWrap180(aeYaw);
    torsoRef.forearmRelativeBodyYawDeg = trWrap180(beYaw);

    if(okA && torsoRef.upperLocalSemanticReady){
      float inPlane=0.0f,side=0.0f;
      if(trLocalSemanticAngles(ga,torsoRef.upperSemanticDownLocal,torsoRef.upperSemanticFrontLocal,torsoRef.upperSemanticSideLocal,inPlane,side)){
        torsoRef.upperInPlaneParticipationDeg=fabsf(trWrap180(inPlane-torsoRef.upperBaselineInPlaneDeg));
        torsoRef.upperSideTiltDeg=fabsf(side-torsoRef.upperBaselineSideTiltDeg);
      }
    }
    if(okB && torsoRef.foreLocalSemanticReady){
      float inPlane=0.0f,side=0.0f; BFSemanticVec f;
      if(trLocalSemanticAngles(gb,torsoRef.foreSemanticDownLocal,torsoRef.foreSemanticFrontLocal,torsoRef.foreSemanticSideLocal,inPlane,side,&f)){
        torsoRef.foreSideTiltDeg=fabsf(side-torsoRef.foreBaselineSideTiltDeg);
        torsoRef.foreFrontComponent=f.front;
        torsoRef.foreSideComponent=f.side;
      }
    }

    // Quaternion segment outputs. Upper remains diagnostic; forearm is rotated with E yaw and is formal P in V4.4.24.
    if(okA && okB){
      BFSemanticVec u,f;
      if(trSemanticSegmentWorld(TORSO_REF_UPPER_SLOT,torsoRef.upperLongLocal,
          torsoRef.upperDownAxisWorld,torsoRef.upperFrontAxisWorld,torsoRef.upperSideAxisWorld,u))
        torsoRef.upperSagittalPlaneDeviationDeg=trPlaneDeviationFromSemantic(u);
      // V4.4.25: always keep an uncompensated copy against the explicitly calibrated
      // BODY-FRONT X-Z plane. This is stable during K11 calibration and is intentionally
      // NOT used as the final training plane when the person turns their whole body.
      BFSemanticVec fRaw;
      if(trSemanticSegmentWorld(TORSO_REF_FORE_SLOT,torsoRef.foreLongLocal,
          torsoRef.foreDownAxisWorld,torsoRef.foreFrontAxisWorld,torsoRef.foreSideAxisWorld,fRaw))
        torsoRef.foreSagittalPlaneDeviationRawDeg=trPlaneDeviationFromSemantic(fRaw);

      // V4.4.29 formal training path: WT901 absolute/Euler yaw is diagnostic only.
      // Rotate BODY-FRONT only by a deliberate waist turn confirmed by E gyro.
      // This prevents magnetic-heading jumps from making an X-Z curl look like Y-Z
      // (or rotating the accepted plane toward a genuinely wrong side curl).
      WT901Vec3 foreFrontBody=torsoRef.foreFrontAxisWorld;
      WT901Vec3 foreSideBody=torsoRef.foreSideAxisWorld;
      torsoRef.torsoYawFromFrontDeg=trTorsoYawFromFrontNowDeg(); // diagnostic only
      if(torsoRef.torsoBodyTurnYawReady){
        foreFrontBody=trRotateAroundAxisDeg(foreFrontBody,torsoRef.foreDownAxisWorld,torsoRef.torsoBodyTurnYawDeg);
        foreSideBody=trRotateAroundAxisDeg(foreSideBody,torsoRef.foreDownAxisWorld,torsoRef.torsoBodyTurnYawDeg);
      }
      if(trSemanticSegmentWorld(TORSO_REF_FORE_SLOT,torsoRef.foreLongLocal,
          torsoRef.foreDownAxisWorld,foreFrontBody,foreSideBody,f))
        torsoRef.foreSagittalPlaneDeviationDeg=trPlaneDeviationFromSemantic(f);
      torsoRef.elbowProxyDeg=fabsf(torsoRef.foreNeutralElevationDeg-torsoRef.upperNeutralElevationDeg);
    }
  }

  // E torso tilt remains an independent quality metric T.
  if(wt901SlotReceiving(TORSO_REF_SLOT)){
    WT901Quat qE=trCurrentQuat(TORSO_REF_SLOT);
    torsoRef.torsoDeviationDeg=wt901QuatDifferenceDeg(
      torsoRef.torsoExerciseBaselineReady?torsoRef.torsoExerciseBaselineQ:torsoRef.torsoNeutralQ,qE);

    const WT901EulerData &eTorso=wt901Slots[TORSO_REF_SLOT].latest;
    WT901Vec3 ge;
    if(torsoRef.torsoExerciseBaselineGravityReady && eTorso.valid && eTorso.sampleMs!=0 &&
       eTorso.sampleMs!=torsoRef.torsoTiltLastSampleMs && trLocalGravity(TORSO_REF_SLOT,ge)){
      torsoRef.torsoTiltLastSampleMs=eTorso.sampleMs;
      float tilt=trAngleDeg(torsoRef.torsoExerciseBaselineGravityLocal,ge);
      if(isfinite(tilt)&&tilt>=0.0f&&tilt<180.0f){
        if(!torsoRef.torsoTiltFilterReady){torsoRef.torsoTiltFilteredDeg=tilt;torsoRef.torsoTiltFilterReady=true;}
        else torsoRef.torsoTiltFilteredDeg=0.82f*torsoRef.torsoTiltFilteredDeg+0.18f*tilt;
        torsoRef.torsoTiltDeviationDeg=torsoRef.torsoTiltFilteredDeg;
      }
    }
  }
}

static bool torsoRefRebaselineUpperFromLatest(){
  WT901Vec3 ga; if(!torsoRef.neutralReady||!trLocalGravity(TORSO_REF_UPPER_SLOT,ga))return false;
  const WT901EulerData &aNow=wt901Slots[TORSO_REF_UPPER_SLOT].latest;
  if(!aNow.valid||aNow.sampleMs==0)return false;
  torsoRef.upperBaselineGravityLocal=ga;torsoRef.upperBaselineReady=true;torsoRef.upperRelTorsoDeg=0.0f;
  torsoRef.upperExerciseBaselineQ=wt901EulerDegToQuat(aNow.roll,aNow.pitch,aNow.yaw); // compatibility only
  torsoRef.upperExerciseBaselineQReady=true;
  // Matching E rebaseline immediately follows and resets the continuous A-E yaw tracker.
  torsoRef.bodyPlaneReferenceValid=false;
  torsoRef.armVerticalTwistDeltaDeg=0.0f;
  torsoRef.bodyRelativeAzimuthDeg=0.0f;
  if(torsoRef.frontReady && torsoRef.upperLocalSemanticReady){
    float inPlane=0.0f,side=0.0f;
    if(trLocalSemanticAngles(ga,torsoRef.upperSemanticDownLocal,torsoRef.upperSemanticFrontLocal,torsoRef.upperSemanticSideLocal,inPlane,side)){
      torsoRef.upperBaselineInPlaneDeg=inPlane;
      torsoRef.upperBaselineSideTiltDeg=side;
      torsoRef.upperInPlaneParticipationDeg=0.0f;
      torsoRef.upperSideTiltDeg=0.0f;
    }
  }
  WT901Vec3 gb;
  if(torsoRef.frontReady && torsoRef.foreLocalSemanticReady && trLocalGravity(TORSO_REF_FORE_SLOT,gb)){
    float inPlane=0.0f,side=0.0f;
    if(trLocalSemanticAngles(gb,torsoRef.foreSemanticDownLocal,torsoRef.foreSemanticFrontLocal,torsoRef.foreSemanticSideLocal,inPlane,side)){
      torsoRef.foreBaselineSideTiltDeg=side;
      torsoRef.foreSideTiltDeg=0.0f;
    }
  }
  Serial.printf("TORSO_REF_UPPER_BASELINE V4.4.16: A gravity + A pose baseline saved; U=in-plane. A_SIDE is P; B_SIDE is DIAGNOSTIC_ONLY. E is NOT used for elbow ROM.\n");
  return true;
}
static bool torsoRefRebaselineTorsoFromLatest(){
  if(!torsoRef.neutralReady||!wt901SlotReceiving(TORSO_REF_SLOT))return false;
  torsoRef.torsoExerciseBaselineQ=trCurrentQuat(TORSO_REF_SLOT);
  torsoRef.torsoExerciseBaselineReady=true;
  WT901Vec3 ge;if(!trLocalGravity(TORSO_REF_SLOT,ge))return false;
  torsoRef.torsoExerciseBaselineGravityLocal=ge;
  torsoRef.torsoExerciseBaselineGravityReady=true;
  torsoRef.torsoDeviationDeg=0.0f;torsoRef.torsoTiltDeviationDeg=0.0f;torsoRef.torsoTiltFilteredDeg=0.0f;
  torsoRef.torsoTiltFilterReady=true;torsoRef.torsoTiltLastSampleMs=wt901Slots[TORSO_REF_SLOT].latest.sampleMs;
  // One session/group start establishes zero once. During training BODY_AZ is continuous:
  // vertical gyro(A) - vertical gyro(E). Whole-body yaw cancels without re-calibration.
  trResetBodyRelativeAzimuthFromLatest();
  Serial.println("TORSO_REF_TORSO_BASELINE V4.4.16: BODY_AZ=integral(verticalGyroA-verticalGyroE); heading-independent; breathing does not gate training.");
  return true;
}
// Formal U: only sagittal/in-plane upper-arm participation. Compatibility total-A tilt stays in upperRelTorsoDeg.
static float torsoRefUpperDeviationDeg(){torsoRefUpdateMetrics();return torsoRef.upperInPlaneParticipationDeg;}
static float torsoRefUpperTotalTiltDiagnosticDeg(){torsoRefUpdateMetrics();return torsoRef.upperRelTorsoDeg;}
static float torsoRefUpperSideTiltDeg(){torsoRefUpdateMetrics();return torsoRef.upperSideTiltDeg;}
static float torsoRefForeSideTiltDeg(){torsoRefUpdateMetrics();return torsoRef.foreSideTiltDeg;}
static float torsoRefForeQuaternionPlaneDeviationDeg(){torsoRefUpdateMetrics();return torsoRef.frontReady?torsoRef.foreSagittalPlaneDeviationDeg:0.0f;}
static float torsoRefForeQuaternionPlaneRawDeviationDeg(){torsoRefUpdateMetrics();return torsoRef.frontReady?torsoRef.foreSagittalPlaneDeviationRawDeg:0.0f;}
static float torsoRefTorsoYawFromFrontDeg(){torsoRefUpdateMetrics();return torsoRef.frontReady?fabsf(torsoRef.torsoYawFromFrontDeg):0.0f;}
static float torsoRefUpperarmRelativeBodyYawSignedDeg(){torsoRefUpdateMetrics();return torsoRef.frontReady?torsoRef.upperarmRelativeBodyYawDeg:0.0f;}
static float torsoRefForearmRelativeBodyYawSignedDeg(){torsoRefUpdateMetrics();return torsoRef.frontReady?torsoRef.forearmRelativeBodyYawDeg:0.0f;}
static float torsoRefUpperarmRelativeBodyYawDeg(){return fabsf(torsoRefUpperarmRelativeBodyYawSignedDeg());}
static float torsoRefForearmRelativeBodyYawDeg(){return fabsf(torsoRefForearmRelativeBodyYawSignedDeg());}
static float torsoRefBodyRelativeAzimuthDeg(){torsoRefUpdateMetrics();return torsoRef.bodyPlaneReferenceValid?torsoRef.bodyRelativeAzimuthDeg:0.0f;}
static bool torsoRefBodyPlaneReferenceValid(){torsoRefUpdateMetrics();return torsoRef.bodyPlaneReferenceValid;}
static float torsoRefArmVerticalYawIntegralDeg(){torsoRefUpdateMetrics();return torsoRef.armVerticalYawIntegralDeg;}
static float torsoRefTorsoVerticalYawIntegralDeg(){torsoRefUpdateMetrics();return torsoRef.torsoVerticalYawIntegralDeg;}
static float torsoRefArmVerticalTwistDeltaDeg(){torsoRefUpdateMetrics();return torsoRef.armVerticalTwistDeltaDeg;}
static float torsoRefTorsoVerticalTwistDeltaDeg(){torsoRefUpdateMetrics();return torsoRef.torsoVerticalTwistDeltaDeg;}
static float torsoRefTorsoDeviationDeg(){torsoRefUpdateMetrics();return torsoRef.torsoDeviationDeg;}
static float torsoRefTorsoTiltDeviationDeg(){torsoRefUpdateMetrics();return torsoRef.torsoTiltDeviationDeg;}
static float torsoRefUpperNeutralElevationDeg(){torsoRefUpdateMetrics();return torsoRef.upperNeutralElevationDeg;}
static float torsoRefForeNeutralElevationDeg(){torsoRefUpdateMetrics();return torsoRef.foreNeutralElevationDeg;}
static float torsoRefElbowProxyDeg(){torsoRefUpdateMetrics();return torsoRef.elbowProxyDeg;}
static float torsoRefForePlaneDeviationDeg(){torsoRefUpdateMetrics();return torsoRef.frontReady?torsoRef.foreSideTiltDeg:999.0f;}
static float torsoRefUpperPlaneDeviationDeg(){torsoRefUpdateMetrics();return torsoRef.frontReady?torsoRef.upperSideTiltDeg:999.0f;}
static float torsoRefForeFrontComponent(){torsoRefUpdateMetrics();return torsoRef.foreFrontComponent;}
static float torsoRefForeSideComponent(){torsoRefUpdateMetrics();return torsoRef.foreSideComponent;}
static bool torsoRefFrontReady(){return torsoRef.frontReady;}

static bool trNeutralPosePlausible(bool verbose){
  WT901Vec3 ga,gb;
  bool okA=trLocalGravity(TORSO_REF_UPPER_SLOT,ga);
  bool okB=trLocalGravity(TORSO_REF_FORE_SLOT,gb);
  bool ok=okA&&okB;
  bodyFrameCal[0].pairAlignmentDeg=0.0f; // deprecated: cross-sensor local-vector angle is mathematically invalid
  if(verbose){
    if(ok) Serial.printf("TORSO_REF_NEUTRAL_CHECK: A_g=(%.3f,%.3f,%.3f) B_g=(%.3f,%.3f,%.3f) result=PASS; NO_CROSS_SENSOR_LOCAL_VECTOR_COMPARE=1 E_NOT_USED_FOR_FRONT_GATE=1\n",
      ga.x,ga.y,ga.z,gb.x,gb.y,gb.z);
    else Serial.println("TORSO_REF_NEUTRAL_CHECK: missing A/B gravity sample; WAIT.");
  }
  return ok;
}

static bool trCaptureNeutral(){
  TRFrontSnapshot s;if(!trTakeFrontSnapshot(s))return false;
  WT901Vec3 ga,gb,ge;if(!trGravityFromData(s.a,ga)||!trGravityFromData(s.b,gb)||!trGravityFromData(s.e,ge))return false;
  // Never compare ga vs gb directly: they live in different IMU-local coordinate frames.

  torsoRef.upperNeutralGravityLocal=ga;torsoRef.foreNeutralGravityLocal=gb;
  torsoRef.upperLongLocal=ga;torsoRef.foreLongLocal=gb;
  torsoRef.upperNeutralQ=wt901EulerDegToQuat(s.a.roll,s.a.pitch,s.a.yaw);
  torsoRef.foreNeutralQ =wt901EulerDegToQuat(s.b.roll,s.b.pitch,s.b.yaw);
  torsoRef.upperBaselineGravityLocal=ga;torsoRef.upperBaselineReady=true;

  torsoRef.torsoNeutralGravityLocal=ge;
  torsoRef.torsoExerciseBaselineGravityLocal=ge;
  torsoRef.torsoExerciseBaselineGravityReady=true;
  torsoRef.torsoNeutralQ=wt901EulerDegToQuat(s.e.roll,s.e.pitch,s.e.yaw);
  torsoRef.torsoExerciseBaselineQ=torsoRef.torsoNeutralQ;
  torsoRef.torsoExerciseBaselineReady=true;
  torsoRef.torsoTiltFilterReady=true;torsoRef.torsoTiltFilteredDeg=0.0f;torsoRef.torsoTiltDeviationDeg=0.0f;
  torsoRef.torsoTiltLastSampleMs=s.e.sampleMs;

  // E-local down is retained only as a compatibility/diagnostic value.
  torsoRef.downAxisTorso=ge;torsoRef.frontAxisTorso=trVec(0,0,0);torsoRef.sideAxisTorso=trVec(0,0,0);
  torsoRef.tangent1Torso=trVec(0,0,0);torsoRef.tangent2Torso=trVec(0,0,0);
  torsoRef.frontReady=false;torsoRef.neutralReady=true;
  bodyFrameCal[0].pairAlignmentDeg=0.0f; // deprecated invalid cross-sensor metric
  torsoRefUpdateMetrics();

  Serial.printf("TORSO_REF_NEUTRAL_READY V4.4.16: A0_g=(%.4f,%.4f,%.4f) B0_g=(%.4f,%.4f,%.4f) E0_g=(%.4f,%.4f,%.4f) MOUNT_INDEPENDENT_AB=1 FRONT_CAL_E_INDEPENDENT=1\n",
    ga.x,ga.y,ga.z,gb.x,gb.y,gb.z,ge.x,ge.y,ge.z);
  return true;
}

struct TRFrontGeometry {
  float aDelta=999.0f;
  float bDelta=999.0f;
  float deltaDiff=999.0f;
  float torsoTilt=999.0f;
  WT901Vec3 ga={0,0,1};
  WT901Vec3 gb={0,0,1};
  WT901Vec3 ge={0,0,1};
  bool valid=false;
};

static bool trFrontGeometry(const TRFrontSnapshot&s,TRFrontGeometry &g){
  g=TRFrontGeometry();
  if(!torsoRef.neutralReady||!s.valid||s.skewMs>TR_FRONT_MAX_SAMPLE_SKEW_MS)return false;
  if(!trGravityFromData(s.a,g.ga)||!trGravityFromData(s.b,g.gb)||!trGravityFromData(s.e,g.ge))return false;
  g.aDelta=trAngleDeg(g.ga,torsoRef.upperNeutralGravityLocal);
  g.bDelta=trAngleDeg(g.gb,torsoRef.foreNeutralGravityLocal);
  g.deltaDiff=fabsf(g.aDelta-g.bDelta);
  g.torsoTilt=trAngleDeg(g.ge,torsoRef.torsoNeutralGravityLocal);
  g.valid=true;return true;
}

static const char* trFrontPoseReason(const TRFrontSnapshot&s,const TRFrontGeometry&g){
  if(!s.valid||!g.valid)return "SNAPSHOT_INVALID";
  if(s.skewMs>TR_FRONT_MAX_SAMPLE_SKEW_MS)return "SAMPLE_SKEW";
  if(g.aDelta<TR_FRONT_MIN_ELEV_DEG)return "A_NOT_RAISED_ENOUGH";
  if(g.bDelta<TR_FRONT_MIN_ELEV_DEG)return "B_NOT_RAISED_ENOUGH";
  if(g.aDelta>TR_FRONT_MAX_ELEV_DEG)return "A_RAISED_TOO_HIGH";
  if(g.bDelta>TR_FRONT_MAX_ELEV_DEG)return "B_RAISED_TOO_HIGH";
  if(g.deltaDiff>TR_FRONT_AB_DELTA_DIFF_MAX_DEG)return "A_B_RAISE_MISMATCH";
  if(g.torsoTilt>TR_FRONT_TORSO_TILT_MAX_DEG)return "TORSO_TILT";
  return "OK";
}

static void trPrintFrontGeometry(const char *tag,const TRFrontSnapshot&s,const TRFrontGeometry&g,const char*reason){
  Serial.printf("%s: A_delta=%.1f B_delta=%.1f diff=%.1f E_tilt=%.1f skew=%lums reason=%s MOUNT_INDEPENDENT_AB=1 FRONT_CAL_E_INDEPENDENT=1\n",
    tag,g.aDelta,g.bDelta,g.deltaDiff,g.torsoTilt,(unsigned long)s.skewMs,reason);
}

static bool trSaveFrontCandidate(BodyFrameSideCal &c,const TRFrontGeometry&g,const TRFrontSnapshot&s){
  c.frontCandidateAGravityLocal=g.ga;c.frontCandidateBGravityLocal=g.gb;
  c.frontCandidateAQ=wt901EulerDegToQuat(s.a.roll,s.a.pitch,s.a.yaw);
  c.frontCandidateBQ=wt901EulerDegToQuat(s.b.roll,s.b.pitch,s.b.yaw);
  c.frontCandidateReady=true;c.frontCandidateMs=s.maxSampleMs;return true;
}

static bool trCaptureFrontFromCandidate(const WT901Vec3&aFront,const WT901Vec3&bFront,
                                        const WT901Quat&qAFront,const WT901Quat&qBFront){
  WT901Vec3 a=aFront,b=bFront;if(!trNormalize(a)||!trNormalize(b))return false;
  torsoRef.upperFrontQ=qAFront;torsoRef.foreFrontQ=qBFront;

  // Build BODY-FRONT in each sensor's own orientation frame from the fixed limb axis.
  // Constant sensor yaw offsets cancel because neutral/front/current all use the same IMU.
  WT901Vec3 aDownW=trRotateLocalVecByQuat(torsoRef.upperNeutralQ,torsoRef.upperLongLocal);
  WT901Vec3 aFrontW=trRotateLocalVecByQuat(qAFront,torsoRef.upperLongLocal);
  WT901Vec3 bDownW=trRotateLocalVecByQuat(torsoRef.foreNeutralQ,torsoRef.foreLongLocal);
  WT901Vec3 bFrontW=trRotateLocalVecByQuat(qBFront,torsoRef.foreLongLocal);
  if(!trBuildSemanticBasis(aDownW,aFrontW,torsoRef.upperDownAxisWorld,torsoRef.upperFrontAxisWorld,torsoRef.upperSideAxisWorld))return false;
  if(!trBuildSemanticBasis(bDownW,bFrontW,torsoRef.foreDownAxisWorld,torsoRef.foreFrontAxisWorld,torsoRef.foreSideAxisWorld))return false;

  // Compatibility copies only; formal plane uses the WORLD axes above.
  torsoRef.upperDownAxisLocal=torsoRef.upperDownAxisWorld;
  torsoRef.upperFrontAxisLocal=torsoRef.upperFrontAxisWorld;
  torsoRef.upperSideAxisLocal=torsoRef.upperSideAxisWorld;
  torsoRef.foreDownAxisLocal=torsoRef.foreDownAxisWorld;
  torsoRef.foreFrontAxisLocal=torsoRef.foreFrontAxisWorld;
  torsoRef.foreSideAxisLocal=torsoRef.foreSideAxisWorld;
  torsoRef.upperFrontGravityLocal=a;torsoRef.foreFrontGravityLocal=b;
  torsoRef.upperLocalSemanticReady=trBuildSemanticBasis(torsoRef.upperNeutralGravityLocal,a,
    torsoRef.upperSemanticDownLocal,torsoRef.upperSemanticFrontLocal,torsoRef.upperSemanticSideLocal);
  torsoRef.foreLocalSemanticReady=trBuildSemanticBasis(torsoRef.foreNeutralGravityLocal,b,
    torsoRef.foreSemanticDownLocal,torsoRef.foreSemanticFrontLocal,torsoRef.foreSemanticSideLocal);
  if(!torsoRef.upperLocalSemanticReady||!torsoRef.foreLocalSemanticReady)return false;
  torsoRef.frontReady=true;

  // V4.4.24: anchor BODY-FRONT to the waist/abdomen E yaw at the exact
  // accepted front-calibration pose. Only the yaw DELTA is used later, so
  // constant mounting / heading offsets cancel.
  const WT901EulerData &eFrontRef=wt901Slots[TORSO_REF_SLOT].latest;
  torsoRef.torsoFrontYawReferenceReady=eFrontRef.valid && eFrontRef.sampleMs!=0;
  torsoRef.torsoFrontYawReferenceDeg=torsoRef.torsoFrontYawReferenceReady?eFrontRef.yaw:0.0f;
  torsoRef.torsoYawFromFrontDeg=0.0f;

  // Initialize vertical-yaw trackers at the exact accepted BODY-FRONT pose.
  // V4.4.30 formal plane uses B-E physical Z rotation; A-E remains diagnostic.
  wt901FusedVerticalYawReset(TORSO_REF_UPPER_SLOT);
  wt901FusedVerticalYawReset(TORSO_REF_FORE_SLOT);
  wt901FusedVerticalYawReset(TORSO_REF_SLOT);
  torsoRef.upperarmRelativeBodyYawDeg = 0.0f;
  torsoRef.forearmRelativeBodyYawDeg = 0.0f;
  trResetBodyRelativeAzimuthFromLatest();
  torsoRef.frontAxisTorso=trVec(0,0,0);torsoRef.sideAxisTorso=trVec(0,0,0);
  torsoRef.tangent1Torso=trVec(0,0,0);torsoRef.tangent2Torso=trVec(0,0,0);
  torsoRefUpdateMetrics();
  Serial.printf("TORSO_REF_FRONT_READY V4.4.30: A_delta=%.1f B_delta=%.1f; E_yaw_ref=%.1f ready=%d; ARM_PLANE=AB_DIFFERENTIAL_MOTION_AXIS_ROTATED_BY_AE_BE_VERTICAL_GYRO_PLUS_A_SIDE_PLUS_K11_HINGE; ABSOLUTE_B_QUAT_AND_E_EULER_YAW_DIAGNOSTIC_ONLY; BODY_AZ_DIAGNOSTIC_ONLY E_USED_FOR_PLANE=1 E_USED_FOR_ELBOW=0\n",
    trAngleDeg(a,torsoRef.upperNeutralGravityLocal),trAngleDeg(b,torsoRef.foreNeutralGravityLocal),
    torsoRef.torsoFrontYawReferenceDeg,torsoRef.torsoFrontYawReferenceReady?1:0);
  return true;
}

static void bodyFrameStart(int sideIndex,int fixedSlot,int movingSlot){
  (void)sideIndex;(void)fixedSlot;(void)movingSlot;
  bodyFrameActiveSide=0;bodyFrameCal[0]=BodyFrameSideCal();torsoRef=TorsoReferenceState();
  bodyFrameCal[0].phase=BF_DOWN_STILL;
  Serial.println("TORSO_REF_START V4.4.16: step1 neutral. Arm naturally DOWN + straight. Hold comfortably; E allows breathing-scale motion. A/B arm calibration is independent of E yaw.");
}
static bool bodyFrameReady(int sideIndex){(void)sideIndex;return bodyFrameCal[0].phase==BF_READY&&torsoRef.neutralReady&&torsoRef.frontReady;}
static BodyFrameCalPhase bodyFramePhase(){return bodyFrameCal[0].phase;}
static const char* bodyFramePhaseName(){
  switch(bodyFramePhase()){
    case BF_DOWN_STILL:return "TORSO_NEUTRAL";
    case BF_FRONT_RAISES:return bodyFrameCal[0].frontArmed?"BODY_FRONT_ARMED":"BODY_FRONT_ARMING";
    case BF_READY:return "READY";case BF_FAILED:return "FAILED";default:return "OFF";
  }
}
static int bodyFrameCompletedCycles(){return bodyFramePhase()==BF_FRONT_RAISES?1:(bodyFramePhase()==BF_READY?2:0);}
static float bodyFrameCurrentLiftDeg(){
  if(!torsoRef.neutralReady)return 0.0f;WT901Vec3 ga;if(!trLocalGravity(TORSO_REF_UPPER_SLOT,ga))return 0.0f;
  return trAngleDeg(ga,torsoRef.upperNeutralGravityLocal);
}
static float bodyFrameDownStillProgress(){
  BodyFrameSideCal &c=bodyFrameCal[0];
  if(c.phase==BF_FRONT_RAISES&&!c.frontArmed){
    if(c.frontNeutralConfirmStartMs==0)return 0.0f;
    float p=(float)(millis()-c.frontNeutralConfirmStartMs)/(float)TR_FRONT_ARM_NEUTRAL_CONFIRM_MS;return p<0?0:(p>1?1:p);
  }
  if(c.stillStartMs==0)return 0.0f;
  unsigned long hold=(c.phase==BF_FRONT_RAISES)?TR_FRONT_HOLD_MS:TR_NEUTRAL_HOLD_MS;
  float p=(float)(millis()-c.stillStartMs)/(float)hold;return p<0?0:(p>1?1:p);
}
static const char* bodyFrameFailReason(){return bodyFrameCal[0].failReason;}

static void bodyFrameUpdate(){
  BodyFrameSideCal &c=bodyFrameCal[0];unsigned long now=millis();

  if(c.phase==BF_DOWN_STILL){
    bool still=trSlotFreshStill(TORSO_REF_UPPER_SLOT)&&trSlotFreshStill(TORSO_REF_FORE_SLOT)&&trSlotFreshStill(TORSO_REF_SLOT);
    bool pose=trNeutralPosePlausible(false);
    if(!pose){
      c.stillStartMs=0;c.unstableStartMs=0;
      if(now-c.lastDiagMs>=TR_DIAG_MS){c.lastDiagMs=now;trNeutralPosePlausible(true);trPrintStillDiag("TORSO_REF_NEUTRAL_POSE_BLOCKED",still);}return;
    }
    if(!still){
      bool gross=trCombinedGrossMotion();
      if(c.stillStartMs!=0&&!gross){if(c.unstableStartMs==0)c.unstableStartMs=now;if(now-c.unstableStartMs<=TR_STILL_GLITCH_GRACE_MS)return;}
      c.stillStartMs=0;c.unstableStartMs=0;
      if(now-c.lastDiagMs>=TR_DIAG_MS){c.lastDiagMs=now;trNeutralPosePlausible(true);trPrintStillDiag("TORSO_REF_NEUTRAL_BLOCKED",false);}return;
    }
    c.unstableStartMs=0;
    if(c.stillStartMs==0){c.stillStartMs=now;trNeutralPosePlausible(true);trPrintStillDiag("TORSO_REF_NEUTRAL_HOLD",true);Serial.println("TORSO_REF_NEUTRAL_HOLD: valid; keep the arm relaxed; normal breathing is allowed.");}
    if(now-c.stillStartMs<TR_NEUTRAL_HOLD_MS)return;
    if(!trCaptureNeutral()){c.phase=BF_FAILED;c.failReason="neutral_capture_failed";Serial.println("TORSO_REF_FAILED: neutral capture failed; retry.");return;}
    c.phase=BF_FRONT_RAISES;c.stillStartMs=0;c.unstableStartMs=0;c.poseUnstableStartMs=0;c.lastDiagMs=0;
    // V4.4.18: BF_DOWN_STILL has already completed a stable neutral hold and captured neutral.
    // Arm FRONT immediately here so the UI can truthfully tell the user to raise as soon as the front page appears.
    // Stage 2B still requires a real A+B departure and the 40-100 deg front-pose hold, so this does not bypass front calibration.
    c.frontArmed=true;c.frontRaiseSeen=false;c.frontNeutralConfirmStartMs=0;c.frontCandidateReady=false;
    Serial.println("TORSO_REF_FRONT_ARMED_IMMEDIATE V4.4.18: neutral/down was already stably captured. NOW raise the whole arm toward true BODY FRONT about 40-100deg and hold briefly.");
    return;
  }

  if(c.phase==BF_FRONT_RAISES){
    TRFrontSnapshot s;bool snapOk=trTakeFrontSnapshot(s);TRFrontGeometry g;bool geomOk=snapOk&&trFrontGeometry(s,g);
    bool still=snapOk&&trFrontSnapshotStill(s);

    // Stage 2A: prove we really started from the just-captured neutral/down pose.
    if(!c.frontArmed){
      bool neutralLike=geomOk && g.aDelta<=TR_FRONT_ARMED_MAX_DELTA_DEG && g.bDelta<=TR_FRONT_ARMED_MAX_DELTA_DEG &&
                       g.torsoTilt<=TR_FRONT_TORSO_TILT_MAX_DEG && still;
      if(neutralLike){
        if(c.frontNeutralConfirmStartMs==0)c.frontNeutralConfirmStartMs=now;
        if(now-c.frontNeutralConfirmStartMs>=TR_FRONT_ARM_NEUTRAL_CONFIRM_MS){
          c.frontArmed=true;c.frontRaiseSeen=false;c.frontNeutralConfirmStartMs=0;c.lastDiagMs=0;
          Serial.println("TORSO_REF_FRONT_ARMED: neutral/down confirmed. NOW raise the whole arm toward true BODY FRONT about 40-100deg. Keep the elbow roughly straight; software will NOT compare A-local XYZ directly with B-local XYZ. Keeping the arm down can NEVER pass this stage.");
        }
      }else{
        c.frontNeutralConfirmStartMs=0;
        if(now-c.lastDiagMs>=TR_DIAG_MS){
          c.lastDiagMs=now;
          if(geomOk)trPrintFrontGeometry("TORSO_REF_FRONT_ARMING_WAIT",s,g,"WAIT_FOR_STABLE_NEUTRAL");
          else Serial.println("TORSO_REF_FRONT_ARMING_WAIT: snapshot invalid/stale.");
        }
      }
      return;
    }

    // Stage 2B: require a real A+B departure from each sensor's own neutral gravity.
    if(!c.frontRaiseSeen){
      if(geomOk && g.aDelta>=TR_FRONT_RAISE_DETECT_MIN_DEG && g.bDelta>=TR_FRONT_RAISE_DETECT_MIN_DEG){
        c.frontRaiseSeen=true;c.lastDiagMs=0;
        Serial.printf("TORSO_REF_FRONT_RAISE_DETECTED: real A/B motion seen A_delta=%.1f B_delta=%.1f. Now reach about 40-100deg toward BODY FRONT and hold; A/B agreement is judged only by each sensor's own neutral excursion.\n",g.aDelta,g.bDelta);
      }else{
        if(now-c.lastDiagMs>=TR_DIAG_MS){
          c.lastDiagMs=now;
          if(geomOk)trPrintFrontGeometry("TORSO_REF_FRONT_WAIT_FOR_REAL_RAISE",s,g,"A_AND_B_DELTA_MUST_BOTH_REACH_30DEG");
          else Serial.println("TORSO_REF_FRONT_WAIT_FOR_REAL_RAISE: snapshot invalid/stale.");
        }
        return;
      }
    }

    const char *reason=(geomOk?trFrontPoseReason(s,g):"SNAPSHOT_INVALID");
    bool pose=geomOk&&strcmp(reason,"OK")==0;

    if(!pose){
      bool canGrace=(c.stillStartMs!=0&&snapOk&&!trFrontSnapshotGrossMotion(s));
      if(canGrace){
        if(c.poseUnstableStartMs==0)c.poseUnstableStartMs=now;
        if(now-c.poseUnstableStartMs<=TR_FRONT_POSE_GLITCH_GRACE_MS){
          if(now-c.lastDiagMs>=TR_DIAG_MS){c.lastDiagMs=now;trPrintFrontGeometry("TORSO_REF_FRONT_POSE_GLITCH_TOLERATED",s,g,reason);}return;
        }
      }
      c.stillStartMs=0;c.unstableStartMs=0;c.poseUnstableStartMs=0;c.frontCandidateReady=false;
      if(now-c.lastDiagMs>=TR_DIAG_MS){
        c.lastDiagMs=now;
        if(geomOk)trPrintFrontGeometry("TORSO_REF_FRONT_WAIT",s,g,reason);else Serial.println("TORSO_REF_FRONT_WAIT: snapshot invalid/stale.");
      }
      return;
    }

    c.poseUnstableStartMs=0;trSaveFrontCandidate(c,g,s);
    if(!still){
      bool gross=trFrontSnapshotGrossMotion(s);
      if(c.stillStartMs!=0&&!gross){if(c.unstableStartMs==0)c.unstableStartMs=now;if(now-c.unstableStartMs<=TR_STILL_GLITCH_GRACE_MS)return;}
      c.stillStartMs=0;c.unstableStartMs=0;c.frontCandidateReady=false;
      if(now-c.lastDiagMs>=TR_DIAG_MS){c.lastDiagMs=now;trPrintFrontSnapshotDiag("TORSO_REF_FRONT_STILL_BLOCKED",s,false);}return;
    }

    c.unstableStartMs=0;
    if(c.stillStartMs==0){
      c.stillStartMs=now;trPrintFrontGeometry("TORSO_REF_FRONT_HOLD",s,g,"OK");
      Serial.printf("TORSO_REF_FRONT_HOLD: valid A/B-only arm pose; hold %.2fs. E is checked only for torso tilt/stillness.\n",(float)TR_FRONT_HOLD_MS/1000.0f);
    }
    if(now-c.stillStartMs<TR_FRONT_HOLD_MS)return;

    if(!c.frontCandidateReady||!trCaptureFrontFromCandidate(c.frontCandidateAGravityLocal,c.frontCandidateBGravityLocal,
        c.frontCandidateAQ,c.frontCandidateBQ)){
      c.phase=BF_FAILED;c.failReason="front_capture_failed";Serial.println("TORSO_REF_FAILED: A/B local front-plane capture failed; retry calibration.");return;
    }
    c.phase=BF_READY;c.stillStartMs=0;c.poseUnstableStartMs=0;
    Serial.println("TORSO_REF_CAL_DONE V4.4.29: BODY-FRONT ready; formal P uses B forearm body-plane rotated only by deliberate E-gyro body turn + A-side + K11 hinge; E Euler yaw/BODY_AZ diagnostic only; K11 semantic-frame elbow ROM next.");
  }
}

static void bodyFrameStartBoth(){bodyFrameStart(0,0,1);}
static void bodyFrameUpdateBoth(){bodyFrameUpdate();}
static bool bodyFrameBothReady(){return bodyFrameReady(0);}

static bool bodyFrameSemanticVector(int sideIndex,int segmentIndex,BFSemanticVec &out){
  (void)sideIndex;if(!torsoRef.frontReady)return false;
  if(segmentIndex==0)
    return trSemanticSegmentWorld(TORSO_REF_UPPER_SLOT,torsoRef.upperLongLocal,
      torsoRef.upperDownAxisWorld,torsoRef.upperFrontAxisWorld,torsoRef.upperSideAxisWorld,out);
  return trSemanticSegmentWorld(TORSO_REF_FORE_SLOT,torsoRef.foreLongLocal,
    torsoRef.foreDownAxisWorld,torsoRef.foreFrontAxisWorld,torsoRef.foreSideAxisWorld,out);
}
static float bodyFrameElevationFromDownDeg(const BFSemanticVec &v){if(!v.valid)return 999.0f;return acosf(trClamp1(v.down))*57.2957795131f;}
static float bodyFrameSemanticAngleDeg(const BFSemanticVec&a,const BFSemanticVec&b){if(!a.valid||!b.valid)return 999.0f;return acosf(trClamp1(a.front*b.front+a.side*b.side+a.down*b.down))*57.2957795131f;}
static float bodyFrameAzimuthDeg(const BFSemanticVec &v){if(!v.valid)return 999.0f;return atan2f(v.side,v.front)*57.2957795131f;}
static float bodyFrameSagittalPlaneDeviationDeg(const BFSemanticVec &v){return trPlaneDeviationFromSemantic(v);}
