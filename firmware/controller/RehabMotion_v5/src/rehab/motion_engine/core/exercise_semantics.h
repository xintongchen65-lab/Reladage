#pragma once
#include "motion_types.h"
namespace rehab {
struct ExerciseSemantics {
  ExerciseId id;
  const char* sensorLayoutZh;
  const char* startCueZh;
  const char* activeCueZh;
  const char* peakCueZh;
  const char* returnCueZh;
  const char* primaryMetricZh;
  const char* secondaryMetricsZh;
  const char* commonErrorsZh;
  const char* jsonNamespace;
};
const ExerciseSemantics& exerciseSemantics(ExerciseId id);
}
