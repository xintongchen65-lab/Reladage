#include "game_mapping.h"
#include <algorithm>
namespace rehab {
static const GameRule kRules[] = {
 {GameId::Rowing,65,true,true,"pull","hold","标准屈肘驱动划船推进"},
 {GameId::FruitCatch,65,true,true,"reach","hold","标准伸肘触发高处水果采集"},
 {GameId::FruitCatch,70,true,true,"catch","hold","肩胛平面抬手达标后完成摘取"},
 {GameId::WhackMole,70,true,false,"hit","hold","沿墙爬升达到目标高度后触发击打"},
 {GameId::PenaltyKick,65,true,true,"kick","hold","膝关节屈伸达标后触发射门"},
 {GameId::VitalityPark,65,true,false,"advance","hold","完整坐站循环推动角色前进"},
 {GameId::BalanceTrail,70,true,false,"step","hold","稳定深蹲后解锁下一段平衡步道"},
 {GameId::StepAdventure,65,true,true,"climb","hold","标准台阶循环触发攀登动作"}
};
GameId defaultGameFor(ExerciseId id){ return gameRuleFor(id).game; }
const GameRule& gameRuleFor(ExerciseId id){ auto i=static_cast<unsigned>(id); if(i>=8)i=0; return kRules[i]; }
const char* gameName(GameId id){ switch(id){case GameId::FruitCatch:return "摘水果";case GameId::PenaltyKick:return "点球大战";case GameId::VitalityPark:return "活力公园";case GameId::WhackMole:return "打地鼠";case GameId::Rowing:return "划船挑战";case GameId::Racing:return "康复赛车";case GameId::BalanceTrail:return "平衡步道";case GameId::StepAdventure:return "台阶冒险";} return "训练游戏"; }
GameEvent mapFeedbackToGame(ExerciseId id,const ExerciseFeedback& f){
  const auto& r=gameRuleFor(id); GameEvent e{}; e.game=r.game;
  const bool qualityOk=f.qualityScore>=r.minQualityScore;
  e.trigger=f.repCompleted && qualityOk && (!r.requireAcceptedRep || f.repAccepted);
  e.strength=std::clamp(static_cast<int>(f.qualityScore),0,100);
  e.action=e.trigger?r.goodAction:r.badAction;
  if(!f.repCompleted)e.reason="waiting_for_completed_rep";
  else if(!f.repAccepted)e.reason="motion_quality_rejected";
  else if(!qualityOk)e.reason="quality_score_below_game_threshold";
  else e.reason="accepted_motion_event";
  return e;
}
}
