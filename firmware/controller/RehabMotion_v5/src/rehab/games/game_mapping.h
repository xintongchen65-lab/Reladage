#pragma once
#include "../motion_engine/core/motion_types.h"
#include <string>
namespace rehab {
enum class GameId : uint8_t { FruitCatch, PenaltyKick, VitalityPark, WhackMole, Rowing, Racing, BalanceTrail, StepAdventure };
struct GameRule {
  GameId game{GameId::FruitCatch};
  int minQualityScore{60};
  bool requireAcceptedRep{true};
  bool scaleStrengthByRom{true};
  const char* goodAction{"score"};
  const char* badAction{"hold"};
  const char* instructionZh{"完成标准动作触发游戏"};
};
struct GameEvent {
  GameId game{GameId::FruitCatch};
  bool trigger{false};
  int strength{0};
  std::string action;
  std::string reason;
};
GameId defaultGameFor(ExerciseId id);
const GameRule& gameRuleFor(ExerciseId id);
GameEvent mapFeedbackToGame(ExerciseId id,const ExerciseFeedback& feedback);
const char* gameName(GameId id);
}
