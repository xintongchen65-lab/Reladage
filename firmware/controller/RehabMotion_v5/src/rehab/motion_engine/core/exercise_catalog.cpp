#include "exercise_profile.h"

namespace rehab {

static const ExerciseProfile kProfiles[] = {
  { ExerciseId::DumbbellCurl, "dumbbell_curl", "哑铃弯举 / 屈肘训练", "elbow", "上臂自然垂下，肘部靠近躯干", "肘关节屈曲ROM",
    {SensorId::A,SensorId::B,SensorId::E,SensorId::C,SensorId::D}, 3, 40, 80, 20, 35, 35, 30, 180, 0.55f },
  { ExerciseId::TricepsExtension, "triceps_extension", "肱三头肌伸展", "elbow", "上臂举过头，肘关节屈曲", "肘关节伸展ROM",
    {SensorId::A,SensorId::B,SensorId::E,SensorId::C,SensorId::D}, 3, 40, 80, 20, 30, 35, 30, 180, 0.55f },
  { ExerciseId::ScaptionRaise, "scaption_raise", "肩胛平面抬手", "shoulder", "手臂自然下垂，肩部放松", "肩部抬举角",
    {SensorId::A,SensorId::B,SensorId::E,SensorId::C,SensorId::D}, 3, 35, 90, 20, 22, 25, 20, 150, 0.60f },
  { ExerciseId::WallCrawl, "wall_crawl", "墙面爬手", "shoulder", "面向墙面，手指接触墙面", "上肢抬举高度/角度",
    {SensorId::A,SensorId::B,SensorId::E,SensorId::C,SensorId::D}, 3, 30, 100, 20, 25, 30, 20, 120, 0.65f },
  { ExerciseId::KneeFlexExtend, "knee_flex_extend", "膝关节屈伸", "knee", "坐姿或站姿，膝关节处于起始角", "膝关节屈伸ROM",
    {SensorId::C,SensorId::D,SensorId::E,SensorId::A,SensorId::B}, 3, 35, 80, 20, 30, 30, 25, 180, 0.55f },
  { ExerciseId::SitToStand, "sit_to_stand", "坐到站训练", "functional_lower", "坐姿，双脚稳定支撑", "起立相位+躯干前倾+膝伸展",
    {SensorId::C,SensorId::D,SensorId::E,SensorId::A,SensorId::B}, 5, 20, 70, 15, 30, 35, 25, 160, 0.55f },
  { ExerciseId::BoxSquat, "box_squat", "箱式深蹲", "functional_lower", "站姿，双脚与肩同宽，后方有箱/椅", "下蹲深度+膝/躯干控制",
    {SensorId::C,SensorId::D,SensorId::E,SensorId::A,SensorId::B}, 5, 25, 75, 15, 30, 30, 25, 150, 0.60f },
  { ExerciseId::StepUp, "step_up", "台阶踩踏 Step Up", "functional_lower", "面向低台阶，双脚站稳", "抬腿高度+支撑相位+躯干稳定",
    {SensorId::C,SensorId::D,SensorId::E,SensorId::A,SensorId::B}, 5, 20, 60, 15, 30, 35, 25, 180, 0.55f }
};

const ExerciseProfile& exerciseProfile(ExerciseId id) {
  auto i = static_cast<uint8_t>(id);
  if (i >= static_cast<uint8_t>(ExerciseId::Count)) i = 0;
  return kProfiles[i];
}
const char* exerciseName(ExerciseId id) { return exerciseProfile(id).nameZh; }
const char* exerciseCode(ExerciseId id) { return exerciseProfile(id).code; }

} // namespace rehab
