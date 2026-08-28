#pragma once
#include "exercise_detector.h"
#include <memory>
namespace rehab {
class ExerciseEngine {
public:
  ExerciseEngine();
  void select(ExerciseId id);
  void reset();
  ExerciseId selected() const { return selected_; }
  ExerciseFeedback update(const MotionFrame& frame);
private:
  std::unique_ptr<IExerciseDetector> makeDetector(ExerciseId id);
  ExerciseId selected_{ExerciseId::DumbbellCurl};
  std::unique_ptr<IExerciseDetector> detector_;
};
}
