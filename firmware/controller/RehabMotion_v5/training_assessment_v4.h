#pragma once
#include <Arduino.h>
#include <math.h>
#include "training_logic.h"

// V4.4 whole-session assessment.
// Planned-slot outcome is primary; attempts/retries are tracked separately.
// Failure causes are no longer forced into a ROM-first hierarchy.
struct V4SessionAssessment {
  int completedSlots = 0;
  int passedSlots = 0;
  int failedSlots = 0;

  int totalAttempts = 0;
  int retryAttempts = 0;
  int recoveredOnRetry = 0;

  int finalRomLowSlots = 0;
  int finalUpperArmExcessSlots = 0;
  int finalPlaneDeviationSlots = 0;
  int finalTorsoCompensationSlots = 0;
  int finalMultiIssueSlots = 0;

  int romLowAttempts = 0;
  int upperArmExcessAttempts = 0;
  int planeDeviationAttempts = 0;
  int torsoCompensationAttempts = 0;
  int multiIssueAttempts = 0;

  float passRomSum = 0.0f;
  float passRomSqSum = 0.0f;
  float passRomMin = 0.0f;
  float passRomMax = 0.0f;

  float attemptMeanSpeedSum = 0.0f;
  float attemptDurationSumMs = 0.0f;
  float attemptUpperArmMaxSum = 0.0f;
  float attemptPlaneMaxSum = 0.0f;
  float attemptTorsoMaxSum = 0.0f;

  unsigned long sessionStartMs = 0;

  void reset(unsigned long nowMs = 0) {
    completedSlots = passedSlots = failedSlots = 0;
    totalAttempts = retryAttempts = recoveredOnRetry = 0;
    finalRomLowSlots = finalUpperArmExcessSlots = finalPlaneDeviationSlots = 0;
    finalTorsoCompensationSlots = finalMultiIssueSlots = 0;
    romLowAttempts = upperArmExcessAttempts = planeDeviationAttempts = 0;
    torsoCompensationAttempts = multiIssueAttempts = 0;
    passRomSum = passRomSqSum = passRomMin = passRomMax = 0.0f;
    attemptMeanSpeedSum = attemptDurationSumMs = 0.0f;
    attemptUpperArmMaxSum = attemptPlaneMaxSum = attemptTorsoMaxSum = 0.0f;
    sessionStartMs = nowMs ? nowMs : millis();
  }

  void recordAttempt(const TrainingData &d) {
    float rom = d.lastCompletedRepPeakAngle;
    if (!isfinite(rom) || rom < 0.0f) rom = 0.0f;

    totalAttempts++;
    if (d.lastAttemptWasRetry) retryAttempts++;
    if (d.lastRomLow) romLowAttempts++;
    if (d.lastUpperArmExcess) upperArmExcessAttempts++;
    if (d.lastPlaneDeviation) planeDeviationAttempts++;
    if (d.lastTorsoCompensation) torsoCompensationAttempts++;
    if (d.lastRepQuality == REP_QUALITY_MULTI_ISSUE) multiIssueAttempts++;

    float durationMs = (float)d.lastCompletedRepDurationMs;
    if (!isfinite(durationMs) || durationMs < 0.0f) durationMs = 0.0f;
    attemptDurationSumMs += durationMs;

    float meanSpeed = 0.0f;
    if (durationMs >= 100.0f && rom > 0.0f) {
      meanSpeed = (2.0f * rom) / (durationMs / 1000.0f);
      if (d.lastCompletedRepMaxSpeed > 0.0f && meanSpeed > d.lastCompletedRepMaxSpeed) meanSpeed = d.lastCompletedRepMaxSpeed;
    }
    if (isfinite(meanSpeed) && meanSpeed >= 0.0f) attemptMeanSpeedSum += meanSpeed;

    if (isfinite(d.lastCompletedRepEvalMaxUpperArmDev) && d.lastCompletedRepEvalMaxUpperArmDev >= 0.0f) attemptUpperArmMaxSum += d.lastCompletedRepEvalMaxUpperArmDev;
    if (isfinite(d.lastCompletedRepEvalMaxPlaneDev) && d.lastCompletedRepEvalMaxPlaneDev >= 0.0f) attemptPlaneMaxSum += d.lastCompletedRepEvalMaxPlaneDev;
    if (isfinite(d.lastCompletedRepEvalMaxTorsoDev) && d.lastCompletedRepEvalMaxTorsoDev >= 0.0f) attemptTorsoMaxSum += d.lastCompletedRepEvalMaxTorsoDev;

    if (!d.lastSlotCompleted) return;

    completedSlots++;
    if (d.lastSlotPassed) {
      passedSlots++;
      if (d.lastAttemptWasRetry) recoveredOnRetry++;
      passRomSum += rom;
      passRomSqSum += rom * rom;
      if (passedSlots == 1 || rom < passRomMin) passRomMin = rom;
      if (passedSlots == 1 || rom > passRomMax) passRomMax = rom;
    } else {
      failedSlots++;
      switch(d.lastRepQuality) {
        case REP_QUALITY_ROM_LOW: finalRomLowSlots++; break;
        case REP_QUALITY_UPPER_ARM_EXCESS: finalUpperArmExcessSlots++; break;
        case REP_QUALITY_PLANE_DEVIATION: finalPlaneDeviationSlots++; break;
        case REP_QUALITY_TORSO_COMPENSATION: finalTorsoCompensationSlots++; break;
        default: finalMultiIssueSlots++; break;
      }
    }
  }

  float completionRatePct(int targetTotalSlots) const {
    if (targetTotalSlots <= 0) return 0.0f;
    float p = 100.0f * (float)completedSlots / (float)targetTotalSlots;
    if (p < 0.0f) p = 0.0f; if (p > 100.0f) p = 100.0f; return p;
  }
  float passRatePct() const { return completedSlots > 0 ? 100.0f*(float)passedSlots/(float)completedSlots : 0.0f; }
  float passRomAvgDeg() const { return passedSlots > 0 ? passRomSum/(float)passedSlots : 0.0f; }
  float passRomSdDeg() const {
    if (passedSlots <= 1) return 0.0f;
    float mean=passRomAvgDeg(); float variance=passRomSqSum/(float)passedSlots-mean*mean;
    if(variance<0.0f)variance=0.0f; return sqrtf(variance);
  }
  float avgAttemptMeanSpeedDegS() const { return totalAttempts>0?attemptMeanSpeedSum/(float)totalAttempts:0.0f; }
  float avgAttemptDurationSec() const { return totalAttempts>0?attemptDurationSumMs/(1000.0f*(float)totalAttempts):0.0f; }
  float avgAttemptUpperArmMaxDeg() const { return totalAttempts>0?attemptUpperArmMaxSum/(float)totalAttempts:0.0f; }
  float avgAttemptPlaneMaxDeg() const { return totalAttempts>0?attemptPlaneMaxSum/(float)totalAttempts:0.0f; }
  float avgAttemptTorsoMaxDeg() const { return totalAttempts>0?attemptTorsoMaxSum/(float)totalAttempts:0.0f; }
  int firstTryPassedSlots() const { int v=completedSlots-retryAttempts; return v>0?v:0; }
  float firstTryPassRatePct() const { return completedSlots>0?100.0f*(float)firstTryPassedSlots()/(float)completedSlots:0.0f; }
  float retryRecoveryRatePct() const { return retryAttempts>0?100.0f*(float)recoveredOnRetry/(float)retryAttempts:0.0f; }
  bool hasPassRomStats() const { return passedSlots>0; }

  bool accountingOk() const {
    if(completedSlots!=passedSlots+failedSlots)return false;
    if(failedSlots!=finalRomLowSlots+finalUpperArmExcessSlots+finalPlaneDeviationSlots+finalTorsoCompensationSlots+finalMultiIssueSlots)return false;
    if(retryAttempts<0||retryAttempts>totalAttempts)return false;
    if(recoveredOnRetry<0||recoveredOnRetry>retryAttempts||recoveredOnRetry>passedSlots)return false;
    int openFirstFailure=totalAttempts-completedSlots-retryAttempts;
    return openFirstFailure>=0&&openFirstFailure<=1;
  }

  unsigned long elapsedSec(unsigned long nowMs=0) const {
    if(sessionStartMs==0)return 0;if(nowMs==0)nowMs=millis();return(nowMs-sessionStartMs)/1000UL;
  }

  const char *summaryCode(int targetTotalSlots) const {
    if(completedSlots<=0)return "NO_COMPLETED_SLOTS";
    if(completedSlots<targetTotalSlots)return "SESSION_INCOMPLETE";
    if(failedSlots<=0)return "ALL_SLOTS_PASSED";
    int kinds=0;
    if(finalRomLowSlots>0)kinds++;
    if(finalUpperArmExcessSlots>0)kinds++;
    if(finalPlaneDeviationSlots>0)kinds++;
    if(finalTorsoCompensationSlots>0)kinds++;
    if(finalMultiIssueSlots>0)kinds++;
    if(kinds>1||finalMultiIssueSlots>0)return "COMPLETED_WITH_MIXED_ISSUES";
    if(finalRomLowSlots>0)return "COMPLETED_WITH_ROM_LOW";
    if(finalUpperArmExcessSlots>0)return "COMPLETED_WITH_UPPER_ARM_COMPENSATION";
    if(finalPlaneDeviationSlots>0)return "COMPLETED_WITH_PLANE_DEVIATION";
    if(finalTorsoCompensationSlots>0)return "COMPLETED_WITH_TORSO_COMPENSATION";
    return "COMPLETED_WITH_UNCLASSIFIED_FAIL";
  }
};
