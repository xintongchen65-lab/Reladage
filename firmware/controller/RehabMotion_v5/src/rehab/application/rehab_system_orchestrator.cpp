#include "rehab_system_orchestrator.h"
#include "../motion_engine/core/exercise_profile.h"
#include "../motion_engine/core/telemetry_schema.h"
#include <algorithm>
#include <cstdio>

namespace rehab {

RehabSystemOrchestrator::RehabSystemOrchestrator(){ reset(); }

void RehabSystemOrchestrator::reset(){
  runtime_.reset();
  runtime_.setWiFiConnected(wifiConnected_);
  runtime_.setSdReady(sdReady_);
  session_.reset();
  offlineQueue_.clear();
  activeExercise_ = ExerciseId::DumbbellCurl;
  finishedLatched_ = false;
}

void RehabSystemOrchestrator::loadPrescription(const Prescription& prescription){
  runtime_.loadPrescription(prescription);
  session_.reset();
  finishedLatched_ = false;
  if(prescription.itemCount > 0) activeExercise_ = prescription.items[0].exercise;
}

bool RehabSystemOrchestrator::selectExercise(ExerciseId exercise){
  if(!runtime_.selectExercise(exercise)) return false;
  activeExercise_ = exercise;
  finishedLatched_ = false;
  return true;
}

bool RehabSystemOrchestrator::start(){ return runtime_.start(); }
void RehabSystemOrchestrator::pause(){ runtime_.pause(); }
void RehabSystemOrchestrator::resume(){ runtime_.resume(); }
void RehabSystemOrchestrator::stop(){ runtime_.stop(); }

void RehabSystemOrchestrator::setConnectivity(bool wifiConnected, bool sdReady){
  wifiConnected_ = wifiConnected;
  sdReady_ = sdReady;
  runtime_.setWiFiConnected(wifiConnected);
  runtime_.setSdReady(sdReady);
}

void RehabSystemOrchestrator::setDeviceIdentity(const std::string& patientId, const std::string& deviceId){
  if(!patientId.empty()) patientId_ = patientId;
  if(!deviceId.empty()) deviceId_ = deviceId;
}

std::string RehabSystemOrchestrator::makeLiveEnvelope(const ExerciseFeedback& f, const MotionFrame& frame) const {
  char b[1024];
  std::snprintf(b,sizeof(b),
    "{\"type\":\"live_training\",\"device_id\":\"%s\",\"exercise_id\":%u,\"exercise\":\"%s\","
    "\"timestamp_ms\":%lu,\"phase\":%u,\"rep_count\":%u,\"current_angle_deg\":%.1f,\"peak_rom_deg\":%.1f,"
    "\"plane_deviation_deg\":%.1f,\"compensation_deg\":%.1f,\"torso_tilt_deg\":%.1f,\"quality_score\":%.1f,"
    "\"sync_queue\":%lu}",
    deviceId_.c_str(), static_cast<unsigned>(activeExercise_), exerciseCode(activeExercise_),
    static_cast<unsigned long>(frame.timestampMs), static_cast<unsigned>(f.phase), static_cast<unsigned>(f.repCount),
    f.currentAngleDeg, f.peakRomDeg, frame.planeDeviationDeg, frame.upperArmDeviationDeg,
    frame.torsoTiltDeg, f.qualityScore, static_cast<unsigned long>(offlineQueue_.size()));
  return b;
}

std::string RehabSystemOrchestrator::makeRepEnvelope(const ExerciseFeedback& f, const MotionFrame& frame) const {
  char b[1400];
  std::snprintf(b,sizeof(b),
    "{\"schema\":\"rehabmotion.rep.v1\",\"patient_id\":\"%s\",\"device_id\":\"%s\","
    "\"exercise_id\":%u,\"exercise\":\"%s\",\"timestamp_ms\":%lu,\"rep_count\":%u,"
    "\"accepted\":%s,\"quality_code\":%u,\"quality_flags\":%lu,\"quality_score\":%.1f,"
    "\"rom_deg\":%.1f,\"duration_ms\":%lu,\"plane_deviation_deg\":%.1f,\"compensation_deg\":%.1f,"
    "\"torso_tilt_deg\":%.1f,\"asymmetry_deg\":%.1f,\"forward_lean_deg\":%.1f,\"stability\":%.3f}",
    patientId_.c_str(), deviceId_.c_str(), static_cast<unsigned>(activeExercise_), exerciseCode(activeExercise_),
    static_cast<unsigned long>(frame.timestampMs), static_cast<unsigned>(f.repCount), f.repAccepted?"true":"false",
    static_cast<unsigned>(f.quality), static_cast<unsigned long>(f.qualityFlags), f.qualityScore, f.peakRomDeg,
    static_cast<unsigned long>(f.metrics.durationMs), f.metrics.maxPlaneDeviationDeg, f.metrics.maxUpperArmDeviationDeg,
    f.metrics.maxTorsoTiltDeg, f.metrics.maxAsymmetryDeg, f.metrics.maxForwardLeanDeg, f.metrics.meanStability);
  return b;
}

std::string RehabSystemOrchestrator::makeCloudEnvelope(const ExerciseFeedback& f, const MotionFrame& frame) const {
  char b[1800];
  const std::string feedbackJson = feedbackToJson(f);
  std::snprintf(b,sizeof(b),
    "{\"schema\":\"rehabmotion.sync.v1\",\"patient_id\":\"%s\",\"device_id\":\"%s\","
    "\"exercise_id\":%u,\"timestamp_ms\":%lu,\"payload\":%s}",
    patientId_.c_str(), deviceId_.c_str(), static_cast<unsigned>(activeExercise_),
    static_cast<unsigned long>(frame.timestampMs), feedbackJson.c_str());
  return b;
}

void RehabSystemOrchestrator::queueOrExposeCloud(const std::string& payload, RehabSystemOutput& out){
  if(wifiConnected_){
    out.cloudPayloadReady = true;
    out.cloudJson = payload;
  }else{
    offlineQueue_.push_back(payload);
  }
}

bool RehabSystemOrchestrator::popPendingSync(std::string& payload){
  if(offlineQueue_.empty()) return false;
  payload = offlineQueue_.front();
  offlineQueue_.pop_front();
  return true;
}

RehabSystemOutput RehabSystemOrchestrator::update(const V5MotionSnapshot& snapshot){
  RehabSystemOutput out{};
  const MotionFrame frame = makeMotionFrameFromV5(snapshot);
  out.twin = makeTwinPacket(frame);
  out.twinJson = twinPacketToJson(out.twin);

  // Keep the active exercise aligned with a loaded prescription as the runtime
  // advances from one item to the next.
  const RuntimeState before = runtime_.state();
  if(runtime_.prescription().itemCount > 0 && before.currentItem < runtime_.prescription().itemCount)
    activeExercise_ = runtime_.prescription().items[before.currentItem].exercise;

  out.runtime = runtime_.update(frame);
  out.emitLivePacket = out.runtime.state.mode == RuntimeMode::Training || out.runtime.feedback.repCompleted;
  if(out.emitLivePacket) out.liveJson = makeLiveEnvelope(out.runtime.feedback, frame);

  if(out.runtime.feedback.repCompleted){
    session_.add(out.runtime.feedback);
    out.report = session_.summary();
    out.reportJson = session_.summaryJson();
    out.persistRepRecord = sdReady_;
    out.persistCheckpoint = true;
    out.repJson = makeRepEnvelope(out.runtime.feedback, frame);

    out.game = mapFeedbackToGame(activeExercise_, out.runtime.feedback);
    out.emitGameEvent = out.game.trigger;

    const std::string cloud = makeCloudEnvelope(out.runtime.feedback, frame);
    queueOrExposeCloud(cloud, out);
  }else{
    out.report = session_.summary();
  }

  // Surface one queued item immediately after connectivity returns. The caller
  // can continue draining with popPendingSync() without blocking the motion loop.
  if(wifiConnected_ && !out.cloudPayloadReady && !offlineQueue_.empty()){
    out.cloudPayloadReady = true;
    out.cloudJson = offlineQueue_.front();
    offlineQueue_.pop_front();
  }

  if(out.runtime.state.mode == RuntimeMode::Finished && !finishedLatched_){
    finishedLatched_ = true;
    out.sessionFinished = true;
    out.report = session_.summary();
    out.reportJson = session_.summaryJson();
  }
  return out;
}

} // namespace rehab
