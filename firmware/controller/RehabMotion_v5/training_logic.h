#pragma once
#include <Arduino.h>
#include <math.h>

enum TrainingState {
  IDLE,
  RUNNING,
  PAUSED,
  FINISHED,
  STOPPED
};

enum RepQualityCode {
  REP_QUALITY_NONE = 0,
  REP_QUALITY_BASELINE,
  REP_QUALITY_GOOD,
  REP_QUALITY_ROM_LOW,
  REP_QUALITY_UPPER_ARM_EXCESS,
  REP_QUALITY_PLANE_DEVIATION,
  REP_QUALITY_TORSO_COMPENSATION,
  REP_QUALITY_MOVEMENT_UNSTABLE,
  REP_QUALITY_MULTI_ISSUE
};

// V4.4.20: dominant source of formal sagittal-plane deviation inside the
// meaningful elbow evaluation region. These sources are kept separate so
// reports can explain WHY P was high instead of exposing one opaque max().
// V4.4.30: PLANE_SOURCE_FOREARM_QUAT_PLANE is kept as a compatibility enum name,
// but its value is now the BODY-relative B MOTION-axis plane from B-E vertical gyro.
// Absolute quaternion/yaw and BODY_AZIMUTH are diagnostic only.
enum PlaneDeviationSource {
  PLANE_SOURCE_NONE = 0,
  PLANE_SOURCE_BODY_AZIMUTH,
  PLANE_SOURCE_UPPER_SIDE,
  PLANE_SOURCE_FOREARM_QUAT_PLANE,
  PLANE_SOURCE_HINGE_OFF_AXIS
};

static inline const char *planeDeviationSourceName(PlaneDeviationSource s) {
  switch (s) {
    case PLANE_SOURCE_BODY_AZIMUTH: return "BODY_AZIMUTH";
    case PLANE_SOURCE_UPPER_SIDE: return "UPPER_SIDE";
    case PLANE_SOURCE_FOREARM_QUAT_PLANE: return "FOREARM_MOTION_PLANE";
    case PLANE_SOURCE_HINGE_OFF_AXIS: return "HINGE_OFF_AXIS";
    default: return "NONE";
  }
}

static constexpr bool DEFAULT_UPPER_ARM_QUALITY_ACTIVE = true;
static constexpr bool DEFAULT_PLANE_QUALITY_ACTIVE = true;
static constexpr bool DEFAULT_TORSO_QUALITY_ACTIVE = true;

// V4.4.16 formal-quality thresholds + 3deg ROM target tolerance. These are engineering tuning values used by the training-quality layer; clinical interpretation remains outside the firmware.
// Mild hard-limit crossings still need persistence, but severe biomechanical errors are
// latched immediately inside the meaningful flexion region so a fast bad rep cannot escape
// merely because the 300/500 ms evidence window was too short.
static constexpr float DEFAULT_UPPER_ARM_SOFT_DEG = 25.0f;
static constexpr float DEFAULT_PLANE_SOFT_DEG = 7.0f;
static constexpr float DEFAULT_TORSO_SOFT_DEG = 15.0f;
static constexpr float DEFAULT_UPPER_ARM_MAX_DEG = 35.0f;
static constexpr float DEFAULT_PLANE_MAX_DEG = 9.0f;
static constexpr float DEFAULT_TORSO_MAX_DEG = 30.0f;

// Judge quality only in the meaningful part of the elbow movement. Near the start/end
// posture the forearm's projected direction is numerically sensitive and should not fail a rep.
static constexpr float QUALITY_EVAL_MIN_ANGLE_DEG = 30.0f;
static constexpr unsigned long QUALITY_BAD_CONTINUOUS_MS = 300UL;
static constexpr unsigned long QUALITY_MIN_EVAL_MS = 500UL;
static constexpr float QUALITY_BAD_TIME_RATIO = 0.30f;
// Fast but clearly wrong reps must not escape merely because the active elbow window is short.
// A +5 deg clear exceedance confirmed across >=100 ms, or >=75% bad time across >=150 ms,
// is sufficient. Near-threshold crossings still use the original persistence rule.
static constexpr unsigned long QUALITY_FAST_CONFIRM_MIN_MS = 100UL;
static constexpr unsigned long QUALITY_FAST_RATIO_MIN_MS = 150UL;
static constexpr float QUALITY_FAST_CLEAR_MARGIN_DEG = 5.0f;
static constexpr float QUALITY_FAST_BAD_TIME_RATIO = 0.75f;
static constexpr unsigned long QUALITY_DT_CLAMP_MS = 200UL;
// Severe gate: a single reliable in-motion sample at/above these values is enough.
static constexpr float QUALITY_UPPER_SEVERE_DEG = 50.0f;
static constexpr float QUALITY_PLANE_SEVERE_DEG = 16.0f;
static constexpr float QUALITY_TORSO_SEVERE_DEG = 40.0f;

static constexpr float ROM_TARGET_TOLERANCE_DEG = 3.0f; // sensor tolerance: target 80 passes at >=77

static constexpr float K11_RELAXED_RETURN_MARGIN_DEG = 5.0f;
static constexpr float K11_RELAXED_RETURN_SPEED_MAX_DEG_S = 8.0f;
static constexpr unsigned long K11_RELAXED_RETURN_HOLD_MS = 500UL;

static inline const char *repQualityCodeName(RepQualityCode q) {
  switch (q) {
    case REP_QUALITY_GOOD: return "GOOD";
    case REP_QUALITY_UPPER_ARM_EXCESS: return "UPPER_ARM_COMPENSATION";
    case REP_QUALITY_PLANE_DEVIATION: return "PLANE_DEVIATION";
    case REP_QUALITY_TORSO_COMPENSATION: return "TORSO_COMPENSATION";
    case REP_QUALITY_ROM_LOW: return "ROM_LOW";
    case REP_QUALITY_MOVEMENT_UNSTABLE: return "MOVEMENT_UNSTABLE";
    case REP_QUALITY_MULTI_ISSUE: return "MULTI_ISSUE";
    case REP_QUALITY_BASELINE: return "BASELINE";
    default: return "NONE";
  }
}

struct TrainingData {
  TrainingState state = IDLE;

  int targetAngle = 80;
  int validThreshold = 40;
  int resetThreshold = 20;
  int targetCount = 10;

  int currentCount = 0;
  int completedMotionCount = 0;

  int totalAttemptCount = 0;
  int retryAttemptCount = 0;
  int passedSlotCount = 0;
  int failedSlotCount = 0;
  int recoveredOnRetryCount = 0;
  int finalRomLowSlotCount = 0;
  int finalUpperArmExcessSlotCount = 0;
  int finalPlaneDeviationSlotCount = 0;
  int finalTorsoCompensationSlotCount = 0;
  int finalMultiIssueSlotCount = 0;
  int romLowAttemptCount = 0;
  int upperArmExcessAttemptCount = 0;
  int planeDeviationAttemptCount = 0;
  int torsoCompensationAttemptCount = 0;
  int multiIssueAttemptCount = 0;
  bool retryPending = false;
  bool lastAttemptWasRetry = false;
  bool lastAttemptPassed = false;
  bool lastSlotCompleted = false;
  bool lastSlotPassed = false;
  bool lastSlotForcedFail = false;

  float currentAngle = 0.0f;
  float maxAngle = 0.0f;
  float currentRepMaxAngle = 0.0f;
  float lastCompletedRepPeakAngle = 0.0f;

  bool readyForRep = false;
  bool reachedHighAngle = false;
  unsigned long relaxedReturnHoldStartMs = 0;

  // Compatibility fields retained so existing UI/CSV code remains buildable.
  static constexpr int BASELINE_REPS_REQUIRED = 0;
  int baselineRepCount = 0;
  bool baselineReady = true;
  float baselineUpperMean = 0.0f;
  float baselineUpperPeak = 0.0f;
  float baselineDurationMs = 0.0f;
  float baselineSmoothness = 0.0f;
  float baselineUpperMeanSamples[1] = {0.0f};
  float baselineUpperPeakSamples[1] = {0.0f};
  float baselineDurationSamples[1] = {0.0f};
  float baselineSmoothnessSamples[1] = {0.0f};
  float upperMeanLimit = 0.0f;
  float upperPeakLimit = 0.0f;
  float fastDurationLimitMs = 0.0f;
  float smoothnessLimit = 0.0f;

  float currentRepMaxUpperArmDev = 0.0f;
  float currentRepUpperArmDevSum = 0.0f;
  uint32_t currentRepUpperArmSamples = 0;
  float currentRepMaxPlaneDev = 0.0f;
  float currentRepMaxTorsoDev = 0.0f;
  // Maxima only while elbow is in the meaningful flexion region (>=30 deg).
  float currentRepEvalMaxUpperArmDev = 0.0f;
  float currentRepEvalMaxPlaneDev = 0.0f;
  float currentRepEvalMaxTorsoDev = 0.0f;
  // Formal P component maxima, ONLY while elbow >= QUALITY_EVAL_MIN_ANGLE_DEG.
  float currentRepEvalMaxPlaneBodyAzimuth = 0.0f;
  float currentRepEvalMaxPlaneUpperSide = 0.0f;
  float currentRepEvalMaxPlaneForearmQuat = 0.0f;
  float currentRepEvalMaxPlaneHinge = 0.0f;
  bool currentRepUpperSevere = false;
  bool currentRepPlaneSevere = false;
  bool currentRepTorsoSevere = false;

  // V4.4.2 robust evidence: quality failures use sustained time above the hard limit,
  // not the single largest frame. Only samples with joint angle >= QUALITY_EVAL_MIN_ANGLE_DEG
  // contribute to these timers.
  unsigned long currentRepQualityEvalMs = 0;
  unsigned long currentRepQualityLastSampleMs = 0;
  unsigned long currentRepUpperBadMs = 0;
  unsigned long currentRepPlaneBadMs = 0;
  unsigned long currentRepTorsoBadMs = 0;
  unsigned long currentRepUpperBadRunMs = 0;
  unsigned long currentRepPlaneBadRunMs = 0;
  unsigned long currentRepTorsoBadRunMs = 0;
  unsigned long currentRepUpperMaxBadRunMs = 0;
  unsigned long currentRepPlaneMaxBadRunMs = 0;
  unsigned long currentRepTorsoMaxBadRunMs = 0;
  unsigned long currentRepUpperSoftMs = 0;
  unsigned long currentRepPlaneSoftMs = 0;
  unsigned long currentRepTorsoSoftMs = 0;

  unsigned long currentRepStartMs = 0;
  float currentRepPrevSpeed = 0.0f;
  bool currentRepHasPrevSpeed = false;
  float currentRepSpeedChangeSum = 0.0f;
  uint32_t currentRepSpeedChangeSamples = 0;
  float currentRepMaxSpeed = 0.0f;

  float lastCompletedRepMaxUpperArmDev = 0.0f;
  float lastCompletedRepMeanUpperArmDev = 0.0f;
  float lastCompletedRepMaxPlaneDev = 0.0f;
  float lastCompletedRepMaxTorsoDev = 0.0f;
  float lastCompletedRepEvalMaxUpperArmDev = 0.0f;
  float lastCompletedRepEvalMaxPlaneDev = 0.0f;
  float lastCompletedRepEvalMaxTorsoDev = 0.0f;
  float lastCompletedRepEvalMaxPlaneBodyAzimuth = 0.0f;
  float lastCompletedRepEvalMaxPlaneUpperSide = 0.0f;
  float lastCompletedRepEvalMaxPlaneForearmQuat = 0.0f;
  float lastCompletedRepEvalMaxPlaneHinge = 0.0f;
  PlaneDeviationSource lastPlaneSource = PLANE_SOURCE_NONE;
  bool lastUpperSevere = false;
  bool lastPlaneSevere = false;
  bool lastTorsoSevere = false;
  unsigned long lastCompletedRepQualityEvalMs = 0;
  float lastCompletedRepUpperBadPct = 0.0f;
  float lastCompletedRepPlaneBadPct = 0.0f;
  float lastCompletedRepTorsoBadPct = 0.0f;
  unsigned long lastCompletedRepUpperMaxBadRunMs = 0;
  unsigned long lastCompletedRepPlaneMaxBadRunMs = 0;
  unsigned long lastCompletedRepTorsoMaxBadRunMs = 0;
  bool lastUpperArmSoftWarning = false;
  bool lastPlaneSoftWarning = false;
  bool lastTorsoSoftWarning = false;
  unsigned long lastCompletedRepDurationMs = 0;
  float lastCompletedRepSmoothness = 0.0f;
  float lastCompletedRepMaxSpeed = 0.0f;

  bool lastRomLow = false;
  bool lastUpperArmExcess = false;
  bool lastPlaneDeviation = false;
  bool lastTorsoCompensation = false;
  bool lastMovementUnstable = false;
  RepQualityCode lastRepQuality = REP_QUALITY_NONE;

  int goodRepCount = 0;
  int issueRepCount = 0;
};

class TrainingLogic {
public:
  TrainingData data;
  bool upperArmQualityActive = DEFAULT_UPPER_ARM_QUALITY_ACTIVE;
  bool planeQualityActive = DEFAULT_PLANE_QUALITY_ACTIVE;
  bool torsoQualityActive = DEFAULT_TORSO_QUALITY_ACTIVE;
  float upperArmQualityThresholdDeg = DEFAULT_UPPER_ARM_MAX_DEG;
  float planeQualityThresholdDeg = DEFAULT_PLANE_MAX_DEG;
  float torsoQualityThresholdDeg = DEFAULT_TORSO_MAX_DEG;

  // Backward-compatible configuration.
  void configureQuality(bool active, float thresholdDeg) {
    upperArmQualityActive = active;
    upperArmQualityThresholdDeg = thresholdDeg;
  }

  void configureMotionQuality(bool upperActive, float upperDeg,
                              bool planeActive, float planeDeg,
                              bool torsoActive, float torsoDeg) {
    upperArmQualityActive = upperActive;
    upperArmQualityThresholdDeg = upperDeg;
    planeQualityActive = planeActive;
    planeQualityThresholdDeg = planeDeg;
    torsoQualityActive = torsoActive;
    torsoQualityThresholdDeg = torsoDeg;
  }

  void startOrPause() {
    if (data.state == IDLE || data.state == STOPPED || data.state == FINISHED) {
      reset();
      data.state = RUNNING;
    } else if (data.state == RUNNING) {
      data.state = PAUSED;
    } else if (data.state == PAUSED) {
      data.state = RUNNING;
    }
  }

  void stop() { data.state = STOPPED; }
  void calibrate() {}

  // Formal attempt quality now keeps independent evidence for:
  //   ROM, upper-arm compensation, sagittal-plane deviation, torso compensation.
  // If more than one failure exists, the final code is MULTI_ISSUE rather than hiding
  // all secondary causes behind ROM_LOW.
  void update(float angle,
              float upperArmDev = 0.0f,
              float planeDev = 0.0f,
              float torsoDev = 0.0f,
              float speedDegS = 0.0f,
              unsigned long nowMs = 0,
              float planeBodyAzimuth = 0.0f,
              float planeUpperSide = 0.0f,
              float planeForearmQuat = 0.0f,
              float planeHinge = 0.0f) {
    if (nowMs == 0) nowMs = millis();
    if (!isfinite(angle)) return;
    if (angle < 0.0f) angle = 0.0f;
    sanitizeMetric(upperArmDev);
    sanitizeMetric(planeDev);
    sanitizeMetric(torsoDev);
    sanitizeMetric(planeBodyAzimuth);
    sanitizeMetric(planeUpperSide);
    sanitizeMetric(planeForearmQuat);
    sanitizeMetric(planeHinge);

    data.currentAngle = angle;
    if (data.state != RUNNING) return;

    if (angle > data.maxAngle) data.maxAngle = angle;

    if (!data.readyForRep && !data.reachedHighAngle) {
      if (angle <= data.resetThreshold) {
        data.readyForRep = true;
        resetCurrentAttempt(angle);
      }
      return;
    }

    if (data.readyForRep && !data.reachedHighAngle && angle <= data.resetThreshold) {
      resetCurrentAttempt(angle);
      return;
    }

    if (data.currentRepStartMs == 0) {
      data.currentRepStartMs = nowMs;
      data.currentRepMaxUpperArmDev = 0.0f;
      data.currentRepUpperArmDevSum = 0.0f;
      data.currentRepUpperArmSamples = 0;
      data.currentRepMaxPlaneDev = 0.0f;
      data.currentRepMaxTorsoDev = 0.0f;
      data.currentRepEvalMaxUpperArmDev = 0.0f;
      data.currentRepEvalMaxPlaneDev = 0.0f;
      data.currentRepEvalMaxTorsoDev = 0.0f;
      data.currentRepEvalMaxPlaneBodyAzimuth = 0.0f;
      data.currentRepEvalMaxPlaneUpperSide = 0.0f;
      data.currentRepEvalMaxPlaneForearmQuat = 0.0f;
      data.currentRepEvalMaxPlaneHinge = 0.0f;
      data.currentRepUpperSevere = data.currentRepPlaneSevere = data.currentRepTorsoSevere = false;
    }
    if (angle > data.currentRepMaxAngle) data.currentRepMaxAngle = angle;
    if (upperArmDev > data.currentRepMaxUpperArmDev) data.currentRepMaxUpperArmDev = upperArmDev;
    if (planeDev > data.currentRepMaxPlaneDev) data.currentRepMaxPlaneDev = planeDev;
    if (torsoDev > data.currentRepMaxTorsoDev) data.currentRepMaxTorsoDev = torsoDev;
    data.currentRepUpperArmDevSum += upperArmDev;
    data.currentRepUpperArmSamples++;
    if (fabsf(speedDegS) > data.currentRepMaxSpeed) data.currentRepMaxSpeed = fabsf(speedDegS);

    // Robust quality evidence. Ignore the near-zero joint region, where a small sensor/yaw
    // disturbance produces a large apparent plane azimuth change. During the useful motion
    // region, a hard-limit excursion must persist or occupy a meaningful fraction of time.
    if (angle >= QUALITY_EVAL_MIN_ANGLE_DEG) {
      if (upperArmDev > data.currentRepEvalMaxUpperArmDev) data.currentRepEvalMaxUpperArmDev = upperArmDev;
      if (planeDev > data.currentRepEvalMaxPlaneDev) data.currentRepEvalMaxPlaneDev = planeDev;
      if (torsoDev > data.currentRepEvalMaxTorsoDev) data.currentRepEvalMaxTorsoDev = torsoDev;
      if (planeBodyAzimuth > data.currentRepEvalMaxPlaneBodyAzimuth) data.currentRepEvalMaxPlaneBodyAzimuth = planeBodyAzimuth;
      if (planeUpperSide > data.currentRepEvalMaxPlaneUpperSide) data.currentRepEvalMaxPlaneUpperSide = planeUpperSide;
      if (planeForearmQuat > data.currentRepEvalMaxPlaneForearmQuat) data.currentRepEvalMaxPlaneForearmQuat = planeForearmQuat;
      if (planeHinge > data.currentRepEvalMaxPlaneHinge) data.currentRepEvalMaxPlaneHinge = planeHinge;
      if (upperArmQualityActive && upperArmDev >= QUALITY_UPPER_SEVERE_DEG) data.currentRepUpperSevere = true;
      if (planeQualityActive && planeDev >= QUALITY_PLANE_SEVERE_DEG) data.currentRepPlaneSevere = true;
      if (torsoQualityActive && torsoDev >= QUALITY_TORSO_SEVERE_DEG) data.currentRepTorsoSevere = true;
      unsigned long dt = 0;
      if (data.currentRepQualityLastSampleMs != 0 && nowMs >= data.currentRepQualityLastSampleMs) {
        dt = nowMs - data.currentRepQualityLastSampleMs;
        if (dt > QUALITY_DT_CLAMP_MS) dt = QUALITY_DT_CLAMP_MS;
      }
      data.currentRepQualityLastSampleMs = nowMs;
      data.currentRepQualityEvalMs += dt;
      updateQualityEvidence(upperArmDev, upperArmQualityThresholdDeg, DEFAULT_UPPER_ARM_SOFT_DEG, dt,
        data.currentRepUpperBadMs, data.currentRepUpperBadRunMs, data.currentRepUpperMaxBadRunMs, data.currentRepUpperSoftMs);
      updateQualityEvidence(planeDev, planeQualityThresholdDeg, DEFAULT_PLANE_SOFT_DEG, dt,
        data.currentRepPlaneBadMs, data.currentRepPlaneBadRunMs, data.currentRepPlaneMaxBadRunMs, data.currentRepPlaneSoftMs);
      updateQualityEvidence(torsoDev, torsoQualityThresholdDeg, DEFAULT_TORSO_SOFT_DEG, dt,
        data.currentRepTorsoBadMs, data.currentRepTorsoBadRunMs, data.currentRepTorsoMaxBadRunMs, data.currentRepTorsoSoftMs);
    } else {
      data.currentRepQualityLastSampleMs = 0;
      data.currentRepUpperBadRunMs = 0;
      data.currentRepPlaneBadRunMs = 0;
      data.currentRepTorsoBadRunMs = 0;
    }

    if (data.readyForRep && !data.reachedHighAngle && angle >= data.validThreshold) {
      data.reachedHighAngle = true;
      data.relaxedReturnHoldStartMs = 0;
    }

    bool strictReturn = data.readyForRep && data.reachedHighAngle && angle <= data.resetThreshold;
    bool inRelaxedReturnBand = data.readyForRep && data.reachedHighAngle &&
      angle > data.resetThreshold && angle <= (data.resetThreshold + K11_RELAXED_RETURN_MARGIN_DEG) &&
      fabsf(speedDegS) <= K11_RELAXED_RETURN_SPEED_MAX_DEG_S;

    bool relaxedReturn = false;
    if (inRelaxedReturnBand) {
      if (data.relaxedReturnHoldStartMs == 0) data.relaxedReturnHoldStartMs = nowMs;
      relaxedReturn = (nowMs - data.relaxedReturnHoldStartMs) >= K11_RELAXED_RETURN_HOLD_MS;
    } else if (!strictReturn) {
      data.relaxedReturnHoldStartMs = 0;
    }

    if (!(strictReturn || relaxedReturn)) return;

    finalizeAttempt(nowMs);

    data.readyForRep = false;
    data.reachedHighAngle = false;
    data.relaxedReturnHoldStartMs = 0;
    resetCurrentAttempt(angle);

    if (data.currentCount >= data.targetCount) data.state = FINISHED;
  }

  void armForNextRepFromCurrentAngle(float angle, float upperArmDev = 0.0f) {
    (void)upperArmDev;
    if (!isfinite(angle)) angle = 0.0f;
    if (angle < 0.0f) angle = 0.0f;
    data.currentAngle = angle;
    data.reachedHighAngle = false;
    data.readyForRep = (angle <= data.resetThreshold);
    resetCurrentAttempt(angle);
  }

  void reset() { data = TrainingData(); }
  void resetForNextGroupKeepBaseline() { data = TrainingData(); data.baselineReady = true; }

private:
  static void sanitizeMetric(float &v) {
    if (!isfinite(v) || v < 0.0f) v = 0.0f;
    if (v > 180.0f) v = 180.0f;
  }

  static void updateQualityEvidence(float metric, float hardDeg, float softDeg, unsigned long dt,
                                    unsigned long &badMs, unsigned long &badRunMs,
                                    unsigned long &maxBadRunMs, unsigned long &softMs) {
    if (dt == 0) return;
    if (metric >= softDeg) softMs += dt;
    if (metric >= hardDeg) {
      badMs += dt;
      badRunMs += dt;
      if (badRunMs > maxBadRunMs) maxBadRunMs = badRunMs;
    } else {
      badRunMs = 0;
    }
  }

  static float badPct(unsigned long badMs, unsigned long evalMs) {
    return evalMs > 0 ? (100.0f * (float)badMs / (float)evalMs) : 0.0f;
  }

  static PlaneDeviationSource dominantPlaneSource(float bodyAz, float upperSide, float foreQuat, float hinge) {
    // V4.4.31F: upperSide slot carries the ONLY formal per-attempt A-E metric.
    (void)bodyAz; (void)foreQuat; (void)hinge;
    return upperSide > 0.05f ? PLANE_SOURCE_UPPER_SIDE : PLANE_SOURCE_NONE;
  }

  static bool confirmedIssue(unsigned long badMs, unsigned long maxRunMs, unsigned long evalMs,
                             float evalMax, float hardDeg) {
    if (maxRunMs >= QUALITY_BAD_CONTINUOUS_MS) return true;
    if (evalMs >= QUALITY_FAST_CONFIRM_MIN_MS && evalMax >= hardDeg + QUALITY_FAST_CLEAR_MARGIN_DEG) return true;
    if (evalMs >= QUALITY_FAST_RATIO_MIN_MS && ((float)badMs / (float)evalMs) >= QUALITY_FAST_BAD_TIME_RATIO) return true;
    if (evalMs < QUALITY_MIN_EVAL_MS) return false;
    return ((float)badMs / (float)evalMs) >= QUALITY_BAD_TIME_RATIO;
  }

  // V4.4.21: plane P is intrinsically noisier than ROM/U/T because it contains a
  // learned relative hinge-axis term.  A single 45-degree sample must NOT condemn an
  // otherwise correct repetition.  Require sustained/corroborated evidence across the
  // completed movement.  This preserves real persistent out-of-plane motion while
  // rejecting short IMU/gyro spikes.
  static bool confirmedPlaneIssue(unsigned long badMs, unsigned long maxRunMs,
                                  unsigned long evalMs, float evalMax, float hardDeg) {
    if (evalMs == 0) return false;
    const float ratio = (float)badMs / (float)evalMs;
    // Per-attempt A-E delta is smooth/cumulative, so real side motion stays high.
    if (maxRunMs >= 140UL) return true;
    if (evalMs >= 180UL && evalMax >= hardDeg + 2.0f && ratio >= 0.30f) return true;
    if (evalMs >= 350UL && ratio >= 0.25f) return true;
    return false;
  }

  void resetCurrentAttempt(float angle) {
    data.currentRepMaxAngle = angle;
    data.currentRepMaxUpperArmDev = 0.0f;
    data.currentRepUpperArmDevSum = 0.0f;
    data.currentRepUpperArmSamples = 0;
    data.currentRepMaxPlaneDev = 0.0f;
    data.currentRepMaxTorsoDev = 0.0f;
    data.currentRepEvalMaxUpperArmDev = 0.0f;
    data.currentRepEvalMaxPlaneDev = 0.0f;
    data.currentRepEvalMaxTorsoDev = 0.0f;
    data.currentRepEvalMaxPlaneBodyAzimuth = 0.0f;
    data.currentRepEvalMaxPlaneUpperSide = 0.0f;
    data.currentRepEvalMaxPlaneForearmQuat = 0.0f;
    data.currentRepEvalMaxPlaneHinge = 0.0f;
    data.currentRepUpperSevere = data.currentRepPlaneSevere = data.currentRepTorsoSevere = false;
    data.currentRepQualityEvalMs = 0;
    data.currentRepQualityLastSampleMs = 0;
    data.currentRepUpperBadMs = data.currentRepPlaneBadMs = data.currentRepTorsoBadMs = 0;
    data.currentRepUpperBadRunMs = data.currentRepPlaneBadRunMs = data.currentRepTorsoBadRunMs = 0;
    data.currentRepUpperMaxBadRunMs = data.currentRepPlaneMaxBadRunMs = data.currentRepTorsoMaxBadRunMs = 0;
    data.currentRepUpperSoftMs = data.currentRepPlaneSoftMs = data.currentRepTorsoSoftMs = 0;
    data.currentRepStartMs = 0;
    data.currentRepMaxSpeed = 0.0f;
    data.relaxedReturnHoldStartMs = 0;
  }

  void finalizeAttempt(unsigned long nowMs) {
    data.lastCompletedRepPeakAngle = data.currentRepMaxAngle;
    data.lastCompletedRepMaxUpperArmDev = data.currentRepMaxUpperArmDev;
    data.lastCompletedRepMeanUpperArmDev = data.currentRepUpperArmSamples > 0
      ? data.currentRepUpperArmDevSum / (float)data.currentRepUpperArmSamples : 0.0f;
    data.lastCompletedRepMaxPlaneDev = data.currentRepMaxPlaneDev;
    data.lastCompletedRepMaxTorsoDev = data.currentRepMaxTorsoDev;
    data.lastCompletedRepEvalMaxUpperArmDev = data.currentRepEvalMaxUpperArmDev;
    data.lastCompletedRepEvalMaxPlaneDev = data.currentRepEvalMaxPlaneDev;
    data.lastCompletedRepEvalMaxTorsoDev = data.currentRepEvalMaxTorsoDev;
    data.lastCompletedRepEvalMaxPlaneBodyAzimuth = data.currentRepEvalMaxPlaneBodyAzimuth;
    data.lastCompletedRepEvalMaxPlaneUpperSide = data.currentRepEvalMaxPlaneUpperSide;
    data.lastCompletedRepEvalMaxPlaneForearmQuat = data.currentRepEvalMaxPlaneForearmQuat;
    data.lastCompletedRepEvalMaxPlaneHinge = data.currentRepEvalMaxPlaneHinge;
    data.lastPlaneSource = dominantPlaneSource(
      data.lastCompletedRepEvalMaxPlaneBodyAzimuth,
      data.lastCompletedRepEvalMaxPlaneUpperSide,
      data.lastCompletedRepEvalMaxPlaneForearmQuat,
      data.lastCompletedRepEvalMaxPlaneHinge);
    data.lastUpperSevere = data.currentRepUpperSevere;
    data.lastPlaneSevere = data.currentRepPlaneSevere;
    data.lastTorsoSevere = data.currentRepTorsoSevere;
    data.lastCompletedRepQualityEvalMs = data.currentRepQualityEvalMs;
    data.lastCompletedRepUpperBadPct = badPct(data.currentRepUpperBadMs, data.currentRepQualityEvalMs);
    data.lastCompletedRepPlaneBadPct = badPct(data.currentRepPlaneBadMs, data.currentRepQualityEvalMs);
    data.lastCompletedRepTorsoBadPct = badPct(data.currentRepTorsoBadMs, data.currentRepQualityEvalMs);
    data.lastCompletedRepUpperMaxBadRunMs = data.currentRepUpperMaxBadRunMs;
    data.lastCompletedRepPlaneMaxBadRunMs = data.currentRepPlaneMaxBadRunMs;
    data.lastCompletedRepTorsoMaxBadRunMs = data.currentRepTorsoMaxBadRunMs;
    data.lastUpperArmSoftWarning = data.currentRepQualityEvalMs > 0 && data.currentRepUpperSoftMs > 0;
    data.lastPlaneSoftWarning = data.currentRepQualityEvalMs > 0 && data.currentRepPlaneSoftMs > 0;
    data.lastTorsoSoftWarning = data.currentRepQualityEvalMs > 0 && data.currentRepTorsoSoftMs > 0;
    data.lastCompletedRepDurationMs = data.currentRepStartMs > 0 ? (nowMs - data.currentRepStartMs) : 0;
    data.lastCompletedRepMaxSpeed = data.currentRepMaxSpeed;

    data.lastRomLow = (data.lastCompletedRepPeakAngle + ROM_TARGET_TOLERANCE_DEG) < (float)data.targetAngle;
    data.lastUpperArmExcess = upperArmQualityActive && (data.currentRepUpperSevere || confirmedIssue(
      data.currentRepUpperBadMs, data.currentRepUpperMaxBadRunMs, data.currentRepQualityEvalMs,
      data.currentRepEvalMaxUpperArmDev, upperArmQualityThresholdDeg));
    // V4.4.21: no single-frame severe latch for plane.  Decide P only from the
    // finished repetition's sustained evidence window.
    data.lastPlaneDeviation = planeQualityActive && confirmedPlaneIssue(
      data.currentRepPlaneBadMs, data.currentRepPlaneMaxBadRunMs, data.currentRepQualityEvalMs,
      data.currentRepEvalMaxPlaneDev, planeQualityThresholdDeg);
    data.lastTorsoCompensation = torsoQualityActive && (data.currentRepTorsoSevere || confirmedIssue(
      data.currentRepTorsoBadMs, data.currentRepTorsoMaxBadRunMs, data.currentRepQualityEvalMs,
      data.currentRepEvalMaxTorsoDev, torsoQualityThresholdDeg));
    data.lastMovementUnstable = false;

    int issueKinds = 0;
    if (data.lastRomLow) issueKinds++;
    if (data.lastUpperArmExcess) issueKinds++;
    if (data.lastPlaneDeviation) issueKinds++;
    if (data.lastTorsoCompensation) issueKinds++;

    if (issueKinds == 0) data.lastRepQuality = REP_QUALITY_GOOD;
    else if (issueKinds > 1) data.lastRepQuality = REP_QUALITY_MULTI_ISSUE;
    else if (data.lastRomLow) data.lastRepQuality = REP_QUALITY_ROM_LOW;
    else if (data.lastPlaneDeviation) data.lastRepQuality = REP_QUALITY_PLANE_DEVIATION;
    else if (data.lastUpperArmExcess) data.lastRepQuality = REP_QUALITY_UPPER_ARM_EXCESS;
    else if (data.lastTorsoCompensation) data.lastRepQuality = REP_QUALITY_TORSO_COMPENSATION;
    else data.lastRepQuality = REP_QUALITY_MOVEMENT_UNSTABLE;

    bool passed = data.lastRepQuality == REP_QUALITY_GOOD;
    bool wasRetry = data.retryPending;

    data.lastAttemptWasRetry = wasRetry;
    data.lastAttemptPassed = passed;
    data.lastSlotCompleted = false;
    data.lastSlotPassed = false;
    data.lastSlotForcedFail = false;

    data.totalAttemptCount++;
    data.completedMotionCount++;
    if (wasRetry) data.retryAttemptCount++;

    if (passed) data.goodRepCount++;
    else {
      data.issueRepCount++;
      if (data.lastRomLow) data.romLowAttemptCount++;
      if (data.lastUpperArmExcess) data.upperArmExcessAttemptCount++;
      if (data.lastPlaneDeviation) data.planeDeviationAttemptCount++;
      if (data.lastTorsoCompensation) data.torsoCompensationAttemptCount++;
      if (data.lastRepQuality == REP_QUALITY_MULTI_ISSUE) data.multiIssueAttemptCount++;
    }

    if (passed) {
      data.currentCount++;
      data.passedSlotCount++;
      data.lastSlotCompleted = true;
      data.lastSlotPassed = true;
      if (wasRetry) data.recoveredOnRetryCount++;
      data.retryPending = false;
    } else if (!wasRetry) {
      data.retryPending = true;
    } else {
      data.currentCount++;
      data.failedSlotCount++;
      data.lastSlotCompleted = true;
      data.lastSlotPassed = false;
      data.lastSlotForcedFail = true;
      switch(data.lastRepQuality) {
        case REP_QUALITY_ROM_LOW: data.finalRomLowSlotCount++; break;
        case REP_QUALITY_UPPER_ARM_EXCESS: data.finalUpperArmExcessSlotCount++; break;
        case REP_QUALITY_PLANE_DEVIATION: data.finalPlaneDeviationSlotCount++; break;
        case REP_QUALITY_TORSO_COMPENSATION: data.finalTorsoCompensationSlotCount++; break;
        default: data.finalMultiIssueSlotCount++; break;
      }
      data.retryPending = false;
    }
  }
};
