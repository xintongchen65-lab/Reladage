#pragma once
#include "rehab_runtime.h"
#include "../integration/v5_motion_adapter.h"
#include "../storage/session_report.h"
#include "../games/game_mapping.h"
#include "../digital_twin/motionframe_packet.h"
#include <cstdint>
#include <deque>
#include <string>

namespace rehab {

// High-level application coordinator used by the embedded product shell.
// It intentionally owns the cross-module concerns that should not live inside
// individual motion detectors: screen telemetry, game events, SD records,
// digital-twin packets, cloud/offline queueing and final session summary.
struct RehabSystemOutput {
  RuntimeOutput runtime{};
  TwinPacket twin{};
  GameEvent game{};
  ReportSummary report{};

  bool emitLivePacket{false};
  bool emitGameEvent{false};
  bool persistRepRecord{false};
  bool persistCheckpoint{false};
  bool sessionFinished{false};
  bool cloudPayloadReady{false};

  std::string liveJson;
  std::string twinJson;
  std::string repJson;
  std::string cloudJson;
  std::string reportJson;
};

class RehabSystemOrchestrator {
public:
  RehabSystemOrchestrator();

  void reset();
  void loadPrescription(const Prescription& prescription);
  bool selectExercise(ExerciseId exercise);
  bool start();
  void pause();
  void resume();
  void stop();

  void setConnectivity(bool wifiConnected, bool sdReady);
  void setDeviceIdentity(const std::string& patientId, const std::string& deviceId);

  RehabSystemOutput update(const V5MotionSnapshot& snapshot);

  const RuntimeState& state() const { return runtime_.state(); }
  const ReportSummary summary() const { return session_.summary(); }
  ExerciseId activeExercise() const { return activeExercise_; }
  size_t pendingSyncCount() const { return offlineQueue_.size(); }
  bool popPendingSync(std::string& payload);

private:
  std::string makeRepEnvelope(const ExerciseFeedback& feedback, const MotionFrame& frame) const;
  std::string makeCloudEnvelope(const ExerciseFeedback& feedback, const MotionFrame& frame) const;
  std::string makeLiveEnvelope(const ExerciseFeedback& feedback, const MotionFrame& frame) const;
  void queueOrExposeCloud(const std::string& payload, RehabSystemOutput& out);

  RehabRuntime runtime_{};
  SessionAccumulator session_{};
  ExerciseId activeExercise_{ExerciseId::DumbbellCurl};
  std::deque<std::string> offlineQueue_{};
  std::string patientId_{"patient-local"};
  std::string deviceId_{"RM-Core-01"};
  bool wifiConnected_{false};
  bool sdReady_{true};
  bool finishedLatched_{false};
};

} // namespace rehab
