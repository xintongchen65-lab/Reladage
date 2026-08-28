#include "exercise_semantics.h"
namespace rehab {
static const ExerciseSemantics kSemantics[] = {
 {ExerciseId::DumbbellCurl,"A上臂+B前臂+E腰腹","上臂贴近躯干，前臂自然下垂","保持上臂稳定并缓慢屈肘","前臂接近目标角度时短暂停稳","缓慢伸肘回到自然下垂","肘屈曲ROM","上臂代偿/运动平面/躯干稳定/速度","幅度不足、抬肩或上臂前摆、平面偏移、躯干代偿、速度过快","upper.elbow_curl"},
 {ExerciseId::TricepsExtension,"A上臂+B前臂+E腰腹","上臂保持目标位置，肘部屈曲","固定上臂并缓慢伸肘","接近完全伸展时短暂停稳","受控回到屈肘起始位","肘伸展ROM","上臂稳定/平面/躯干/速度","幅度不足、上臂漂移、平面偏移、躯干后仰、速度过快","upper.triceps_extension"},
 {ExerciseId::ScaptionRaise,"A上臂+B前臂+E腰腹","手臂体侧自然下垂","沿肩胛平面向前外侧抬起","接近肩高保持","沿原路径回到体侧","肩部抬举角","肩胛平面偏差/躯干倾斜/稳定性","幅度不足、外展平面偏离、耸肩代偿、躯干侧倾","upper.scaption_raise"},
 {ExerciseId::WallCrawl,"A上臂+B前臂+E腰腹","面对墙面，手指在低位接触墙面","手指沿墙缓慢向上爬","达到目标高度后短暂停留","沿墙返回起始高度","抬举高度与角度","前倾/平面/速度/稳定性","爬升不足、身体前扑、路径偏移、速度过快","upper.wall_crawl"},
 {ExerciseId::KneeFlexExtend,"C大腿+D小腿+E腰腹","大腿稳定，膝关节处于起始位","缓慢完成膝屈或膝伸","达到目标ROM时短暂停稳","回到起始角度","膝关节屈伸ROM","冠状面偏移/躯干/左右差异/速度","幅度不足、膝轨迹偏移、躯干代偿、左右差异","lower.knee_flex_extend"},
 {ExerciseId::SitToStand,"C大腿+D小腿+E腰腹，A/B可辅助上肢监测","坐姿双脚稳定支撑","躯干适度前倾并连续起立","完全站稳","受控坐回椅面","起立相位与膝伸展/垂直位移","前倾/左右负荷/稳定性/节律","未完全站起、过度前倾、左右偏载、站立不稳","functional.sit_to_stand"},
 {ExerciseId::BoxSquat,"C大腿+D小腿+E腰腹，A/B辅助姿态","站姿双脚稳定，箱体位于身后","髋膝同步屈曲后坐","接近箱面达到目标深度","发力站回起始位","下蹲深度与膝角","躯干前倾/左右差异/平面/稳定性","深度不足、前倾过大、左右不对称、膝轨迹偏移","functional.box_squat"},
 {ExerciseId::StepUp,"C大腿+D小腿+E腰腹，A/B可监测扶持代偿","面向低台阶双脚站稳","抬腿踏上台阶并完成重心转移","站上台阶稳定支撑","退回地面起始位","垂直位移与膝髋相位","躯干/左右差异/稳定性/步频","抬腿不足、躯干侧倾、左右偏载、节律过快","functional.step_up"}
};
const ExerciseSemantics& exerciseSemantics(ExerciseId id){auto i=static_cast<unsigned>(id);if(i>=8)i=0;return kSemantics[i];}
}
