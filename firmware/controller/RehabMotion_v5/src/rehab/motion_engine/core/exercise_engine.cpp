#include "exercise_engine.h"
#include "../exercises/dumbbell_curl.h"
#include "../exercises/triceps_extension.h"
#include "../exercises/scaption_raise.h"
#include "../exercises/wall_crawl.h"
#include "../exercises/knee_flex_extend.h"
#include "../exercises/sit_to_stand.h"
#include "../exercises/box_squat.h"
#include "../exercises/step_up.h"
namespace rehab {
ExerciseEngine::ExerciseEngine(){ select(ExerciseId::DumbbellCurl); }
std::unique_ptr<IExerciseDetector> ExerciseEngine::makeDetector(ExerciseId id){
  switch(id){
    case ExerciseId::DumbbellCurl: return std::make_unique<DumbbellCurlDetector>();
    case ExerciseId::TricepsExtension: return std::make_unique<TricepsExtensionDetector>();
    case ExerciseId::ScaptionRaise: return std::make_unique<ScaptionRaiseDetector>();
    case ExerciseId::WallCrawl: return std::make_unique<WallCrawlDetector>();
    case ExerciseId::KneeFlexExtend: return std::make_unique<KneeFlexExtendDetector>();
    case ExerciseId::SitToStand: return std::make_unique<SitToStandDetector>();
    case ExerciseId::BoxSquat: return std::make_unique<BoxSquatDetector>();
    case ExerciseId::StepUp: return std::make_unique<StepUpDetector>();
    default: return std::make_unique<DumbbellCurlDetector>();
  }
}
void ExerciseEngine::select(ExerciseId id){ selected_=id; detector_=makeDetector(id); }
void ExerciseEngine::reset(){ if(detector_) detector_->reset(); }
ExerciseFeedback ExerciseEngine::update(const MotionFrame& frame){ return detector_ ? detector_->update(frame) : ExerciseFeedback{}; }
}
