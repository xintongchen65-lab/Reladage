#pragma once
#include <Arduino.h>

// Runtime-facing action catalog for the integrated RehabMotion_v5 sketch.
// The full detector implementations live under src/rehab/motion_engine/.
// This lightweight table keeps the legacy V5 state machine, prescription
// persistence and 7-inch screen protocol on the same eight-action vocabulary.

struct RehabV5ActionProfile {
  const char *code;
  const char *nameZh;
  const char *metricZh;
  const char *regionCode;
  uint8_t fixedSlot;
  uint8_t movingSlot;
  uint8_t torsoSlot;
  int defaultSets;
  int defaultReps;
  int defaultTargetDeg;
  int validDeg;
  int returnDeg;
  float upperHardDeg;
  float planeHardDeg;
  float torsoHardDeg;
};

enum RehabV5ActionId : uint8_t {
  RM_ACTION_DUMBBELL_CURL = 0,
  RM_ACTION_TRICEPS_EXTENSION = 1,
  RM_ACTION_SCAPTION_RAISE = 2,
  RM_ACTION_WALL_CRAWL = 3,
  RM_ACTION_KNEE_FLEX_EXTEND = 4,
  RM_ACTION_SIT_TO_STAND = 5,
  RM_ACTION_BOX_SQUAT = 6,
  RM_ACTION_STEP_UP = 7,
  RM_ACTION_COUNT = 8
};

static const RehabV5ActionProfile RM_ACTIONS[RM_ACTION_COUNT] = {
  {"dumbbell_curl",       "哑铃弯举",          "目标屈肘",     "elbow",            0,1,4, 3,10,80,40,20, 35,35,30},
  {"triceps_extension",   "肱三头肌伸展",      "目标伸展",     "elbow",            0,1,4, 2,10,75,40,20, 30,35,30},
  {"scaption_raise",      "肩胛平面抬手",      "目标抬举",     "shoulder",         0,1,4, 3,10,90,35,20, 25,30,25},
  {"wall_crawl",          "墙面爬手",          "目标抬举",     "shoulder",         0,1,4, 3,10,100,30,20,25,30,25},
  {"knee_flex_extend",    "膝关节屈伸",        "目标屈伸",     "knee",             2,3,4, 3,10,80,35,20, 30,30,25},
  {"sit_to_stand",       "坐到站训练",        "目标起立",     "functional_lower", 2,3,4, 3,8,70,20,15, 30,35,25},
  {"box_squat",           "箱式深蹲",          "目标下蹲",     "functional_lower", 2,3,4, 3,8,75,25,15, 30,30,25},
  {"step_up",             "台阶踩踏 Step Up", "目标抬腿",     "functional_lower", 2,3,4, 3,10,60,20,15,30,35,25}
};

static inline uint8_t rehabV5NormalizeActionIndex(int idx) {
  if (idx < 0) return RM_ACTION_COUNT - 1;
  if (idx >= RM_ACTION_COUNT) return 0;
  return (uint8_t)idx;
}

static inline const RehabV5ActionProfile &rehabV5Action(int idx) {
  return RM_ACTIONS[rehabV5NormalizeActionIndex(idx)];
}

static inline bool rehabV5ActionUsesUpperPair(int idx) {
  return rehabV5Action(idx).fixedSlot == 0;
}

static inline bool rehabV5ActionUsesLowerPair(int idx) {
  return rehabV5Action(idx).fixedSlot == 2;
}
