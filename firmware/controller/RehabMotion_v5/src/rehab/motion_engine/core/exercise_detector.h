#pragma once
#include "motion_types.h"
#include "exercise_profile.h"

namespace rehab {

class IExerciseDetector {
public:
  virtual ~IExerciseDetector() = default;
  virtual ExerciseId id() const = 0;
  virtual void reset() = 0;
  virtual ExerciseFeedback update(const MotionFrame& frame) = 0;
};

class RuleBasedDetector : public IExerciseDetector {
public:
  explicit RuleBasedDetector(ExerciseId id);
  ExerciseId id() const override { return profile_.id; }
  void reset() override;
  ExerciseFeedback update(const MotionFrame& frame) override;
protected:
  virtual float primarySignal(const MotionFrame& frame) const;
  virtual QualityCode evaluateQuality(const MotionFrame& aggregateFrame, float peak) const;
  virtual uint32_t collectQualityFlags(const MotionFrame& aggregateFrame, float peak, const RepMetrics& metrics) const;
  virtual const char* qualityWarning(QualityCode q) const;
  virtual float scoreRep(const MotionFrame& aggregateFrame, float peak, const RepMetrics& metrics, uint32_t flags) const;
  virtual bool startPosePlausible(const MotionFrame& frame) const;
  virtual bool sensorSetPlausible(const MotionFrame& frame) const;
  virtual uint32_t minimumRepDurationMs() const { return 450; }
  virtual uint32_t maximumRepDurationMs() const { return 12000; }

  void beginRep(const MotionFrame& frame, float signal);
  void accumulate(const MotionFrame& frame, float signal);
  MotionFrame aggregateFrameForEvaluation(const MotionFrame& terminalFrame) const;
  QualityCode dominantQualityFromFlags(uint32_t flags) const;

  ExerciseProfile profile_;
  Phase phase_{Phase::Ready};
  uint16_t reps_{0};
  float peak_{0};
  uint32_t activeStartMs_{0};
  RepMetrics repMetrics_{};
  float speedSum_{0};
  float stabilitySum_{0};
};

} // namespace rehab
