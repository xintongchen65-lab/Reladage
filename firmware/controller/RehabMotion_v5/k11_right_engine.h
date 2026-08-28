#pragma once
#include <Arduino.h>
#include <math.h>

// Independent K11 engine used only for the right C/D pair when bilateral mode is active.
// It is intentionally a namespaced copy of the already validated K11 core so the left A/B
// and right C/D pipelines keep separate bias, learned hinge axis, integration timestamps and
// upper-arm gravity baseline while running concurrently.
namespace K11R {


// K11-derived gyro-differential elbow EXTENSION-EXCURSION core for V4 A2.
// IMPORTANT mounting rule: IMU_A (upper arm) and IMU_B (forearm) must be mounted
// with the same box orientation (same face outward, same arrow/connector direction).
//
// No Euler angle, yaw, magnetometer heading, quaternion or A/B packet pairing is
// used for the formal elbow angle.  Each IMU is integrated on its own packet
// timestamp, then the learned hinge-axis components are subtracted:
//   elbow = integral(dot(gyro_B-bias_B, axis)) - integral(dot(gyro_A-bias_A, axis))
// The motion axis is learned from ONE slow overhead elbow-extension -> return-to-flexed-start calibration movement.

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
  float axis[3] = {1,0,0};
  float axisDominance = 0.0f; // diagnostic only

  float rawProjectedDeg = 0.0f;
  float formalZeroOffsetDeg = 0.0f;
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

static K11GyroElbowState rK11Gyro;

static int rK11FixedSlot = 0;
static int rK11MovingSlot = 1;

static int rK11GlobalSlot(int localIndex) {
  return localIndex == 0 ? rK11FixedSlot : rK11MovingSlot;
}

static void rWt901K11SelectPair(int fixedSlot, int movingSlot) {
  if (fixedSlot < 0 || fixedSlot >= WT901_ABCD_COUNT || movingSlot < 0 || movingSlot >= WT901_ABCD_COUNT || fixedSlot == movingSlot) return;
  rK11FixedSlot = fixedSlot;
  rK11MovingSlot = movingSlot;
  K11GyroElbowState fresh;
  rK11Gyro = fresh;
  Serial.printf("K11_PAIR_SELECTED: proximal=IMU_%s(%d) distal=IMU_%s(%d)\n",
    wt901Slots[rK11FixedSlot].role, rK11FixedSlot, wt901Slots[rK11MovingSlot].role, rK11MovingSlot);
}

static int rWt901K11FixedSlot() { return rK11FixedSlot; }
static int rWt901K11MovingSlot() { return rK11MovingSlot; }

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

static float rK11AbsMax3(float x, float y, float z) {
  float m = fabsf(x);
  if (fabsf(y) > m) m = fabsf(y);
  if (fabsf(z) > m) m = fabsf(z);
  return m;
}

static float rK11Norm3(const float v[3]) {
  return sqrtf(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
}

static float rK11Dot3(const float a[3], const float b[3]) {
  return a[0]*b[0] + a[1]*b[1] + a[2]*b[2];
}

static void rK11Normalize3(float v[3]) {
  float n = rK11Norm3(v);
  if (n < 1e-5f) { v[0]=1.0f; v[1]=0.0f; v[2]=0.0f; return; }
  v[0] /= n; v[1] /= n; v[2] /= n;
}

static void rWt901K11ResetBiasAccum() {
  for (int i=0;i<2;i++) {
    rK11Gyro.s[i].biasSum[0]=rK11Gyro.s[i].biasSum[1]=rK11Gyro.s[i].biasSum[2]=0.0f;
    rK11Gyro.s[i].biasSamples=0;
  }
  rK11Gyro.stillHoldStartMs = millis();
  rK11Gyro.biasResets++;
}

static void rWt901K11ResetIntegrators() {
  for (int i=0;i<2;i++) {
    rK11Gyro.s[i].angle[0]=rK11Gyro.s[i].angle[1]=rK11Gyro.s[i].angle[2]=0.0f;
    rK11Gyro.s[i].correctedRate[0]=rK11Gyro.s[i].correctedRate[1]=rK11Gyro.s[i].correctedRate[2]=0.0f;
    rK11Gyro.s[i].prevRate[0]=rK11Gyro.s[i].prevRate[1]=rK11Gyro.s[i].prevRate[2]=0.0f;
    rK11Gyro.s[i].prevRateReady = true; // reset occurs while physically still/near zero
    rK11Gyro.s[i].lastSampleMs = wt901Slots[rK11GlobalSlot(i)].latest.sampleMs;
    rK11Gyro.s[i].lastPacketCount = wt901Slots[rK11GlobalSlot(i)].packetCount;
    wt901ClearSampleQueue(rK11GlobalSlot(i));
  }
  rK11Gyro.relVec[0]=rK11Gyro.relVec[1]=rK11Gyro.relVec[2]=0.0f;
  rK11Gyro.rawProjectedDeg = 0.0f;
  rK11Gyro.formalZeroOffsetDeg = 0.0f;
  rK11Gyro.elbowDeg = 0.0f;
  rK11Gyro.elbowSpeedDegS = 0.0f;
  rK11Gyro.calAngleDeg = 0.0f;
}

static void rWt901K11Start() {
  K11GyroElbowState fresh;
  rK11Gyro = fresh;
  rK11Gyro.started = true;
  rK11Gyro.phase = K11_BIAS_STILL;
  rK11Gyro.phaseStartMs = millis();
  rK11Gyro.stillHoldStartMs = millis();
  for (int i=0;i<2;i++) {
    wt901ClearSampleQueue(rK11GlobalSlot(i));
    rK11Gyro.s[i].lastPacketCount = wt901Slots[rK11GlobalSlot(i)].packetCount;
    rK11Gyro.s[i].lastSampleMs = wt901Slots[rK11GlobalSlot(i)].latest.sampleMs;
  }
  Serial.println("K11_START: hold the selected exercise start pose STILL for gyro bias calibration.");
}

static void rWt901K11Fail(const char *reason) {
  rK11Gyro.phase = K11_FAILED;
  rK11Gyro.phaseStartMs = millis();
  Serial.print("K11_FAIL: ");
  Serial.println(reason);
}

static bool rWt901K11BothReceiving() {
  return wt901Slots[rK11FixedSlot].latest.valid && wt901Slots[rK11MovingSlot].latest.valid && wt901Slots[rK11FixedSlot].packetCount>0 && wt901Slots[rK11MovingSlot].packetCount>0;
}

static void rWt901K11ProcessBiasSample(int idx, const WT901EulerData &d) {
  float g[3] = {d.gx,d.gy,d.gz};
  if (rK11AbsMax3(g[0],g[1],g[2]) > K11_BIAS_MAX_GYRO_DEG_S) {
    rWt901K11ResetBiasAccum();
    return;
  }
  K11SensorIntegrator &s = rK11Gyro.s[idx];
  s.biasSum[0] += g[0]; s.biasSum[1] += g[1]; s.biasSum[2] += g[2];
  s.biasSamples++;
}

static void rWt901K11ProcessIntegratedSample(int idx, const WT901EulerData &d) {
  K11SensorIntegrator &s = rK11Gyro.s[idx];
  unsigned long t = d.sampleMs;
  if (s.lastSampleMs == 0) { s.lastSampleMs=t; return; }
  unsigned long dtMs = t - s.lastSampleMs;
  s.lastSampleMs = t;

  float raw[3] = {d.gx,d.gy,d.gz};
  float curRate[3];
  for (int a=0;a<3;a++) {
    float r = raw[a] - s.bias[a];
    if (fabsf(r) < K11_GYRO_DEADZONE_DEG_S) r = 0.0f;
    curRate[a] = r;
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
    if (idx==0) rK11Gyro.rejectedDtA++; else rK11Gyro.rejectedDtB++;
    for (int a=0;a<3;a++) {
      s.correctedRate[a] = curRate[a];
      s.prevRate[a] = curRate[a];
    }
    s.prevRateReady = true;
    return;
  }

  // Diagnostic only: this interval would have been completely discarded by K11.2.1.
  if (dtMs > K11_SOFT_GAP_MS) {
    if (idx==0) rK11Gyro.bridgedGapA++; else rK11Gyro.bridgedGapB++;
  }

  float dt = dtMs * 0.001f;
  for (int a=0;a<3;a++) {
    float integrateRate = curRate[a];
    if (s.prevRateReady) integrateRate = 0.5f * (s.prevRate[a] + curRate[a]);
    if (!rK11Gyro.paused) s.angle[a] += integrateRate * dt;
    s.correctedRate[a] = curRate[a];
    s.prevRate[a] = curRate[a];
  }
  s.prevRateReady = true;
}

static void rWt901K11RefreshDerived() {
  for (int a=0;a<3;a++) rK11Gyro.relVec[a] = rK11Gyro.s[1].angle[a] - rK11Gyro.s[0].angle[a];
  float mag = rK11Norm3(rK11Gyro.relVec);

  if (rK11Gyro.phase == K11_AXIS_FLEX || rK11Gyro.phase == K11_AXIS_RETURN) {
    if (mag > rK11Gyro.peakMagDeg) {
      rK11Gyro.peakMagDeg = mag;
      for (int a=0;a<3;a++) rK11Gyro.peakVec[a] = rK11Gyro.relVec[a];
      if (mag > 1.0f) {
        for (int a=0;a<3;a++) rK11Gyro.axis[a] = rK11Gyro.peakVec[a];
        rK11Normalize3(rK11Gyro.axis);
      }
    }
    if (rK11Gyro.peakMagDeg > 1.0f) {
      rK11Gyro.calAngleDeg = fabsf(rK11Dot3(rK11Gyro.relVec, rK11Gyro.axis));
    } else {
      rK11Gyro.calAngleDeg = mag;
    }
  }

  if (rK11Gyro.phase == K11_READY) {
    rK11Gyro.rawProjectedDeg = rK11Dot3(rK11Gyro.relVec, rK11Gyro.axis);
    float projected = rK11Gyro.rawProjectedDeg - rK11Gyro.formalZeroOffsetDeg;
    float rateA = rK11Gyro.s[0].correctedRate[0]*rK11Gyro.axis[0] + rK11Gyro.s[0].correctedRate[1]*rK11Gyro.axis[1] + rK11Gyro.s[0].correctedRate[2]*rK11Gyro.axis[2];
    float rateB = rK11Gyro.s[1].correctedRate[0]*rK11Gyro.axis[0] + rK11Gyro.s[1].correctedRate[1]*rK11Gyro.axis[1] + rK11Gyro.s[1].correctedRate[2]*rK11Gyro.axis[2];
    rK11Gyro.elbowSpeedDegS = rateB - rateA;
    if (projected < 0.0f) projected = 0.0f;
    if (projected > K11_FORMAL_MAX_DEG) projected = K11_FORMAL_MAX_DEG;
    rK11Gyro.elbowDeg = projected;
  }
}

static void rWt901K11Update() {
  if (!rK11Gyro.started) return;

  // K12.1.1: during group rest, manual pause, or after the final result is
  // frozen, keep draining BLE FIFO so it cannot overflow, but DO NOT integrate
  // those samples into the elbow angle. Resume starts from the then-current
  // timestamp via rWt901K11SetPaused(false), so rest-period motion is ignored.
  if (rK11Gyro.phase == K11_READY && rK11Gyro.paused) {
    for (int idx=0; idx<2; idx++) {
      WT901EulerData discard;
      while (wt901PopSample(rK11GlobalSlot(idx), discard)) {
        if (discard.valid) rK11Gyro.s[idx].lastSampleMs = discard.sampleMs;
      }
      rK11Gyro.s[idx].lastPacketCount = wt901Slots[rK11GlobalSlot(idx)].packetCount;
      rK11Gyro.s[idx].correctedRate[0]=rK11Gyro.s[idx].correctedRate[1]=rK11Gyro.s[idx].correctedRate[2]=0.0f;
      rK11Gyro.s[idx].prevRate[0]=rK11Gyro.s[idx].prevRate[1]=rK11Gyro.s[idx].prevRate[2]=0.0f;
      rK11Gyro.s[idx].prevRateReady = false;
    }
    rK11Gyro.elbowSpeedDegS = 0.0f;
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
      if (!wt901PopSample(rK11GlobalSlot(idx), d)) continue;
      if (!d.valid) continue;
      roundAny = true;
      anyNew = true;
      if (idx==0) rK11Gyro.processedA++; else rK11Gyro.processedB++;

      if (rK11Gyro.phase == K11_BIAS_STILL) rWt901K11ProcessBiasSample(idx, d);
      else if (rK11Gyro.phase == K11_AXIS_FLEX || rK11Gyro.phase == K11_AXIS_RETURN || rK11Gyro.phase == K11_READY) rWt901K11ProcessIntegratedSample(idx, d);
    }
    if (!roundAny) break;
    if (rK11Gyro.phase != K11_BIAS_STILL) rWt901K11RefreshDerived();
  }
  for (int idx=0; idx<2; idx++) {
    // Retain this field for existing diagnostics/compatibility only.
    rK11Gyro.s[idx].lastPacketCount = wt901Slots[rK11GlobalSlot(idx)].packetCount;
  }
  if (!anyNew) return;

  if (rK11Gyro.phase == K11_BIAS_STILL) {
    unsigned long now = millis();
    bool enoughSamples = rK11Gyro.s[0].biasSamples >= K11_BIAS_MIN_SAMPLES && rK11Gyro.s[1].biasSamples >= K11_BIAS_MIN_SAMPLES;
    bool enoughTime = (now - rK11Gyro.stillHoldStartMs) >= K11_BIAS_HOLD_MS;
    if (enoughSamples && enoughTime) {
      for (int i=0;i<2;i++) {
        for (int a=0;a<3;a++) rK11Gyro.s[i].bias[a] = rK11Gyro.s[i].biasSum[a] / (float)rK11Gyro.s[i].biasSamples;
      }
      rWt901K11ResetIntegrators();
      rK11Gyro.peakMagDeg=0.0f;
      rK11Gyro.peakVec[0]=rK11Gyro.peakVec[1]=rK11Gyro.peakVec[2]=0.0f;
      rK11Gyro.axis[0]=1.0f; rK11Gyro.axis[1]=0.0f; rK11Gyro.axis[2]=0.0f;
      rK11Gyro.phase = K11_AXIS_FLEX;
      rK11Gyro.phaseStartMs = now;
      Serial.printf("K11_BIAS_READY: A=(%.3f,%.3f,%.3f) B=(%.3f,%.3f,%.3f) samples=%u/%u\n",
        rK11Gyro.s[0].bias[0],rK11Gyro.s[0].bias[1],rK11Gyro.s[0].bias[2],
        rK11Gyro.s[1].bias[0],rK11Gyro.s[1].bias[1],rK11Gyro.s[1].bias[2],
        rK11Gyro.s[0].biasSamples,rK11Gyro.s[1].biasSamples);
    }
    return;
  }

  rWt901K11RefreshDerived();
  unsigned long now = millis();

  if ((rK11Gyro.phase == K11_AXIS_FLEX || rK11Gyro.phase == K11_AXIS_RETURN) && (now - rK11Gyro.phaseStartMs) > K11_CAL_TIMEOUT_MS) {
    rWt901K11Fail("axis calibration timeout");
    return;
  }

  if (rK11Gyro.phase == K11_AXIS_FLEX) {
    if (rK11Gyro.peakMagDeg >= K11_CAL_FLEX_MIN_DEG) {
      if (rK11Gyro.flexHoldStartMs == 0) rK11Gyro.flexHoldStartMs = now;
      if ((now - rK11Gyro.flexHoldStartMs) >= K11_CAL_FLEX_HOLD_MS) {
        rK11Gyro.phase = K11_AXIS_RETURN;
        rK11Gyro.phaseStartMs = now;
        rK11Gyro.returnHoldStartMs = 0;
        Serial.printf("K11_CAL_EXCURSION_REACHED: peak=%.1f axis=(%.3f,%.3f,%.3f)\n",
          rK11Gyro.peakMagDeg,rK11Gyro.axis[0],rK11Gyro.axis[1],rK11Gyro.axis[2]);
      }
    } else {
      rK11Gyro.flexHoldStartMs = 0;
    }
    return;
  }

  if (rK11Gyro.phase == K11_AXIS_RETURN) {
    // Use ONLY the visible extension-excursion coordinate for return completion.
    // Near zero now means the forearm has returned behind the head to the calibrated flexed start pose.
    float projectedReturn = fabsf(rK11Dot3(rK11Gyro.relVec, rK11Gyro.axis));
    rK11Gyro.calAngleDeg = projectedReturn;
    bool nearStartPose = projectedReturn <= K11_CAL_RETURN_DEG;
    if (nearStartPose) {
      if (rK11Gyro.returnHoldStartMs == 0) {
        rK11Gyro.returnHoldStartMs = now;
        Serial.printf("K11_RETURN_HOLD_START: cal=%.1f target<=%.1f hold=%lums\n",
          projectedReturn, K11_CAL_RETURN_DEG, (unsigned long)K11_CAL_RETURN_HOLD_MS);
      }
      if ((now - rK11Gyro.returnHoldStartMs) >= K11_CAL_RETURN_HOLD_MS) {
        for (int a=0;a<3;a++) rK11Gyro.axis[a] = rK11Gyro.peakVec[a];
        rK11Normalize3(rK11Gyro.axis);
        float maxComp = rK11AbsMax3(rK11Gyro.axis[0],rK11Gyro.axis[1],rK11Gyro.axis[2]);
        rK11Gyro.axisDominance = maxComp;
        if (rK11Gyro.peakMagDeg < K11_CAL_FLEX_MIN_DEG) {
          rWt901K11Fail("calibration excursion too small");
          return;
        }
        rWt901K11ResetIntegrators();
        rK11Gyro.phase = K11_READY;
        rK11Gyro.phaseStartMs = now;
        rK11Gyro.paused = false;
        Serial.printf("K11_READY: peak=%.1f axis=(%.4f,%.4f,%.4f) dominance=%.3f\n",
          rK11Gyro.peakMagDeg,rK11Gyro.axis[0],rK11Gyro.axis[1],rK11Gyro.axis[2],rK11Gyro.axisDominance);
      }
    } else {
      rK11Gyro.returnHoldStartMs = 0;
    }
    return;
  }

  if (rK11Gyro.phase == K11_READY) {
    if (rK11Gyro.paused) return;
    // Formal zero-drift snap also uses only the SAME displayed elbow coordinate and its
    // differential speed, not hidden per-sensor rate gates.
    if (rK11Gyro.elbowDeg <= K11_ZERO_SNAP_ANGLE_DEG && fabsf(rK11Gyro.elbowSpeedDegS) <= K11_ZERO_SNAP_SPEED_DEG_S) {
      if (rK11Gyro.zeroSnapHoldStartMs == 0) rK11Gyro.zeroSnapHoldStartMs = now;
      if ((now - rK11Gyro.zeroSnapHoldStartMs) >= K11_ZERO_SNAP_HOLD_MS) {
        rK11Gyro.formalZeroOffsetDeg = rK11Gyro.rawProjectedDeg;
        rK11Gyro.elbowDeg = 0.0f;
        rK11Gyro.elbowSpeedDegS = 0.0f;
      }
    } else {
      rK11Gyro.zeroSnapHoldStartMs = 0;
    }
  }
}

static void rWt901K11SetPaused(bool paused) {
  rK11Gyro.paused = paused;
  for (int i=0;i<2;i++) {
    rK11Gyro.s[i].lastPacketCount = wt901Slots[rK11GlobalSlot(i)].packetCount;
    rK11Gyro.s[i].lastSampleMs = wt901Slots[rK11GlobalSlot(i)].latest.sampleMs;
    rK11Gyro.s[i].correctedRate[0]=rK11Gyro.s[i].correctedRate[1]=rK11Gyro.s[i].correctedRate[2]=0.0f;
    rK11Gyro.s[i].prevRate[0]=rK11Gyro.s[i].prevRate[1]=rK11Gyro.s[i].prevRate[2]=0.0f;
    rK11Gyro.s[i].prevRateReady = true;
  }
  rK11Gyro.elbowSpeedDegS = 0.0f;
}

static void rWt901K11RezeroFormal() {
  if (rK11Gyro.phase != K11_READY) return;
  rWt901K11ResetIntegrators();
  rK11Gyro.formalZeroOffsetDeg = 0.0f;
  rK11Gyro.zeroSnapHoldStartMs = 0;
}

static bool rWt901K11Started() { return rK11Gyro.started; }
static bool rWt901K11BiasActive() { return rK11Gyro.phase == K11_BIAS_STILL; }
static bool rWt901K11AxisFlexActive() { return rK11Gyro.phase == K11_AXIS_FLEX; }
static bool rWt901K11AxisReturnActive() { return rK11Gyro.phase == K11_AXIS_RETURN; }
static bool rWt901K11Ready() { return rK11Gyro.phase == K11_READY; }
static bool rWt901K11Failed() { return rK11Gyro.phase == K11_FAILED; }
static bool rWt901K11CalibrationActive() { return rK11Gyro.phase == K11_BIAS_STILL || rK11Gyro.phase == K11_AXIS_FLEX || rK11Gyro.phase == K11_AXIS_RETURN; }
static K11GyroPhase rWt901K11Phase() { return rK11Gyro.phase; }
static const char* rWt901K11PhaseName() {
  switch(rK11Gyro.phase) {
    case K11_BIAS_STILL: return "BIAS_STILL";
    case K11_AXIS_FLEX: return "CAL_EXTEND";
    case K11_AXIS_RETURN: return "CAL_RETURN";
    case K11_READY: return "TRAIN";
    case K11_FAILED: return "FAILED";
    default: return "IDLE";
  }
}
static float rWt901K11ElbowAngleDeg() { return rK11Gyro.elbowDeg; }
static float rWt901K11ElbowSpeedDegS() { return rK11Gyro.elbowSpeedDegS; }
static float rWt901K11CalAngleDeg() { return rK11Gyro.calAngleDeg; }
static float rWt901K11PeakDeg() { return rK11Gyro.peakMagDeg; }
static float rWt901K11AxisX() { return rK11Gyro.axis[0]; }
static float rWt901K11AxisY() { return rK11Gyro.axis[1]; }
static float rWt901K11AxisZ() { return rK11Gyro.axis[2]; }
static float rWt901K11AxisDominance() { return rK11Gyro.axisDominance; }
static uint16_t rWt901K11BiasSamplesA() { return rK11Gyro.s[0].biasSamples; }
static uint16_t rWt901K11BiasSamplesB() { return rK11Gyro.s[1].biasSamples; }
static float rWt901K11BiasProgress() {
  float a = (float)rK11Gyro.s[0].biasSamples / (float)K11_BIAS_MIN_SAMPLES;
  float b = (float)rK11Gyro.s[1].biasSamples / (float)K11_BIAS_MIN_SAMPLES;
  float p = a < b ? a : b; if (p>1.0f) p=1.0f; return p;
}
static float rWt901K11RawProjectedDeg() { return rK11Gyro.rawProjectedDeg - rK11Gyro.formalZeroOffsetDeg; }
static float rWt901K11ReturnHoldProgress() {
  if (rK11Gyro.phase != K11_AXIS_RETURN || rK11Gyro.returnHoldStartMs == 0) return 0.0f;
  unsigned long elapsed = millis() - rK11Gyro.returnHoldStartMs;
  float p = (float)elapsed / (float)K11_CAL_RETURN_HOLD_MS;
  if (p < 0.0f) p = 0.0f;
  if (p > 1.0f) p = 1.0f;
  return p;
}
static uint32_t rWt901K11RejectedDtA() { return rK11Gyro.rejectedDtA; }
static uint32_t rWt901K11RejectedDtB() { return rK11Gyro.rejectedDtB; }
static uint32_t rWt901K11BridgedGapA() { return rK11Gyro.bridgedGapA; }
static uint32_t rWt901K11BridgedGapB() { return rK11Gyro.bridgedGapB; }
static uint32_t rWt901K11ProcessedA() { return rK11Gyro.processedA; }
static uint32_t rWt901K11ProcessedB() { return rK11Gyro.processedB; }
static uint8_t rWt901K11QueueDepthA() { return wt901SampleQueueDepth(rK11FixedSlot); }
static uint8_t rWt901K11QueueDepthB() { return wt901SampleQueueDepth(rK11MovingSlot); }
static uint32_t rWt901K11QueueDroppedA() { return wt901SampleQueueDropped(rK11FixedSlot); }
static uint32_t rWt901K11QueueDroppedB() { return wt901SampleQueueDropped(rK11MovingSlot); }



// K11.3 upper-arm participation metric (measurement source unchanged from K11.2).
// A = upper-arm IMU. At the initial overhead-flexed/still bias phase we learn the
// gravity direction expressed in the selected upper-arm IMU's local frame. During training we
// compare the low-pass gravity direction with that baseline:
//   tilt = acos(g0_hat dot g_hat)
// This deliberately ignores yaw / magnetometer heading and is completely
// separate from the K11 gyro-differential elbow-angle pipeline.
//
// V4 A2 K1 records per-attempt maximum tilt but deliberately does not classify it yet.
// The gravity-tilt estimator remains independent from the extension-excursion pipeline.

struct K11UpperArmTiltState {
  bool started = false;
  bool baselineReady = false;
  bool filteredReady = false;
  bool lastSampleAccepted = false;
  uint32_t lastPacketCount = 0;
  uint16_t baselineSamples = 0;
  float baselineSum[3] = {0,0,0};
  float baseline[3] = {0,0,1};
  float filtered[3] = {0,0,1};
  float currentTiltDeg = 0.0f;
  float rawTiltDeg = 0.0f;
  float accelNormG = 0.0f;
};

static K11UpperArmTiltState rK11UpperTilt;

static int rK11UpperArmSlot = 0;
static void rWt901K11UpperTiltSelectSlot(int slot) {
  if (slot < 0 || slot >= WT901_ABCD_COUNT) return;
  rK11UpperArmSlot = slot;
  K11UpperArmTiltState fresh;
  rK11UpperTilt = fresh;
  Serial.printf("K11_UPPER_SLOT_SELECTED: IMU_%s(%d)\n", wt901Slots[slot].role, slot);
}
static int rWt901K11UpperTiltSlot() { return rK11UpperArmSlot; }

// Acceleration magnitude gate. During faster motion linear acceleration can
// contaminate the gravity direction; reject those samples from the LPF rather
// than letting one transient create a false shoulder excursion.
static constexpr float K11_UPPER_ACCEL_NORM_MIN_G = 0.72f;
static constexpr float K11_UPPER_ACCEL_NORM_MAX_G = 1.28f;
static constexpr uint16_t K11_UPPER_BASELINE_MIN_SAMPLES = 6;
static constexpr float K11_UPPER_LPF_ALPHA = 0.24f;
static constexpr float V4_A2_START_POSE_REACQUIRE_MAX_DEG = 35.0f; // engineering guard: reject grossly wrong rest/pause resume pose, e.g. arm still lowered.

static float rK11UpperNorm3(const float v[3]) {
  return sqrtf(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
}

static bool rK11UpperNormalize3(float v[3]) {
  float n = rK11UpperNorm3(v);
  if (!isfinite(n) || n < 1e-5f) return false;
  v[0] /= n; v[1] /= n; v[2] /= n;
  return true;
}

static float rK11UpperAngleDeg(const float a[3], const float b[3]) {
  float d = a[0]*b[0] + a[1]*b[1] + a[2]*b[2];
  if (d > 1.0f) d = 1.0f;
  if (d < -1.0f) d = -1.0f;
  return acosf(d) * 57.2957795131f;
}

static void rWt901K11UpperTiltStart() {
  K11UpperArmTiltState fresh;
  rK11UpperTilt = fresh;
  rK11UpperTilt.started = true;
  rK11UpperTilt.lastPacketCount = wt901Slots[rK11UpperArmSlot].packetCount;
  Serial.println("K11_UPPER_TILT_START: learn selected upper-arm IMU gravity baseline in overhead-flexed start pose.");
}

static void rWt901K11UpperTiltFinalizeBaseline() {
  if (rK11UpperTilt.baselineReady) return;

  float b[3] = {rK11UpperTilt.baselineSum[0], rK11UpperTilt.baselineSum[1], rK11UpperTilt.baselineSum[2]};
  bool ok = rK11UpperTilt.baselineSamples >= K11_UPPER_BASELINE_MIN_SAMPLES && rK11UpperNormalize3(b);

  if (!ok && wt901Slots[rK11UpperArmSlot].latest.valid) {
    b[0] = wt901Slots[rK11UpperArmSlot].latest.ax;
    b[1] = wt901Slots[rK11UpperArmSlot].latest.ay;
    b[2] = wt901Slots[rK11UpperArmSlot].latest.az;
    ok = rK11UpperNormalize3(b);
  }

  if (!ok) return;

  for (int i=0;i<3;i++) {
    rK11UpperTilt.baseline[i] = b[i];
    rK11UpperTilt.filtered[i] = b[i];
  }
  rK11UpperTilt.filteredReady = true;
  rK11UpperTilt.baselineReady = true;
  rK11UpperTilt.currentTiltDeg = 0.0f;
  rK11UpperTilt.rawTiltDeg = 0.0f;
  Serial.printf("K11_UPPER_TILT_BASELINE_READY: samples=%u g0=(%.4f,%.4f,%.4f)\n",
                rK11UpperTilt.baselineSamples,
                rK11UpperTilt.baseline[0],rK11UpperTilt.baseline[1],rK11UpperTilt.baseline[2]);
}

// Call once each main loop BEFORE rWt901K11Update(). That ensures the same A
// packets used for the gyro-bias phase are also available for gravity-baseline
// averaging before the gyro core switches to the extension-axis calibration phase.
static void rWt901K11UpperTiltUpdate() {
  if (!rK11UpperTilt.started || !wt901Slots[rK11UpperArmSlot].latest.valid) return;

  // Once the gyro bias phase has ended, freeze the baseline.
  if (rWt901K11Phase() != K11_BIAS_STILL && !rK11UpperTilt.baselineReady) {
    rWt901K11UpperTiltFinalizeBaseline();
  }

  if (wt901Slots[rK11UpperArmSlot].packetCount == rK11UpperTilt.lastPacketCount) return;
  rK11UpperTilt.lastPacketCount = wt901Slots[rK11UpperArmSlot].packetCount;

  float a[3] = {wt901Slots[rK11UpperArmSlot].latest.ax, wt901Slots[rK11UpperArmSlot].latest.ay, wt901Slots[rK11UpperArmSlot].latest.az};
  float norm = rK11UpperNorm3(a);
  rK11UpperTilt.accelNormG = norm;
  if (!isfinite(norm) || norm < 1e-5f) {
    rK11UpperTilt.lastSampleAccepted = false;
    return;
  }

  float unit[3] = {a[0]/norm, a[1]/norm, a[2]/norm};

  if (rWt901K11Phase() == K11_BIAS_STILL && !rK11UpperTilt.baselineReady) {
    if (norm >= K11_UPPER_ACCEL_NORM_MIN_G && norm <= K11_UPPER_ACCEL_NORM_MAX_G) {
      rK11UpperTilt.baselineSum[0] += unit[0];
      rK11UpperTilt.baselineSum[1] += unit[1];
      rK11UpperTilt.baselineSum[2] += unit[2];
      rK11UpperTilt.baselineSamples++;
      rK11UpperTilt.lastSampleAccepted = true;
    } else {
      rK11UpperTilt.lastSampleAccepted = false;
    }
    return;
  }

  if (!rK11UpperTilt.baselineReady) return;

  rK11UpperTilt.rawTiltDeg = rK11UpperAngleDeg(rK11UpperTilt.baseline, unit);

  bool gravityLike = norm >= K11_UPPER_ACCEL_NORM_MIN_G && norm <= K11_UPPER_ACCEL_NORM_MAX_G;
  rK11UpperTilt.lastSampleAccepted = gravityLike;
  if (gravityLike) {
    if (!rK11UpperTilt.filteredReady) {
      for (int i=0;i<3;i++) rK11UpperTilt.filtered[i] = unit[i];
      rK11UpperTilt.filteredReady = true;
    } else {
      for (int i=0;i<3;i++) {
        rK11UpperTilt.filtered[i] = (1.0f-K11_UPPER_LPF_ALPHA)*rK11UpperTilt.filtered[i] + K11_UPPER_LPF_ALPHA*unit[i];
      }
      rK11UpperNormalize3(rK11UpperTilt.filtered);
    }
  }

  if (rK11UpperTilt.filteredReady) {
    rK11UpperTilt.currentTiltDeg = rK11UpperAngleDeg(rK11UpperTilt.baseline, rK11UpperTilt.filtered);
    if (!isfinite(rK11UpperTilt.currentTiltDeg)) rK11UpperTilt.currentTiltDeg = 0.0f;
    if (rK11UpperTilt.currentTiltDeg < 0.0f) rK11UpperTilt.currentTiltDeg = 0.0f;
    if (rK11UpperTilt.currentTiltDeg > 180.0f) rK11UpperTilt.currentTiltDeg = 180.0f;
  }
}



// V4 A2: after a rest/pause the user may lower the arm. When they explicitly
// return to the overhead-flexed start pose and press continue, rebaseline the
// gravity reference to that CURRENT start pose. This keeps the K1 deviation
// metric relative to each group's actual starting upper-arm pose, while the
// gyro extension axis itself remains the session-calibrated axis.
static bool rWt901K11UpperTiltRebaselineFromLatest() {
  if (!rK11UpperTilt.started || !wt901Slots[rK11UpperArmSlot].latest.valid) return false;
  float gyroMag = sqrtf(wt901Slots[rK11UpperArmSlot].latest.gx*wt901Slots[rK11UpperArmSlot].latest.gx +
                         wt901Slots[rK11UpperArmSlot].latest.gy*wt901Slots[rK11UpperArmSlot].latest.gy +
                         wt901Slots[rK11UpperArmSlot].latest.gz*wt901Slots[rK11UpperArmSlot].latest.gz);
  if (!isfinite(gyroMag) || gyroMag > 5.0f) return false;  // explicit continue must be pressed while the start pose is still.

  float a[3] = {wt901Slots[rK11UpperArmSlot].latest.ax, wt901Slots[rK11UpperArmSlot].latest.ay, wt901Slots[rK11UpperArmSlot].latest.az};
  float norm = rK11UpperNorm3(a);
  if (!isfinite(norm) || norm < K11_UPPER_ACCEL_NORM_MIN_G || norm > K11_UPPER_ACCEL_NORM_MAX_G) return false;
  float unit[3] = {a[0]/norm, a[1]/norm, a[2]/norm};
  if (!rK11UpperNormalize3(unit)) return false;

  // Before overwriting the baseline, confirm that the user has actually returned
  // near the PREVIOUS calibrated start pose. This is relative to their own pose,
  // not an anatomical/clinical angle threshold.
  if (rK11UpperTilt.baselineReady) {
    float reacquireDelta = rK11UpperAngleDeg(rK11UpperTilt.baseline, unit);
    if (!isfinite(reacquireDelta) || reacquireDelta > V4_A2_START_POSE_REACQUIRE_MAX_DEG) {
      Serial.printf("V4_A2_REACQUIRE_REJECT: delta=%.1f > %.1f deg; return to prior overhead-flexed start pose.\n",
                    reacquireDelta, V4_A2_START_POSE_REACQUIRE_MAX_DEG);
      return false;
    }
  }

  float oldDelta = rK11UpperTilt.baselineReady ? rK11UpperAngleDeg(rK11UpperTilt.baseline, unit) : 0.0f;
  for (int i=0;i<3;i++) {
    rK11UpperTilt.baseline[i] = unit[i];
    rK11UpperTilt.filtered[i] = unit[i];
    rK11UpperTilt.baselineSum[i] = unit[i];
  }
  rK11UpperTilt.baselineReady = true;
  rK11UpperTilt.filteredReady = true;
  rK11UpperTilt.baselineSamples = 1;
  rK11UpperTilt.currentTiltDeg = 0.0f;
  rK11UpperTilt.rawTiltDeg = 0.0f;
  rK11UpperTilt.accelNormG = norm;
  rK11UpperTilt.lastSampleAccepted = true;
  rK11UpperTilt.lastPacketCount = wt901Slots[rK11UpperArmSlot].packetCount;
  Serial.printf("V4_A2_UPPER_REBASELINE: g0=(%.4f,%.4f,%.4f) norm=%.3f\\n",
                unit[0],unit[1],unit[2],norm);
  return true;
}

static bool rWt901K11UpperTiltBaselineReady() { return rK11UpperTilt.baselineReady; }
static uint16_t rWt901K11UpperTiltBaselineSamples() { return rK11UpperTilt.baselineSamples; }
static float rWt901K11UpperTiltDeg() { return rK11UpperTilt.baselineReady ? rK11UpperTilt.currentTiltDeg : 0.0f; }
static float rWt901K11UpperTiltRawDeg() { return rK11UpperTilt.baselineReady ? rK11UpperTilt.rawTiltDeg : 0.0f; }
static float rWt901K11UpperAccelNormG() { return rK11UpperTilt.accelNormG; }
static bool rWt901K11UpperTiltSampleAccepted() { return rK11UpperTilt.lastSampleAccepted; }
}
