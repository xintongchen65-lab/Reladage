#pragma once
#include <stdint.h>

struct RehabUiTask {
  const char* name;
  uint8_t exerciseIndex;
  uint8_t sets;
  uint8_t reps;
  float targetDeg;
  uint8_t restSec;
  bool completed;
};

struct RehabUiModel {
  const char* patientName;
  const char* currentExercise;
  uint8_t exerciseIndex;
  uint8_t trainMode; // 0 normal, 1 game, 2 guided

  float currentAngleDeg;
  float targetAngleDeg;
  float peakRomDeg;
  uint8_t currentRep;
  uint8_t targetReps;
  uint8_t currentSet;
  uint8_t targetSets;
  uint16_t completedTotal;
  uint16_t targetTotal;
  uint16_t restSeconds;
  uint32_t elapsedSeconds;

  float planeDeviationDeg;
  float compensationDeg;
  float torsoTiltDeg;
  float asymmetryDeg;
  float qualityScore;
  uint16_t qualityFlags;
  const char* qualityText;

  uint8_t imuMask;
  int batteryPct;
  bool wifiConnected;
  bool sdReady;
  bool voiceReady;
  bool bodyReferenceReady;
  bool calibrationReady;
  uint32_t syncQueueCount;
  uint32_t storageFreeMb;

  uint8_t taskCount;
  RehabUiTask tasks[8];

  float weeklyPassRate;
  float weeklyMaxRomDeg;
  float weeklyAvgRomDeg;
  float weeklyQualityScore;
  uint8_t weeklyTrainingDays;
  uint16_t weeklyMinutes;
  float romTrend[7];
};

inline const char* rehab_ui_exercise_name(uint8_t index){
  static const char* kNames[8]={
    "哑铃弯举","肱三头肌伸展","肩胛平面抬手","墙面爬手",
    "膝关节屈伸","坐到站训练","箱式深蹲","台阶踩踏 Step Up"
  };
  return kNames[index < 8 ? index : 0];
}

inline const char* rehab_ui_exercise_region(uint8_t index){
  static const char* kRegions[8]={
    "肘关节","肘关节","肩关节","肩关节/功能训练",
    "膝关节","下肢功能","下肢功能","下肢功能"
  };
  return kRegions[index < 8 ? index : 0];
}

inline const char* rehab_ui_exercise_metric(uint8_t index){
  static const char* kMetrics[8]={
    "ROM · 上臂稳定 · 平面 · 躯干","伸展ROM · 上臂稳定 · 平面","抬举角 · 肩胛平面 · 躯干",
    "抬举高度 · 路径 · 躯干","膝ROM · 平面 · 左右差异","起立相位 · 前倾 · 对称性",
    "下蹲深度 · 对称性 · 躯干","抬腿高度 · 支撑相位 · 返回控制"
  };
  return kMetrics[index < 8 ? index : 0];
}

inline RehabUiModel rehab_ui_default_model(){
  RehabUiModel m{};
  m.patientName="用户";
  m.currentExercise="哑铃弯举";
  m.exerciseIndex=0;
  m.trainMode=0;
  m.currentAngleDeg=0;
  m.targetAngleDeg=80;
  m.peakRomDeg=0;
  m.targetReps=10;
  m.targetSets=3;
  m.targetTotal=30;
  m.qualityScore=100;
  m.qualityText="等待训练";
  m.imuMask=0x1F;
  m.batteryPct=82;
  m.wifiConnected=true;
  m.sdReady=true;
  m.voiceReady=true;
  m.bodyReferenceReady=true;
  m.calibrationReady=true;
  m.storageFreeMb=7420;

  m.taskCount=8;
  const uint8_t sets[8]={3,2,3,3,3,3,3,3};
  const uint8_t reps[8]={10,10,10,10,10,8,8,10};
  const float target[8]={80,75,90,100,80,70,75,60};
  for(uint8_t i=0;i<8;i++){
    m.tasks[i]={rehab_ui_exercise_name(i),i,sets[i],reps[i],target[i],30,false};
  }

  m.weeklyPassRate=87;
  m.weeklyMaxRomDeg=92;
  m.weeklyAvgRomDeg=84;
  m.weeklyQualityScore=89;
  m.weeklyTrainingDays=5;
  m.weeklyMinutes=128;
  const float trend[7]={72,76,79,81,84,89,92};
  for(int i=0;i<7;i++) m.romTrend[i]=trend[i];
  return m;
}
