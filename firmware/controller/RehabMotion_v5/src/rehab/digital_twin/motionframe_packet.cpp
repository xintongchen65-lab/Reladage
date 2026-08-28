#include "motionframe_packet.h"
#include <cstdio>
namespace rehab {
TwinPacket makeTwinPacket(const MotionFrame& f){ TwinPacket p{}; p.timestampMs=f.timestampMs;p.primaryAngleDeg=f.primaryJointAngleDeg;p.secondaryAngleDeg=f.secondaryJointAngleDeg;p.torsoTiltDeg=f.torsoTiltDeg;p.torsoYawDeg=f.torsoYawRelativeDeg;p.planeDeviationDeg=f.planeDeviationDeg;p.stability=f.stabilityScore;return p; }
std::string twinPacketToJson(const TwinPacket& p){ char b[384];std::snprintf(b,sizeof(b),"{\"timestamp_ms\":%lu,\"primary_angle_deg\":%.1f,\"secondary_angle_deg\":%.1f,\"torso_tilt_deg\":%.1f,\"torso_yaw_deg\":%.1f,\"plane_deviation_deg\":%.1f,\"stability\":%.3f}",(unsigned long)p.timestampMs,p.primaryAngleDeg,p.secondaryAngleDeg,p.torsoTiltDeg,p.torsoYawDeg,p.planeDeviationDeg,p.stability);return b; }
}
