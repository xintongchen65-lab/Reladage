#pragma once

#include <Arduino.h>
#include "training_assessment_v4.h"

// Power-loss resume checkpoint schema must live in a normal header rather than
// inside the .ino. Arduino's sketch preprocessor auto-generates function
// forward declarations before .ino type declarations; placing this type in an included
// header guarantees those generated declarations can see it.
static constexpr uint32_t POWER_RESUME_MAGIC = 0x524D5635UL; // "RMV5"
static constexpr uint16_t POWER_RESUME_VERSION = 1;
static constexpr const char *POWER_RESUME_NAMESPACE = "rehab_v5";
static constexpr const char *POWER_RESUME_KEY = "checkpoint";

struct PowerResumeCheckpoint {
  uint32_t magic = POWER_RESUME_MAGIC;
  uint16_t version = POWER_RESUME_VERSION;
  uint16_t size = 0;
  uint8_t active = 0;
  uint8_t exercise = 0;
  uint8_t mode = 0;
  uint8_t reserved = 0;
  int16_t targetSets = 0;
  int16_t targetReps = 0;
  int16_t targetAngle = 0;
  int16_t validAngle = 0;
  int16_t returnAngle = 0;
  int16_t restSec = 0;
  int16_t totalCompletedSlots = 0;
  int16_t reserved2 = 0;
  V4SessionAssessment assessment;
  uint32_t checksum = 0;
};
