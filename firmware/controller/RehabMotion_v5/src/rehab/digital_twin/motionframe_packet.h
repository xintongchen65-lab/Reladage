#pragma once
#include "../motion_engine/core/motion_types.h"
#include <string>
namespace rehab {
struct TwinPacket {
  uint32_t timestampMs{0};
  float primaryAngleDeg{0};
  float secondaryAngleDeg{0};
  float torsoTiltDeg{0};
  float torsoYawDeg{0};
  float planeDeviationDeg{0};
  float stability{1};
};
TwinPacket makeTwinPacket(const MotionFrame& frame);
std::string twinPacketToJson(const TwinPacket& p);
}
