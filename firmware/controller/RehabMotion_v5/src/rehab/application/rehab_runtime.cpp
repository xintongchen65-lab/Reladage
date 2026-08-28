#include "rehab_runtime.h"
#include "../motion_engine/core/telemetry_schema.h"

namespace rehab {

RehabRuntime::RehabRuntime(){ reset(); }

void RehabRuntime::reset(){
  state_ = RuntimeState{};
  engine_.select(ExerciseId::DumbbellCurl);
}

void RehabRuntime::loadPrescription(const Prescription& p){
  prescription_ = p;
  state_.prescriptionLoaded = p.itemCount > 0;
  state_.currentItem = 0;
  state_.currentSet = 0;
  state_.repsInSet = 0;
  state_.acceptedReps = 0;
  state_.rejectedReps = 0;
  if(state_.prescriptionLoaded){
    engine_.select(p.items[0].exercise);
    state_.mode = RuntimeMode::Ready;
  }
}

bool RehabRuntime::selectExercise(ExerciseId id){
  if(state_.mode == RuntimeMode::Training) return false;
  engine_.select(id);
  state_.mode = RuntimeMode::Ready;
  return true;
}

bool RehabRuntime::start(){
  if(state_.mode == RuntimeMode::Paused){ state_.mode = RuntimeMode::Training; return true; }
  if(state_.mode != RuntimeMode::Ready && state_.mode != RuntimeMode::Idle) return false;
  engine_.reset();
  state_.mode = RuntimeMode::Training;
  return true;
}

void RehabRuntime::pause(){ if(state_.mode == RuntimeMode::Training) state_.mode = RuntimeMode::Paused; }
void RehabRuntime::resume(){ if(state_.mode == RuntimeMode::Paused) state_.mode = RuntimeMode::Training; }
void RehabRuntime::stop(){ state_.mode = RuntimeMode::Finished; }
void RehabRuntime::setWiFiConnected(bool v){ state_.wifiConnected = v; }
void RehabRuntime::setSdReady(bool v){ state_.sdReady = v; }

void RehabRuntime::advancePlanIfNeeded(){
  if(!state_.prescriptionLoaded || prescription_.itemCount == 0) return;
  const auto& item = prescription_.items[state_.currentItem];
  if(state_.repsInSet < item.repsPerSet) return;
  state_.repsInSet = 0;
  ++state_.currentSet;
  if(state_.currentSet < item.sets){ state_.mode = RuntimeMode::Rest; return; }
  state_.currentSet = 0;
  ++state_.currentItem;
  if(state_.currentItem >= prescription_.itemCount){ state_.mode = RuntimeMode::Finished; return; }
  engine_.select(prescription_.items[state_.currentItem].exercise);
  state_.mode = RuntimeMode::Ready;
}

RuntimeOutput RehabRuntime::update(const MotionFrame& frame){
  RuntimeOutput out{};
  if(state_.mode != RuntimeMode::Training){ out.state=state_; return out; }
  out.feedback = engine_.update(frame);
  out.telemetryJson = feedbackToJson(out.feedback);
  out.sendDisplayUpdate = true;
  if(out.feedback.repCompleted){
    out.writeSessionRecord = state_.sdReady;
    out.enqueueCloudSync = state_.wifiConnected;
    if(out.feedback.repAccepted){ ++state_.acceptedReps; ++state_.repsInSet; }
    else { ++state_.rejectedReps; }
    advancePlanIfNeeded();
  }
  out.state = state_;
  return out;
}

} // namespace rehab
