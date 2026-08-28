#pragma once

#include <Arduino.h>
#include <math.h>

// K11-derived gyro-differential elbow core for RehabMotion V4.4.16.
// A and B are independent IMUs: their local XYZ axes are never assumed to match.
// BODY-FRONT calibration already builds FRONT/SIDE/DOWN semantic axes separately
// for A and B. Each local gyro sample is projected into that sensor's own semantic
// basis first; A/B differential math happens only in the anatomical semantic frame.
// Formal elbow ROM uses the personalized hinge axis learned by the one slow K11
// movement, expressed in the calibrated FRONT/SIDE/DOWN semantic frame. This avoids
// falsely rejecting a valid elbow motion merely because the user's arm is not held
// in a textbook body-side orientation. Normal training is continuous with no per-rep
// re-calibration or stillness gate.

enum K11GyroPhase : uint8_t {
  K11_IDLE = 0,
  K11_BIAS_STILL,
  K11_AXIS_FLEX,
  K11_AXIS_RETURN,
  K11_READY,
  K11_FAILED
};

struct K11SensorIntegrator {
  uint32_t lastPacketCount = 0;
  unsigned long lastSampleMs = 0;
  float bias[3] = {0,0,0};
  float biasSum[3] = {0,0,0};
  uint16_t biasSamples = 0;
  float angle[3] = {0,0,0};
  float correctedRate[3] = {0,0,0};
  float prevRate[3] = {0,0,0};
  bool prevRateReady = false;
};

struct K11GyroElbowState {
  K11GyroPhase phase = K11_IDLE;
  bool started = false;
  bool paused = false;

  K11SensorIntegrator s[2]; // 0=A upper arm, 1=B forearm

  unsigned long phaseStartMs = 0;
  unsigned long stillHoldStartMs = 0;
  unsigned long flexHoldStartMs = 0;
  unsigned long returnHoldStartMs = 0;
  unsigned long zeroSnapHoldStartMs = 0;

  float relVec[3] = {0,0,0};
  float peakVec[3] = {0,0,0};
  float peakMagDeg = 0.0f;
  float axis[3] = {0,1,0}; // semantic formal axis after calibration
  float learnedAxisSemantic[3] = {0,1,0};
  float axisDominance = 0.0f; // |learned semantic SIDE component|

  float rawProjectedDeg = 0.0f;
  float formalZeroOffsetDeg = 0.0f;
  // V4.4.7: vector zero lets us measure rotation components perpendicular to the
  // learned hinge axis. This is an independent out-of-plane safeguard.
  float formalZeroRelVec[3] = {0,0,0};
  float hingeOffAxisRotationDeg = 0.0f;
  float hingeAxisDeviationDeg = 0.0f;
  float elbowDeg = 0.0f;
  float elbowSpeedDegS = 0.0f;
  float calAngleDeg = 0.0f;

  uint32_t processedA = 0;
  uint32_t processedB = 0;
  uint32_t rejectedDtA = 0;
  uint32_t rejectedDtB = 0;
  uint32_t bridgedGapA = 0;
  uint32_t bridgedGapB = 0;
  uint32_t biasResets = 0;
};

static K11GyroElbowState k11Gyro;

static int k11FixedSlot = 0;
static int k11MovingSlot = 1;

static int k11GlobalSlot(int localIndex) {
  return localIndex == 0 ? k11FixedSlot : k11MovingSlot;
}

static void wt901K11SelectPair(int fixedSlot, int movingSlot) {
  if (fixedSlot < 0 || fixedSlot >= WT901_ABCD_COUNT || movingSlot < 0 || movingSlot >= WT901_ABCD_COUNT || fixedSlot == movingSlot) return;
  k11FixedSlot = fixedSlot;
  k11MovingSlot = movingSlot;
  K11GyroElbowState fresh;
  k11Gyro = fresh;
  Serial.printf("K11_PAIR_SELECTED: proximal=IMU_%s(%d) distal=IMU_%s(%d)\n",
    wt901Slots[k11FixedSlot].role, k11FixedSlot, wt901Slots[k11MovingSlot].role, k11MovingSlot);
}

static int wt901K11FixedSlot() { return k11FixedSlot; }
static int wt901K11MovingSlot() { return k11MovingSlot; }

static constexpr float K11_BIAS_MAX_GYRO_DEG_S = 12.0f;
static constexpr unsigned long K11_BIAS_HOLD_MS = 1400UL;
static constexpr uint16_t K11_BIAS_MIN_SAMPLES = 9;
static constexpr float K11_GYRO_DEADZONE_DEG_S = 0.8f;
// BLE notifications in the real log are usually around 300 ms apart but can jitter
// above 350 ms. K11.2.1 discarded the whole interval in that case, which lost
// return-motion area and caused the elbow integral to stick near ~79 deg after rep 1.
// K11.2.2 bridges normal jitter with trapezoidal integration and rejects only a
// genuine long outage.
static constexpr unsigned long K11_SOFT_GAP_MS = 350UL;
static constexpr unsigned long K11_HARD_GAP_MS = 1200UL;
static constexpr float K11_CAL_FLEX_MIN_DEG = 55.0f;
// V4.4.17 calibration-flow fix: a real elbow hinge axis can contain a substantial
// FRONT component when the upper arm is rotated relative to the torso. Reject only
// an implausibly vertical (DOWN-dominated) learned axis; do not require SIDE>=0.65.
static constexpr float K11_CAL_MAX_SEMANTIC_DOWN_ALIGNMENT = 0.45f;
static constexpr unsigned long K11_CAL_FLEX_HOLD_MS = 220UL;
static constexpr float K11_CAL_RETURN_DEG = 18.0f;
// Calibration completion is judged ONLY on the same visible extension-excursion coordinate
// shown on screen. Do not gate readiness on hidden per-sensor gyro-rate thresholds.
static constexpr unsigned long K11_CAL_RETURN_HOLD_MS = 1500UL;
static constexpr unsigned long K11_CAL_TIMEOUT_MS = 45000UL;
static constexpr float K11_FORMAL_MAX_DEG = 160.0f;
static constexpr float K11_ZERO_SNAP_ANGLE_DEG = 10.0f;
static constexpr float K11_ZERO_SNAP_SPEED_DEG_S = 5.0f;
static constexpr unsigned long K11_ZERO_SNAP_HOLD_MS = 650UL;

static float k11AbsMax3(float x, float y, float z) {
  float m = fabsf(x);
  if (fabsf(y) > m) m = fabsf(y);
  if (fabsf(z) > m) m = fabsf(z);
  return m;
}

static float k11Norm3(const float v[3]) {
  return sqrtf(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
}

static float k11Dot3(const float a[3], const float b[3]) {
  return a[0]*b[0] + a[1]*b[1] + a[2]*b[2];
}

static void k11Normalize3(float v[3]) {
  float n = k11Norm3(v);
  if (n < 1e-5f) { v[0]=1.0f; v[1]=0.0f; v[2]=0.0f; return; }
  v[0] /= n; v[1] /= n; v[2] /= n;
}

static bool k11SemanticBasisReady() {
  return torsoRef.upperLocalSemanticReady && torsoRef.foreLocalSemanticReady &&
         k11FixedSlot == TORSO_REF_UPPER_SLOT && k11MovingSlot == TORSO_REF_FORE_SLOT;
}

// local sensor XYZ -> that sensor's calibrated anatomical FRONT/SIDE/DOWN frame.
static void k11ProjectLocalRateToSemantic(int idx,const float local[3],float out[3]) {
  if (!k11SemanticBasisReady()) {
    out[0]=local[0]; out[1]=local[1]; out[2]=local[2];
    return;
  }
  const WT901Vec3 &front = (idx==0) ? torsoRef.upperSemanticFrontLocal : torsoRef.foreSemanticFrontLocal;
  const WT901Vec3 &side  = (idx==0) ? torsoRef.upperSemanticSideLocal  : torsoRef.foreSemanticSideLocal;
  const WT901Vec3 &down  = (idx==0) ? torsoRef.upperSemanticDownLocal  : torsoRef.foreSemanticDownLocal;
  out[0] = local[0]*front.x + local[1]*front.y + local[2]*front.z;
  out[1] = local[0]*side.x  + local[1]*side.y  + local[2]*side.z;
  out[2] = local[0]*down.x  + local[1]*down.y  + local[2]*down.z;
}

static void wt901K11ResetBiasAccum() {
  for (int i=0;i<2;i++) {
    k11Gyro.s[i].biasSum[0]=k11Gyro.s[i].biasSum[1]=k11Gyro.s[i].biasSum[2]=0.0f;
    k11Gyro.s[i].biasSamples=0;
  }
  k11Gyro.stillHoldStartMs = millis();
  k11Gyro.biasResets++;
}

static void wt901K11ResetIntegrators() {
  for (int i=0;i<2;i++) {
    k11Gyro.s[i].angle[0]=k11Gyro.s[i].angle[1]=k11Gyro.s[i].angle[2]=0.0f;
    k11Gyro.s[i].correctedRate[0]=k11Gyro.s[i].correctedRate[1]=k11Gyro.s[i].correctedRate[2]=0.0f;
    k11Gyro.s[i].prevRate[0]=k11Gyro.s[i].prevRate[1]=k11Gyro.s[i].prevRate[2]=0.0f;
    k11Gyro.s[i].prevRateReady = true; // reset occurs while physically still/near zero
    k11Gyro.s[i].lastSampleMs = wt901Slots[k11GlobalSlot(i)].latest.sampleMs;
    k11Gyro.s[i].lastPacketCount = wt901Slots[k11GlobalSlot(i)].packetCount;
    wt901ClearSampleQueue(k11GlobalSlot(i));
  }
  k11Gyro.relVec[0]=k11Gyro.relVec[1]=k11Gyro.relVec[2]=0.0f;
  k11Gyro.rawProjectedDeg = 0.0f;
  k11Gyro.formalZeroOffsetDeg = 0.0f;
  k11Gyro.formalZeroRelVec[0]=k11Gyro.formalZeroRelVec[1]=k11Gyro.formalZeroRelVec[2]=0.0f;
  k11Gyro.hingeOffAxisRotationDeg = 0.0f;
  k11Gyro.hingeAxisDeviationDeg = 0.0f;
  k11Gyro.elbowDeg = 0.0f;
  k11Gyro.elbowSpeedDegS = 0.0f;
  k11Gyro.calAngleDeg = 0.0f;
}

static void wt901K11Start() {
  K11GyroElbowState fresh;
  k11Gyro = fresh;
  k11Gyro.started = true;
  k11Gyro.phase = K11_BIAS_STILL;
  k11Gyro.phaseStartMs = millis();
  k11Gyro.stillHoldStartMs = millis();
  for (int i=0;i<2;i++) {
    wt901ClearSampleQueue(k11GlobalSlot(i));
    k11Gyro.s[i].lastPacketCount = wt901Slots[k11GlobalSlot(i)].packetCount;
    k11Gyro.s[i].lastSampleMs = wt901Slots[k11GlobalSlot(i)].latest.sampleMs;
  }
  Serial.println("K11_START: hold the selected exercise start pose STILL for gyro bias calibration.");
}

static void wt901K11Fail(const char *reason) {
  k11Gyro.phase = K11_FAILED;
  k11Gyro.phaseStartMs = millis();
  Serial.print("K11_FAIL: ");
  Serial.println(reason);
}

static bool wt901K11BothReceiving() {
  return wt901Slots[k11FixedSlot].latest.valid && wt901Slots[k11MovingSlot].latest.valid && wt901Slots[k11FixedSlot].packetCount>0 && wt901Slots[k11MovingSlot].packetCount>0;
}

static void wt901K11ProcessBiasSample(int idx, const WT901EulerData &d) {
  float g[3] = {d.gx,d.gy,d.gz};
  if (k11AbsMax3(g[0],g[1],g[2]) > K11_BIAS_MAX_GYRO_DEG_S) {
    wt901K11ResetBiasAccum();
    return;
  }
  K11SensorIntegrator &s = k11Gyro.s[idx];
  s.biasSum[0] += g[0]; s.biasSum[1] += g[1]; s.biasSum[2] += g[2];
  s.biasSamples++;
}

static void wt901K11ProcessIntegratedSample(int idx, const WT901EulerData &d) {
  K11SensorIntegrator &s = k11Gyro.s[idx];
  unsigned long t = d.sampleMs;
  if (s.lastSampleMs == 0) { s.lastSampleMs=t; return; }
  unsigned long dtMs = t - s.lastSampleMs;
  s.lastSampleMs = t;

  float raw[3] = {d.gx,d.gy,d.gz};
  float localRate[3];
  for (int a=0;a<3;a++) localRate[a] = raw[a] - s.bias[a];

  float curRate[3];
  k11ProjectLocalRateToSemantic(idx, localRate, curRate);
  for (int a=0;a<3;a++) {
    if (fabsf(curRate[a]) < K11_GYRO_DEADZONE_DEG_S) curRate[a] = 0.0f;
  }

  if (dtMs == 0) {
    for (int a=0;a<3;a++) {
      s.correctedRate[a] = curRate[a];
      s.prevRate[a] = curRate[a];
    }
    s.prevRateReady = true;
    return;
  }

  // Only discard a genuine BLE outage. When this happens, restart integration from
  // the current rate so the next normal packet does not bridge an unknown long gap.
  if (dtMs > K11_HARD_GAP_MS) {
    if (idx==0) k11Gyro.rejectedDtA++; else k11Gyro.rejectedDtB++;
    for (int a=0;a<3;a++) {
      s.correctedRate[a] = curRate[a];
      s.prevRate[a] = curRate[a];
    }
    s.prevRateReady = true;
    return;
  }

  // Diagnostic only: this interval would have been completely discarded by K11.2.1.
  if (dtMs > K11_SOFT_GAP_MS) {
    if (idx==0) k11Gyro.bridgedGapA++; else k11Gyro.bridgedGapB++;
  }

  float dt = dtMs * 0.001f;
  for (int a=0;a<3;a++) {
    float integrateRate = curRate[a];
    if (s.prevRateReady) integrateRate = 0.5f * (s.prevRate[a] + curRate[a]);
    if (!k11Gyro.paused) s.angle[a] += integrateRate * dt;
    s.correctedRate[a] = curRate[a];
    s.prevRate[a] = curRate[a];
  }
  s.prevRateReady = true;
}

static void wt901K11RefreshDerived() {
  for (int a=0;a<3;a++) k11Gyro.relVec[a] = k11Gyro.s[1].angle[a] - k11Gyro.s[0].angle[a];
  float mag = k11Norm3(k11Gyro.relVec);

  if (k11Gyro.phase == K11_AXIS_FLEX || k11Gyro.phase == K11_AXIS_RETURN) {
    if (mag > k11Gyro.peakMagDeg) {
      k11Gyro.peakMagDeg = mag;
      for (int a=0;a<3;a++) k11Gyro.peakVec[a] = k11Gyro.relVec[a];
      if (mag > 1.0f) {
        for (int a=0;a<3;a++) k11Gyro.axis[a] = k11Gyro.peakVec[a];
        k11Normalize3(k11Gyro.axis);
      }
    }
    if (k11Gyro.peakMagDeg > 1.0f) {
      k11Gyro.calAngleDeg = fabsf(k11Dot3(k11Gyro.relVec, k11Gyro.axis));
    } else {
      k11Gyro.calAngleDeg = mag;
    }
  }

  if (k11Gyro.phase == K11_READY) {
    k11Gyro.rawProjectedDeg = k11Dot3(k11Gyro.relVec, k11Gyro.axis);

    // V4.4.26: start-pose-guarded zero-floor correction.
    // A repetition can be finalized before the last few degrees of return motion have
    // physically finished.  If wt901K11RezeroFormal() runs at that moment, the residual
    // return tail integrates below zero.  V4.4.25 real data then showed hidden values
    // such as -31.8 deg while the UI displayed 0 deg; the next real 90-100 deg curl had
    // to climb out of that negative hole first and was reported as only 45-60 deg.
    //
    // Correct the zero floor ONLY when the independent gravity references confirm that
    // both A (upper arm) and B (forearm) are genuinely back near the natural-down start
    // pose.  This prevents a large negative integration excursion during an abnormal
    // raised/compensated pose (the same log contained -76 deg while A was ~32 deg up)
    // from being mistaken for a new zero.  This is drift/return-tail correction, not
    // recalibration: the learned K11 hinge axis is never changed.
    const bool k11AtGravityStartPose =
        torsoRefUpperNeutralElevationDeg() <= 25.0f &&
        torsoRefForeNeutralElevationDeg()  <= 30.0f;
    if (k11AtGravityStartPose && k11Gyro.rawProjectedDeg < k11Gyro.formalZeroOffsetDeg) {
      k11Gyro.formalZeroOffsetDeg = k11Gyro.rawProjectedDeg;
      for (int a=0; a<3; ++a) k11Gyro.formalZeroRelVec[a] = k11Gyro.relVec[a];
      k11Gyro.hingeOffAxisRotationDeg = 0.0f;
      k11Gyro.hingeAxisDeviationDeg = 0.0f;
    }

    float projected = k11Gyro.rawProjectedDeg - k11Gyro.formalZeroOffsetDeg;

    float dRel[3] = {
      k11Gyro.relVec[0]-k11Gyro.formalZeroRelVec[0],
      k11Gyro.relVec[1]-k11Gyro.formalZeroRelVec[1],
      k11Gyro.relVec[2]-k11Gyro.formalZeroRelVec[2]
    };
    float axial = k11Dot3(dRel,k11Gyro.axis);
    float total2 = k11Dot3(dRel,dRel);
    float perp2 = total2 - axial*axial; if(perp2 < 0.0f) perp2 = 0.0f;
    k11Gyro.hingeOffAxisRotationDeg = sqrtf(perp2);
    float total = sqrtf(total2);
    if(total >= 12.0f)
      k11Gyro.hingeAxisDeviationDeg = atan2f(k11Gyro.hingeOffAxisRotationDeg,fabsf(axial))*57.2957795131f;
    else
      k11Gyro.hingeAxisDeviationDeg = 0.0f;

    float rateA = k11Gyro.s[0].correctedRate[0]*k11Gyro.axis[0] + k11Gyro.s[0].correctedRate[1]*k11Gyro.axis[1] + k11Gyro.s[0].correctedRate[2]*k11Gyro.axis[2];
    float rateB = k11Gyro.s[1].correctedRate[0]*k11Gyro.axis[0] + k11Gyro.s[1].correctedRate[1]*k11Gyro.axis[1] + k11Gyro.s[1].correctedRate[2]*k11Gyro.axis[2];
    k11Gyro.elbowSpeedDegS = rateB - rateA;
    if (projected < 0.0f) projected = 0.0f;
    if (projected > K11_FORMAL_MAX_DEG) projected = K11_FORMAL_MAX_DEG;
    k11Gyro.elbowDeg = projected;
  }
}

static void wt901K11Update() {
  if (!k11Gyro.started) return;

  // K12.1.1: during group rest, manual pause, or after the final result is
  // frozen, keep draining BLE FIFO so it cannot overflow, but DO NOT integrate
  // those samples into the elbow angle. Resume starts from the then-current
  // timestamp via wt901K11SetPaused(false), so rest-period motion is ignored.
  if (k11Gyro.phase == K11_READY && k11Gyro.paused) {
    for (int idx=0; idx<2; idx++) {
      WT901EulerData discard;
      while (wt901PopSample(k11GlobalSlot(idx), discard)) {
        if (discard.valid) k11Gyro.s[idx].lastSampleMs = discard.sampleMs;
      }
      k11Gyro.s[idx].lastPacketCount = wt901Slots[k11GlobalSlot(idx)].packetCount;
      k11Gyro.s[idx].correctedRate[0]=k11Gyro.s[idx].correctedRate[1]=k11Gyro.s[idx].correctedRate[2]=0.0f;
      k11Gyro.s[idx].prevRate[0]=k11Gyro.s[idx].prevRate[1]=k11Gyro.s[idx].prevRate[2]=0.0f;
      k11Gyro.s[idx].prevRateReady = false;
    }
    k11Gyro.elbowSpeedDegS = 0.0f;
    return;
  }

  // K12.1: drain every queued BLE sample. Previously only WT901Slot::latest was
  // consumed once per loop, so several gyro packets could be overwritten while
  // Serial/TFT/SD blocked the main loop. That lost angular area and could leave
  // the displayed relative excursion stuck far from the calibrated start pose.
  bool anyNew = false;
  // Drain in A/B rounds rather than draining all A first and all B second. This
  // keeps the learned relative-motion trajectory close to the real chronology
  // when several samples accumulated during a blocking UI/Serial operation.
  for (;;) {
    bool roundAny = false;
    for (int idx=0; idx<2; idx++) {
      WT901EulerData d;
      if (!wt901PopSample(k11GlobalSlot(idx), d)) continue;
      if (!d.valid) continue;
      roundAny = true;
      anyNew = true;
      if (idx==0) k11Gyro.processedA++; else k11Gyro.processedB++;

      if (k11Gyro.phase == K11_BIAS_STILL) wt901K11ProcessBiasSample(idx, d);
      else if (k11Gyro.phase == K11_AXIS_FLEX || k11Gyro.phase == K11_AXIS_RETURN || k11Gyro.phase == K11_READY) wt901K11ProcessIntegratedSample(idx, d);
    }
    if (!roundAny) break;
    if (k11Gyro.phase != K11_BIAS_STILL) wt901K11RefreshDerived();
  }
  for (int idx=0; idx<2; idx++) {
    // Retain this field for existing diagnostics/compatibility only.
    k11Gyro.s[idx].lastPacketCount = wt901Slots[k11GlobalSlot(idx)].packetCount;
  }
  if (!anyNew) return;

  if (k11Gyro.phase == K11_BIAS_STILL) {
    unsigned long now = millis();
    bool enoughSamples = k11Gyro.s[0].biasSamples >= K11_BIAS_MIN_SAMPLES && k11Gyro.s[1].biasSamples >= K11_BIAS_MIN_SAMPLES;
    bool enoughTime = (now - k11Gyro.stillHoldStartMs) >= K11_BIAS_HOLD_MS;
    if (enoughSamples && enoughTime) {
      for (int i=0;i<2;i++) {
        for (int a=0;a<3;a++) k11Gyro.s[i].bias[a] = k11Gyro.s[i].biasSum[a] / (float)k11Gyro.s[i].biasSamples;
      }
      wt901K11ResetIntegrators();
      k11Gyro.peakMagDeg=0.0f;
      k11Gyro.peakVec[0]=k11Gyro.peakVec[1]=k11Gyro.peakVec[2]=0.0f;
      k11Gyro.axis[0]=0.0f; k11Gyro.axis[1]=1.0f; k11Gyro.axis[2]=0.0f;
      k11Gyro.phase = K11_AXIS_FLEX;
      k11Gyro.phaseStartMs = now;
      Serial.printf("K11_BIAS_READY: A=(%.3f,%.3f,%.3f) B=(%.3f,%.3f,%.3f) samples=%u/%u\n",
        k11Gyro.s[0].bias[0],k11Gyro.s[0].bias[1],k11Gyro.s[0].bias[2],
        k11Gyro.s[1].bias[0],k11Gyro.s[1].bias[1],k11Gyro.s[1].bias[2],
        k11Gyro.s[0].biasSamples,k11Gyro.s[1].biasSamples);
    }
    return;
  }

  wt901K11RefreshDerived();
  unsigned long now = millis();

  if ((k11Gyro.phase == K11_AXIS_FLEX || k11Gyro.phase == K11_AXIS_RETURN) && (now - k11Gyro.phaseStartMs) > K11_CAL_TIMEOUT_MS) {
    wt901K11Fail("axis calibration timeout");
    return;
  }

  if (k11Gyro.phase == K11_AXIS_FLEX) {
    if (k11Gyro.peakMagDeg >= K11_CAL_FLEX_MIN_DEG) {
      if (k11Gyro.flexHoldStartMs == 0) k11Gyro.flexHoldStartMs = now;
      if ((now - k11Gyro.flexHoldStartMs) >= K11_CAL_FLEX_HOLD_MS) {
        k11Gyro.phase = K11_AXIS_RETURN;
        k11Gyro.phaseStartMs = now;
        k11Gyro.returnHoldStartMs = 0;
        Serial.printf("K11_CAL_EXCURSION_REACHED: peak=%.1f axis=(%.3f,%.3f,%.3f)\n",
          k11Gyro.peakMagDeg,k11Gyro.axis[0],k11Gyro.axis[1],k11Gyro.axis[2]);
      }
    } else {
      k11Gyro.flexHoldStartMs = 0;
    }
    return;
  }

  if (k11Gyro.phase == K11_AXIS_RETURN) {
    // Use ONLY the visible extension-excursion coordinate for return completion.
    // Near zero now means the forearm has returned behind the head to the calibrated flexed start pose.
    float projectedReturn = fabsf(k11Dot3(k11Gyro.relVec, k11Gyro.axis));
    k11Gyro.calAngleDeg = projectedReturn;
    bool nearStartPose = projectedReturn <= K11_CAL_RETURN_DEG;
    if (nearStartPose) {
      if (k11Gyro.returnHoldStartMs == 0) {
        k11Gyro.returnHoldStartMs = now;
        Serial.printf("K11_RETURN_HOLD_START: cal=%.1f target<=%.1f hold=%lums\n",
          projectedReturn, K11_CAL_RETURN_DEG, (unsigned long)K11_CAL_RETURN_HOLD_MS);
      }
      if ((now - k11Gyro.returnHoldStartMs) >= K11_CAL_RETURN_HOLD_MS) {
        for (int a=0;a<3;a++) k11Gyro.learnedAxisSemantic[a] = k11Gyro.peakVec[a];
        k11Normalize3(k11Gyro.learnedAxisSemantic);
        k11Gyro.axisDominance = fabsf(k11Gyro.learnedAxisSemantic[1]);
        const float semanticDownAlignment = fabsf(k11Gyro.learnedAxisSemantic[2]);
        if (k11Gyro.peakMagDeg < K11_CAL_FLEX_MIN_DEG) {
          wt901K11Fail("calibration excursion too small");
          return;
        }
        if (semanticDownAlignment > K11_CAL_MAX_SEMANTIC_DOWN_ALIGNMENT) {
          wt901K11Fail("calibration hinge axis is implausibly vertical; repeat elbow motion");
          return;
        }
        // V4.4.17: use the actually learned personalized hinge axis in the already
        // calibrated anatomical semantic frame. The previous fixed SIDE-axis gate
        // rejected valid trials such as peak=79deg, learned=(-0.851,-0.526,0.010).
        // Keeping the learned axis preserves the full elbow excursion while the
        // independent U/P/T safeguards still detect compensation and wrong plane.
        for (int a=0;a<3;a++) k11Gyro.axis[a] = k11Gyro.learnedAxisSemantic[a];
        wt901K11ResetIntegrators();
        k11Gyro.phase = K11_READY;
        k11Gyro.phaseStartMs = now;
        k11Gyro.paused = false;
        Serial.printf("K11_READY V4.4.17: peak=%.1f personalizedSemanticAxis=(%.3f,%.3f,%.3f) sideAlignment=%.3f downAlignment=%.3f\n",
          k11Gyro.peakMagDeg,
          k11Gyro.axis[0],k11Gyro.axis[1],k11Gyro.axis[2],
          k11Gyro.axisDominance, semanticDownAlignment);
      }
    } else {
      k11Gyro.returnHoldStartMs = 0;
    }
    return;
  }

  if (k11Gyro.phase == K11_READY) {
    if (k11Gyro.paused) return;

    // V4.4.31F1 emergency angle-floor fix:
    // The gyro-integrated elbow coordinate can drift POSITIVE as well as negative.
    // In the real 2026-08-26 log the arm was physically back at natural-down
    // (upper elevation 0.6 deg, forearm elevation 1.1 deg) while the integrated
    // elbow coordinate was still 28.8 deg.  The old zero snap could never recover
    // because it required the already-drifted displayed elbowDeg <= 10 deg.
    //
    // Use the independent gravity references as an additional zero-pose witness.
    // This changes ONLY the elbow zero offset; it does NOT touch K11 hinge axis,
    // BODY/plane references, A-E plane logic, or any training-quality threshold.
    const bool k11GravityZeroPose =
        torsoRefUpperNeutralElevationDeg() <= 12.0f &&
        torsoRefForeNeutralElevationDeg()  <= 15.0f;
    const bool k11ZeroPoseEvidence =
        (k11Gyro.elbowDeg <= K11_ZERO_SNAP_ANGLE_DEG) || k11GravityZeroPose;

    if (k11ZeroPoseEvidence && fabsf(k11Gyro.elbowSpeedDegS) <= K11_ZERO_SNAP_SPEED_DEG_S) {
      if (k11Gyro.zeroSnapHoldStartMs == 0) k11Gyro.zeroSnapHoldStartMs = now;
      if ((now - k11Gyro.zeroSnapHoldStartMs) >= K11_ZERO_SNAP_HOLD_MS) {
        k11Gyro.formalZeroOffsetDeg = k11Gyro.rawProjectedDeg;
        for(int a=0;a<3;a++) k11Gyro.formalZeroRelVec[a]=k11Gyro.relVec[a];
        k11Gyro.hingeOffAxisRotationDeg=0.0f;
        k11Gyro.hingeAxisDeviationDeg=0.0f;
        k11Gyro.elbowDeg = 0.0f;
        k11Gyro.elbowSpeedDegS = 0.0f;
      }
    } else {
      k11Gyro.zeroSnapHoldStartMs = 0;
    }
  }
}

static void wt901K11SetPaused(bool paused) {
  k11Gyro.paused = paused;
  for (int i=0;i<2;i++) {
    k11Gyro.s[i].lastPacketCount = wt901Slots[k11GlobalSlot(i)].packetCount;
    k11Gyro.s[i].lastSampleMs = wt901Slots[k11GlobalSlot(i)].latest.sampleMs;
    k11Gyro.s[i].correctedRate[0]=k11Gyro.s[i].correctedRate[1]=k11Gyro.s[i].correctedRate[2]=0.0f;
    k11Gyro.s[i].prevRate[0]=k11Gyro.s[i].prevRate[1]=k11Gyro.s[i].prevRate[2]=0.0f;
    k11Gyro.s[i].prevRateReady = true;
  }
  k11Gyro.elbowSpeedDegS = 0.0f;
}

static void wt901K11RezeroFormal() {
  if (k11Gyro.phase != K11_READY) return;
  wt901K11ResetIntegrators();
  k11Gyro.formalZeroOffsetDeg = 0.0f;
  k11Gyro.zeroSnapHoldStartMs = 0;
}

static bool wt901K11Started() { return k11Gyro.started; }
static bool wt901K11BiasActive() { return k11Gyro.phase == K11_BIAS_STILL; }
static bool wt901K11AxisFlexActive() { return k11Gyro.phase == K11_AXIS_FLEX; }
static bool wt901K11AxisReturnActive() { return k11Gyro.phase == K11_AXIS_RETURN; }
static bool wt901K11Ready() { return k11Gyro.phase == K11_READY; }
static bool wt901K11Failed() { return k11Gyro.phase == K11_FAILED; }
static bool wt901K11CalibrationActive() { return k11Gyro.phase == K11_BIAS_STILL || k11Gyro.phase == K11_AXIS_FLEX || k11Gyro.phase == K11_AXIS_RETURN; }
static K11GyroPhase wt901K11Phase() { return k11Gyro.phase; }
static const char* wt901K11PhaseName() {
  switch(k11Gyro.phase) {
    case K11_BIAS_STILL: return "BIAS_STILL";
    case K11_AXIS_FLEX: return "CAL_EXTEND";
    case K11_AXIS_RETURN: return "CAL_RETURN";
    case K11_READY: return "TRAIN";
    case K11_FAILED: return "FAILED";
    default: return "IDLE";
  }
}
static float wt901K11ElbowAngleDeg() { return k11Gyro.elbowDeg; }
static float wt901K11ElbowSpeedDegS() { return k11Gyro.elbowSpeedDegS; }
static float wt901K11CalAngleDeg() { return k11Gyro.calAngleDeg; }
static float wt901K11PeakDeg() { return k11Gyro.peakMagDeg; }
static float wt901K11AxisX() { return k11Gyro.axis[0]; }
static float wt901K11AxisY() { return k11Gyro.axis[1]; }
static float wt901K11AxisZ() { return k11Gyro.axis[2]; }
static float wt901K11AxisDominance() { return k11Gyro.axisDominance; }
static float wt901K11LearnedAxisFront() { return k11Gyro.learnedAxisSemantic[0]; }
static float wt901K11LearnedAxisSide() { return k11Gyro.learnedAxisSemantic[1]; }
static float wt901K11LearnedAxisDown() { return k11Gyro.learnedAxisSemantic[2]; }
// V4.4.30: current bias-corrected gyro rates in each sensor's calibrated
// FRONT/SIDE/DOWN semantic frame. The formal plane mapper rotates A and B
// separately into the BODY frame before taking their differential axis.
static float wt901K11FixedRateFrontDegS() { return k11Gyro.s[0].correctedRate[0]; }
static float wt901K11FixedRateSideDegS() { return k11Gyro.s[0].correctedRate[1]; }
static float wt901K11FixedRateDownDegS() { return k11Gyro.s[0].correctedRate[2]; }
static float wt901K11MovingRateFrontDegS() { return k11Gyro.s[1].correctedRate[0]; }
static float wt901K11MovingRateSideDegS() { return k11Gyro.s[1].correctedRate[1]; }
static float wt901K11MovingRateDownDegS() { return k11Gyro.s[1].correctedRate[2]; }
static bool wt901K11UsesSemanticFrame() { return k11SemanticBasisReady(); }
static float wt901K11HingeOffAxisRotationDeg() { return k11Gyro.hingeOffAxisRotationDeg; }
static float wt901K11HingeAxisDeviationDeg() { return k11Gyro.hingeAxisDeviationDeg; }
static uint16_t wt901K11BiasSamplesA() { return k11Gyro.s[0].biasSamples; }
static uint16_t wt901K11BiasSamplesB() { return k11Gyro.s[1].biasSamples; }
static float wt901K11BiasProgress() {
  float a = (float)k11Gyro.s[0].biasSamples / (float)K11_BIAS_MIN_SAMPLES;
  float b = (float)k11Gyro.s[1].biasSamples / (float)K11_BIAS_MIN_SAMPLES;
  float p = a < b ? a : b; if (p>1.0f) p=1.0f; return p;
}
static float wt901K11RawProjectedDeg() { return k11Gyro.rawProjectedDeg - k11Gyro.formalZeroOffsetDeg; }
static float wt901K11ReturnHoldProgress() {
  if (k11Gyro.phase != K11_AXIS_RETURN || k11Gyro.returnHoldStartMs == 0) return 0.0f;
  unsigned long elapsed = millis() - k11Gyro.returnHoldStartMs;
  float p = (float)elapsed / (float)K11_CAL_RETURN_HOLD_MS;
  if (p < 0.0f) p = 0.0f;
  if (p > 1.0f) p = 1.0f;
  return p;
}
static uint32_t wt901K11RejectedDtA() { return k11Gyro.rejectedDtA; }
static uint32_t wt901K11RejectedDtB() { return k11Gyro.rejectedDtB; }
static uint32_t wt901K11BridgedGapA() { return k11Gyro.bridgedGapA; }
static uint32_t wt901K11BridgedGapB() { return k11Gyro.bridgedGapB; }
static uint32_t wt901K11ProcessedA() { return k11Gyro.processedA; }
static uint32_t wt901K11ProcessedB() { return k11Gyro.processedB; }
static uint8_t wt901K11QueueDepthA() { return wt901SampleQueueDepth(k11FixedSlot); }
static uint8_t wt901K11QueueDepthB() { return wt901SampleQueueDepth(k11MovingSlot); }
static uint32_t wt901K11QueueDroppedA() { return wt901SampleQueueDropped(k11FixedSlot); }
static uint32_t wt901K11QueueDroppedB() { return wt901SampleQueueDropped(k11MovingSlot); }
