#pragma once
#include "motion_types.h"
#include <cstdint>
#include <array>
namespace rehab {
struct PrescriptionItem {
  ExerciseId exercise{ExerciseId::DumbbellCurl};
  uint8_t sets{3};
  uint8_t repsPerSet{10};
  float targetDeg{80};
  uint16_t restSec{30};
};
struct Prescription {
  uint32_t planId{0};
  uint32_t issuedAtUnix{0};
  std::array<PrescriptionItem,8> items{};
  uint8_t itemCount{0};
};
struct SessionProgress {
  uint32_t planId{0};
  uint8_t itemIndex{0};
  uint8_t setIndex{0};
  uint8_t repIndex{0};
  uint32_t totalAcceptedReps{0};
  uint32_t totalRejectedReps{0};
};
}
