#pragma once
#include "../motion_engine/core/exercise_engine.h"
#include "../motion_engine/core/prescription.h"
#include <cstdint>
#include <string>

namespace rehab {

enum class RuntimeMode : uint8_t { Idle, Ready, Training, Paused, Rest, Finished };

struct RuntimeState {
  RuntimeMode mode{RuntimeMode::Idle};
  bool wifiConnected{false};
  bool sdReady{true};
  bool prescriptionLoaded{false};
  uint8_t currentItem{0};
  uint8_t currentSet{0};
  uint8_t repsInSet{0};
  uint32_t acceptedReps{0};
  uint32_t rejectedReps{0};
};

struct RuntimeOutput {
  RuntimeState state{};
  ExerciseFeedback feedback{};
  bool writeSessionRecord{false};
  bool sendDisplayUpdate{false};
  bool enqueueCloudSync{false};
  std::string telemetryJson;
};

class RehabRuntime {
public:
  RehabRuntime();
  void reset();
  void loadPrescription(const Prescription& prescription);
  bool selectExercise(ExerciseId id);
  bool start();
  void pause();
  void resume();
  void stop();
  void setWiFiConnected(bool connected);
  void setSdReady(bool ready);
  const RuntimeState& state() const { return state_; }
  const Prescription& prescription() const { return prescription_; }
  RuntimeOutput update(const MotionFrame& frame);
private:
  void advancePlanIfNeeded();
  ExerciseEngine engine_{};
  Prescription prescription_{};
  RuntimeState state_{};
};

} // namespace rehab
