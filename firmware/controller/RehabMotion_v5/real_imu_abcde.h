#pragma once

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <freertos/FreeRTOS.h>
#include <math.h>
#include <string.h>

// =====================================================
// WT901 BLE A/B/C/D/E input adapter
//
// Pair mapping:
//   Left  joint = A/B   A = proximal/fixed, B = distal/moving
//   Right joint = C/D   C = proximal/fixed, D = distal/moving
//   Torso reference = E (waist/abdomen), WT901BLE67
//
// K10 functional hinge-axis elbow angle:
//   No sensor X/Y/Z axis is assumed to be the anatomical limb long axis.
//   Calibration stores full A/B orientations plus the K6 reference qRel0 = inverse(qA) * qB.
//   During training four candidate relative-rotation formulations are evaluated on synchronized A/B packet pairs; A^-1*B is retained as the synchronized relative orientation representation.
 //   K10 no longer uses the total 3D relative-rotation magnitude as the elbow angle.
 //   Instead it learns a functional flexion/extension axis from several slow flex/extend
 //   motions, then integrates only the signed incremental rotation projected on that axis.
//
// K7 does NOT assume in advance whether the WT901 Euler->quaternion convention
// should use left/world-frame or right/body-frame composition. That is why C1-C4
// are logged together. The correct candidate must be selected from real data by
// requiring shoulder-only common motion to stay low while true elbow motion rises.
// =====================================================

#define WT901_ABCD_PRINT_RAW 0

static NimBLEUUID WT901_SERVICE_UUID("0000ffe5-0000-1000-8000-00805f9a34fb");
static NimBLEUUID WT901_NOTIFY_UUID ("0000ffe4-0000-1000-8000-00805f9a34fb");
static NimBLEUUID WT901_WRITE_UUID  ("0000ffe9-0000-1000-8000-00805f9a34fb");

struct WT901EulerData {
  float ax = 0.0f;
  float ay = 0.0f;
  float az = 0.0f;
  float gx = 0.0f;
  float gy = 0.0f;
  float gz = 0.0f;
  float roll = 0.0f;
  float pitch = 0.0f;
  float yaw = 0.0f;
  unsigned long sampleMs = 0;
  bool valid = false;
};

struct WT901Slot {
  const char *role = "";
  String mac;
  NimBLEAddress address;
  bool addressReady = false;
  NimBLEClient *client = NULL;
  NimBLERemoteCharacteristic *notifyChar = NULL;
  NimBLERemoteCharacteristic *writeChar = NULL;

  bool found = false;
  bool connected = false;
  bool notifyReady = false;

  uint8_t buffer[20];
  uint8_t bufferIndex = 0;

  uint32_t packetCount = 0;
  uint32_t badPacketCount = 0;
  unsigned long lastPacketMs = 0;
  int rssi = -999;

  // V4.4.30: magnetometer-independent vertical-rotation tracker.
  // The local gyro is transformed to world Z using roll/pitch only (the world-Z
  // component is mathematically independent of Euler yaw).  This lets B and E
  // accumulate real rotation about the body vertical without trusting fused yaw.
  bool fusedVerticalYawTracking = false;
  float fusedVerticalYawIntegralDeg = 0.0f;
  float fusedVerticalYawBiasDegS = 0.0f;
  float fusedVerticalYawRateDegS = 0.0f;
  float fusedVerticalYawPrevRateDegS = 0.0f;
  bool fusedVerticalYawPrevReady = false;
  unsigned long fusedVerticalYawLastSampleMs = 0;

  WT901EulerData latest;
};

struct WT901Quat {
  float w, x, y, z;
};

struct WT901JointPair {
  const char *jointName = "";
  int fixedIndex = 0;
  int movingIndex = 1;

  bool zeroCalibrated = false;
  bool filterReady = false;
  bool angleReady = false;

  float zeroRollDiff = 0.0f;
  float zeroPitchDiff = 0.0f;
  float zeroYawDiff = 0.0f;

  // K2 metric test: remember the proximal/fixed IMU's own orientation at zero.
  // For upper-limb mode this is A (left upper arm) / C (right upper arm).
  // These values are diagnostic only; they do NOT affect rep counting yet.
  float fixedZeroRoll = 0.0f;
  float fixedZeroPitch = 0.0f;
  float fixedZeroYaw = 0.0f;
  float fixedMoveRollDeg = 0.0f;
  float fixedMovePitchDeg = 0.0f;
  float fixedMoveYawDeg = 0.0f;
  float fixedOrientationDeviationDeg = 0.0f;  // K2 reference: full 3D orientation difference
  // K3: direction change of each local sensor basis axis. Rotating around one axis
  // does not change that axis direction, so these values help identify which axis
  // is physically aligned with the upper-arm long axis.
  float fixedXAxisDeviationDeg = 0.0f;
  float fixedYAxisDeviationDeg = 0.0f;
  float fixedZAxisDeviationDeg = 0.0f;

  // K6: full A/B relative-orientation zero. No anatomical axis guessing.
  bool relativePoseReady = false;
  float zeroRelW = 1.0f;
  float zeroRelX = 0.0f;
  float zeroRelY = 0.0f;
  float zeroRelZ = 0.0f;
  float rawJointAngleDeg = 0.0f;

  // K7 sync + candidate diagnostics. K6 counting candidate remains A^-1*B,
  // but it is updated only from a synchronized A/B packet pair.
  float zeroFixedW = 1.0f, zeroFixedX = 0.0f, zeroFixedY = 0.0f, zeroFixedZ = 0.0f;
  float zeroMovingW = 1.0f, zeroMovingX = 0.0f, zeroMovingY = 0.0f, zeroMovingZ = 0.0f;
  float candidateAinvBDeg = 0.0f;
  float candidateBAinvDeg = 0.0f;
  float candidateWorldDeltaDeg = 0.0f;
  float candidateBodyDeltaDeg = 0.0f;
  unsigned long lastPairSkewMs = 0;
  uint32_t syncAcceptedCount = 0;
  uint32_t syncDroppedSkewCount = 0;
  uint32_t syncWaitingCount = 0;

  // ---------- K10 functional hinge-axis calibration / signed angle ----------
  // qRel = inverse(qA) * qB maps the distal sensor orientation into the proximal
  // sensor frame.  For a hinge-like elbow motion, the left-increment
  // dq = qRel_now * inverse(qRel_prev) should rotate primarily around one axis
  // that is approximately fixed in the proximal sensor coordinate frame.
  bool k10FunctionalCalActive = false;
  bool k10FunctionalAxisReady = false;
  bool k10WaitStraight = false;
  bool k10TrainingAngleReady = false;
  bool k10CalibrationFailed = false;
  unsigned long k10CalStartMs = 0;
  unsigned long k10StraightHoldStartMs = 0;
  unsigned long k10PrevSampleMs = 0;
  WT901Quat k10PrevRel = {1.0f, 0.0f, 0.0f, 0.0f};
  bool k10PrevRelReady = false;

  // Symmetric covariance of accepted incremental rotation vectors, degrees^2.
  float k10Cxx = 0.0f, k10Cxy = 0.0f, k10Cxz = 0.0f;
  float k10Cyy = 0.0f, k10Cyz = 0.0f, k10Czz = 0.0f;
  uint32_t k10AxisSamples = 0;

  // K10.2 calibration is cycle-driven with adaptive baseline/return detection.
  // A calibration cycle is: leave straight pose -> reach enough flexion -> return near straight.
  uint8_t k10CalCyclesCompleted = 0;
  uint8_t k10CalCyclePhase = 0; // 0=WAIT_FLEX, 1=WAIT_RETURN
  unsigned long k10CalPhaseHoldStartMs = 0;
  float k10CalCyclePeakDeg = 0.0f;
  float k10CalCycleBaselineDeg = 999.0f;
  bool k10CalCycleBaselineReady = false;
  WT901Quat k10TrainingZeroRel = {1.0f, 0.0f, 0.0f, 0.0f};
  bool k10TrainingZeroReady = false;
  float k10TrainingZeroDistanceDeg = 0.0f;

  float k10FirstVx = 0.0f, k10FirstVy = 0.0f, k10FirstVz = 0.0f;
  bool k10FirstVectorReady = false;
  WT901Quat k10PeakRel = {1.0f, 0.0f, 0.0f, 0.0f};
  float k10PeakTotalDeg = 0.0f;

  float k10AxisX = 1.0f, k10AxisY = 0.0f, k10AxisZ = 0.0f;
  float k10AxisQuality = 0.0f;
  float k10FunctionalAngleDeg = 0.0f;
  float k10FunctionalSpeedDegS = 0.0f;
  float k10ResidualIncrementDeg = 0.0f;
  float k10TotalRelativeFromStaticZeroDeg = 0.0f;
  uint32_t k10RejectedJumpCount = 0;

  float filteredAngleDeg = 0.0f;
  float rawPitchDeg = 0.0f; // legacy Euler debug only; NOT used for counting in K7
  float rollCrossDeg = 0.0f;
  float yawCrossDeg = 0.0f;

  uint32_t lastFixedPackets = 0;
  uint32_t lastMovingPackets = 0;
};

static constexpr int WT901_SENSOR_COUNT = 5; // A/B/C/D limbs + E waist/abdomen
// Compatibility alias retained because K11 headers already use this symbol as a slot-count bound.
static constexpr int WT901_ABCD_COUNT = WT901_SENSOR_COUNT;
static constexpr int WT901_ACTIVE_COUNT = 5;
static constexpr int WT901_ACTIVE_INDICES[WT901_ACTIVE_COUNT] = {0, 1, 2, 3, 4};
static constexpr int WT901_PAIR_COUNT = 2;

// NimBLE's connection limit is compile-time, not a runtime setting. build_opt.h
// raises it to six so five sensors can stay connected with one spare slot.
static_assert(NIMBLE_MAX_CONNECTIONS >= WT901_ACTIVE_COUNT,
              "NimBLE was compiled for fewer than five connections. Keep build_opt.h in the sketch folder and use NimBLE-Arduino 2.5.0 or newer.");

static constexpr uint8_t WT901_CONNECT_ATTEMPTS = 3;
static constexpr uint32_t WT901_CONNECT_TIMEOUT_MS = 10000;
static constexpr uint32_t WT901_FIRST_PACKET_TIMEOUT_MS = 3000;
static constexpr uint32_t WT901_RETRY_DELAY_MS = 600;

// V4.4.27: idle/wear-page hot reconnect. A sensor that was OFF at boot can be
// powered on later and discovered without rebooting the ESP32.
static constexpr uint32_t WT901_RECONNECT_INTERVAL_MS = 2800;
static constexpr uint32_t WT901_RECONNECT_SCAN_MS = 1800;
static constexpr uint32_t WT901_RECONNECT_CONNECT_TIMEOUT_MS = 4500;
static constexpr uint32_t WT901_RECONNECT_FIRST_PACKET_TIMEOUT_MS = 1800;

static WT901Slot wt901Slots[WT901_ABCD_COUNT];
static WT901JointPair wt901Pairs[WT901_PAIR_COUNT];

// K12.1: preserve every parsed 0x61 sample instead of letting WT901Slot::latest
// overwrite intermediate gyro packets while TFT/SD/Serial work is blocking loop().
// 64 samples gives several seconds of backlog at the current WT901 BLE rate.
static constexpr uint8_t WT901_SAMPLE_QUEUE_SIZE = 64;
struct WT901SampleQueue {
  WT901EulerData items[WT901_SAMPLE_QUEUE_SIZE];
  volatile uint8_t head = 0;
  volatile uint8_t tail = 0;
  volatile uint8_t count = 0;
  volatile uint32_t dropped = 0;
  volatile uint32_t pushed = 0;
  volatile uint32_t popped = 0;
};
static WT901SampleQueue wt901SampleQueues[WT901_ABCD_COUNT];
static portMUX_TYPE wt901SampleQueueMux = portMUX_INITIALIZER_UNLOCKED;

static void wt901ClearSampleQueue(int index) {
  if (index < 0 || index >= WT901_ABCD_COUNT) return;
  portENTER_CRITICAL(&wt901SampleQueueMux);
  wt901SampleQueues[index].head = 0;
  wt901SampleQueues[index].tail = 0;
  wt901SampleQueues[index].count = 0;
  // K12.1.1: queue diagnostics are per training session. Before this fix,
  // packets accumulated while the user was still in the menu made QDROP look
  // non-zero even though formal training itself had no overflow.
  wt901SampleQueues[index].dropped = 0;
  wt901SampleQueues[index].pushed = 0;
  wt901SampleQueues[index].popped = 0;
  portEXIT_CRITICAL(&wt901SampleQueueMux);
}

static void wt901PushSample(int index, const WT901EulerData &d) {
  if (index < 0 || index >= WT901_ABCD_COUNT) return;
  WT901SampleQueue &q = wt901SampleQueues[index];
  portENTER_CRITICAL(&wt901SampleQueueMux);
  q.pushed++;
  if (q.count >= WT901_SAMPLE_QUEUE_SIZE) {
    // Do not silently overwrite an old sample: that would recreate an unknown
    // integration gap. Keep the queued chronological stream and expose QDROP.
    q.dropped++;
    portEXIT_CRITICAL(&wt901SampleQueueMux);
    return;
  }
  q.items[q.head] = d;
  q.head = (uint8_t)((q.head + 1U) % WT901_SAMPLE_QUEUE_SIZE);
  q.count++;
  portEXIT_CRITICAL(&wt901SampleQueueMux);
}

static bool wt901PopSample(int index, WT901EulerData &out) {
  if (index < 0 || index >= WT901_ABCD_COUNT) return false;
  WT901SampleQueue &q = wt901SampleQueues[index];
  bool ok = false;
  portENTER_CRITICAL(&wt901SampleQueueMux);
  if (q.count > 0) {
    out = q.items[q.tail];
    q.tail = (uint8_t)((q.tail + 1U) % WT901_SAMPLE_QUEUE_SIZE);
    q.count--;
    q.popped++;
    ok = true;
  }
  portEXIT_CRITICAL(&wt901SampleQueueMux);
  return ok;
}

static uint8_t wt901SampleQueueDepth(int index) {
  if (index < 0 || index >= WT901_ABCD_COUNT) return 0;
  portENTER_CRITICAL(&wt901SampleQueueMux);
  uint8_t v = wt901SampleQueues[index].count;
  portEXIT_CRITICAL(&wt901SampleQueueMux);
  return v;
}

static uint32_t wt901SampleQueueDropped(int index) {
  if (index < 0 || index >= WT901_ABCD_COUNT) return 0;
  portENTER_CRITICAL(&wt901SampleQueueMux);
  uint32_t v = wt901SampleQueues[index].dropped;
  portEXIT_CRITICAL(&wt901SampleQueueMux);
  return v;
}

// Coherent read-side A/B/E snapshot used by calibration state logic.
// All three latest structs are copied while BLE publication is excluded.
static bool wt901SnapshotLatestABE(WT901EulerData &a, WT901EulerData &b, WT901EulerData &e) {
  portENTER_CRITICAL(&wt901SampleQueueMux);
  a = wt901Slots[0].latest;
  b = wt901Slots[1].latest;
  e = wt901Slots[4].latest;
  portEXIT_CRITICAL(&wt901SampleQueueMux);
  return a.valid && b.valid && e.valid;
}

static bool wt901ScanFinished = false;
static bool wt901ConnectStarted = false;
static unsigned long wt901LastReconnectAttemptMs = 0;

static constexpr float WT901_FILTER_ALPHA = 0.40f;
static constexpr float WT901_DEAD_ZONE_DEG = 3.0f;
static constexpr float WT901_MAX_ANGLE_DEG = 160.0f;
static constexpr unsigned long WT901_MAX_PAIR_SKEW_MS = 80; // K7 diagnostic gate; logged for validation

static float wt901Wrap180(float x) {
  while (x > 180.0f) x -= 360.0f;
  while (x < -180.0f) x += 360.0f;
  return x;
}

static int16_t wt901LeInt16(const uint8_t *p, int index) {
  return (int16_t)((uint16_t)p[index] | ((uint16_t)p[index + 1] << 8));
}

static void wt901ClearData(WT901EulerData &d) {
  d.ax = 0.0f;
  d.ay = 0.0f;
  d.az = 0.0f;
  d.gx = 0.0f;
  d.gy = 0.0f;
  d.gz = 0.0f;
  d.roll = 0.0f;
  d.pitch = 0.0f;
  d.yaw = 0.0f;
  d.sampleMs = 0;
  d.valid = false;
}

static void wt901SetupSlot(int index, const char *role, const char *mac) {
  WT901Slot &imu = wt901Slots[index];

  imu.role = role;
  imu.mac = String(mac);
  imu.mac.toLowerCase();

  imu.address = NimBLEAddress();
  imu.addressReady = false;
  imu.client = NULL;
  imu.notifyChar = NULL;
  imu.writeChar = NULL;
  imu.found = false;
  imu.connected = false;
  imu.notifyReady = false;
  memset(imu.buffer, 0, sizeof(imu.buffer));
  imu.bufferIndex = 0;
  imu.packetCount = 0;
  imu.badPacketCount = 0;
  imu.lastPacketMs = 0;
  imu.rssi = -999;
  imu.fusedVerticalYawTracking = false;
  imu.fusedVerticalYawIntegralDeg = 0.0f;
  imu.fusedVerticalYawBiasDegS = 0.0f;
  imu.fusedVerticalYawRateDegS = 0.0f;
  imu.fusedVerticalYawPrevRateDegS = 0.0f;
  imu.fusedVerticalYawPrevReady = false;
  imu.fusedVerticalYawLastSampleMs = 0;
  wt901ClearData(imu.latest);
  wt901ClearSampleQueue(index);
}

static void wt901SetupPair(int pairIndex, const char *jointName, int fixedIndex, int movingIndex) {
  WT901JointPair &pair = wt901Pairs[pairIndex];
  pair.jointName = jointName;
  pair.fixedIndex = fixedIndex;
  pair.movingIndex = movingIndex;

  pair.zeroCalibrated = false;
  pair.filterReady = false;
  pair.angleReady = false;

  pair.zeroRollDiff = 0.0f;
  pair.zeroPitchDiff = 0.0f;
  pair.zeroYawDiff = 0.0f;

  pair.fixedZeroRoll = 0.0f;
  pair.fixedZeroPitch = 0.0f;
  pair.fixedZeroYaw = 0.0f;
  pair.fixedMoveRollDeg = 0.0f;
  pair.fixedMovePitchDeg = 0.0f;
  pair.fixedMoveYawDeg = 0.0f;
  pair.fixedOrientationDeviationDeg = 0.0f;
  pair.fixedXAxisDeviationDeg = 0.0f;
  pair.fixedYAxisDeviationDeg = 0.0f;
  pair.fixedZAxisDeviationDeg = 0.0f;

  pair.relativePoseReady = false;
  pair.zeroRelW = 1.0f;
  pair.zeroRelX = 0.0f;
  pair.zeroRelY = 0.0f;
  pair.zeroRelZ = 0.0f;
  pair.rawJointAngleDeg = 0.0f;
  pair.zeroFixedW = 1.0f; pair.zeroFixedX = 0.0f; pair.zeroFixedY = 0.0f; pair.zeroFixedZ = 0.0f;
  pair.zeroMovingW = 1.0f; pair.zeroMovingX = 0.0f; pair.zeroMovingY = 0.0f; pair.zeroMovingZ = 0.0f;
  pair.candidateAinvBDeg = 0.0f;
  pair.candidateBAinvDeg = 0.0f;
  pair.candidateWorldDeltaDeg = 0.0f;
  pair.candidateBodyDeltaDeg = 0.0f;
  pair.lastPairSkewMs = 0;
  pair.syncAcceptedCount = 0;
  pair.syncDroppedSkewCount = 0;
  pair.syncWaitingCount = 0;

  pair.k10FunctionalCalActive = false;
  pair.k10FunctionalAxisReady = false;
  pair.k10WaitStraight = false;
  pair.k10TrainingAngleReady = false;
  pair.k10CalibrationFailed = false;
  pair.k10CalStartMs = 0;
  pair.k10StraightHoldStartMs = 0;
  pair.k10PrevSampleMs = 0;
  pair.k10PrevRel = {1.0f, 0.0f, 0.0f, 0.0f};
  pair.k10PrevRelReady = false;
  pair.k10Cxx = pair.k10Cxy = pair.k10Cxz = 0.0f;
  pair.k10Cyy = pair.k10Cyz = pair.k10Czz = 0.0f;
  pair.k10AxisSamples = 0;
  pair.k10CalCyclesCompleted = 0;
  pair.k10CalCyclePhase = 0;
  pair.k10CalPhaseHoldStartMs = 0;
  pair.k10CalCyclePeakDeg = 0.0f;
  pair.k10CalCycleBaselineDeg = 999.0f;
  pair.k10CalCycleBaselineReady = false;
  pair.k10TrainingZeroReady = false;
  pair.k10TrainingZeroDistanceDeg = 0.0f;
  pair.k10FirstVx = pair.k10FirstVy = pair.k10FirstVz = 0.0f;
  pair.k10FirstVectorReady = false;
  pair.k10PeakRel = {1.0f, 0.0f, 0.0f, 0.0f};
  pair.k10PeakTotalDeg = 0.0f;
  pair.k10AxisX = 1.0f; pair.k10AxisY = 0.0f; pair.k10AxisZ = 0.0f;
  pair.k10AxisQuality = 0.0f;
  pair.k10FunctionalAngleDeg = 0.0f;
  pair.k10FunctionalSpeedDegS = 0.0f;
  pair.k10ResidualIncrementDeg = 0.0f;
  pair.k10TotalRelativeFromStaticZeroDeg = 0.0f;
  pair.k10RejectedJumpCount = 0;

  pair.filteredAngleDeg = 0.0f;
  pair.rawPitchDeg = 0.0f;
  pair.rollCrossDeg = 0.0f;
  pair.yawCrossDeg = 0.0f;

  pair.lastFixedPackets = 0;
  pair.lastMovingPackets = 0;
}

static void wt901InitSlotsAndPairs() {
  // MAC identities verified in previous scan tests.
  wt901SetupSlot(0, "A", "ed:cb:50:f9:a2:76");
  wt901SetupSlot(1, "B", "f4:04:f0:83:3a:0e");
  wt901SetupSlot(2, "C", "e9:36:69:8c:77:a4");
  wt901SetupSlot(3, "D", "ca:b2:9f:35:e2:9a");
  // E = waist/abdomen torso reference. Screenshot-verified WT901BLE67.
  wt901SetupSlot(4, "E", "f3:72:ae:1f:e4:e9");

  // left = A/B, right = C/D; E is not part of a joint pair
  wt901SetupPair(0, "AB", 0, 1);
  wt901SetupPair(1, "CD", 2, 3);

  wt901ScanFinished = false;
  wt901ConnectStarted = false;
}

static int wt901FindByMac(const String &addr) {
  for (int i = 0; i < WT901_ACTIVE_COUNT; i++) {
    int slot = WT901_ACTIVE_INDICES[i];
    if (addr == wt901Slots[slot].mac) return slot;
  }
  return -1;
}

static int wt901FindByNotifyChar(NimBLERemoteCharacteristic *ch) {
  for (int i = 0; i < WT901_ACTIVE_COUNT; i++) {
    int slot = WT901_ACTIVE_INDICES[i];
    if (wt901Slots[slot].notifyChar == ch) return slot;
  }
  return -1;
}

static bool wt901AllFound() {
  for (int i = 0; i < WT901_ACTIVE_COUNT; i++) {
    int slot = WT901_ACTIVE_INDICES[i];
    if (!wt901Slots[slot].found) return false;
  }
  return true;
}

static bool wt901SlotReceiving(int index) {
  WT901Slot &imu = wt901Slots[index];
  return imu.connected && imu.notifyReady && imu.packetCount > 0 &&
         imu.lastPacketMs > 0 && (millis() - imu.lastPacketMs < 3000);
}

static bool wt901AllReceiving() {
  for (int i = 0; i < WT901_ACTIVE_COUNT; i++) {
    if (!wt901SlotReceiving(WT901_ACTIVE_INDICES[i])) return false;
  }
  return true;
}

static bool wt901PairReceiving(const WT901JointPair &pair) {
  return wt901SlotReceiving(pair.fixedIndex) && wt901SlotReceiving(pair.movingIndex);
}

static bool wt901PairPoseReady(const WT901JointPair &pair) {
  const WT901Slot &fixed = wt901Slots[pair.fixedIndex];
  const WT901Slot &moving = wt901Slots[pair.movingIndex];

  return fixed.latest.valid && moving.latest.valid &&
         fixed.connected && moving.connected &&
         fixed.notifyReady && moving.notifyReady;
}

static void wt901PrintRawPacket(const char *role, const uint8_t *packet, size_t len) {
  Serial.print("RAW_IMU_");
  Serial.print(role);
  Serial.print(":");
  for (size_t i = 0; i < len; i++) {
    if (packet[i] < 0x10) Serial.print('0');
    Serial.print(packet[i], HEX);
    if (i + 1 < len) Serial.print(' ');
  }
  Serial.println();
}

// V4.4.30: world-vertical angular velocity from local gyro + fused roll/pitch.
// For ZYX yaw-pitch-roll, the world-Z row of the rotation matrix is
// [-sin(pitch), cos(pitch)sin(roll), cos(pitch)cos(roll)], so yaw cancels exactly.
static float wt901WorldVerticalGyroRateDegS(const WT901EulerData &d) {
  const float d2r = 0.01745329251994329577f;
  const float r = d.roll * d2r;
  const float p = d.pitch * d2r;
  const float sr = sinf(r), cr = cosf(r), sp = sinf(p), cp = cosf(p);
  const float v = (-sp) * d.gx + (cp * sr) * d.gy + (cp * cr) * d.gz;
  return isfinite(v) ? v : 0.0f;
}

static void wt901UpdateFusedVerticalYawTracker(WT901Slot &imu, const WT901EulerData &d) {
  const float raw = wt901WorldVerticalGyroRateDegS(d);
  imu.fusedVerticalYawRateDegS = raw;
  if (!imu.fusedVerticalYawTracking || d.sampleMs == 0) return;

  if (imu.fusedVerticalYawLastSampleMs == 0) {
    imu.fusedVerticalYawLastSampleMs = d.sampleMs;
    imu.fusedVerticalYawBiasDegS = (fabsf(raw) <= 4.0f) ? raw : 0.0f;
    imu.fusedVerticalYawPrevRateDegS = 0.0f;
    imu.fusedVerticalYawPrevReady = false;
    return;
  }

  const unsigned long dtMs = d.sampleMs - imu.fusedVerticalYawLastSampleMs;
  imu.fusedVerticalYawLastSampleMs = d.sampleMs;
  if (dtMs == 0) return;
  if (dtMs > 650UL) {
    imu.fusedVerticalYawPrevReady = false;
    return;
  }

  // Bias adaptation is allowed only around genuine rest.  A deliberate arm/body
  // turn is much faster and therefore cannot be learned away as zero bias.
  if (fabsf(raw) <= 3.0f) {
    imu.fusedVerticalYawBiasDegS = 0.995f * imu.fusedVerticalYawBiasDegS + 0.005f * raw;
  }
  float rate = raw - imu.fusedVerticalYawBiasDegS;
  if (fabsf(rate) < 1.5f) rate = 0.0f;

  const float dt = (float)dtMs * 0.001f;
  const float useRate = imu.fusedVerticalYawPrevReady
      ? 0.5f * (imu.fusedVerticalYawPrevRateDegS + rate) : rate;
  imu.fusedVerticalYawIntegralDeg += useRate * dt;
  imu.fusedVerticalYawPrevRateDegS = rate;
  imu.fusedVerticalYawPrevReady = true;
}

static void wt901ParsePacket20(int index, const uint8_t *p) {
  WT901Slot &imu = wt901Slots[index];

  if (p[0] != 0x55) return;
  uint8_t flag = p[1];

  if (flag == 0x61) {
    int16_t rawAx = wt901LeInt16(p, 2);
    int16_t rawAy = wt901LeInt16(p, 4);
    int16_t rawAz = wt901LeInt16(p, 6);

    int16_t rawGx = wt901LeInt16(p, 8);
    int16_t rawGy = wt901LeInt16(p, 10);
    int16_t rawGz = wt901LeInt16(p, 12);

    int16_t rawRoll  = wt901LeInt16(p, 14);
    int16_t rawPitch = wt901LeInt16(p, 16);
    int16_t rawYaw   = wt901LeInt16(p, 18);

    WT901EulerData parsed;
    parsed.ax = rawAx / 32768.0f * 16.0f;
    parsed.ay = rawAy / 32768.0f * 16.0f;
    parsed.az = rawAz / 32768.0f * 16.0f;

    parsed.gx = rawGx / 32768.0f * 2000.0f;
    parsed.gy = rawGy / 32768.0f * 2000.0f;
    parsed.gz = rawGz / 32768.0f * 2000.0f;

    parsed.roll  = rawRoll  / 32768.0f * 180.0f;
    parsed.pitch = rawPitch / 32768.0f * 180.0f;
    parsed.yaw   = rawYaw   / 32768.0f * 180.0f;
    unsigned long packetNowMs = millis();
    parsed.sampleMs = packetNowMs;
    parsed.valid = true;

    // Publish latest + update the V4.4.30 vertical-rotation integral under the
    // same mux, so a reset/read can never observe a half-updated tracker state.
    // This exact BLE-packet integration also avoids losing a fast 90-degree plane
    // rotation while the main loop is blocked by TFT/SD/Serial work.
    portENTER_CRITICAL(&wt901SampleQueueMux);
    wt901UpdateFusedVerticalYawTracker(imu, parsed);
    imu.latest = parsed;
    portEXIT_CRITICAL(&wt901SampleQueueMux);

    // K12.1: preserve this exact parsed sample for K11's FIFO integrator.
    wt901PushSample(index, parsed);

    imu.packetCount++;
    imu.lastPacketMs = packetNowMs;

#if WT901_ABCD_PRINT_RAW
    Serial.printf(
      "IMU_%s,%lu,#%lu,roll=%.2f,pitch=%.2f,yaw=%.2f,gx=%.2f,gy=%.2f,gz=%.2f,ax=%.3f,ay=%.3f,az=%.3f\n",
      imu.role,
      millis(),
      (unsigned long)imu.packetCount,
      parsed.roll, parsed.pitch, parsed.yaw,
      parsed.gx, parsed.gy, parsed.gz,
      parsed.ax, parsed.ay, parsed.az
    );
#endif
  } else if (flag == 0x71) {
    Serial.print("REG_PACKET_IMU_");
    Serial.print(imu.role);
    Serial.print(' ');
    wt901PrintRawPacket(imu.role, p, 20);
  } else {
    imu.badPacketCount++;
    Serial.print("UNKNOWN_PACKET_IMU_");
    Serial.print(imu.role);
    Serial.print(' ');
    wt901PrintRawPacket(imu.role, p, 20);
  }
}

static void wt901FeedBytes(int index, const uint8_t *data, size_t len) {
  WT901Slot &imu = wt901Slots[index];

  for (size_t i = 0; i < len; i++) {
    uint8_t b = data[i];

    if (imu.bufferIndex == 0) {
      if (b == 0x55) {
        imu.buffer[imu.bufferIndex++] = b;
      }
      continue;
    }

    imu.buffer[imu.bufferIndex++] = b;

    if (imu.bufferIndex == 2) {
      if (imu.buffer[1] != 0x61 && imu.buffer[1] != 0x71) {
        imu.bufferIndex = 0;
      }
      continue;
    }

    if (imu.bufferIndex >= 20) {
      wt901ParsePacket20(index, imu.buffer);
      imu.bufferIndex = 0;
    }
  }
}

static void wt901NotifyCallback(
  NimBLERemoteCharacteristic *remoteCharacteristic,
  uint8_t *data,
  size_t length,
  bool isNotify
) {
  (void)isNotify;
  int index = wt901FindByNotifyChar(remoteCharacteristic);
  if (index < 0) {
    Serial.println("Notify from unknown characteristic.");
    return;
  }
  wt901FeedBytes(index, data, length);
}

class WT901ClientCallbacks : public NimBLEClientCallbacks {
public:
  explicit WT901ClientCallbacks(int index) : _index(index) {}

  void onConnect(NimBLEClient *client) override {
    (void)client;
    Serial.print("BLE client connected: IMU_");
    Serial.println(wt901Slots[_index].role);
  }

  void onDisconnect(NimBLEClient *client, int reason) override {
    (void)client;
    wt901Slots[_index].connected = false;
    wt901Slots[_index].notifyReady = false;
    Serial.print("BLE disconnected: IMU_");
    Serial.print(wt901Slots[_index].role);
    Serial.print("  reason=");
    Serial.println(reason);
  }

private:
  int _index;
};

class WT901ScanCallbacks : public NimBLEScanCallbacks {
  void onResult(const NimBLEAdvertisedDevice *advertisedDevice) override {
    if (advertisedDevice == NULL) return;

    String addr = advertisedDevice->getAddress().toString().c_str();
    addr.toLowerCase();

    String name = "";
    if (advertisedDevice->haveName()) {
      name = advertisedDevice->getName().c_str();
    }

    int index = wt901FindByMac(addr);
    if (index < 0) {
      return;
    }

    WT901Slot &imu = wt901Slots[index];
    // Copy the full address, including public/random address type. Scan result
    // objects belong to NimBLE and are released after scanning.
    imu.address = advertisedDevice->getAddress();
    imu.addressReady = true;
    imu.found = true;
    imu.rssi = advertisedDevice->getRSSI();

    Serial.print("FOUND IMU_");
    Serial.print(imu.role);
    Serial.print("  mac=");
    Serial.print(addr);
    Serial.print("  name=");
    Serial.print(name);
    Serial.print("  rssi=");
    Serial.println(imu.rssi);

    if (wt901AllFound()) {
      Serial.println("A/B/C/D/E found. Stop scanning.");
      NimBLEDevice::getScan()->stop();
    }
  }
};

static WT901ScanCallbacks wt901ScanCallbacks;

static void wt901ScanForTargets(uint32_t scanDurationMs = 12000) {
  Serial.println();
  Serial.println("Scanning for IMU_A / IMU_B / IMU_C / IMU_D / IMU_E...");
  Serial.println("Target list:");
  for (int i = 0; i < WT901_ACTIVE_COUNT; i++) {
    int slot = WT901_ACTIVE_INDICES[i];
    Serial.print("  IMU_");
    Serial.print(wt901Slots[slot].role);
    Serial.print(" = ");
    Serial.println(wt901Slots[slot].mac);
  }

  NimBLEScan *scan = NimBLEDevice::getScan();
  scan->setScanCallbacks(&wt901ScanCallbacks, false);
  scan->setInterval(1349);
  scan->setWindow(449);
  scan->setActiveScan(true);
  // NimBLE-Arduino 2.x uses milliseconds. getResults() is the blocking scan
  // API; start() is asynchronous and would otherwise be cleared immediately.
  scan->getResults(scanDurationMs, false);
  scan->clearResults();

  Serial.println();
  Serial.println("========== A/B/C/D/E SCAN SUMMARY ==========");
  int foundCount = 0;
  for (int i = 0; i < WT901_ACTIVE_COUNT; i++) {
    int slot = WT901_ACTIVE_INDICES[i];
    if (wt901Slots[slot].found) foundCount++;
    Serial.print("IMU_");
    Serial.print(wt901Slots[slot].role);
    Serial.print("  mac=");
    Serial.print(wt901Slots[slot].mac);
    Serial.print("  ");
    Serial.print(wt901Slots[slot].found ? "FOUND" : "MISSING");
    Serial.print("  RSSI=");
    Serial.println(wt901Slots[slot].rssi);
  }
  Serial.print("Found IMUs: ");
  Serial.print(foundCount);
  Serial.print('/');
  Serial.println(WT901_ACTIVE_COUNT);
  Serial.println("=======================================");

  wt901ScanFinished = true;
}

static void wt901ReleaseClient(int index) {
  WT901Slot &imu = wt901Slots[index];
  imu.connected = false;
  imu.notifyReady = false;
  imu.notifyChar = NULL;
  imu.writeChar = NULL;

  if (imu.client == NULL) return;

  // deleteClient also removes the object from NimBLE's fixed client table and
  // safely stops an active connection before deleting the callback object.
  NimBLEDevice::deleteClient(imu.client);
  imu.client = NULL;
}

static bool wt901WaitForFirstPacket(int index, uint32_t timeoutMs) {
  WT901Slot &imu = wt901Slots[index];
  const uint32_t startPackets = imu.packetCount;
  const unsigned long startedMs = millis();

  while (millis() - startedMs < timeoutMs) {
    if (imu.client == NULL || !imu.client->isConnected()) return false;
    if (imu.packetCount > startPackets) return true;
    delay(10);
  }

  return false;
}

static bool wt901ConnectOne(int index,
                            uint8_t maxAttempts = WT901_CONNECT_ATTEMPTS,
                            uint32_t connectTimeoutMs = WT901_CONNECT_TIMEOUT_MS,
                            uint32_t firstPacketTimeoutMs = WT901_FIRST_PACKET_TIMEOUT_MS) {
  WT901Slot &imu = wt901Slots[index];

  Serial.println();
  Serial.print("Connecting IMU_");
  Serial.print(imu.role);
  Serial.print("  MAC=");
  Serial.println(imu.mac);

  if (!imu.found || !imu.addressReady) {
    Serial.print("Cannot connect: IMU_");
    Serial.print(imu.role);
    Serial.println(" was not found during scan.");
    return false;
  }

  for (uint8_t attempt = 1; attempt <= maxAttempts; attempt++) {
    wt901ReleaseClient(index);

    Serial.print("Connection attempt ");
    Serial.print(attempt);
    Serial.print('/');
    Serial.print(maxAttempts);
    Serial.print(" for IMU_");
    Serial.println(imu.role);

    imu.client = NimBLEDevice::createClient();
    if (imu.client == NULL) {
      Serial.print("Cannot create BLE client for IMU_");
      Serial.print(imu.role);
      Serial.print("  created=");
      Serial.print((unsigned long)NimBLEDevice::getCreatedClientCount());
      Serial.print("  compiled_max=");
      Serial.println((int)NIMBLE_MAX_CONNECTIONS);
      delay(WT901_RETRY_DELAY_MS);
      continue;
    }

    imu.client->setClientCallbacks(new WT901ClientCallbacks(index), true);
    // 20-30 ms connection interval: enough bandwidth for WT901 notifications
    // while leaving radio time for all five simultaneous links.
    imu.client->setConnectionParams(16, 24, 0, 400);
    imu.client->setConnectTimeout(connectTimeoutMs);
    imu.client->setConnectRetries(2);

    // Address was copied from the scan result, including its address type.
    // MTU exchange is disabled: WT901 packets are 20 bytes and fit MTU 23.
    if (!imu.client->connect(imu.address, true, false, false)) {
      const int error = imu.client->getLastError();
      Serial.print("Connect failed: IMU_");
      Serial.print(imu.role);
      Serial.print("  error=");
      Serial.println(error);
      wt901ReleaseClient(index);
      delay(WT901_RETRY_DELAY_MS * attempt);
      continue;
    }
    imu.connected = true;

    NimBLERemoteService *service = imu.client->getService(WT901_SERVICE_UUID);
    if (service == NULL) {
      Serial.print("Service FFE5 not found: IMU_");
      Serial.println(imu.role);
      wt901ReleaseClient(index);
      delay(WT901_RETRY_DELAY_MS * attempt);
      continue;
    }

    imu.notifyChar = service->getCharacteristic(WT901_NOTIFY_UUID);
    if (imu.notifyChar == NULL || !imu.notifyChar->canNotify()) {
      Serial.print("Notify FFE4 unavailable: IMU_");
      Serial.println(imu.role);
      wt901ReleaseClient(index);
      delay(WT901_RETRY_DELAY_MS * attempt);
      continue;
    }

    imu.writeChar = service->getCharacteristic(WT901_WRITE_UUID);
    if (!imu.notifyChar->subscribe(true, wt901NotifyCallback, true)) {
      Serial.print("Notify subscribe failed: IMU_");
      Serial.println(imu.role);
      wt901ReleaseClient(index);
      delay(WT901_RETRY_DELAY_MS * attempt);
      continue;
    }
    imu.notifyReady = true;

    // Do not start the next GATT discovery until this sensor has actually sent
    // data. This keeps five discovery/subscription procedures from overlapping.
    if (!wt901WaitForFirstPacket(index, firstPacketTimeoutMs)) {
      Serial.print("No first notify packet: IMU_");
      Serial.println(imu.role);
      wt901ReleaseClient(index);
      delay(WT901_RETRY_DELAY_MS * attempt);
      continue;
    }

    Serial.print("IMU_");
    Serial.print(imu.role);
    Serial.println(" connected, subscribed and receiving.");
    return true;
  }

  Serial.print("All connection attempts failed: IMU_");
  Serial.println(imu.role);
  return false;
}

static bool wt901ConnectTargets() {
  if (wt901ConnectStarted) {
    return wt901AllReceiving();
  }
  wt901ConnectStarted = true;

  bool allConnected = true;
  for (int i = 0; i < WT901_ACTIVE_COUNT; i++) {
    int slot = WT901_ACTIVE_INDICES[i];
    bool ok = wt901ConnectOne(slot);
    allConnected = allConnected && ok;
    delay(300);
  }

  Serial.println();
  if (allConnected) {
    Serial.println("All target IMUs connected. Waiting for notify packets...");
  } else {
    Serial.println("A/B/C/D/E connection incomplete. Keep all five IMUs on, APP disconnected, then restart ESP32.");
  }

  return allConnected;
}


static uint8_t wt901ReceivingMask() {
  uint8_t mask = 0;
  for (int i = 0; i < WT901_ACTIVE_COUNT; ++i) {
    const int slot = WT901_ACTIVE_INDICES[i];
    if (wt901SlotReceiving(slot)) mask |= (uint8_t)(1U << slot);
  }
  return mask;
}

static void wt901InvalidateForRediscovery(int index) {
  if (index < 0 || index >= WT901_ABCD_COUNT) return;
  WT901Slot &imu = wt901Slots[index];

  // Drop any stale client first so NimBLE's fixed client table does not leak a
  // slot across repeated power cycles.
  wt901ReleaseClient(index);

  imu.found = false;
  imu.addressReady = false;
  imu.address = NimBLEAddress();
  imu.rssi = -999;
  imu.bufferIndex = 0;
  imu.lastPacketMs = 0;
  imu.fusedVerticalYawTracking = false;
  imu.fusedVerticalYawIntegralDeg = 0.0f;
  imu.fusedVerticalYawBiasDegS = 0.0f;
  imu.fusedVerticalYawRateDegS = 0.0f;
  imu.fusedVerticalYawPrevRateDegS = 0.0f;
  imu.fusedVerticalYawPrevReady = false;
  imu.fusedVerticalYawLastSampleMs = 0;

  // Do not expose stale pose data while the device is physically absent.
  portENTER_CRITICAL(&wt901SampleQueueMux);
  imu.latest.valid = false;
  imu.latest.sampleMs = 0;
  portEXIT_CRITICAL(&wt901SampleQueueMux);
  wt901ClearSampleQueue(index);
}

// V4.4.27: call this only from non-training pages. It deliberately rescans only
// when at least one slot is not receiving, then reconnects only devices that
// were actually advertising during that rescan. This avoids 10-second blind
// connect attempts to a sensor that is still powered off.
static bool wt901ServiceReconnect() {
  const uint8_t beforeMask = wt901ReceivingMask();
  if (beforeMask == 0x1F) return false;

  const unsigned long now = millis();
  if (wt901LastReconnectAttemptMs != 0 &&
      now - wt901LastReconnectAttemptMs < WT901_RECONNECT_INTERVAL_MS) {
    return false;
  }
  wt901LastReconnectAttemptMs = now;

  Serial.printf("WT901_AUTO_RECONNECT_BEGIN mask=0x%02X\n", beforeMask);

  // Every non-receiving slot must prove that it is advertising again. This
  // handles both "off during boot" and "powered off then back on" cleanly.
  for (int i = 0; i < WT901_ACTIVE_COUNT; ++i) {
    const int slot = WT901_ACTIVE_INDICES[i];
    if (!wt901SlotReceiving(slot)) {
      wt901InvalidateForRediscovery(slot);
    }
  }

  wt901ScanForTargets(WT901_RECONNECT_SCAN_MS);

  for (int i = 0; i < WT901_ACTIVE_COUNT; ++i) {
    const int slot = WT901_ACTIVE_INDICES[i];
    if (wt901SlotReceiving(slot)) continue;
    WT901Slot &imu = wt901Slots[slot];
    if (!imu.found || !imu.addressReady) continue;

    Serial.printf("WT901_AUTO_RECONNECT_FOUND IMU_%s -> connect\n", imu.role);
    wt901ConnectOne(slot,
                    1,
                    WT901_RECONNECT_CONNECT_TIMEOUT_MS,
                    WT901_RECONNECT_FIRST_PACKET_TIMEOUT_MS);
    delay(120);
  }

  const uint8_t afterMask = wt901ReceivingMask();
  if (afterMask == 0x1F) {
    Serial.println("WT901_AUTO_RECONNECT_ALL_READY: all five IMUs are receiving; no ESP32 reboot required.");
  } else {
    Serial.printf("WT901_AUTO_RECONNECT_WAIT mask=0x%02X; power on missing IMU(s), next scan will retry automatically.\n",
                  afterMask);
  }
  return afterMask != beforeMask;
}

static bool wt901Begin() {
  wt901InitSlotsAndPairs();

  Serial.println("Initializing WT901 BLE A/B/C/D + E waist/abdomen input...");
  if (!NimBLEDevice::init("RehabMotion_V4_4_ABCDE")) {
    Serial.println("NimBLE initialization failed.");
    return false;
  }
  if (!NimBLEDevice::setPower(9)) {
    Serial.println("WARNING: unable to set BLE transmit power to +9 dBm.");
  }
  if (!NimBLEDevice::setMTU(23)) {
    Serial.println("WARNING: unable to set BLE MTU to 23.");
  }
  Serial.print("NimBLE compiled max connections: ");
  Serial.println((int)NIMBLE_MAX_CONNECTIONS);
  Serial.print("Required simultaneous IMU connections: ");
  Serial.println(WT901_ACTIVE_COUNT);
  delay(500);

  wt901ScanForTargets();
  delay(500);
  return wt901ConnectTargets();
}


// Quaternion helpers are defined below; forward declarations are needed by zero calibration.
static WT901Quat wt901EulerDegToQuat(float rollDeg, float pitchDeg, float yawDeg);
static WT901Quat wt901RelativeQuat(const WT901Quat &qFixed, const WT901Quat &qMoving);
static float wt901QuatDifferenceDeg(const WT901Quat &q0, const WT901Quat &q1);

static bool wt901CalibratePair(WT901JointPair &pair) {
  if (!wt901PairPoseReady(pair)) {
    Serial.print("CALIB_FAIL: ");
    Serial.print(pair.jointName);
    Serial.println(" pair not ready yet.");
    return false;
  }

  const WT901Slot &fixed = wt901Slots[pair.fixedIndex];
  const WT901Slot &moving = wt901Slots[pair.movingIndex];

  pair.zeroRollDiff = wt901Wrap180(moving.latest.roll - fixed.latest.roll);
  pair.zeroPitchDiff = wt901Wrap180(moving.latest.pitch - fixed.latest.pitch);
  pair.zeroYawDiff = wt901Wrap180(moving.latest.yaw - fixed.latest.yaw);

  // K3: capture the fixed/proximal IMU's own zero pose for direction tests.
  pair.fixedZeroRoll = fixed.latest.roll;
  pair.fixedZeroPitch = fixed.latest.pitch;
  pair.fixedZeroYaw = fixed.latest.yaw;
  pair.fixedMoveRollDeg = 0.0f;
  pair.fixedMovePitchDeg = 0.0f;
  pair.fixedMoveYawDeg = 0.0f;
  pair.fixedOrientationDeviationDeg = 0.0f;
  pair.fixedXAxisDeviationDeg = 0.0f;
  pair.fixedYAxisDeviationDeg = 0.0f;
  pair.fixedZAxisDeviationDeg = 0.0f;

  WT901Quat qFixedZero = wt901EulerDegToQuat(fixed.latest.roll, fixed.latest.pitch, fixed.latest.yaw);
  WT901Quat qMovingZero = wt901EulerDegToQuat(moving.latest.roll, moving.latest.pitch, moving.latest.yaw);
  pair.zeroFixedW = qFixedZero.w; pair.zeroFixedX = qFixedZero.x; pair.zeroFixedY = qFixedZero.y; pair.zeroFixedZ = qFixedZero.z;
  pair.zeroMovingW = qMovingZero.w; pair.zeroMovingX = qMovingZero.x; pair.zeroMovingY = qMovingZero.y; pair.zeroMovingZ = qMovingZero.z;
  WT901Quat qRelZero = wt901RelativeQuat(qFixedZero, qMovingZero);
  pair.relativePoseReady = true;
  pair.zeroRelW = qRelZero.w;
  pair.zeroRelX = qRelZero.x;
  pair.zeroRelY = qRelZero.y;
  pair.zeroRelZ = qRelZero.z;
  pair.rawJointAngleDeg = 0.0f;

  pair.zeroCalibrated = true;
  pair.filterReady = false;
  pair.angleReady = false;

  pair.filteredAngleDeg = 0.0f;
  pair.rawPitchDeg = 0.0f;
  pair.rollCrossDeg = 0.0f;
  pair.yawCrossDeg = 0.0f;

  pair.candidateAinvBDeg = 0.0f;
  pair.candidateBAinvDeg = 0.0f;
  pair.candidateWorldDeltaDeg = 0.0f;
  pair.candidateBodyDeltaDeg = 0.0f;
  pair.lastPairSkewMs = (fixed.latest.sampleMs > moving.latest.sampleMs)
    ? (fixed.latest.sampleMs - moving.latest.sampleMs)
    : (moving.latest.sampleMs - fixed.latest.sampleMs);
  pair.syncAcceptedCount = 0;
  pair.syncDroppedSkewCount = 0;
  pair.syncWaitingCount = 0;

  // K10 starts from a clean static zero. Functional-axis calibration begins
  // explicitly from the UI after this static zero has been accepted.
  pair.k10FunctionalCalActive = false;
  pair.k10FunctionalAxisReady = false;
  pair.k10WaitStraight = false;
  pair.k10TrainingAngleReady = false;
  pair.k10CalibrationFailed = false;
  pair.k10CalStartMs = 0;
  pair.k10StraightHoldStartMs = 0;
  pair.k10PrevSampleMs = 0;
  pair.k10PrevRel = qRelZero;
  pair.k10PrevRelReady = true;
  pair.k10Cxx = pair.k10Cxy = pair.k10Cxz = 0.0f;
  pair.k10Cyy = pair.k10Cyz = pair.k10Czz = 0.0f;
  pair.k10AxisSamples = 0;
  pair.k10CalCyclesCompleted = 0;
  pair.k10CalCyclePhase = 0;
  pair.k10CalPhaseHoldStartMs = 0;
  pair.k10CalCyclePeakDeg = 0.0f;
  pair.k10FirstVectorReady = false; 
  pair.k10PeakRel = qRelZero;
  pair.k10PeakTotalDeg = 0.0f;
  pair.k10AxisX = 1.0f; pair.k10AxisY = 0.0f; pair.k10AxisZ = 0.0f;
  pair.k10AxisQuality = 0.0f;
  pair.k10FunctionalAngleDeg = 0.0f;
  pair.k10FunctionalSpeedDegS = 0.0f;
  pair.k10ResidualIncrementDeg = 0.0f;
  pair.k10TotalRelativeFromStaticZeroDeg = 0.0f;
  pair.k10RejectedJumpCount = 0;

  pair.lastFixedPackets = fixed.packetCount;
  pair.lastMovingPackets = moving.packetCount;

  Serial.print("zero_");
  Serial.print(pair.jointName);
  Serial.printf(
    ": roll=%.2f, pitch=%.2f, yaw=%.2f\n",
    pair.zeroRollDiff,
    pair.zeroPitchDiff,
    pair.zeroYawDiff
  );
  Serial.printf(
    "K10_REL_ZERO_%s: w=%.5f x=%.5f y=%.5f z=%.5f\n",
    pair.jointName, pair.zeroRelW, pair.zeroRelX, pair.zeroRelY, pair.zeroRelZ
  );
  Serial.printf("K10_CAL_SYNC_%s: A_ms=%lu B_ms=%lu skew=%lu ms\n", pair.jointName,
    (unsigned long)fixed.latest.sampleMs, (unsigned long)moving.latest.sampleMs,
    (unsigned long)pair.lastPairSkewMs);

  return true;
}

static bool wt901CalibrateZero() {
  bool okAb = wt901CalibratePair(wt901Pairs[0]);
  if (!okAb) return false;

  Serial.println();
  Serial.println("========== REAL IMU AB ZERO CALIBRATED ==========");
  Serial.println("Use current A/B pose as 0 degree elbow pose.");
  Serial.println("AB relative pose source: synchronized A^-1*B. K10 learns a functional hinge axis before training.");
  Serial.println("Elbow counting is disabled until K10 functional-axis calibration succeeds.");
  Serial.println("==================================================");
  Serial.println();
  return true;
}

// Convert WT901 roll/pitch/yaw (degrees) to a unit quaternion using
// the documented intrinsic ZYX / yaw(Z)-pitch(Y)-roll(X) convention.
static WT901Quat wt901EulerDegToQuat(float rollDeg, float pitchDeg, float yawDeg) {
  const float d2r = 0.01745329251994329577f;
  float r = rollDeg * d2r * 0.5f;
  float p = pitchDeg * d2r * 0.5f;
  float y = yawDeg * d2r * 0.5f;

  float cr = cosf(r), sr = sinf(r);
  float cp = cosf(p), sp = sinf(p);
  float cy = cosf(y), sy = sinf(y);

  WT901Quat q;
  q.w = cr * cp * cy + sr * sp * sy;
  q.x = sr * cp * cy - cr * sp * sy;
  q.y = cr * sp * cy + sr * cp * sy;
  q.z = cr * cp * sy - sr * sp * cy;

  float n = sqrtf(q.w*q.w + q.x*q.x + q.y*q.y + q.z*q.z);
  if (n > 1e-6f) {
    q.w /= n; q.x /= n; q.y /= n; q.z /= n;
  }
  return q;
}

static WT901Quat wt901QuatConjugate(const WT901Quat &q) {
  WT901Quat out = {q.w, -q.x, -q.y, -q.z};
  return out;
}

static WT901Quat wt901QuatMultiply(const WT901Quat &a, const WT901Quat &b) {
  WT901Quat q;
  q.w = a.w*b.w - a.x*b.x - a.y*b.y - a.z*b.z;
  q.x = a.w*b.x + a.x*b.w + a.y*b.z - a.z*b.y;
  q.y = a.w*b.y - a.x*b.z + a.y*b.w + a.z*b.x;
  q.z = a.w*b.z + a.x*b.y - a.y*b.x + a.z*b.w;
  float n = sqrtf(q.w*q.w + q.x*q.x + q.y*q.y + q.z*q.z);
  if (n > 1e-6f) { q.w/=n; q.x/=n; q.y/=n; q.z/=n; }
  return q;
}

static WT901Quat wt901RelativeQuat(const WT901Quat &qFixed, const WT901Quat &qMoving) {
  return wt901QuatMultiply(wt901QuatConjugate(qFixed), qMoving);
}

static WT901Quat wt901RelativeQuatAlt(const WT901Quat &qFixed, const WT901Quat &qMoving) {
  // Alternative convention candidate: qMoving * inverse(qFixed).
  return wt901QuatMultiply(qMoving, wt901QuatConjugate(qFixed));
}

static float wt901QuatDifferenceDeg(const WT901Quat &q0, const WT901Quat &q1) {
  // q and -q are the same orientation, so use the absolute quaternion dot.
  float dot = fabsf(q0.w*q1.w + q0.x*q1.x + q0.y*q1.y + q0.z*q1.z);
  if (dot > 1.0f) dot = 1.0f;
  if (dot < 0.0f) dot = 0.0f;
  return 2.0f * acosf(dot) * 57.29577951308232f;
}


struct WT901Vec3 {
  float x, y, z;
};

static float wt901QuatDot(const WT901Quat &a, const WT901Quat &b) {
  return a.w*b.w + a.x*b.x + a.y*b.y + a.z*b.z;
}

static WT901Quat wt901QuatEnsureShortest(WT901Quat q) {
  if (q.w < 0.0f) {
    q.w = -q.w; q.x = -q.x; q.y = -q.y; q.z = -q.z;
  }
  return q;
}

static WT901Vec3 wt901QuatToRotationVectorDeg(WT901Quat q) {
  q = wt901QuatEnsureShortest(q);
  if (q.w > 1.0f) q.w = 1.0f;
  if (q.w < -1.0f) q.w = -1.0f;
  float angleRad = 2.0f * acosf(q.w);
  float sHalf = sqrtf(fmaxf(0.0f, 1.0f - q.w*q.w));
  WT901Vec3 v = {0.0f, 0.0f, 0.0f};
  const float r2d = 57.29577951308232f;
  if (angleRad < 1e-5f) {
    // small-angle: rotation vector ~= 2*q.xyz radians
    v.x = 2.0f * q.x * r2d;
    v.y = 2.0f * q.y * r2d;
    v.z = 2.0f * q.z * r2d;
    return v;
  }
  if (sHalf < 1e-6f) return v;
  float angleDeg = angleRad * r2d;
  v.x = q.x / sHalf * angleDeg;
  v.y = q.y / sHalf * angleDeg;
  v.z = q.z / sHalf * angleDeg;
  return v;
}

static float wt901VecNorm(const WT901Vec3 &v) {
  return sqrtf(v.x*v.x + v.y*v.y + v.z*v.z);
}

static float wt901VecDot(const WT901Vec3 &a, const WT901Vec3 &b) {
  return a.x*b.x + a.y*b.y + a.z*b.z;
}

static WT901Vec3 wt901VecNormalized(WT901Vec3 v) {
  float n = wt901VecNorm(v);
  if (n < 1e-6f) return {1.0f, 0.0f, 0.0f};
  v.x /= n; v.y /= n; v.z /= n;
  return v;
}

// K10.2 functional calibration is driven by THREE COMPLETE adaptive flex/return cycles.
// The 60 s timeout is only a safety escape; it is NOT the normal finish condition.
static constexpr uint8_t WT901_K10_CAL_TARGET_CYCLES = 3;
static constexpr unsigned long WT901_K10_FUNCTIONAL_CAL_TIMEOUT_MS = 60000UL;
static constexpr unsigned long WT901_K10_CAL_PHASE_HOLD_MS = 180UL;
static constexpr float WT901_K10_CAL_FLEX_DEG = 60.0f;
static constexpr float WT901_K10_CAL_FLEX_EXCURSION_DEG = 55.0f;
static constexpr float WT901_K10_CAL_RETURN_BASE_MARGIN_DEG = 35.0f;
static constexpr float WT901_K10_CAL_RETURN_PEAK_RATIO = 0.45f;
static constexpr float WT901_K10_CAL_RETURN_MIN_DROP_DEG = 50.0f;
static constexpr unsigned long WT901_K10_STRAIGHT_HOLD_MS = 900UL;
static constexpr float WT901_K10_MIN_CAL_INCREMENT_DEG = 0.20f;
static constexpr float WT901_K10_MAX_CAL_INCREMENT_DEG = 30.0f;
static constexpr float WT901_K10_MAX_TRAIN_INCREMENT_DEG = 25.0f;

static void wt901K10ResetCovariance(WT901JointPair &pair) {
  pair.k10Cxx = pair.k10Cxy = pair.k10Cxz = 0.0f;
  pair.k10Cyy = pair.k10Cyz = pair.k10Czz = 0.0f;
  pair.k10AxisSamples = 0;
  pair.k10CalCyclesCompleted = 0;
  pair.k10CalCyclePhase = 0;
  pair.k10CalPhaseHoldStartMs = 0;
  pair.k10CalCyclePeakDeg = 0.0f;
  pair.k10FirstVectorReady = false;
  pair.k10FirstVx = pair.k10FirstVy = pair.k10FirstVz = 0.0f;
  pair.k10PeakTotalDeg = 0.0f;
}

static void wt901K10AccumulateAxisSample(WT901JointPair &pair, const WT901Vec3 &rv) {
  float mag = wt901VecNorm(rv);
  if (mag < WT901_K10_MIN_CAL_INCREMENT_DEG || mag > WT901_K10_MAX_CAL_INCREMENT_DEG) return;
  pair.k10Cxx += rv.x * rv.x;
  pair.k10Cxy += rv.x * rv.y;
  pair.k10Cxz += rv.x * rv.z;
  pair.k10Cyy += rv.y * rv.y;
  pair.k10Cyz += rv.y * rv.z;
  pair.k10Czz += rv.z * rv.z;
  pair.k10AxisSamples++;
  if (!pair.k10FirstVectorReady && mag >= 0.8f) {
    pair.k10FirstVx = rv.x; pair.k10FirstVy = rv.y; pair.k10FirstVz = rv.z;
    pair.k10FirstVectorReady = true;
  }
}

static bool wt901K10UpdateCalibrationCycle(WT901JointPair &pair) {
  const float d = pair.k10TotalRelativeFromStaticZeroDeg;
  const unsigned long now = millis();

  if (pair.k10CalCyclePhase == 0) {
    // WAIT_FLEX: learn an adaptive baseline from the lowest extension-like pose
    // reached before the next flexion. This avoids assuming that full 3-D relative
    // orientation must return to the original startup quaternion after pronation/twist.
    if (!pair.k10CalCycleBaselineReady || d < pair.k10CalCycleBaselineDeg) {
      pair.k10CalCycleBaselineDeg = d;
      pair.k10CalCycleBaselineReady = true;
    }

    float flexThreshold = fmaxf(WT901_K10_CAL_FLEX_DEG,
                                pair.k10CalCycleBaselineDeg + WT901_K10_CAL_FLEX_EXCURSION_DEG);
    if (d >= flexThreshold) {
      if (pair.k10CalPhaseHoldStartMs == 0) pair.k10CalPhaseHoldStartMs = now;
      if (now - pair.k10CalPhaseHoldStartMs >= WT901_K10_CAL_PHASE_HOLD_MS) {
        pair.k10CalCyclePhase = 1;
        pair.k10CalPhaseHoldStartMs = 0;
        pair.k10CalCyclePeakDeg = d;
        Serial.printf("K10_CAL_FLEX_REACHED: cycle=%u/%u dist=%.1f baseline=%.1f flex_thr=%.1f\n",
          (unsigned)(pair.k10CalCyclesCompleted + 1),
          (unsigned)WT901_K10_CAL_TARGET_CYCLES, d,
          pair.k10CalCycleBaselineDeg, flexThreshold);
      }
    } else {
      pair.k10CalPhaseHoldStartMs = 0;
    }
  } else {
    // WAIT_RETURN: use a peak-relative + adaptive-baseline criterion instead of
    // requiring return to the original full 3-D quaternion. The latter incorrectly
    // treats forearm axial rotation / strap twist as failed elbow extension.
    if (d > pair.k10CalCyclePeakDeg) pair.k10CalCyclePeakDeg = d;
    float returnThreshold = fmaxf(
      pair.k10CalCycleBaselineDeg + WT901_K10_CAL_RETURN_BASE_MARGIN_DEG,
      pair.k10CalCyclePeakDeg * WT901_K10_CAL_RETURN_PEAK_RATIO
    );
    bool enoughDrop = (pair.k10CalCyclePeakDeg - d) >= WT901_K10_CAL_RETURN_MIN_DROP_DEG;
    if (d <= returnThreshold && enoughDrop) {
      if (pair.k10CalCyclesCompleted < WT901_K10_CAL_TARGET_CYCLES) {
        pair.k10CalCyclesCompleted++;
      }
      Serial.printf("K10_CAL_CYCLE_DONE: %u/%u peak=%.1f return=%.1f baseline=%.1f return_thr=%.1f samples=%lu\n",
        (unsigned)pair.k10CalCyclesCompleted,
        (unsigned)WT901_K10_CAL_TARGET_CYCLES,
        pair.k10CalCyclePeakDeg, d, pair.k10CalCycleBaselineDeg,
        returnThreshold, (unsigned long)pair.k10AxisSamples);

      // The just-detected return posture becomes the baseline for the next cycle.
      pair.k10CalCycleBaselineDeg = d;
      pair.k10CalCycleBaselineReady = true;
      pair.k10CalCyclePhase = 0;
      pair.k10CalPhaseHoldStartMs = 0;
      pair.k10CalCyclePeakDeg = 0.0f;
      return true;
    }
  }
  return false;
}

static bool wt901K10FitAxis(WT901JointPair &pair) {
  float trace = pair.k10Cxx + pair.k10Cyy + pair.k10Czz;
  if (pair.k10AxisSamples < 12 || trace < 5.0f) return false;

  WT901Vec3 v = {1.0f, 0.3f, 0.1f};
  v = wt901VecNormalized(v);
  for (int i = 0; i < 20; ++i) {
    WT901Vec3 nv;
    nv.x = pair.k10Cxx*v.x + pair.k10Cxy*v.y + pair.k10Cxz*v.z;
    nv.y = pair.k10Cxy*v.x + pair.k10Cyy*v.y + pair.k10Cyz*v.z;
    nv.z = pair.k10Cxz*v.x + pair.k10Cyz*v.y + pair.k10Czz*v.z;
    float n = wt901VecNorm(nv);
    if (n < 1e-8f) return false;
    v.x = nv.x/n; v.y = nv.y/n; v.z = nv.z/n;
  }

  WT901Vec3 Cv;
  Cv.x = pair.k10Cxx*v.x + pair.k10Cxy*v.y + pair.k10Cxz*v.z;
  Cv.y = pair.k10Cxy*v.x + pair.k10Cyy*v.y + pair.k10Cyz*v.z;
  Cv.z = pair.k10Cxz*v.x + pair.k10Cyz*v.y + pair.k10Czz*v.z;
  float principal = wt901VecDot(v, Cv);
  pair.k10AxisQuality = principal / fmaxf(trace, 1e-6f);

  // Define flexion-positive from the largest excursion reached during the
  // calibration window. This is more robust than trusting one noisy first sample.
  if (pair.k10PeakTotalDeg >= 20.0f) {
    WT901Quat qRelZero = {pair.zeroRelW, pair.zeroRelX, pair.zeroRelY, pair.zeroRelZ};
    WT901Quat dqPeak = wt901QuatMultiply(pair.k10PeakRel, wt901QuatConjugate(qRelZero));
    WT901Vec3 peakRv = wt901QuatToRotationVectorDeg(dqPeak);
    if (wt901VecDot(v, peakRv) < 0.0f) {
      v.x = -v.x; v.y = -v.y; v.z = -v.z;
    }
  } else if (pair.k10FirstVectorReady) {
    WT901Vec3 first = {pair.k10FirstVx, pair.k10FirstVy, pair.k10FirstVz};
    if (wt901VecDot(v, first) < 0.0f) {
      v.x = -v.x; v.y = -v.y; v.z = -v.z;
    }
  }

  pair.k10AxisX = v.x; pair.k10AxisY = v.y; pair.k10AxisZ = v.z;
  return pair.k10AxisQuality >= 0.65f;
}

static void wt901K10StartFunctionalCalibration(WT901JointPair &pair) {
  if (!pair.zeroCalibrated || !pair.relativePoseReady) return;
  wt901K10ResetCovariance(pair);
  pair.k10FunctionalCalActive = true;
  pair.k10FunctionalAxisReady = false;
  pair.k10WaitStraight = false;
  pair.k10TrainingAngleReady = false;
  pair.k10CalibrationFailed = false;
  pair.k10CalStartMs = millis();
  pair.k10StraightHoldStartMs = 0;
  pair.k10FunctionalAngleDeg = 0.0f;
  pair.k10FunctionalSpeedDegS = 0.0f;
  pair.k10ResidualIncrementDeg = 0.0f;
  pair.k10RejectedJumpCount = 0;
  pair.k10PrevSampleMs = 0;
  pair.k10PrevRelReady = false; // next synchronized pair becomes the first reference
  pair.k10PeakRel = {pair.zeroRelW, pair.zeroRelX, pair.zeroRelY, pair.zeroRelZ};
  pair.k10PeakTotalDeg = 0.0f;
  pair.k10CalCyclesCompleted = 0;
  pair.k10CalCyclePhase = 0;
  pair.k10CalPhaseHoldStartMs = 0;
  pair.k10CalCyclePeakDeg = 0.0f;
  pair.k10CalCycleBaselineDeg = 999.0f;
  pair.k10CalCycleBaselineReady = false;
  pair.k10TrainingZeroReady = false;
  pair.k10TrainingZeroDistanceDeg = 0.0f;
}

static float wt901OrientationDifferenceDeg(
  float roll0, float pitch0, float yaw0,
  float roll1, float pitch1, float yaw1
) {
  WT901Quat q0 = wt901EulerDegToQuat(roll0, pitch0, yaw0);
  WT901Quat q1 = wt901EulerDegToQuat(roll1, pitch1, yaw1);

  // q and -q represent the same orientation, hence fabs(dot).
  float dot = fabsf(q0.w*q1.w + q0.x*q1.x + q0.y*q1.y + q0.z*q1.z);
  if (dot > 1.0f) dot = 1.0f;
  if (dot < 0.0f) dot = 0.0f;
  return 2.0f * acosf(dot) * 57.29577951308232f;
}

// K3: compare the *direction* of a selected local sensor axis instead of the
// whole sensor orientation. This intentionally ignores twist around that same
// axis, which is exactly what we want when looking for upper-arm segment motion.

static WT901Vec3 wt901RotateLocalVectorByQuat(const WT901Quat &q, float vx, float vy, float vz) {
  // Efficient q * v * q^-1 for unit quaternion q.
  float tx = 2.0f * (q.y * vz - q.z * vy);
  float ty = 2.0f * (q.z * vx - q.x * vz);
  float tz = 2.0f * (q.x * vy - q.y * vx);

  WT901Vec3 out;
  out.x = vx + q.w * tx + (q.y * tz - q.z * ty);
  out.y = vy + q.w * ty + (q.z * tx - q.x * tz);
  out.z = vz + q.w * tz + (q.x * ty - q.y * tx);
  return out;
}

static float wt901VectorAngleDeg(const WT901Vec3 &a, const WT901Vec3 &b) {
  float na = sqrtf(a.x*a.x + a.y*a.y + a.z*a.z);
  float nb = sqrtf(b.x*b.x + b.y*b.y + b.z*b.z);
  if (na < 1e-6f || nb < 1e-6f) return 0.0f;
  float dot = (a.x*b.x + a.y*b.y + a.z*b.z) / (na * nb);
  if (dot > 1.0f) dot = 1.0f;
  if (dot < -1.0f) dot = -1.0f;
  return acosf(dot) * 57.29577951308232f;
}

static void wt901AxisDirectionDeviationDeg(
  float roll0, float pitch0, float yaw0,
  float roll1, float pitch1, float yaw1,
  float &xDevDeg, float &yDevDeg, float &zDevDeg
) {
  WT901Quat q0 = wt901EulerDegToQuat(roll0, pitch0, yaw0);
  WT901Quat q1 = wt901EulerDegToQuat(roll1, pitch1, yaw1);

  WT901Vec3 x0 = wt901RotateLocalVectorByQuat(q0, 1.0f, 0.0f, 0.0f);
  WT901Vec3 y0 = wt901RotateLocalVectorByQuat(q0, 0.0f, 1.0f, 0.0f);
  WT901Vec3 z0 = wt901RotateLocalVectorByQuat(q0, 0.0f, 0.0f, 1.0f);
  WT901Vec3 x1 = wt901RotateLocalVectorByQuat(q1, 1.0f, 0.0f, 0.0f);
  WT901Vec3 y1 = wt901RotateLocalVectorByQuat(q1, 0.0f, 1.0f, 0.0f);
  WT901Vec3 z1 = wt901RotateLocalVectorByQuat(q1, 0.0f, 0.0f, 1.0f);

  xDevDeg = wt901VectorAngleDeg(x0, x1);
  yDevDeg = wt901VectorAngleDeg(y0, y1);
  zDevDeg = wt901VectorAngleDeg(z0, z1);
}

static bool wt901UpdatePair(WT901JointPair &pair) {
  if (!pair.zeroCalibrated || !pair.relativePoseReady || !wt901PairPoseReady(pair)) {
    return false;
  }

  WT901Slot &fixed = wt901Slots[pair.fixedIndex];
  WT901Slot &moving = wt901Slots[pair.movingIndex];

  uint32_t fixedPackets = fixed.packetCount;
  uint32_t movingPackets = moving.packetCount;
  bool fixedNew = fixedPackets != pair.lastFixedPackets;
  bool movingNew = movingPackets != pair.lastMovingPackets;
  if (!fixedNew || !movingNew) {
    pair.syncWaitingCount++;
    return wt901PairReceiving(pair);
  }

  unsigned long tA = fixed.latest.sampleMs;
  unsigned long tB = moving.latest.sampleMs;
  unsigned long skew = (tA > tB) ? (tA - tB) : (tB - tA);
  pair.lastPairSkewMs = skew;
  pair.lastFixedPackets = fixedPackets;
  pair.lastMovingPackets = movingPackets;

  if (skew > WT901_MAX_PAIR_SKEW_MS) {
    pair.syncDroppedSkewCount++;
    return wt901PairReceiving(pair);
  }
  pair.syncAcceptedCount++;

  // Keep K3 diagnostics for logging only. They do not decide K10 repetition quality.
  pair.fixedMoveRollDeg = wt901Wrap180(fixed.latest.roll - pair.fixedZeroRoll);
  pair.fixedMovePitchDeg = wt901Wrap180(fixed.latest.pitch - pair.fixedZeroPitch);
  pair.fixedMoveYawDeg = wt901Wrap180(fixed.latest.yaw - pair.fixedZeroYaw);
  pair.fixedOrientationDeviationDeg = wt901OrientationDifferenceDeg(
    pair.fixedZeroRoll, pair.fixedZeroPitch, pair.fixedZeroYaw,
    fixed.latest.roll, fixed.latest.pitch, fixed.latest.yaw
  );
  wt901AxisDirectionDeviationDeg(
    pair.fixedZeroRoll, pair.fixedZeroPitch, pair.fixedZeroYaw,
    fixed.latest.roll, fixed.latest.pitch, fixed.latest.yaw,
    pair.fixedXAxisDeviationDeg,
    pair.fixedYAxisDeviationDeg,
    pair.fixedZAxisDeviationDeg
  );

  WT901Quat qA0 = {pair.zeroFixedW, pair.zeroFixedX, pair.zeroFixedY, pair.zeroFixedZ};
  WT901Quat qB0 = {pair.zeroMovingW, pair.zeroMovingX, pair.zeroMovingY, pair.zeroMovingZ};
  WT901Quat qA = wt901EulerDegToQuat(fixed.latest.roll, fixed.latest.pitch, fixed.latest.yaw);
  WT901Quat qB = wt901EulerDegToQuat(moving.latest.roll, moving.latest.pitch, moving.latest.yaw);

  WT901Quat rel1_0 = wt901RelativeQuat(qA0, qB0);
  WT901Quat rel1_n = wt901RelativeQuat(qA, qB);
  pair.candidateAinvBDeg = wt901QuatDifferenceDeg(rel1_0, rel1_n);

  WT901Quat rel2_0 = wt901RelativeQuatAlt(qA0, qB0);
  WT901Quat rel2_n = wt901RelativeQuatAlt(qA, qB);
  pair.candidateBAinvDeg = wt901QuatDifferenceDeg(rel2_0, rel2_n);

  WT901Quat dAWorld = wt901QuatMultiply(qA, wt901QuatConjugate(qA0));
  WT901Quat dBWorld = wt901QuatMultiply(qB, wt901QuatConjugate(qB0));
  pair.candidateWorldDeltaDeg = wt901QuatDifferenceDeg(dAWorld, dBWorld);

  WT901Quat dABody = wt901QuatMultiply(wt901QuatConjugate(qA0), qA);
  WT901Quat dBBody = wt901QuatMultiply(wt901QuatConjugate(qB0), qB);
  pair.candidateBodyDeltaDeg = wt901QuatDifferenceDeg(dABody, dBBody);

  pair.k10TotalRelativeFromStaticZeroDeg = pair.candidateAinvBDeg;

  WT901Quat qRel = rel1_n;
  if (!pair.k10PrevRelReady) {
    pair.k10PrevRel = qRel;
    pair.k10PrevRelReady = true;
    pair.k10PrevSampleMs = (tA + tB) / 2UL;
    pair.angleReady = true;
    pair.filteredAngleDeg = pair.k10FunctionalAngleDeg;
    pair.rawJointAngleDeg = pair.k10FunctionalAngleDeg;
    return wt901PairReceiving(pair);
  }

  unsigned long sampleMs = (tA + tB) / 2UL;
  unsigned long dtMs = sampleMs >= pair.k10PrevSampleMs ? sampleMs - pair.k10PrevSampleMs : 0UL;
  pair.k10PrevSampleMs = sampleMs;

  // Left increment keeps the hinge axis expressed in the proximal/A sensor frame.
  WT901Quat dq = wt901QuatMultiply(qRel, wt901QuatConjugate(pair.k10PrevRel));
  pair.k10PrevRel = qRel;
  WT901Vec3 rv = wt901QuatToRotationVectorDeg(dq);
  float incMag = wt901VecNorm(rv);

  if (dtMs > 0 && dtMs < 500) {
    pair.k10FunctionalSpeedDegS = incMag * 1000.0f / (float)dtMs;
  } else {
    pair.k10FunctionalSpeedDegS = 0.0f;
  }

  if (pair.k10FunctionalCalActive) {
    if (pair.k10TotalRelativeFromStaticZeroDeg > pair.k10PeakTotalDeg) {
      pair.k10PeakTotalDeg = pair.k10TotalRelativeFromStaticZeroDeg;
      pair.k10PeakRel = qRel;
    }
    wt901K10AccumulateAxisSample(pair, rv);
    wt901K10UpdateCalibrationCycle(pair);

    // Normal completion: ONLY after three complete flex->return cycles.
    if (pair.k10CalCyclesCompleted >= WT901_K10_CAL_TARGET_CYCLES) {
      pair.k10FunctionalCalActive = false;
      if (wt901K10FitAxis(pair)) {
        pair.k10FunctionalAxisReady = true;
        pair.k10WaitStraight = false;
        pair.k10TrainingAngleReady = true;
        pair.k10CalibrationFailed = false;
        pair.k10StraightHoldStartMs = 0;
        pair.k10FunctionalAngleDeg = 0.0f;
        pair.k10FunctionalSpeedDegS = 0.0f;
        pair.k10ResidualIncrementDeg = 0.0f;
        // The third cycle has *just* satisfied the adaptive return criterion, so
        // this synchronized relative pose is a much better formal 0-degree anchor
        // than asking the user to rediscover the startup full-3D quaternion.
        pair.k10TrainingZeroRel = qRel;
        pair.k10TrainingZeroReady = true;
        pair.k10TrainingZeroDistanceDeg = 0.0f;
        pair.k10PrevRel = qRel;
        pair.k10PrevRelReady = true;
        Serial.printf("K10_AXIS_READY: cycles=%u/%u axis=(%.4f,%.4f,%.4f) quality=%.3f samples=%lu\n",
          (unsigned)pair.k10CalCyclesCompleted, (unsigned)WT901_K10_CAL_TARGET_CYCLES,
          pair.k10AxisX, pair.k10AxisY, pair.k10AxisZ, pair.k10AxisQuality,
          (unsigned long)pair.k10AxisSamples);
        Serial.println("K10_TRAINING_READY: third return pose auto-locked as 0 deg; no extra WAIT_STRAIGHT gate.");
      } else {
        pair.k10FunctionalAxisReady = false;
        pair.k10CalibrationFailed = true;
        Serial.printf("K10_AXIS_FAIL_AFTER_3_CYCLES: quality=%.3f samples=%lu. Recalibrate with three slower, cleaner cycles.\n",
          pair.k10AxisQuality, (unsigned long)pair.k10AxisSamples);
      }
    } else if (millis() - pair.k10CalStartMs >= WT901_K10_FUNCTIONAL_CAL_TIMEOUT_MS) {
      // Safety timeout only. Do not silently treat time elapsed as three cycles.
      pair.k10FunctionalCalActive = false;
      pair.k10FunctionalAxisReady = false;
      pair.k10CalibrationFailed = true;
      Serial.printf("K10_AXIS_TIMEOUT: only %u/%u complete cycles detected in 60 s; samples=%lu.\n",
        (unsigned)pair.k10CalCyclesCompleted, (unsigned)WT901_K10_CAL_TARGET_CYCLES,
        (unsigned long)pair.k10AxisSamples);
    }
  } else if (pair.k10WaitStraight && pair.k10FunctionalAxisReady) {
    // Return close to the original straight pose and hold still. The total 3D
    // relative distance is only used here as a coarse zero-pose check, not as elbow angle.
    bool nearStaticZero = pair.k10TotalRelativeFromStaticZeroDeg <= 18.0f;
    bool stillEnough = pair.k10FunctionalSpeedDegS <= 10.0f;
    if (nearStaticZero && stillEnough) {
      if (pair.k10StraightHoldStartMs == 0) pair.k10StraightHoldStartMs = millis();
      if (millis() - pair.k10StraightHoldStartMs >= WT901_K10_STRAIGHT_HOLD_MS) {
        pair.k10WaitStraight = false;
        pair.k10TrainingAngleReady = true;
        pair.k10FunctionalAngleDeg = 0.0f;
        pair.k10FunctionalSpeedDegS = 0.0f;
        pair.k10ResidualIncrementDeg = 0.0f;
        pair.k10PrevRel = qRel;
        pair.k10PrevRelReady = true;
        Serial.println("K10_TRAINING_READY: straight zero locked; signed hinge-axis elbow angle = 0 deg.");
      }
    } else {
      pair.k10StraightHoldStartMs = 0;
    }
  } else if (pair.k10TrainingAngleReady && pair.k10FunctionalAxisReady) {
    if (incMag <= WT901_K10_MAX_TRAIN_INCREMENT_DEG && dtMs > 0 && dtMs < 500) {
      WT901Vec3 axis = {pair.k10AxisX, pair.k10AxisY, pair.k10AxisZ};
      float signedInc = wt901VecDot(rv, axis);
      float residualSq = fmaxf(0.0f, incMag*incMag - signedInc*signedInc);
      pair.k10ResidualIncrementDeg = sqrtf(residualSq);

      float signedSpeed = signedInc * 1000.0f / (float)dtMs;
      // Reject impossible one-sample angular jumps while keeping normal voluntary motion.
      if (fabsf(signedSpeed) <= 500.0f) {
        pair.k10FunctionalAngleDeg += signedInc;
      } else {
        pair.k10RejectedJumpCount++;
      }
    } else if (incMag > WT901_K10_MAX_TRAIN_INCREMENT_DEG) {
      pair.k10RejectedJumpCount++;
    }

    // The learned sign is flexion-positive. Prevent small integration drift below zero;
    // do not impose a 90-degree fold or 180-angle transform.
    if (pair.k10FunctionalAngleDeg < 0.0f) pair.k10FunctionalAngleDeg = 0.0f;
    if (pair.k10FunctionalAngleDeg > WT901_MAX_ANGLE_DEG) pair.k10FunctionalAngleDeg = WT901_MAX_ANGLE_DEG;

    // Optional gentle drift correction uses the FORMAL training-zero pose captured
    // at the third calibrated return, never the old startup full-3D quaternion.
    if (pair.k10TrainingZeroReady) {
      pair.k10TrainingZeroDistanceDeg = wt901QuatDifferenceDeg(pair.k10TrainingZeroRel, qRel);
      if (pair.k10TrainingZeroDistanceDeg <= 8.0f && pair.k10FunctionalSpeedDegS <= 8.0f) {
        pair.k10FunctionalAngleDeg = 0.0f;
      }
    }
  }

  // K10 display/counting angle is the signed functional-axis angle directly.
  // No heavy low-pass filter is used; the screen should follow 89->90->91->92 naturally.
  pair.rawJointAngleDeg = pair.k10FunctionalAngleDeg;
  pair.filteredAngleDeg = pair.k10FunctionalAngleDeg;
  pair.angleReady = true;
  return wt901PairReceiving(pair);
}

static bool wt901UpdateAngles() {
  return wt901UpdatePair(wt901Pairs[0]);
}

static bool wt901PairAngleValid(int pairIndex) {
  return wt901Pairs[pairIndex].angleReady && wt901PairReceiving(wt901Pairs[pairIndex]);
}

static bool wt901AnglesValid() {
  return wt901PairAngleValid(0);
}

static bool wt901ZeroReady() {
  return wt901Pairs[0].zeroCalibrated;
}

static float wt901LeftAngleDeg() {
  return wt901Pairs[0].k10FunctionalAngleDeg;
}

static float wt901RightAngleDeg() {
  return wt901Pairs[1].filteredAngleDeg;
}

static float wt901LeftSignedPitchDeg() {
  return wt901Pairs[0].rawPitchDeg; // legacy Euler debug only
}

static float wt901LeftRawJointAngleDeg() {
  return wt901Pairs[0].k10FunctionalAngleDeg;
}


static void wt901StartFunctionalElbowCalibration() {
  wt901K10StartFunctionalCalibration(wt901Pairs[0]);
}
static bool wt901K10FunctionalCalibrationActive() { return wt901Pairs[0].k10FunctionalCalActive; }
static bool wt901K10FunctionalAxisReady() { return wt901Pairs[0].k10FunctionalAxisReady; }
static bool wt901K10WaitingStraight() { return wt901Pairs[0].k10WaitStraight; }
static bool wt901K10TrainingReady() { return wt901Pairs[0].k10TrainingAngleReady; }
static bool wt901K10CalibrationFailed() { return wt901Pairs[0].k10CalibrationFailed; }
static unsigned long wt901K10CalibrationRemainingMs() {
  WT901JointPair &p = wt901Pairs[0];
  if (!p.k10FunctionalCalActive) return 0;
  unsigned long elapsed = millis() - p.k10CalStartMs;
  return elapsed >= WT901_K10_FUNCTIONAL_CAL_TIMEOUT_MS ? 0 : WT901_K10_FUNCTIONAL_CAL_TIMEOUT_MS - elapsed;
}
static uint8_t wt901K10CalibrationCyclesCompleted() { return wt901Pairs[0].k10CalCyclesCompleted; }
static uint8_t wt901K10CalibrationCyclesTarget() { return WT901_K10_CAL_TARGET_CYCLES; }
static bool wt901K10CalibrationWaitingReturn() { return wt901Pairs[0].k10CalCyclePhase == 1; }
static float wt901K10FunctionalAngleDeg() { return wt901Pairs[0].k10FunctionalAngleDeg; }
static float wt901K10FunctionalSpeedDegS() { return wt901Pairs[0].k10FunctionalSpeedDegS; }
static float wt901K10AxisX() { return wt901Pairs[0].k10AxisX; }
static float wt901K10AxisY() { return wt901Pairs[0].k10AxisY; }
static float wt901K10AxisZ() { return wt901Pairs[0].k10AxisZ; }
static float wt901K10AxisQuality() { return wt901Pairs[0].k10AxisQuality; }
static uint32_t wt901K10AxisSamples() { return wt901Pairs[0].k10AxisSamples; }
static float wt901K10ResidualIncrementDeg() { return wt901Pairs[0].k10ResidualIncrementDeg; }
static float wt901K10StaticZeroDistanceDeg() { return wt901Pairs[0].k10TotalRelativeFromStaticZeroDeg; }
static uint32_t wt901K10RejectedJumpCount() { return wt901Pairs[0].k10RejectedJumpCount; }

static unsigned long wt901LeftPairSkewMs() { return wt901Pairs[0].lastPairSkewMs; }
static unsigned long wt901PairMaxSkewMs() { return WT901_MAX_PAIR_SKEW_MS; }
static uint32_t wt901LeftSyncAcceptedCount() { return wt901Pairs[0].syncAcceptedCount; }
static uint32_t wt901LeftSyncDroppedSkewCount() { return wt901Pairs[0].syncDroppedSkewCount; }
static uint32_t wt901LeftSyncWaitingCount() { return wt901Pairs[0].syncWaitingCount; }
static float wt901LeftCandidateAinvBDeg() { return wt901Pairs[0].candidateAinvBDeg; }
static float wt901LeftCandidateBAinvDeg() { return wt901Pairs[0].candidateBAinvDeg; }
static float wt901LeftCandidateWorldDeltaDeg() { return wt901Pairs[0].candidateWorldDeltaDeg; }
static float wt901LeftCandidateBodyDeltaDeg() { return wt901Pairs[0].candidateBodyDeltaDeg; }
// V4.4.30 vertical-rotation tracker API.  Reset both B and E at the accepted
// BODY-FRONT pose; their integral difference is then the forearm's accumulated
// rotation about body Z relative to the torso. Whole-body turning cancels.
static void wt901FusedVerticalYawReset(int index) {
  if (index < 0 || index >= WT901_ABCD_COUNT) return;
  WT901Slot &imu = wt901Slots[index];
  portENTER_CRITICAL(&wt901SampleQueueMux);
  const WT901EulerData d = imu.latest;
  imu.fusedVerticalYawIntegralDeg = 0.0f;
  imu.fusedVerticalYawRateDegS = d.valid ? wt901WorldVerticalGyroRateDegS(d) : 0.0f;
  imu.fusedVerticalYawBiasDegS = (d.valid && fabsf(imu.fusedVerticalYawRateDegS) <= 4.0f)
      ? imu.fusedVerticalYawRateDegS : 0.0f;
  imu.fusedVerticalYawPrevRateDegS = 0.0f;
  imu.fusedVerticalYawPrevReady = false;
  imu.fusedVerticalYawLastSampleMs = d.valid ? d.sampleMs : 0;
  imu.fusedVerticalYawTracking = d.valid && d.sampleMs != 0;
  portEXIT_CRITICAL(&wt901SampleQueueMux);
}

static float wt901FusedVerticalYawIntegralDeg(int index) {
  if (index < 0 || index >= WT901_ABCD_COUNT) return 0.0f;
  float v;
  portENTER_CRITICAL(&wt901SampleQueueMux);
  v = wt901Slots[index].fusedVerticalYawIntegralDeg;
  portEXIT_CRITICAL(&wt901SampleQueueMux);
  return isfinite(v) ? v : 0.0f;
}

static float wt901FusedVerticalYawRateDegS(int index) {
  if (index < 0 || index >= WT901_ABCD_COUNT) return 0.0f;
  float v;
  portENTER_CRITICAL(&wt901SampleQueueMux);
  v = wt901Slots[index].fusedVerticalYawRateDegS - wt901Slots[index].fusedVerticalYawBiasDegS;
  portEXIT_CRITICAL(&wt901SampleQueueMux);
  return isfinite(v) ? v : 0.0f;
}

static unsigned long wt901ImuSampleMs(int index) { return wt901Slots[index].latest.sampleMs; }
static uint32_t wt901ImuPacketCount(int index) { return wt901Slots[index].packetCount; }
static float wt901ImuRollDeg(int index) { return wt901Slots[index].latest.roll; }
static float wt901ImuPitchDeg(int index) { return wt901Slots[index].latest.pitch; }
static float wt901ImuYawDeg(int index) { return wt901Slots[index].latest.yaw; }

static float wt901RightSignedPitchDeg() {
  return wt901Pairs[1].rawPitchDeg;
}

static float wt901LeftRollCross() {
  return wt901Pairs[0].rollCrossDeg;
}

static float wt901RightRollCross() {
  return wt901Pairs[1].rollCrossDeg;
}

static float wt901LeftYawCross() {
  return wt901Pairs[0].yawCrossDeg;
}

static float wt901RightYawCross() {
  return wt901Pairs[1].yawCrossDeg;
}

// =====================================================
// K7 upper-arm quality getters
// Pair 0 fixed IMU = A (left upper arm). The previously tested A-X direction deviation is still used for
// per-repetition upper-arm quality; Y/Z/full-orientation stay diagnostic only.
// =====================================================
static float wt901LeftFixedMoveRollDeg() { return wt901Pairs[0].fixedMoveRollDeg; }
static float wt901LeftFixedMovePitchDeg() { return wt901Pairs[0].fixedMovePitchDeg; }
static float wt901LeftFixedMoveYawDeg() { return wt901Pairs[0].fixedMoveYawDeg; }
static float wt901LeftUpperArmDeviationDeg() { return wt901Pairs[0].fixedOrientationDeviationDeg; }
static float wt901LeftUpperArmXAxisDevDeg() { return wt901Pairs[0].fixedXAxisDeviationDeg; }
static float wt901LeftUpperArmYAxisDevDeg() { return wt901Pairs[0].fixedYAxisDeviationDeg; }
static float wt901LeftUpperArmZAxisDevDeg() { return wt901Pairs[0].fixedZAxisDeviationDeg; }
static float wt901RightFixedMoveRollDeg() { return wt901Pairs[1].fixedMoveRollDeg; }
static float wt901RightFixedMovePitchDeg() { return wt901Pairs[1].fixedMovePitchDeg; }
static float wt901RightFixedMoveYawDeg() { return wt901Pairs[1].fixedMoveYawDeg; }

static float wt901PairFixedMoveMaxDeg(int pairIndex) {
  float r = fabsf(wt901Pairs[pairIndex].fixedMoveRollDeg);
  float p = fabsf(wt901Pairs[pairIndex].fixedMovePitchDeg);
  float y = fabsf(wt901Pairs[pairIndex].fixedMoveYawDeg);
  return max(r, max(p, y));
}

static float wt901LeftFixedMoveMaxDeg() { return wt901PairFixedMoveMaxDeg(0); }
static float wt901RightFixedMoveMaxDeg() { return wt901PairFixedMoveMaxDeg(1); }

static uint32_t wt901PacketCount(int index) {
  return wt901Slots[index].packetCount;
}

static void wt901PrintStatus() {
  Serial.println();
  Serial.println("========== REAL IMU A/B/C/D/E STATUS ==========");
  int good = 0;
  for (int i = 0; i < WT901_ACTIVE_COUNT; i++) {
    int slot = WT901_ACTIVE_INDICES[i];
    WT901Slot &imu = wt901Slots[slot];
    bool receiving = wt901SlotReceiving(slot);
    if (receiving) good++;

    Serial.print("IMU_");
    Serial.print(imu.role);
    Serial.print("  mac=");
    Serial.print(imu.mac);
    Serial.print("  found=");
    Serial.print(imu.found ? "YES" : "NO");
    Serial.print("  connected=");
    Serial.print(imu.connected ? "YES" : "NO");
    Serial.print("  notify=");
    Serial.print(imu.notifyReady ? "YES" : "NO");
    Serial.print("  packets=");
    Serial.print(imu.packetCount);
    Serial.print("  last=");
    if (imu.lastPacketMs == 0) {
      Serial.print("never");
    } else {
      Serial.print(millis() - imu.lastPacketMs);
      Serial.print("ms ago");
    }
    Serial.print("  receiving=");
    Serial.println(receiving ? "YES" : "NO");
  }

  Serial.print("Receiving IMUs: ");
  Serial.print(good);
  Serial.print('/');
  Serial.println(WT901_ACTIVE_COUNT);

  Serial.print("zero=");
  Serial.print(wt901ZeroReady() ? "YES" : "NO");
  Serial.print("  angles_valid=");
  Serial.println(wt901AnglesValid() ? "YES" : "NO");

  Serial.print("K7 ref_angle=");
  Serial.print(wt901LeftAngleDeg(), 2);
  Serial.print(" raw_relative_rotation=");
  Serial.print(wt901LeftRawJointAngleDeg(), 2);
  Serial.print(" sync_skew_ms="); Serial.print(wt901LeftPairSkewMs());
  Serial.print(" C1="); Serial.print(wt901LeftCandidateAinvBDeg(), 2);
  Serial.print(" C2="); Serial.print(wt901LeftCandidateBAinvDeg(), 2);
  Serial.print(" C3="); Serial.print(wt901LeftCandidateWorldDeltaDeg(), 2);
  Serial.print(" C4="); Serial.print(wt901LeftCandidateBodyDeltaDeg(), 2);
  Serial.print(" upper_arm_offset_X=");
  Serial.println(wt901LeftUpperArmXAxisDevDeg(), 2);


  if (good == WT901_ACTIVE_COUNT) {
    Serial.println("RESULT: PASS, ESP32 is receiving data from A/B/C/D/E.");
  } else {
    Serial.println("RESULT: WAIT/FAIL, keep A/B/C/D/E on, APP disconnected, and check distance/power.");
  }
  Serial.println("=========================================");
  Serial.println();
}
