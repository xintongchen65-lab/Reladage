#pragma once
#include <Arduino.h>
#include <math.h>
#include <string.h>
#include "training_logic.h"

// =====================================================
// Game-facing motion data contract
// Important: game receives processed motion data, not raw IMU packets.
// =====================================================

struct MotionOutput {
  uint32_t seq = 0;
  uint32_t timestampMs = 0;   // current test: millis() since ESP32 boot

  const char *mode = "upper";
  const char *exercise = "overhead_triceps_extension";

  float leftAngleDeg = 0.0f;
  float rightAngleDeg = 0.0f;

  float leftRomDeg = 0.0f;
  float rightRomDeg = 0.0f;
  float lrRomDiffDeg = 0.0f;

  int leftCount = 0;
  int rightCount = 0;
  int targetCount = 10;

  int completionPercent = 0;

  const char *trainingState = "IDLE";
  const char *repEvent = "none";  // single-frame event: none/left_rep_done/right_rep_done/both_rep_done

  float leftSpeedDegS = 0.0f;
  float rightSpeedDegS = 0.0f;
};

static inline const char *trainingStateName(TrainingState state) {
  switch (state) {
    case IDLE: return "IDLE";
    case RUNNING: return "RUNNING";
    case PAUSED: return "PAUSED";
    case FINISHED: return "FINISHED";
    case STOPPED: return "STOPPED";
    default: return "UNKNOWN";
  }
}

static inline bool stateIsFinishedByCount(const TrainingData &left, const TrainingData &right) {
  (void)right;
  return left.currentCount >= left.targetCount;
}

static inline const char *aggregateTrainingState(
  const TrainingData &left,
  const TrainingData &right
) {
  if (stateIsFinishedByCount(left, right)) return "FINISHED";
  if (left.state == STOPPED) return "STOPPED";
  if (left.state == RUNNING) return "RUNNING";
  if (left.state == PAUSED) return "PAUSED";
  return "IDLE";
}

static inline int computeCompletionPercent(
  const TrainingData &left,
  const TrainingData &right
) {
  int target = left.targetCount;
  if (target <= 0) return 0;

  // V4 A2: completion follows the A/B upper-arm/forearm pair only.
  (void)right;
  int completed = left.currentCount;
  int percent = (completed * 100) / target;
  if (percent < 0) percent = 0;
  if (percent > 100) percent = 100;
  return percent;
}

static inline MotionOutput buildMotionOutput(
  const TrainingData &left,
  const TrainingData &right,
  uint32_t seq,
  uint32_t timestampMs,
  const char *repEvent,
  float leftSpeedDegS,
  float rightSpeedDegS,
  const char *mode = "upper",
  const char *exercise = "overhead_triceps_extension"
) {
  MotionOutput out;
  out.seq = seq;
  out.timestampMs = timestampMs;
  out.mode = mode;
  out.exercise = exercise;
  out.leftAngleDeg = left.currentAngle;
  out.rightAngleDeg = right.currentAngle;
  out.leftRomDeg = left.maxAngle;
  out.rightRomDeg = right.maxAngle;
  out.leftCount = left.currentCount;
  out.rightCount = right.currentCount;
  out.targetCount = left.targetCount;
  out.lrRomDiffDeg = fabsf(left.maxAngle - right.maxAngle);
  out.completionPercent = computeCompletionPercent(left, right);
  out.trainingState = aggregateTrainingState(left, right);
  out.repEvent = repEvent ? repEvent : "none";

  // Game-facing speed should be 0 when the motion is not actively running.
  if (strcmp(out.trainingState, "RUNNING") == 0) {
    out.leftSpeedDegS = leftSpeedDegS;
    out.rightSpeedDegS = rightSpeedDegS;
  } else {
    out.leftSpeedDegS = 0.0f;
    out.rightSpeedDegS = 0.0f;
  }
  return out;
}

static inline String motionOutputToJson(const MotionOutput &out) {
  String s = "{";
  s += "\"seq\":" + String(out.seq) + ",";
  s += "\"timestamp_ms\":" + String(out.timestampMs) + ",";
  s += "\"mode\":\"" + String(out.mode) + "\",";
  s += "\"exercise\":\"" + String(out.exercise) + "\",";
  s += "\"ab_angle_deg\":" + String(out.leftAngleDeg, 1) + ",";
  s += "\"ab_rom_deg\":" + String(out.leftRomDeg, 1) + ",";
  s += "\"ab_count\":" + String(out.leftCount) + ",";
  s += "\"target_count\":" + String(out.targetCount) + ",";
  s += "\"completion_percent\":" + String(out.completionPercent) + ",";
  s += "\"training_state\":\"" + String(out.trainingState) + "\",";
  s += "\"rep_event\":\"" + String(out.repEvent) + "\",";
  s += "\"ab_speed_deg_s\":" + String(out.leftSpeedDegS, 1);
  s += "}";
  return s;
}
