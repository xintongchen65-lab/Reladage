#include "rehab_protocol.h"
#include "../motion_engine/core/exercise_profile.h"
#include <cstdio>
#include <cstdlib>

namespace rehab {

static bool starts(const std::string& s,const char* p){ return s.rfind(p,0)==0; }

Command parseCommandLine(const std::string& line){
  Command c{};
  if(line=="START" || line=="CMD|START") c.type=CommandType::Start;
  else if(line=="PAUSE" || line=="CMD|PAUSE") c.type=CommandType::Pause;
  else if(line=="RESUME" || line=="CMD|RESUME") c.type=CommandType::Resume;
  else if(line=="STOP" || line=="CMD|STOP") c.type=CommandType::Stop;
  else if(line=="BEGIN_POSITION" || line=="CMD|BEGIN_POSITION") c.type=CommandType::BeginPosition;
  else if(line=="CMD|DEVICE_STATE?") c.type=CommandType::RequestDeviceState;
  else if(line=="CMD|PRESCRIPTION?") c.type=CommandType::RequestPrescription;
  else if(starts(line,"CMD|SELECT_EXERCISE|")){
    c.type=CommandType::SelectExercise;
    c.value=std::atoi(line.c_str()+20);
  } else if(starts(line,"CMD|RX_ACK|")){
    c.type=CommandType::AcknowledgePrescription;
    c.value=std::atoi(line.c_str()+11);
  }
  return c;
}

std::string makeDeviceStateMessage(bool ready,uint8_t imuMask,int batteryPct,bool wifi,bool sdReady){
  char b[192]; std::snprintf(b,sizeof(b),"EVT|DEVICE|ready=%d|imu=%02X|battery=%d|wifi=%d|sd=%d",ready?1:0,imuMask,batteryPct,wifi?1:0,sdReady?1:0); return b;
}
std::string makeExerciseSelectedMessage(ExerciseId id){ return std::string("EVT|EXERCISE|")+exerciseCode(id)+"|"+exerciseName(id); }
std::string makePrescriptionReceivedMessage(uint32_t id,uint8_t n){ char b[96]; std::snprintf(b,sizeof(b),"EVT|PRESCRIPTION|plan=%lu|items=%u",(unsigned long)id,(unsigned)n); return b; }
std::string makeSyncStateMessage(bool wifi,uint32_t q){ char b[96]; std::snprintf(b,sizeof(b),"EVT|SYNC|wifi=%d|queued=%lu",wifi?1:0,(unsigned long)q); return b; }

} // namespace rehab
