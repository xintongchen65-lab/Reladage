#pragma once
#include "../motion_engine/core/motion_types.h"
#include <cstdint>
#include <string>
#include <vector>

namespace rehab {

enum class CommandType : uint8_t {
  Unknown, Start, Pause, Resume, Stop, SelectExercise, BeginPosition,
  RequestDeviceState, RequestPrescription, AcknowledgePrescription
};

struct Command {
  CommandType type{CommandType::Unknown};
  int value{0};
  std::string text;
};

Command parseCommandLine(const std::string& line);
std::string makeDeviceStateMessage(bool ready, uint8_t imuMask, int batteryPct, bool wifi, bool sdReady);
std::string makeExerciseSelectedMessage(ExerciseId id);
std::string makePrescriptionReceivedMessage(uint32_t planId, uint8_t itemCount);
std::string makeSyncStateMessage(bool wifi, uint32_t queuedRecords);

} // namespace rehab
