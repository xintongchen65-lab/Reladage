#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <pgmspace.h>
#include <ctype.h>
#include <math.h>
#include <string.h>
#include <stddef.h>
#include <Preferences.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

#include "pin_config.h"
#include "real_imu_abcde.h"
#include "torso_reference.h"
#include "gyro_elbow_k11.h"
#include "k11_right_engine.h"
#include "training_logic.h"
#include "training_assessment_v4.h"
#include "power_resume_checkpoint.h"
#include "sd_logger.h"
#include "motion_output.h"
#include "rehab_voice_link.h"
#include "rehab_v5_action_catalog.h"
#include "src/rehab/application/rehab_system_orchestrator.h"

#define RM_BUILD_VERSION "RehabMotion_v5_REPOSITORY_8_ACTION_UI_INTEGRATION"

static void latchCompletedAttemptFeedback(const TrainingData &d);

// =====================================================
// RehabMotion V4.4.16 continuous body-frame revision.
// V4.4.12 per-repetition anchor/wait logic is abandoned: calibration happens once,
// then normal training is continuous and normal breathing never gates repetitions.
// A=left upper, B=left lower, C=right upper, D=right lower, E=waist/abdomen torso reference (WT901BLE67).
// E supplies body-relative azimuth for plane P and torso tilt T; K11 elbow ROM remains E-independent.
// BODY-FRONT calibration builds each arm IMU's own local semantic front/side basis.
// V4.4.30 formal P follows the user's body coordinates:
// X=front, Y=side, Z=body vertical. Standard curl is X-Z.
// P uses A/B differential flexion axis rotated by A-E/B-E physical vertical gyro heading + A side tilt + K11 hinge consistency; absolute B/E yaw/quaternion heading is diagnostic only.
// A-vs-E BODY_AZIMUTH remains diagnostic only.
// V4.4.21 also requires sustained plane evidence before a completed repetition can fail.
// K11 elbow ROM is computed after projecting A and B gyros into EACH sensor's own
// calibrated FRONT/SIDE/DOWN semantic frame, then subtracting the anatomical SIDE components.
// This removes the invalid assumption that A-local XYZ and B-local XYZ are aligned.
// U and P are intentionally separate: sagittal upper-arm lift -> U; side/azimuth errors -> P.
// =====================================================

TFT_eSPI tft = TFT_eSPI();

// 与TFT共用同一条SPI总线，SDLogger会使用这个实例初始化SD卡。
SPIClass &sharedSPI = SPI;

TrainingLogic leftTraining;
TrainingLogic rightTraining;
SDLogger logger;
V4SessionAssessment v4Assessment;
V4SessionAssessment v4AssessmentRight;

// Unified product-level application path. The mature V5 calibration/training loop
// remains the real-time hardware path; this coordinator consumes the same live
// measurements and fans completed repetitions out to report/game/twin/sync modules.
static rehab::RehabSystemOrchestrator productOrchestrator;
static unsigned long productOrchestratorLastMs = 0;


// =====================================================
// 7-inch VIEWE screen link (UART1 on GPIO8/9)
// 115200 baud, framed ASCII + CRC8.
// Live telemetry is sent at 25 Hz; UI commands are received asynchronously.
// =====================================================
HardwareSerial ScreenLink(1);
HardwareSerial VoiceLink(2);
RehabVoiceLink rehabVoice(VoiceLink);
static constexpr uint32_t SCREEN_LINK_BAUD = 115200;
static constexpr uint32_t SCREEN_LIVE_INTERVAL_MS = 40; // 25 Hz
static constexpr uint32_t SCREEN_COUNTDOWN_TIMEOUT_MS = 6000;

static bool integratedScreenFlowActive = false;
static bool integratedAutoTrainingStarted = false;
static bool integratedCountdownActive = false;
static unsigned long integratedCountdownStartedMs = 0;
static unsigned long screenLastLiveTxMs = 0;
static uint32_t screenTxSeq = 0;
static uint32_t screenLastCommandSeq = 0;
static bool voiceSensorMonitoringActive = false;
static bool voiceSensorsPreviouslyReady = true;

static void queueVoiceEvent(const char *command, bool queued) {
  Serial.print(queued ? "VOICE_QUEUED: " : "VOICE_QUEUE_FULL: ");
  Serial.println(command);
}

// Explicit declaration because rest/calibration code calls this before its definition.
static void sendScreenLiveFrame(bool force);
static uint8_t currentImuMask();

static uint8_t screenCrc8(const uint8_t *data, size_t len) {
  uint8_t crc = 0;
  for (size_t i = 0; i < len; ++i) {
    crc ^= data[i];
    for (uint8_t b = 0; b < 8; ++b) crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x07) : (uint8_t)(crc << 1);
  }
  return crc;
}

static bool screenWritePacket(const char *payload) {
  if (!payload) return false;
  const size_t n = strlen(payload);
  const uint8_t crc = screenCrc8((const uint8_t*)payload, n);
  char line[240];
  const int len = snprintf(line, sizeof(line), "@%s*%02X\n", payload, crc);
  if (len <= 0 || len >= (int)sizeof(line)) return false;
  // Never stall the motion loop waiting for UART space. A newer 25-Hz LIVE frame
  // will arrive 40 ms later, so dropping one congested frame is safer than blocking.
  if (ScreenLink.availableForWrite() < len) return false;
  ScreenLink.write((const uint8_t*)line, (size_t)len);
  return true;
}

static void screenSendCommandAck(uint32_t seq, bool ok) {
  char p[48];
  snprintf(p, sizeof(p), "A,%lu,%d", (unsigned long)seq, ok ? 1 : 0);
  screenWritePacket(p);
}

static float v449BodyRelativeAzimuthDeg() {
  float v=torsoRefBodyRelativeAzimuthDeg();
  return (isfinite(v)&&v>=0.0f&&v<=180.0f)?v:0.0f;
}
static float v449UpperSidePlaneDeg() {
  float v=torsoRefUpperSideTiltDeg();
  return (isfinite(v)&&v>=0.0f&&v<=180.0f)?v:0.0f;
}
static float v449ForeSidePlaneDeg() {
  float v=torsoRefForeSideTiltDeg();
  return (isfinite(v)&&v>=0.0f&&v<=180.0f)?v:0.0f;
}
static float v449ForeQuatPlaneDeg() {
  float v=torsoRefForeQuaternionPlaneDeviationDeg();
  return (isfinite(v)&&v>=0.0f&&v<=180.0f)?v:0.0f;
}
static float v449ForeQuatPlaneRawDeg() {
  float v=torsoRefForeQuaternionPlaneRawDeviationDeg();
  return (isfinite(v)&&v>=0.0f&&v<=180.0f)?v:0.0f;
}
static float v449HingePlaneDeviationDeg() {
  if(!wt901K11Ready()) return 0.0f;
  float v=wt901K11HingeAxisDeviationDeg();
  return (isfinite(v)&&v>=0.0f&&v<=180.0f)?v:0.0f;
}
// V4.4.30 formal forearm motion-plane metric.
// X=front, Y=side, Z=vertical. Standard X-Z curl has a horizontal normal learned
// from B during the one K11 calibration flex. During training that current B gyro
// axis is rotated by B-minus-E physical vertical gyro heading into BODY coordinates.
// Whole-body turning cancels because it appears in both B and E.
static float v4430ForearmMotionPlaneDeg() {
  static float heldDeg = 0.0f;
  static bool heldReady = false;
  if(!wt901K11Ready() || !torsoRefFrontReady()) {
    heldDeg = 0.0f; heldReady = false; return 0.0f;
  }

  const float elbow = wt901K11ElbowAngleDeg();
  const float elbowSpeed = wt901K11ElbowSpeedDegS();
  if(elbow <= 14.0f && fabsf(elbowSpeed) <= 8.0f) {
    // Clear only the last sampled motion-axis angle. The A-E/B-E Z-heading
    // integrals remain intact, so rotating the arm plane while hanging down is
    // NOT silently accepted as a new standard.
    heldDeg = 0.0f; heldReady = false;
  }

  float af = wt901K11FixedRateFrontDegS();
  float as = wt901K11FixedRateSideDegS();
  float bf = wt901K11MovingRateFrontDegS();
  float bs = wt901K11MovingRateSideDegS();

  // A and B semantic frames are sensor/segment attached. Rotate each horizontal
  // gyro axis by its own physical Z rotation relative to E so both are expressed
  // in the SAME BODY frame before subtracting them.
  const float ya = torsoRefUpperarmRelativeBodyYawSignedDeg() * 0.01745329251994329577f;
  const float yb = torsoRefForearmRelativeBodyYawSignedDeg() * 0.01745329251994329577f;
  const float ca = cosf(ya), sa = sinf(ya);
  const float cb = cosf(yb), sb = sinf(yb);
  const float aBodyF = ca*af - sa*as;
  const float aBodyS = sa*af + ca*as;
  const float bBodyF = cb*bf - sb*bs;
  const float bBodyS = sb*bf + cb*bs;
  float relF = bBodyF - aBodyF;
  float relS = bBodyS - aBodyS;
  const float h = sqrtf(relF*relF + relS*relS);
  if(!isfinite(h) || h < 10.0f) return heldReady ? heldDeg : 0.0f;
  relF /= h; relS /= h;

  // The one K11 calibration curl already learned the standard differential hinge
  // axis in calibrated FRONT/SIDE/DOWN coordinates. At calibration A-E=B-E=0,
  // so its horizontal projection is also the standard BODY-frame plane normal.
  float refF = wt901K11LearnedAxisFront();
  float refS = wt901K11LearnedAxisSide();
  const float rn = sqrtf(refF*refF + refS*refS);
  if(!isfinite(rn) || rn < 1e-4f) return heldReady ? heldDeg : 0.0f;
  refF /= rn; refS /= rn;

  float dot = fabsf(relF*refF + relS*refS); // +/- axis is the same motion plane
  if(dot > 1.0f) dot = 1.0f;
  if(dot < 0.0f) dot = 0.0f;
  float dev = acosf(dot) * 57.2957795131f;
  if(!isfinite(dev)) return heldReady ? heldDeg : 0.0f;
  if(dev > 90.0f) dev = 90.0f;
  heldDeg = dev; heldReady = true;
  return heldDeg;
}

// V4.4.31F emergency plane tracker. The V4.4.31 A-E vertical-gyro
// relative heading is kept, but only as a DELTA inside one attempt. Long-term
// accumulated yaw is never used as the pass/fail value.
struct V4431FPlaneTracker {
  bool baselineReady=false;
  float baselineAeDeg=0.0f;
  float currentDeg=0.0f;
};
static V4431FPlaneTracker v4431fPlane;

static float v4431fWrap180(float x){
  while(x>180.0f)x-=360.0f;
  while(x<-180.0f)x+=360.0f;
  return x;
}
static float v4431fAeNowSignedDeg(){
  return torsoRefUpperarmRelativeBodyYawSignedDeg();
}
static void v4431fPlaneResetBaseline(const char *reason){
  const float ae=v4431fAeNowSignedDeg();
  if(!isfinite(ae)) return;
  v4431fPlane.baselineAeDeg=ae;
  v4431fPlane.currentDeg=0.0f;
  v4431fPlane.baselineReady=true;
  Serial.printf("REHABMOTION_V5_PLANE_BASELINE reason=%s AE=%.2f\n",reason?reason:"",ae);
}
static float v4431fPerRepPlaneDeg(){
  const float ae=v4431fAeNowSignedDeg();
  if(!isfinite(ae)) return 0.0f;
  if(!v4431fPlane.baselineReady){
    v4431fPlane.baselineAeDeg=ae;
    v4431fPlane.baselineReady=true;
  }
  float d=fabsf(v4431fWrap180(ae-v4431fPlane.baselineAeDeg));
  if(!isfinite(d)) d=0.0f;
  if(d>90.0f)d=90.0f;
  v4431fPlane.currentDeg=d;
  return d;
}
static float v449BodyPlaneDeviationDeg() {
  // Formal plane = per-attempt upper-arm rotation RELATIVE TO TORSO E.
  // Correct curl: A and E have little relative vertical rotation -> small P.
  // Arm goes to the side: A rotates relative to E -> P rises.
  // Whole person turns: A and E rotate together -> their difference cancels.
  // Previous reps cannot contaminate the next because baseline resets after each attempt.
  return v4431fPerRepPlaneDeg();
}
static float v449FormalPlaneDeviationDeg() {
  return v449BodyPlaneDeviationDeg();
}

// Legacy session counters kept for JSON compatibility; K10 quality judgement is disabled.
int sessionBaselineRepCount = 0;


// =====================================================
// Buzzer feedback - K11.3.1 quiet deterministic engine.
//
// Earlier K11.x versions asked the main loop to switch the active-HIGH buzzer
// LOW after 5-10 ms. TFT/SD/JSON/BLE work can delay the next loop pass, so a
// requested 6 ms pulse could remain HIGH much longer. K11.3.1 moves the pulse
// timing into a dedicated FreeRTOS task. Every audible pulse is a fixed 1.5 ms
// tap and no caller can make it longer.
// =====================================================
struct BuzzerCommand {
  uint8_t pulses;
  uint16_t pulseUs;
  uint16_t gapMs;
  uint32_t generation;
};

static QueueHandle_t buzzerQueue = nullptr;
static TaskHandle_t buzzerTaskHandle = nullptr;
static volatile uint32_t buzzerGeneration = 1;

static constexpr uint16_t BUZZER_QUIET_PULSE_US = 1500;
static constexpr uint16_t BUZZER_QUIET_GAP_MS = 85;

static void buzzerTask(void *param) {
  (void)param;
  BuzzerCommand cmd{};
  for (;;) {
    if (xQueueReceive(buzzerQueue, &cmd, portMAX_DELAY) != pdTRUE) continue;
    if (cmd.generation != buzzerGeneration) continue;

    for (uint8_t i = 0; i < cmd.pulses; ++i) {
      if (cmd.generation != buzzerGeneration) break;
      digitalWrite(PIN_BUZZER, HIGH);
      delayMicroseconds(cmd.pulseUs);
      digitalWrite(PIN_BUZZER, LOW);
      if (i + 1 < cmd.pulses) vTaskDelay(pdMS_TO_TICKS(cmd.gapMs));
    }
    digitalWrite(PIN_BUZZER, LOW);
  }
}

void buzzerOff() {
  ++buzzerGeneration;
  digitalWrite(PIN_BUZZER, LOW);
  if (buzzerQueue) xQueueReset(buzzerQueue);
}

void buzzerInit() {
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);
  if (!buzzerQueue) buzzerQueue = xQueueCreate(1, sizeof(BuzzerCommand));
  if (buzzerQueue && !buzzerTaskHandle) {
    xTaskCreatePinnedToCore(buzzerTask, "rehab_buzzer", 2048, nullptr, 3, &buzzerTaskHandle, 1);
  }
}

void buzzerStartPattern(uint8_t pulses, unsigned long pulseMs, unsigned long gapMs) {
  (void)pulseMs;
  if (!buzzerQueue || pulses == 0) return;

  ++buzzerGeneration;
  digitalWrite(PIN_BUZZER, LOW);
  xQueueReset(buzzerQueue);

  BuzzerCommand cmd{};
  cmd.pulses = (pulses > 2) ? 2 : pulses;
  cmd.pulseUs = BUZZER_QUIET_PULSE_US;
  cmd.gapMs = (gapMs == 0) ? BUZZER_QUIET_GAP_MS : (uint16_t)gapMs;
  cmd.generation = buzzerGeneration;
  xQueueOverwrite(buzzerQueue, &cmd);
}

void buzzerBeep(unsigned long durationMs) {
  (void)durationMs;
  buzzerStartPattern(1, 0, 0);
}

void buzzerDoubleBeep(unsigned long pulseMs = 0, unsigned long gapMs = BUZZER_QUIET_GAP_MS) {
  (void)pulseMs;
  buzzerStartPattern(2, 0, gapMs);
}

void buzzerUpdate() {
  // Compatibility no-op. Pulse timing no longer depends on loop cadence.
}


static const int SCREEN_W = 480;
static const int SCREEN_H = 320;

static const uint16_t C_BG       = TFT_BLACK;
static const uint16_t C_PANEL    = 0x1082;
static const uint16_t C_PANEL_2  = 0x2104;
static const uint16_t C_LINE     = 0x39E7;
static const uint16_t C_TEXT     = TFT_WHITE;
static const uint16_t C_MUTED    = TFT_LIGHTGREY;
static const uint16_t C_ACCENT   = TFT_CYAN;
static const uint16_t C_OK       = TFT_GREEN;
static const uint16_t C_WARN     = TFT_YELLOW;
static const uint16_t C_DANGER   = TFT_RED;
static const uint16_t C_PURPLE   = TFT_MAGENTA;

// =====================================================
// 16x16 中文点阵字库：只包含本界面用到的常用字。
// 不依赖外部字体文件，烧录后可直接显示中文。
// =====================================================

struct CnGlyph16 {
  uint32_t code;
  uint8_t data[32];
};

const CnGlyph16 CN_FONT[] PROGMEM = {
  {0x4E00, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}, // 一
  {0x4E0A, {0x03, 0x80, 0x03, 0x80, 0x03, 0x80, 0x03, 0x80, 0x03, 0x80, 0x03, 0xFC, 0x03, 0xFC, 0x03, 0x80, 0x03, 0x80, 0x03, 0x80, 0x03, 0x80, 0x03, 0x80, 0x03, 0x80, 0x7F, 0xFE, 0x7F, 0xFF, 0x00, 0x00}}, // 上
  {0x4E0B, {0x7F, 0xFE, 0x7F, 0xFE, 0x03, 0x80, 0x03, 0x80, 0x03, 0xC0, 0x03, 0xE0, 0x03, 0xF8, 0x03, 0xBC, 0x03, 0x9C, 0x03, 0x80, 0x03, 0x80, 0x03, 0x80, 0x03, 0x80, 0x03, 0x80, 0x03, 0x80, 0x00, 0x00}}, // 下
  {0x4E0D, {0x7F, 0xFE, 0x7F, 0xFE, 0x01, 0xC0, 0x03, 0x80, 0x03, 0xA0, 0x07, 0xF0, 0x0F, 0xF8, 0x3D, 0x9C, 0x79, 0x8E, 0x71, 0x86, 0x01, 0x80, 0x01, 0x80, 0x01, 0x80, 0x01, 0x80, 0x01, 0x80, 0x00, 0x00}}, // 不
  {0x4E0E, {0x0C, 0x00, 0x0F, 0xFC, 0x0F, 0xFC, 0x1C, 0x00, 0x1C, 0x00, 0x1F, 0xF8, 0x1F, 0xF8, 0x18, 0x38, 0x00, 0x38, 0x7F, 0xFE, 0x7F, 0xFE, 0x00, 0x30, 0x00, 0x70, 0x03, 0xF0, 0x03, 0xE0, 0x00, 0x00}}, // 与
  {0x4E2A, {0x03, 0x80, 0x03, 0xC0, 0x07, 0xE0, 0x0E, 0x70, 0x1C, 0x3C, 0x79, 0x9E, 0xE1, 0x8F, 0x41, 0x82, 0x01, 0x80, 0x01, 0x80, 0x01, 0x80, 0x01, 0x80, 0x01, 0x80, 0x01, 0x80, 0x01, 0x80, 0x00, 0x00}}, // 个
  {0x4E2D, {0x01, 0x80, 0x01, 0x80, 0x01, 0x80, 0x7F, 0xFE, 0x7F, 0xFE, 0x71, 0x8E, 0x71, 0x8E, 0x71, 0x8E, 0x71, 0x8E, 0x7F, 0xFE, 0x7F, 0xFE, 0x61, 0x86, 0x01, 0x80, 0x01, 0x80, 0x01, 0x80, 0x01, 0x80}}, // 中
  {0x4E32, {0x01, 0x80, 0x01, 0x80, 0x3F, 0xFC, 0x3F, 0xFC, 0x31, 0x8C, 0x3F, 0xFC, 0x3F, 0xFC, 0x01, 0x80, 0x7F, 0xFE, 0x7F, 0xFE, 0x71, 0x8E, 0x7F, 0xFE, 0x7F, 0xFE, 0x71, 0x8E, 0x01, 0x80, 0x01, 0x80}}, // 串
  {0x4E3A, {0x13, 0x00, 0x3B, 0x00, 0x1B, 0x00, 0x1B, 0x00, 0x7F, 0xFE, 0x7F, 0xFE, 0x03, 0x0E, 0x07, 0x0E, 0x07, 0xCE, 0x06, 0xEE, 0x0E, 0x6E, 0x1C, 0x6C, 0x1C, 0x0C, 0x38, 0x0C, 0x70, 0x7C, 0x20, 0x78}}, // 为
  {0x4E3B, {0x00, 0x00, 0x03, 0x00, 0x03, 0x80, 0x01, 0xC0, 0x7F, 0xFE, 0x7F, 0xFE, 0x01, 0x80, 0x01, 0x80, 0x01, 0x80, 0x3F, 0xFC, 0x3F, 0xFC, 0x01, 0x80, 0x01, 0x80, 0x01, 0x80, 0x7F, 0xFE, 0x7F, 0xFE}}, // 主
  {0x4E49, {0x00, 0x00, 0x03, 0x00, 0x03, 0x8C, 0x31, 0x8C, 0x39, 0xDC, 0x18, 0x18, 0x1C, 0x38, 0x0C, 0x30, 0x0E, 0x70, 0x07, 0xE0, 0x03, 0xC0, 0x03, 0xC0, 0x0F, 0xF0, 0x3E, 0x7C, 0x78, 0x1E, 0x60, 0x06}}, // 义
  {0x4EC5, {0x0C, 0x00, 0x1C, 0x00, 0x1F, 0xFE, 0x3B, 0xFE, 0x3B, 0x0E, 0x7B, 0x8C, 0xF9, 0x8C, 0xF9, 0x9C, 0x79, 0xD8, 0x38, 0xF8, 0x38, 0xF0, 0x38, 0x70, 0x38, 0xF8, 0x3B, 0xDE, 0x3F, 0x8F, 0x3A, 0x02}}, // 仅
  {0x4EF6, {0x18, 0x60, 0x1B, 0xF0, 0x1B, 0x70, 0x3B, 0xFE, 0x77, 0xFE, 0x76, 0x70, 0xF6, 0x70, 0x70, 0x70, 0x37, 0xFF, 0x37, 0xFF, 0x30, 0x70, 0x30, 0x70, 0x30, 0x70, 0x30, 0x70, 0x30, 0x70, 0x30, 0x60}}, // 件
  {0x4F11, {0x0C, 0x60, 0x1C, 0x60, 0x1C, 0x60, 0x38, 0x60, 0x3F, 0xFE, 0x7F, 0xFF, 0xF8, 0xF0, 0x78, 0xF8, 0x79, 0xF8, 0x39, 0xFC, 0x3B, 0x6C, 0x3F, 0x6E, 0x3E, 0x67, 0x3C, 0x62, 0x38, 0x60, 0x38, 0x60}}, // 休
  {0x4F38, {0x18, 0x60, 0x18, 0x60, 0x18, 0x60, 0x3F, 0xFE, 0x37, 0xFE, 0x76, 0x66, 0xF7, 0xFE, 0x77, 0xFE, 0x36, 0x66, 0x37, 0xFE, 0x37, 0xFE, 0x36, 0x66, 0x30, 0x60, 0x30, 0x60, 0x30, 0x60, 0x30, 0x60}}, // 伸
  {0x4F4D, {0x0C, 0x60, 0x1C, 0x60, 0x18, 0x60, 0x3F, 0xFE, 0x3F, 0xFE, 0x78, 0x00, 0xF9, 0x8C, 0x79, 0x8C, 0x39, 0x9C, 0x39, 0x9C, 0x39, 0x98, 0x39, 0xD8, 0x38, 0xB8, 0x3F, 0xFF, 0x3F, 0xFF, 0x38, 0x00}}, // 位
  {0x4F53, {0x18, 0x60, 0x18, 0x60, 0x38, 0x60, 0x37, 0xFE, 0x77, 0xFF, 0x70, 0xF0, 0xF1, 0xF8, 0x71, 0xF8, 0x33, 0xFC, 0x33, 0x6C, 0x37, 0x6E, 0x3F, 0xFF, 0x3D, 0xFA, 0x30, 0x60, 0x30, 0x60, 0x30, 0x60}}, // 体
  {0x4F5C, {0x08, 0xC0, 0x1C, 0xC0, 0x19, 0xC0, 0x39, 0xFF, 0x3B, 0xFF, 0x7F, 0x60, 0xFE, 0x7E, 0x7A, 0x7E, 0x38, 0x60, 0x38, 0x60, 0x38, 0x7E, 0x38, 0x7F, 0x38, 0x60, 0x38, 0x60, 0x38, 0x60, 0x30, 0x60}}, // 作
  {0x4F69, {0x3F, 0xFC, 0x37, 0xFC, 0x37, 0xFC, 0x77, 0xFC, 0x76, 0x6C, 0xF6, 0x6C, 0xF7, 0xFC, 0x77, 0x5C, 0x37, 0x5C, 0x37, 0x5C, 0x3D, 0x5C, 0x3D, 0x76, 0x3D, 0x77, 0x3C, 0x47, 0x38, 0x46, 0x00, 0x00}}, // 佩
  {0x4F9D, {0x08, 0x60, 0x1C, 0x60, 0x18, 0x60, 0x3F, 0xFE, 0x7F, 0xFE, 0x70, 0xF0, 0xF1, 0xF6, 0x73, 0xB6, 0x3F, 0xBE, 0x3F, 0xB8, 0x37, 0x98, 0x33, 0x9C, 0x33, 0xBC, 0x37, 0xFE, 0x37, 0xE7, 0x36, 0x02}}, // 依
  {0x4FBF, {0x18, 0x00, 0x1F, 0xFE, 0x1F, 0xFE, 0x3B, 0xFE, 0x37, 0xFE, 0x77, 0x66, 0xF7, 0x7E, 0x77, 0xFE, 0x37, 0x66, 0x37, 0xFE, 0x33, 0xFE, 0x37, 0xE0, 0x33, 0xC0, 0x33, 0xF8, 0x3F, 0xFF, 0x36, 0x1E}}, // 便
  {0x4FDD, {0x08, 0x00, 0x1F, 0xFE, 0x1B, 0xFE, 0x3B, 0x06, 0x3B, 0x06, 0x7B, 0xFE, 0xFB, 0xFE, 0x78, 0x60, 0x3F, 0xFE, 0x3F, 0xFF, 0x38, 0xF8, 0x39, 0xFC, 0x3B, 0xFE, 0x3F, 0x6F, 0x3E, 0x66, 0x38, 0x60}}, // 保
  {0x5047, {0x3F, 0xFE, 0x3F, 0xFE, 0x36, 0xC6, 0x76, 0xC6, 0x77, 0xFE, 0xF7, 0xFE, 0x76, 0x3E, 0x77, 0xFE, 0x37, 0xB6, 0x36, 0x3C, 0x37, 0xFC, 0x37, 0x9C, 0x36, 0x3C, 0x36, 0xFF, 0x36, 0x46, 0x00, 0x00}}, // 假
  {0x505C, {0x18, 0x60, 0x1F, 0xFE, 0x1F, 0xFF, 0x38, 0x00, 0x73, 0xFE, 0x73, 0x0E, 0xF3, 0xFE, 0x70, 0x00, 0x37, 0xFF, 0x37, 0xFF, 0x37, 0xFF, 0x31, 0xFC, 0x30, 0x60, 0x30, 0x60, 0x31, 0xE0, 0x31, 0xE0}}, // 停
  {0x5148, {0x19, 0x80, 0x19, 0x80, 0x1F, 0xFC, 0x3F, 0xFC, 0x31, 0x80, 0x71, 0x80, 0x21, 0x80, 0x7F, 0xFE, 0x7F, 0xFE, 0x0E, 0x60, 0x0E, 0x60, 0x0C, 0x60, 0x1C, 0x66, 0x38, 0x66, 0x78, 0x7E, 0x60, 0x3C}}, // 先
  {0x5165, {0x1F, 0x80, 0x1F, 0xC0, 0x01, 0xC0, 0x01, 0xC0, 0x03, 0xC0, 0x03, 0xC0, 0x03, 0xC0, 0x07, 0xE0, 0x07, 0x60, 0x0E, 0x70, 0x1C, 0x38, 0x1C, 0x3C, 0x78, 0x1E, 0x70, 0x0E, 0x20, 0x06, 0x00, 0x00}}, // 入
  {0x5173, {0x0C, 0x30, 0x0E, 0x70, 0x06, 0x60, 0x1F, 0xF8, 0x1F, 0xF8, 0x01, 0x80, 0x01, 0x80, 0x1F, 0xFC, 0x1F, 0xF8, 0x03, 0xC0, 0x03, 0xE0, 0x07, 0x60, 0x0E, 0x78, 0x3C, 0x3C, 0x18, 0x08, 0x00, 0x00}}, // 关
  {0x51C6, {0x01, 0x98, 0x61, 0x98, 0x73, 0x98, 0x3B, 0xFE, 0x17, 0xFE, 0x0F, 0x30, 0x0F, 0xFE, 0x07, 0xFE, 0x13, 0x30, 0x1B, 0x30, 0x3B, 0xFE, 0x73, 0xFE, 0x63, 0x30, 0x63, 0xFF, 0x03, 0xFF, 0x03, 0x00}}, // 准
  {0x51CF, {0x00, 0x3C, 0x20, 0x3E, 0x70, 0x3A, 0x3F, 0xFE, 0x17, 0xFE, 0x07, 0xF8, 0x07, 0xFE, 0x16, 0x1E, 0x3F, 0xFE, 0x3F, 0xFC, 0x7D, 0x7C, 0x6D, 0xF8, 0xED, 0xFA, 0x4D, 0x7F, 0x18, 0xEF, 0x08, 0xC6}}, // 减
  {0x5207, {0x33, 0xFE, 0x33, 0xFE, 0x30, 0x66, 0x3E, 0x66, 0xFE, 0x66, 0xF8, 0xE6, 0x30, 0xE6, 0x30, 0xC6, 0x36, 0xC6, 0x36, 0xC6, 0x3F, 0xC6, 0x1F, 0x86, 0x03, 0x8E, 0x07, 0x3E, 0x02, 0x3C, 0x00, 0x00}}, // 切
  {0x5219, {0x7F, 0x86, 0x7F, 0xB6, 0x61, 0xB6, 0x6D, 0xB6, 0x6D, 0xB6, 0x6D, 0xB6, 0x6D, 0xB6, 0x6D, 0xB6, 0x6D, 0xB6, 0x6D, 0xB6, 0x7D, 0x36, 0x1E, 0x06, 0x3F, 0x06, 0x73, 0xBE, 0x60, 0x3C, 0x00, 0x00}}, // 则
  {0x521D, {0x19, 0xFE, 0x1B, 0xFE, 0x7E, 0x66, 0x7E, 0x66, 0x0C, 0x66, 0x1E, 0x66, 0x1F, 0xE6, 0x3E, 0xE6, 0x7E, 0xC6, 0xFE, 0xC6, 0x59, 0xC6, 0x19, 0x86, 0x1B, 0x8E, 0x1F, 0x3E, 0x1A, 0x3C, 0x00, 0x00}}, // 初
  {0x5230, {0x00, 0x06, 0x7F, 0x86, 0xFF, 0xE6, 0x3A, 0x66, 0x33, 0x66, 0x7F, 0xE6, 0x7F, 0xE6, 0x0D, 0xE6, 0x0C, 0x66, 0x7F, 0x66, 0x7F, 0x66, 0x0C, 0x66, 0x0C, 0x06, 0x7F, 0x86, 0xFF, 0xBE, 0x60, 0x1C}}, // 到
  {0x5236, {0x2C, 0x06, 0x7C, 0x36, 0x7F, 0xB6, 0x7F, 0xB6, 0xEC, 0x36, 0x7F, 0xF6, 0xFF, 0xF6, 0x0C, 0x36, 0x7F, 0xB6, 0x7F, 0xB6, 0x6D, 0xB6, 0x6D, 0x86, 0x6D, 0x86, 0x6F, 0x86, 0x6D, 0x9E, 0x0C, 0x1C}}, // 制
  {0x524D, {0x18, 0x18, 0x1C, 0x38, 0x7F, 0xFE, 0x7F, 0xFE, 0x00, 0x00, 0x3F, 0x2E, 0x7F, 0x6E, 0x73, 0x6E, 0x7F, 0x6E, 0x7F, 0x6E, 0x73, 0x6E, 0x7F, 0x6E, 0x7F, 0x6E, 0x73, 0x0E, 0x77, 0x3C, 0x77, 0x1C}}, // 前
  {0x52A8, {0x00, 0x30, 0x7F, 0x30, 0x7F, 0x30, 0x00, 0x30, 0x00, 0xFE, 0x7F, 0xFE, 0x7F, 0x76, 0x38, 0x76, 0x36, 0x76, 0x33, 0x66, 0x33, 0x66, 0x7F, 0xE6, 0x7F, 0xE6, 0x61, 0xCE, 0x01, 0xFE, 0x00, 0x9C}}, // 动
  {0x52A9, {0x00, 0x30, 0x7E, 0x70, 0x7E, 0x70, 0x66, 0x70, 0x7F, 0xFE, 0x7F, 0xFE, 0x66, 0x76, 0x66, 0x66, 0x7E, 0x66, 0x7E, 0x66, 0x66, 0xE6, 0x7F, 0xE6, 0xFF, 0xC6, 0xF9, 0xCE, 0x03, 0xBC, 0x01, 0x3C}}, // 助
  {0x5305, {0x0C, 0x00, 0x1C, 0x00, 0x1F, 0xFE, 0x3F, 0xFE, 0x70, 0x0E, 0xFF, 0xCC, 0x7F, 0xCC, 0x00, 0xCC, 0x00, 0xCC, 0x1F, 0xCC, 0x1F, 0xFC, 0x18, 0x38, 0x18, 0x06, 0x18, 0x06, 0x1F, 0xFE, 0x0F, 0xFC}}, // 包
  {0x5355, {0x0C, 0x18, 0x0C, 0x38, 0x0E, 0x30, 0x3F, 0xFC, 0x3F, 0xFC, 0x31, 0x8C, 0x3F, 0xFC, 0x3F, 0xFC, 0x3F, 0xFC, 0x3F, 0xFC, 0x01, 0x80, 0x7F, 0xFE, 0x7F, 0xFE, 0x01, 0x80, 0x01, 0x80, 0x01, 0x80}}, // 单
  {0x53C2, {0x06, 0x30, 0x0F, 0xFC, 0x7F, 0xFE, 0x7F, 0x04, 0x7F, 0xFE, 0x7F, 0xFF, 0x1C, 0x38, 0x3F, 0x9C, 0x7F, 0x4E, 0xE1, 0xE6, 0x4F, 0xD0, 0x0E, 0x38, 0x01, 0xF0, 0x1F, 0xC0, 0x1E, 0x00, 0x00, 0x00}}, // 参
  {0x53D1, {0x19, 0x98, 0x19, 0x98, 0x1B, 0x9C, 0x3B, 0x88, 0x3F, 0xFE, 0x3F, 0xFE, 0x07, 0x00, 0x07, 0xFC, 0x0F, 0xFC, 0x0F, 0x18, 0x1F, 0xB8, 0x39, 0xF0, 0x79, 0xF0, 0x67, 0xFC, 0x4F, 0xBF, 0x0E, 0x0E}}, // 发
  {0x53D8, {0x01, 0x80, 0x01, 0x80, 0x7F, 0xFE, 0x7F, 0xFE, 0x1E, 0x78, 0x36, 0x7C, 0x76, 0x6E, 0x26, 0x64, 0x3F, 0xF8, 0x3F, 0xF8, 0x1C, 0x38, 0x0E, 0x70, 0x07, 0xE0, 0x1F, 0xF8, 0x7F, 0xFE, 0x78, 0x1E}}, // 变
  {0x53E3, {0x00, 0x00, 0x3F, 0xFE, 0x3F, 0xFE, 0x30, 0x0E, 0x30, 0x0E, 0x30, 0x0E, 0x30, 0x0E, 0x30, 0x0E, 0x30, 0x0E, 0x30, 0x0E, 0x30, 0x0E, 0x3F, 0xFE, 0x3F, 0xFE, 0x30, 0x0E, 0x30, 0x0C, 0x00, 0x00}}, // 口
  {0x53EA, {0x3F, 0xFC, 0x3F, 0xFC, 0x38, 0x0C, 0x38, 0x0C, 0x38, 0x0C, 0x38, 0x0C, 0x38, 0x0C, 0x3F, 0xFC, 0x3F, 0xFC, 0x10, 0x08, 0x0E, 0x30, 0x1E, 0x78, 0x3C, 0x1C, 0x78, 0x0E, 0x20, 0x04, 0x00, 0x00}}, // 只
  {0x53EF, {0x7F, 0xFE, 0x7F, 0xFE, 0x00, 0x1C, 0x00, 0x1C, 0x3F, 0x9C, 0x3F, 0x9C, 0x31, 0x9C, 0x31, 0x9C, 0x31, 0x9C, 0x3F, 0x9C, 0x3F, 0x9C, 0x30, 0x1C, 0x00, 0x1C, 0x00, 0xF8, 0x00, 0xF8, 0x00, 0x00}}, // 可
  {0x53F3, {0x03, 0x00, 0x03, 0x00, 0x07, 0x00, 0x7F, 0xFE, 0x7F, 0xFE, 0x0E, 0x00, 0x0C, 0x00, 0x1C, 0x00, 0x1F, 0xFC, 0x3F, 0xFC, 0x7C, 0x0C, 0xEC, 0x0C, 0x4C, 0x0C, 0x0F, 0xFC, 0x0F, 0xFC, 0x0C, 0x0C}}, // 右
  {0x540C, {0x7F, 0xFE, 0x7F, 0xFE, 0x60, 0x06, 0x6F, 0xF6, 0x6F, 0xF6, 0x60, 0x06, 0x67, 0xE6, 0x67, 0xE6, 0x66, 0x66, 0x66, 0x66, 0x67, 0xE6, 0x67, 0xE6, 0x66, 0x06, 0x60, 0x1E, 0x60, 0x1C, 0x00, 0x00}}, // 同
  {0x540E, {0x00, 0x38, 0x3F, 0xFC, 0x3F, 0xF0, 0x3C, 0x00, 0x30, 0x00, 0x3F, 0xFF, 0x3F, 0xFF, 0x30, 0x00, 0x37, 0xFC, 0x37, 0xFC, 0x36, 0x0C, 0x76, 0x0C, 0x76, 0x0C, 0x67, 0xFC, 0xE7, 0xFC, 0x46, 0x0C}}, // 后
  {0x542B, {0x03, 0xC0, 0x07, 0xE0, 0x0E, 0x70, 0x3F, 0xFC, 0xFF, 0xFF, 0x40, 0x02, 0x3F, 0xF8, 0x1F, 0xF8, 0x00, 0x30, 0x00, 0x70, 0x1F, 0xFC, 0x1F, 0xFC, 0x18, 0x1C, 0x18, 0x1C, 0x1F, 0xFC, 0x1F, 0xFC}}, // 含
  {0x542F, {0x00, 0x78, 0x3F, 0xFC, 0x3F, 0x80, 0x3F, 0xFC, 0x3F, 0xFC, 0x30, 0x0C, 0x30, 0x0C, 0x3F, 0xFC, 0x3F, 0xFC, 0x30, 0x00, 0x3F, 0xFE, 0x7F, 0xFE, 0x7C, 0x0E, 0x6F, 0xFE, 0x6F, 0xFE, 0x0C, 0x0C}}, // 启
  {0x548C, {0x02, 0x00, 0x3F, 0x00, 0x7E, 0xFE, 0x7C, 0xFE, 0x1C, 0xC6, 0x7F, 0xC6, 0x7F, 0xC6, 0x1C, 0xC6, 0x3E, 0xC6, 0x3E, 0xC6, 0x7F, 0xC6, 0x7E, 0xC6, 0xFC, 0xFE, 0x5C, 0xFE, 0x1C, 0xC6, 0x1C, 0x00}}, // 和
  {0x54CD, {0x00, 0x70, 0x7C, 0x70, 0x7C, 0x60, 0x6F, 0xFE, 0x6F, 0xFE, 0x6F, 0x06, 0x6F, 0x7E, 0x6F, 0x7E, 0x6F, 0x5E, 0x6F, 0x5E, 0x7F, 0x7E, 0x7F, 0x7E, 0x63, 0x46, 0x03, 0x06, 0x03, 0x0E, 0x03, 0x0E}}, // 响
  {0x56DE, {0x7F, 0xFE, 0x7F, 0xFE, 0x60, 0x06, 0x60, 0x06, 0x6F, 0xE6, 0x6F, 0xE6, 0x6E, 0x66, 0x6E, 0x66, 0x6F, 0xE6, 0x6F, 0xE6, 0x60, 0x06, 0x60, 0x06, 0x7F, 0xFE, 0x7F, 0xFE, 0x60, 0x06, 0x00, 0x00}}, // 回
  {0x56FA, {0x7F, 0xFE, 0x7F, 0xFE, 0x61, 0x86, 0x61, 0x86, 0x6F, 0xFE, 0x6F, 0xF6, 0x61, 0x86, 0x6F, 0xF6, 0x6F, 0xF6, 0x6C, 0x36, 0x6F, 0xF6, 0x67, 0xE6, 0x7F, 0xFE, 0x7F, 0xFE, 0x60, 0x06, 0x00, 0x00}}, // 固
  {0x56FE, {0x7F, 0xFE, 0x7F, 0xFE, 0x63, 0x06, 0x67, 0xF6, 0x7F, 0xF6, 0x6F, 0xE6, 0x67, 0xE6, 0x7F, 0xFE, 0x7B, 0xD6, 0x61, 0xC6, 0x6F, 0xE6, 0x61, 0xF6, 0x7F, 0xFE, 0x7F, 0xFE, 0x60, 0x06, 0x00, 0x00}}, // 图
  {0x57FA, {0x1C, 0x38, 0x7F, 0xFE, 0x7F, 0xFE, 0x1F, 0xF8, 0x1F, 0xF8, 0x1C, 0x38, 0x1F, 0xF8, 0x1F, 0xF8, 0xFF, 0xFF, 0x7F, 0xFE, 0x39, 0x9C, 0x7F, 0xFE, 0x6F, 0xF6, 0x01, 0x80, 0x3F, 0xFC, 0x3F, 0xFC}}, // 基
  {0x589E, {0x30, 0x78, 0x31, 0xFC, 0x33, 0x0C, 0x37, 0xFF, 0xFF, 0xFE, 0xFF, 0xFE, 0x33, 0xFE, 0x33, 0xFE, 0x33, 0xFE, 0x39, 0xFC, 0x3F, 0xFC, 0xFB, 0x8C, 0xE3, 0xFC, 0x03, 0x8C, 0x03, 0xFC, 0x03, 0xFC}}, // 增
  {0x5907, {0x07, 0x00, 0x0F, 0xF8, 0x1F, 0xF8, 0x7C, 0x70, 0x7F, 0xE0, 0x07, 0xE0, 0x7F, 0xFF, 0xFC, 0x3E, 0x7F, 0xFC, 0x3F, 0xFC, 0x39, 0x9C, 0x3F, 0xFC, 0x3F, 0xFC, 0x39, 0x9C, 0x3F, 0xFC, 0x3F, 0xFC}}, // 备
  {0x590D, {0x06, 0x00, 0x0F, 0xF0, 0x0F, 0xF8, 0x1C, 0x00, 0x3F, 0xF0, 0x1E, 0x30, 0x0F, 0xF0, 0x0E, 0x30, 0x0F, 0xF0, 0x07, 0x00, 0x07, 0xF0, 0x1F, 0xF0, 0x1F, 0xE0, 0x03, 0xE0, 0x1F, 0xF8, 0x1C, 0x38}}, // 复
  {0x5916, {0x0C, 0x38, 0x1C, 0x38, 0x1F, 0xB8, 0x1F, 0xB8, 0x39, 0xF8, 0x73, 0xF8, 0x73, 0x78, 0xFF, 0x38, 0x5F, 0x3C, 0x06, 0x3E, 0x0E, 0x3F, 0x0C, 0x3A, 0x1C, 0x38, 0x78, 0x38, 0x70, 0x38, 0x20, 0x38}}, // 外
  {0x5927, {0x01, 0x80, 0x01, 0x80, 0x01, 0x80, 0x01, 0x80, 0x01, 0x80, 0x7F, 0xFE, 0x7F, 0xFE, 0x03, 0xC0, 0x03, 0xC0, 0x07, 0xE0, 0x07, 0x60, 0x0E, 0x70, 0x1C, 0x38, 0x3C, 0x1E, 0x70, 0x0E, 0x20, 0x06}}, // 大
  {0x59CB, {0x18, 0x70, 0x30, 0x70, 0x30, 0x60, 0x7E, 0xEC, 0xFE, 0xCE, 0x77, 0xFE, 0x67, 0xFF, 0x6F, 0xC3, 0x6C, 0x00, 0x7C, 0xFE, 0x7C, 0xFE, 0x1C, 0xC6, 0x3E, 0xC6, 0x76, 0xFE, 0x70, 0xFE, 0x40, 0xC6}}, // 始
  {0x59FF, {0x21, 0x80, 0x7B, 0xFE, 0x3B, 0xFE, 0x07, 0x6E, 0x0A, 0xFC, 0x38, 0xF8, 0xF3, 0x9C, 0x47, 0x0E, 0x07, 0x06, 0x7F, 0xFF, 0x7F, 0xFE, 0x1C, 0x70, 0x1F, 0xE0, 0x0F, 0xF8, 0x7F, 0xFE, 0x7C, 0x0C}}, // 姿
  {0x5B57, {0x01, 0x80, 0x01, 0x80, 0x7F, 0xFE, 0x7F, 0xFE, 0x60, 0x06, 0x6F, 0xF6, 0x0F, 0xF0, 0x00, 0xE0, 0x01, 0xC0, 0x7F, 0xFE, 0x7F, 0xFE, 0x01, 0x80, 0x01, 0x80, 0x01, 0x80, 0x0F, 0x80, 0x07, 0x00}}, // 字
  {0x5B58, {0x03, 0x00, 0x07, 0x00, 0x7F, 0xFE, 0x7F, 0xFE, 0x0E, 0x00, 0x1D, 0xFC, 0x19, 0xFC, 0x38, 0x1C, 0x78, 0x38, 0xF8, 0x70, 0x7F, 0xFF, 0x1F, 0xFE, 0x18, 0x70, 0x18, 0x70, 0x19, 0xF0, 0x18, 0xE0}}, // 存
  {0x5B8C, {0x01, 0x80, 0x7F, 0xFE, 0x7F, 0xFE, 0x60, 0x06, 0x6F, 0xF6, 0x0F, 0xF0, 0x00, 0x00, 0x7F, 0xFE, 0x7F, 0xFE, 0x0E, 0x60, 0x0E, 0x60, 0x0C, 0x66, 0x3C, 0x66, 0x78, 0x7E, 0x60, 0x7E, 0x00, 0x00}}, // 完
  {0x5B9A, {0x01, 0x80, 0x7F, 0xFE, 0x7F, 0xFE, 0x60, 0x06, 0x7F, 0xFE, 0x1F, 0xF8, 0x01, 0x80, 0x19, 0x80, 0x19, 0xF8, 0x39, 0xFC, 0x3D, 0x80, 0x3D, 0x80, 0x7F, 0x80, 0x67, 0xFE, 0x61, 0xFE, 0x00, 0x00}}, // 定
  {0x5BF9, {0x00, 0x0C, 0x00, 0x0C, 0x7F, 0x0C, 0x7F, 0x0C, 0x07, 0xFF, 0x06, 0xFF, 0x66, 0x0C, 0x7E, 0xCC, 0x3E, 0xCC, 0x1C, 0xEC, 0x1E, 0x6C, 0x1F, 0x4C, 0x3B, 0x0C, 0x72, 0x0C, 0x60, 0x7C, 0x60, 0x38}}, // 对
  {0x5C0F, {0x01, 0x80, 0x01, 0x80, 0x01, 0x80, 0x01, 0x80, 0x19, 0x98, 0x39, 0x9C, 0x39, 0x9C, 0x31, 0x8E, 0x31, 0x8E, 0x71, 0x86, 0x61, 0x87, 0x61, 0x87, 0x01, 0x80, 0x01, 0x80, 0x0F, 0x80, 0x0F, 0x00}}, // 小
  {0x5C48, {0x3F, 0xFE, 0x3F, 0xFE, 0x30, 0x0E, 0x3F, 0xFE, 0x3F, 0xFC, 0x36, 0xEC, 0x36, 0xEC, 0x36, 0xEC, 0x37, 0xFC, 0x37, 0xFC, 0x7C, 0xE6, 0x7E, 0xE6, 0x6E, 0xE6, 0xEF, 0xFE, 0x6F, 0xFE, 0x00, 0x00}}, // 屈
  {0x5C4F, {0x3F, 0xFE, 0x3F, 0xFE, 0x3F, 0xFE, 0x3F, 0xFE, 0x33, 0x18, 0x33, 0x18, 0x3F, 0xFE, 0x3F, 0xFE, 0x33, 0x38, 0x3F, 0xFE, 0x7F, 0xFE, 0x63, 0x38, 0x67, 0x38, 0xEE, 0x38, 0x4C, 0x38, 0x00, 0x00}}, // 屏
  {0x5DE6, {0x07, 0x00, 0x07, 0x00, 0x06, 0x00, 0x7F, 0xFE, 0x7F, 0xFE, 0x0E, 0x00, 0x0C, 0x00, 0x0F, 0xFE, 0x1F, 0xFE, 0x18, 0x60, 0x38, 0x60, 0x30, 0x60, 0x70, 0x60, 0xE0, 0x60, 0x6F, 0xFF, 0x0F, 0xFE}}, // 左
  {0x5DF2, {0x00, 0x00, 0x7F, 0xFC, 0x7F, 0xFC, 0x00, 0x1C, 0x30, 0x1C, 0x30, 0x1C, 0x30, 0x1C, 0x3F, 0xFC, 0x3F, 0xFC, 0x30, 0x18, 0x30, 0x00, 0x30, 0x07, 0x30, 0x06, 0x3F, 0xFE, 0x1F, 0xFC, 0x00, 0x00}}, // 已
  {0x5E2E, {0x0C, 0x00, 0x7F, 0xFE, 0x7F, 0x7E, 0x7F, 0x6C, 0x7F, 0x6C, 0x0C, 0x66, 0xFF, 0xE6, 0x7F, 0x7E, 0x79, 0xEC, 0x71, 0x80, 0x3F, 0xFC, 0x3F, 0xFC, 0x31, 0x8C, 0x31, 0xBC, 0x31, 0xBC, 0x01, 0x80}}, // 帮
  {0x5E38, {0x19, 0x98, 0x7F, 0xFE, 0x7F, 0xFE, 0x60, 0x06, 0x6F, 0xF6, 0x6F, 0xF6, 0x0F, 0xF0, 0x0F, 0xF0, 0x01, 0x80, 0x3F, 0xFC, 0x3F, 0xFC, 0x31, 0x8C, 0x31, 0x8C, 0x31, 0xBC, 0x31, 0xB8, 0x00, 0x00}}, // 常
  {0x5E45, {0x19, 0xFE, 0x19, 0xFE, 0x18, 0x00, 0x7E, 0xFE, 0x7E, 0xFE, 0x5E, 0x86, 0x5E, 0xFE, 0x5E, 0xFC, 0x5F, 0xFE, 0x5F, 0xFE, 0x5F, 0xB6, 0x5D, 0xFE, 0x19, 0xFE, 0x19, 0xB6, 0x19, 0xFE, 0x19, 0xFE}}, // 幅
  {0x5E55, {0x7F, 0xFE, 0x7F, 0xFE, 0x0C, 0x20, 0x3F, 0xFC, 0x38, 0x1C, 0x3F, 0xFC, 0x30, 0x0C, 0x3F, 0xFC, 0x06, 0x00, 0x7F, 0xFE, 0x7F, 0xFE, 0x3F, 0xFE, 0xFF, 0xFE, 0x59, 0x9A, 0x19, 0xB8, 0x09, 0xA0}}, // 幕
  {0x5E76, {0x1C, 0x18, 0x0C, 0x38, 0x0E, 0x30, 0x7F, 0xFE, 0x7F, 0xFE, 0x0C, 0x30, 0x0C, 0x30, 0x0C, 0x30, 0x7F, 0xFE, 0x7F, 0xFE, 0x0C, 0x30, 0x1C, 0x30, 0x1C, 0x30, 0x38, 0x30, 0x70, 0x30, 0x60, 0x30}}, // 并
  {0x5E93, {0x01, 0xC0, 0x3F, 0xFF, 0x3F, 0xFE, 0x31, 0x80, 0x3F, 0xFE, 0x3F, 0xFE, 0x33, 0x60, 0x76, 0x60, 0x77, 0xFC, 0x77, 0xFC, 0x70, 0x60, 0x6F, 0xFF, 0x6F, 0xFE, 0xE0, 0x60, 0x40, 0x60, 0x00, 0x00}}, // 库
  {0x5EA6, {0x01, 0xC0, 0x3F, 0xFE, 0x3F, 0xFE, 0x33, 0x18, 0x33, 0x18, 0x3F, 0xFE, 0x3F, 0xFE, 0x33, 0xF8, 0x33, 0xF8, 0x70, 0x00, 0x7F, 0xFC, 0x6F, 0xFC, 0x63, 0xB8, 0x61, 0xF0, 0xEF, 0xFE, 0x4F, 0x1E}}, // 度
  {0x5EB7, {0x01, 0xC0, 0x3F, 0xFE, 0x3F, 0xFE, 0x30, 0xC0, 0x37, 0xFC, 0x30, 0xCC, 0x3F, 0xFF, 0x3F, 0xFE, 0x70, 0xCC, 0x7F, 0xFC, 0x7C, 0xE4, 0x6E, 0xFE, 0x67, 0xF8, 0x7F, 0xDC, 0xEF, 0xCE, 0x43, 0xC4}}, // 康
  {0x5F00, {0x7F, 0xFE, 0x7F, 0xFE, 0x0E, 0x30, 0x0E, 0x30, 0x0E, 0x30, 0x0E, 0x30, 0x7F, 0xFE, 0x7F, 0xFE, 0x0C, 0x30, 0x0C, 0x30, 0x1C, 0x30, 0x18, 0x30, 0x38, 0x30, 0x70, 0x30, 0x20, 0x30, 0x00, 0x00}}, // 开
  {0x5F0F, {0x00, 0x68, 0x00, 0x6E, 0x00, 0x64, 0x7F, 0xFE, 0x7F, 0xFE, 0x00, 0x60, 0x00, 0x60, 0x7F, 0xE0, 0x7F, 0x60, 0x0C, 0x70, 0x0C, 0x70, 0x0C, 0x32, 0x0F, 0xB3, 0x7F, 0xBF, 0x7E, 0x1E, 0x60, 0x0E}}, // 式
  {0x5F53, {0x01, 0x80, 0x31, 0x8C, 0x39, 0x9C, 0x19, 0x9C, 0x19, 0x98, 0x01, 0x80, 0x3F, 0xFC, 0x3F, 0xFC, 0x00, 0x0C, 0x00, 0x0C, 0x3F, 0xFC, 0x1F, 0xFC, 0x00, 0x0C, 0x3F, 0xFC, 0x3F, 0xFC, 0x00, 0x0C}}, // 当
  {0x5F55, {0x3F, 0xFC, 0x3F, 0xFC, 0x00, 0x1C, 0x3F, 0xF8, 0x3F, 0xF8, 0x7F, 0xFE, 0x7F, 0xFE, 0x11, 0x8C, 0x39, 0xDC, 0x1D, 0xF8, 0x0F, 0xF0, 0x3F, 0xB8, 0x7D, 0x9E, 0x77, 0x8E, 0x07, 0x80, 0x00, 0x00}}, // 录
  {0x5F71, {0x7F, 0x8E, 0x71, 0x9C, 0x7F, 0xBC, 0x61, 0xF8, 0x7F, 0xB0, 0x0C, 0x06, 0xFF, 0xCE, 0x7F, 0xFC, 0x7F, 0xB8, 0x71, 0xA0, 0x71, 0x87, 0x7F, 0x8E, 0x2D, 0x1E, 0x6D, 0xBC, 0xFD, 0xF8, 0x5C, 0x20}}, // 影
  {0x6001, {0x01, 0x80, 0x01, 0x80, 0x7F, 0xFE, 0x7F, 0xFE, 0x03, 0xE0, 0x07, 0x60, 0x0E, 0x78, 0x1F, 0xBE, 0x79, 0xDE, 0x71, 0xC6, 0x2F, 0x8C, 0x3D, 0xCC, 0x7C, 0xDE, 0x6C, 0x3E, 0x6F, 0xF6, 0x07, 0xF0}}, // 态
  {0x606F, {0x01, 0x80, 0x1F, 0xF8, 0x1F, 0xF8, 0x1F, 0xF8, 0x1F, 0xF8, 0x18, 0x18, 0x1F, 0xF8, 0x1F, 0xF8, 0x1F, 0xF8, 0x1F, 0xF8, 0x01, 0x80, 0x37, 0xCC, 0x7E, 0xCE, 0x6E, 0x36, 0xEF, 0xF7, 0x07, 0xF0}}, // 息
  {0x620F, {0x00, 0xFC, 0x7E, 0xEE, 0x7E, 0xE4, 0x06, 0xFE, 0x6F, 0xFE, 0x7D, 0xE4, 0x3C, 0x6E, 0x3C, 0x6C, 0x1C, 0x7C, 0x3C, 0x78, 0x3E, 0x72, 0x76, 0xF3, 0xE1, 0xFE, 0x41, 0x9E, 0x00, 0x0C, 0x00, 0x00}}, // 戏
  {0x6210, {0x00, 0xD8, 0x00, 0xFC, 0x00, 0xCC, 0x3F, 0xFF, 0x3F, 0xFF, 0x30, 0xE0, 0x3F, 0xEC, 0x3F, 0x6C, 0x73, 0x7C, 0x73, 0x78, 0x73, 0x78, 0x77, 0x72, 0x7F, 0x73, 0x6F, 0xFE, 0xE3, 0xDE, 0x61, 0x0E}}, // 成
  {0x6234, {0x0C, 0x68, 0x7F, 0xFC, 0x3F, 0xEE, 0x7F, 0xFE, 0xFF, 0xFE, 0x3F, 0xF0, 0x7F, 0xF6, 0x7F, 0xBE, 0x7F, 0xBC, 0x3F, 0xBC, 0x7F, 0xBC, 0x3F, 0x38, 0x7F, 0xBA, 0x3E, 0x7B, 0x77, 0xFE, 0x63, 0x4E}}, // 戴
  {0x62DF, {0x3B, 0x8E, 0x3B, 0x8E, 0x7F, 0xEE, 0xFF, 0xEE, 0x3B, 0xFE, 0x3B, 0xBC, 0x3F, 0x8C, 0x7F, 0x8C, 0xFF, 0xCC, 0x7B, 0xDC, 0x3F, 0xFC, 0x3F, 0x3E, 0x3E, 0xFE, 0x71, 0xE7, 0x70, 0xC6, 0x00, 0x00}}, // 拟
  {0x62E9, {0x30, 0x00, 0x33, 0xFE, 0x33, 0xFC, 0x7C, 0xDC, 0xFC, 0x78, 0x30, 0xF8, 0x33, 0xFF, 0x3F, 0xAE, 0x7C, 0x70, 0xFF, 0xFE, 0x71, 0xFC, 0x30, 0x70, 0x33, 0xFF, 0x33, 0xFE, 0x70, 0x70, 0x70, 0x70}}, // 择
  {0x6301, {0x30, 0x30, 0x30, 0x30, 0x33, 0xFE, 0x7F, 0xFE, 0xFC, 0x30, 0x33, 0xFF, 0x33, 0xFF, 0x3C, 0x0C, 0x7F, 0xFE, 0xFB, 0xFF, 0x71, 0x8C, 0x31, 0x8C, 0x31, 0xCC, 0x30, 0xCC, 0x70, 0x3C, 0x70, 0x38}}, // 持
  {0x6309, {0x38, 0x70, 0x3B, 0xFE, 0xFF, 0xFF, 0xFF, 0x67, 0x3B, 0x67, 0x38, 0xE0, 0x3F, 0xFF, 0x7F, 0xFF, 0xFD, 0xCC, 0x79, 0xDC, 0x39, 0xF8, 0x38, 0xF8, 0x38, 0xFE, 0x73, 0xEF, 0x73, 0x82, 0x00, 0x00}}, // 按
  {0x6362, {0x31, 0xFC, 0x31, 0xFC, 0x7F, 0x9C, 0xFF, 0xFC, 0x33, 0xFE, 0x33, 0x7E, 0x3F, 0x7E, 0x7F, 0x7E, 0xFF, 0xFE, 0x77, 0xFF, 0x30, 0xF0, 0x30, 0xF8, 0x33, 0xDC, 0x77, 0x8F, 0x76, 0x02, 0x00, 0x00}}, // 换
  {0x636E, {0x33, 0xFE, 0x33, 0xFE, 0x7F, 0x06, 0xFF, 0xFE, 0x33, 0xFE, 0x33, 0x30, 0x33, 0xFE, 0x7F, 0xFF, 0xFB, 0x30, 0x73, 0xFE, 0x37, 0xFE, 0x37, 0xC6, 0x36, 0xC6, 0x7E, 0xFE, 0x64, 0xFE, 0x00, 0x00}}, // 据
  {0x63A5, {0x38, 0x70, 0x38, 0x70, 0x3B, 0xFE, 0x7F, 0xFE, 0xFD, 0xDC, 0x3F, 0xFE, 0x3F, 0xFF, 0x3C, 0xE0, 0xFF, 0xFE, 0xFF, 0xFF, 0x39, 0x9C, 0x39, 0xD8, 0x3B, 0xF8, 0x38, 0xFC, 0x77, 0xFE, 0x77, 0x86}}, // 接
  {0x63A7, {0x30, 0x70, 0x33, 0xFE, 0x37, 0xFF, 0x7E, 0xDF, 0xFE, 0xDA, 0x30, 0xDA, 0x33, 0x9F, 0x3F, 0x1E, 0xFC, 0x00, 0xF3, 0xFE, 0x33, 0xFE, 0x30, 0x60, 0x30, 0x60, 0x30, 0x60, 0x77, 0xFF, 0x77, 0xFF}}, // 控
  {0x64CD, {0x30, 0xF8, 0x31, 0xFC, 0x31, 0x8C, 0x79, 0xFC, 0xF8, 0xF8, 0x33, 0xFE, 0x33, 0xFE, 0x3E, 0xF6, 0x7F, 0xFE, 0xFB, 0xFE, 0x73, 0xFE, 0x37, 0xFF, 0x30, 0xF8, 0x33, 0xFE, 0x77, 0x6E, 0x64, 0x60}}, // 操
  {0x6539, {0x7E, 0xE0, 0x7E, 0xE0, 0x06, 0xFF, 0x07, 0xFF, 0x07, 0xCC, 0x7F, 0xCC, 0x7F, 0xEC, 0x64, 0x7C, 0x60, 0x78, 0x62, 0x78, 0x67, 0x38, 0x7E, 0x78, 0x3F, 0xFE, 0x03, 0xCF, 0x03, 0x06, 0x00, 0x00}}, // 改
  {0x653E, {0x18, 0x60, 0x18, 0x60, 0xFF, 0x7F, 0x7F, 0xFF, 0x30, 0xEC, 0x3F, 0xEC, 0x3F, 0xEC, 0x36, 0xFC, 0x36, 0x38, 0x36, 0x38, 0x36, 0x38, 0x76, 0x7C, 0x66, 0xFE, 0xFF, 0xCF, 0x5F, 0x86, 0x00, 0x00}}, // 放
  {0x6570, {0x7F, 0x70, 0x7F, 0x70, 0xFF, 0xFF, 0x7F, 0xFF, 0x3F, 0xEE, 0x7F, 0xEE, 0x7D, 0xFC, 0x18, 0xFC, 0xFF, 0xBC, 0x7F, 0x38, 0x76, 0x38, 0x7E, 0x7C, 0x1F, 0xFE, 0x7F, 0xEF, 0x72, 0x82, 0x00, 0x00}}, // 数
  {0x65B9, {0x01, 0x80, 0x01, 0x80, 0x01, 0x80, 0x7F, 0xFE, 0x7F, 0xFE, 0x07, 0x00, 0x07, 0x00, 0x07, 0xFC, 0x07, 0xFC, 0x0E, 0x18, 0x0E, 0x18, 0x0C, 0x18, 0x1C, 0x18, 0x38, 0x38, 0x71, 0xF8, 0x61, 0xF0}}, // 方
  {0x65E0, {0x3F, 0xFE, 0x3F, 0xFE, 0x01, 0x80, 0x03, 0x80, 0x03, 0x80, 0x7F, 0xFE, 0x7F, 0xFE, 0x03, 0xC0, 0x03, 0xC0, 0x07, 0xC0, 0x06, 0xC0, 0x0E, 0xC7, 0x3C, 0xC7, 0x78, 0xFE, 0x60, 0x7E, 0x00, 0x00}}, // 无
  {0x65F6, {0x00, 0x0C, 0x7E, 0x0C, 0x7E, 0x0C, 0x67, 0xFF, 0x67, 0xFF, 0x66, 0x0C, 0x7E, 0x0C, 0x7F, 0xCC, 0x66, 0xEC, 0x66, 0x6C, 0x66, 0x6C, 0x7E, 0x0C, 0x7E, 0x0C, 0x60, 0x0C, 0x60, 0x7C, 0x00, 0x78}}, // 时
  {0x663E, {0x00, 0x00, 0x3F, 0xFC, 0x3F, 0xFC, 0x3F, 0xFC, 0x3F, 0xFC, 0x38, 0x1C, 0x3F, 0xFC, 0x1F, 0xF8, 0x06, 0x64, 0x36, 0x6C, 0x36, 0x6C, 0x1E, 0x78, 0x16, 0x60, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00}}, // 显
  {0x667A, {0x30, 0x00, 0x3E, 0x7E, 0x7F, 0xFE, 0x6C, 0xE6, 0xFF, 0xE6, 0x7F, 0xE6, 0x3E, 0xFE, 0x77, 0x7E, 0x60, 0x00, 0x1F, 0xF8, 0x1F, 0xF8, 0x1F, 0xF8, 0x1F, 0xF8, 0x18, 0x18, 0x1F, 0xF8, 0x1F, 0xF8}}, // 智
  {0x6682, {0x18, 0x0E, 0x7F, 0xFE, 0x7F, 0x60, 0x7C, 0x60, 0x7F, 0xFE, 0x7F, 0xFE, 0x3F, 0xEC, 0x7F, 0xCC, 0x0C, 0xCC, 0x1F, 0xFC, 0x3F, 0xFC, 0x38, 0x1C, 0x3F, 0xFC, 0x38, 0x1C, 0x3F, 0xFC, 0x3F, 0xFC}}, // 暂
  {0x6700, {0x00, 0x00, 0x1F, 0xF8, 0x18, 0x18, 0x1F, 0xF8, 0x1F, 0xF8, 0x1F, 0xF8, 0x7F, 0xFE, 0x7F, 0xFE, 0x3F, 0xFC, 0x3F, 0xFE, 0x33, 0x6C, 0x3F, 0x7C, 0x3F, 0x38, 0x7F, 0x7C, 0x7F, 0xFE, 0x03, 0xC6}}, // 最
  {0x672A, {0x01, 0x80, 0x01, 0x80, 0x01, 0x80, 0x3F, 0xFC, 0x3F, 0xFC, 0x01, 0x80, 0x01, 0x80, 0x7F, 0xFE, 0x7F, 0xFE, 0x07, 0xE0, 0x0F, 0xF0, 0x1D, 0xB8, 0x39, 0x9C, 0xF1, 0x9E, 0x61, 0x86, 0x01, 0x80}}, // 未
  {0x675F, {0x01, 0x80, 0x01, 0x80, 0x7F, 0xFE, 0x7F, 0xFE, 0x01, 0x80, 0x3F, 0xFC, 0x3F, 0xFC, 0x31, 0x8C, 0x3F, 0xFC, 0x3F, 0xFC, 0x07, 0xE0, 0x0F, 0xF0, 0x3D, 0xBC, 0x79, 0x9E, 0x71, 0x8E, 0x01, 0x80}}, // 束
  {0x679C, {0x3F, 0xFC, 0x3F, 0xFC, 0x3F, 0xFC, 0x3F, 0xFC, 0x39, 0x9C, 0x3F, 0xFC, 0x3F, 0xFC, 0x01, 0x80, 0x7F, 0xFE, 0x7F, 0xFE, 0x0F, 0xF0, 0x1D, 0xB8, 0x79, 0x9E, 0x71, 0x8E, 0x01, 0x80, 0x00, 0x00}}, // 果
  {0x6807, {0x18, 0x00, 0x19, 0xFE, 0x19, 0xFE, 0x18, 0x00, 0xFE, 0x00, 0xFB, 0xFF, 0x3D, 0xFF, 0x3C, 0x30, 0x7E, 0x34, 0x7D, 0xBE, 0xF9, 0xB6, 0xDB, 0xB6, 0x1B, 0x36, 0x1B, 0x37, 0x18, 0xF0, 0x18, 0xE0}}, // 标
  {0x6821, {0x18, 0x30, 0x18, 0x30, 0x1B, 0xFE, 0x7F, 0xFF, 0x7C, 0x8C, 0x39, 0xCC, 0x39, 0x8E, 0x3F, 0xCF, 0x7F, 0xDE, 0x7C, 0xF8, 0xF8, 0x78, 0xD8, 0x70, 0x58, 0x78, 0x19, 0xFE, 0x1B, 0xCF, 0x1B, 0x06}}, // 校
  {0x6846, {0x18, 0x00, 0x3B, 0xFE, 0x3B, 0xFE, 0x3B, 0x00, 0x7F, 0x7E, 0x7F, 0x7E, 0x3B, 0x18, 0x3F, 0x7E, 0x7F, 0x7E, 0x7F, 0x18, 0xFB, 0xFE, 0xFB, 0xFE, 0x3B, 0x00, 0x3B, 0xFF, 0x3B, 0xFF, 0x1B, 0x00}}, // 框
  {0x6A21, {0x33, 0xFE, 0x33, 0xFE, 0x7C, 0xD8, 0xFF, 0xFE, 0x33, 0xFE, 0x3B, 0xFE, 0x3F, 0x8E, 0x7F, 0xFE, 0x7F, 0xFE, 0xF0, 0x60, 0xF7, 0xFE, 0x73, 0xFE, 0x31, 0xFC, 0x37, 0xCF, 0x33, 0x06, 0x00, 0x00}}, // 模
  {0x6B21, {0x01, 0x80, 0x23, 0x80, 0x73, 0x00, 0x3B, 0xFE, 0x17, 0xFE, 0x06, 0x66, 0x0E, 0x6E, 0x04, 0xEC, 0x18, 0xE0, 0x38, 0xF0, 0x79, 0xF0, 0x71, 0xB8, 0x63, 0x9C, 0x0F, 0x1E, 0x1E, 0x0F, 0x0C, 0x06}}, // 次
  {0x6B63, {0x00, 0x00, 0x7F, 0xFE, 0x7F, 0xFE, 0x01, 0xC0, 0x01, 0xC0, 0x01, 0xC0, 0x19, 0xC0, 0x19, 0xFC, 0x19, 0xFC, 0x19, 0xC0, 0x19, 0xC0, 0x19, 0xC0, 0x19, 0xC0, 0x7F, 0xFE, 0x7F, 0xFF, 0x00, 0x00}}, // 正
  {0x6B65, {0x19, 0x80, 0x19, 0xFC, 0x19, 0xFC, 0x19, 0x80, 0x7F, 0xFE, 0x7F, 0xFE, 0x19, 0x80, 0x39, 0x9C, 0x79, 0x98, 0x77, 0xB8, 0x27, 0xF0, 0x01, 0xE0, 0x0F, 0xC0, 0x3F, 0x00, 0x3C, 0x00, 0x00, 0x00}}, // 步
  {0x6BCF, {0x0C, 0x00, 0x1C, 0x00, 0x1F, 0xFE, 0x3F, 0xFE, 0x70, 0x00, 0x7F, 0xF8, 0x7F, 0xF8, 0x1B, 0xD8, 0xFF, 0xFF, 0xFF, 0xFF, 0x1B, 0x98, 0x39, 0xD8, 0x3F, 0xFE, 0x3F, 0xFE, 0x10, 0x78, 0x00, 0x70}}, // 每
  {0x6CD5, {0x30, 0x60, 0x38, 0x60, 0x1B, 0xFE, 0x03, 0xFE, 0x40, 0x60, 0x70, 0x60, 0x78, 0x60, 0x37, 0xFF, 0x07, 0xFE, 0x11, 0xC0, 0x19, 0x98, 0x39, 0x9C, 0x33, 0x8E, 0x77, 0xFE, 0x67, 0xFF, 0x00, 0x02}}, // 法
  {0x6D41, {0x30, 0x60, 0x78, 0x60, 0x1F, 0xFF, 0x07, 0xFE, 0x00, 0xCC, 0x61, 0xFE, 0x77, 0xFE, 0x37, 0xC6, 0x03, 0x6C, 0x1B, 0x6C, 0x1B, 0x6C, 0x3B, 0x6C, 0x33, 0x6F, 0x77, 0x6F, 0x66, 0x6F, 0x04, 0x06}}, // 流
  {0x6D4B, {0x20, 0x02, 0x7F, 0xE6, 0x3F, 0xFE, 0x07, 0x7E, 0x07, 0xFE, 0x67, 0xFE, 0x77, 0xFE, 0x37, 0xFE, 0x07, 0xFE, 0x07, 0xFE, 0x37, 0x7E, 0x37, 0x7E, 0x73, 0x86, 0x63, 0xC6, 0x6E, 0x6E, 0x24, 0x6E}}, // 测
  {0x6E38, {0x7B, 0x30, 0x3F, 0xFF, 0x0F, 0xFF, 0x06, 0x70, 0x66, 0x7E, 0x77, 0xFE, 0x27, 0xCE, 0x06, 0xCC, 0x26, 0xFF, 0x36, 0xFF, 0x3C, 0xCC, 0x7D, 0xCC, 0x7D, 0x8C, 0x7F, 0xBC, 0x6B, 0xB8, 0x00, 0x00}}, // 游
  {0x70B9, {0x01, 0x80, 0x01, 0xFE, 0x01, 0xFE, 0x01, 0x80, 0x01, 0x80, 0x3F, 0xFC, 0x3F, 0xFC, 0x30, 0x1C, 0x30, 0x1C, 0x3F, 0xFC, 0x3F, 0xFC, 0x00, 0x00, 0x36, 0xEC, 0x76, 0x6E, 0x66, 0x66, 0x66, 0x66}}, // 点
  {0x70E7, {0x18, 0x40, 0x18, 0xC0, 0x19, 0xFE, 0x1F, 0xFE, 0x7E, 0x6E, 0x7C, 0x7C, 0x7D, 0xFA, 0xDB, 0xFE, 0x19, 0x0E, 0x3B, 0xFE, 0x3B, 0xFE, 0x3C, 0xD8, 0x7D, 0xD8, 0x63, 0x9B, 0x67, 0x9F, 0x02, 0x0E}}, // 烧
  {0x7248, {0x6D, 0xFE, 0x6D, 0xFE, 0x6D, 0xC0, 0x7F, 0xC0, 0x7F, 0xFE, 0x61, 0xFE, 0x61, 0xE6, 0x7D, 0xEE, 0x7D, 0xFC, 0x6D, 0xBC, 0x6D, 0xBC, 0x6D, 0xBC, 0x6F, 0xFE, 0xCF, 0xFF, 0x4F, 0x66, 0x00, 0x00}}, // 版
  {0x7259, {0x3F, 0xFE, 0x3F, 0xFC, 0x18, 0x60, 0x18, 0x60, 0x18, 0x60, 0x38, 0x60, 0x3F, 0xFF, 0x3F, 0xFE, 0x03, 0xE0, 0x07, 0x60, 0x0E, 0x60, 0x3C, 0x60, 0x78, 0x60, 0x63, 0xE0, 0x01, 0xC0, 0x00, 0x00}}, // 牙
  {0x72B6, {0x1C, 0x60, 0x1C, 0x6C, 0x5C, 0x6E, 0x7C, 0x66, 0x7C, 0x60, 0x3F, 0xFE, 0x3F, 0xFE, 0x1C, 0x70, 0x1C, 0x70, 0x3C, 0xF0, 0x7C, 0xF8, 0x7D, 0xD8, 0x1D, 0xDC, 0x1F, 0x8E, 0x1F, 0x07, 0x1E, 0x02}}, // 状
  {0x72EC, {0x48, 0x70, 0x6C, 0x70, 0x7C, 0x70, 0x3B, 0xFE, 0x7B, 0xFE, 0xFB, 0x76, 0x5B, 0x76, 0x1B, 0x76, 0x3F, 0xFE, 0x7F, 0xFE, 0xFC, 0x74, 0x58, 0x7C, 0x18, 0x7E, 0x1F, 0xFE, 0x7F, 0xFF, 0x73, 0x06}}, // 独
  {0x73B0, {0xFF, 0xFE, 0x7F, 0xFE, 0x39, 0x86, 0x39, 0xB6, 0x39, 0xB6, 0x7D, 0xB6, 0x7D, 0xB6, 0x39, 0xE6, 0x39, 0xF6, 0x38, 0x70, 0x3E, 0xF2, 0xFD, 0xF3, 0xF3, 0xB3, 0x07, 0xBE, 0x02, 0x0C, 0x00, 0x00}}, // 现
  {0x7528, {0x3F, 0xFE, 0x3F, 0xFE, 0x31, 0x8E, 0x31, 0x8E, 0x3F, 0xFE, 0x3F, 0xFE, 0x31, 0x8E, 0x31, 0x8E, 0x3F, 0xFE, 0x3F, 0xFE, 0x31, 0x8E, 0x71, 0x8E, 0x61, 0x8E, 0xE1, 0xBC, 0x41, 0xBC, 0x00, 0x00}}, // 用
  {0x753B, {0x7F, 0xFE, 0x7F, 0xFE, 0x01, 0x80, 0x6F, 0xF6, 0x6F, 0xF6, 0x6D, 0xB6, 0x6F, 0xF6, 0x6F, 0xF6, 0x6D, 0xB6, 0x6F, 0xF6, 0x6F, 0xF6, 0x60, 0x06, 0x7F, 0xFE, 0x7F, 0xFE, 0x60, 0x06, 0x00, 0x00}}, // 画
  {0x754C, {0x3F, 0xFC, 0x31, 0x8C, 0x3F, 0xFC, 0x3F, 0xFC, 0x31, 0x8C, 0x3F, 0xFC, 0x3F, 0xFC, 0x0E, 0x70, 0x3C, 0x3C, 0x7E, 0x7E, 0x6E, 0x76, 0x0E, 0x70, 0x1C, 0x70, 0x3C, 0x70, 0x30, 0x70, 0x00, 0x00}}, // 界
  {0x7559, {0x7E, 0xFE, 0x78, 0xFE, 0x76, 0x66, 0x36, 0x66, 0x3F, 0xEE, 0xFF, 0xDE, 0x71, 0x9C, 0x1F, 0xF8, 0x3F, 0xFC, 0x39, 0x9C, 0x3F, 0xFC, 0x3F, 0xFC, 0x39, 0x9C, 0x3F, 0xFC, 0x3F, 0xFC, 0x00, 0x00}}, // 留
  {0x7684, {0x18, 0x60, 0x18, 0x60, 0x18, 0xE0, 0x7E, 0xFE, 0x7F, 0xFE, 0x67, 0xC6, 0x67, 0x86, 0x7E, 0x66, 0x7E, 0x66, 0x66, 0x36, 0x66, 0x36, 0x66, 0x06, 0x7E, 0x06, 0x7E, 0x0E, 0x60, 0x7C, 0x60, 0x3C}}, // 的
  {0x76D2, {0x03, 0xC0, 0x07, 0xE0, 0x0F, 0x70, 0x3F, 0xFC, 0x7F, 0xFE, 0x70, 0x06, 0x1F, 0xF8, 0x18, 0x18, 0x1F, 0xF8, 0x00, 0x00, 0x3F, 0xFC, 0x3F, 0xFC, 0x36, 0xEC, 0x36, 0xEC, 0xFF, 0xFF, 0x7F, 0xFE}}, // 盒
  {0x76EE, {0x3F, 0xFC, 0x3F, 0xFC, 0x30, 0x1C, 0x30, 0x1C, 0x3F, 0xFC, 0x3F, 0xFC, 0x30, 0x1C, 0x30, 0x1C, 0x3F, 0xFC, 0x3F, 0xFC, 0x30, 0x1C, 0x30, 0x1C, 0x3F, 0xFC, 0x3F, 0xFC, 0x30, 0x1C, 0x00, 0x00}}, // 目
  {0x76F4, {0x01, 0xC0, 0x7F, 0xFE, 0x7F, 0xFE, 0x01, 0x80, 0x27, 0xFC, 0x77, 0xFC, 0x76, 0x1C, 0x77, 0xFC, 0x76, 0x1C, 0x77, 0xFC, 0x76, 0x1C, 0x77, 0xFC, 0x77, 0xF8, 0x7F, 0xFE, 0x7F, 0xFE, 0x70, 0x00}}, // 直
  {0x76F8, {0x18, 0x00, 0x19, 0xFE, 0x19, 0xFE, 0x19, 0x86, 0x7F, 0x86, 0x7F, 0xFE, 0x39, 0xFE, 0x3D, 0x86, 0x3F, 0x86, 0x7F, 0xFE, 0xF9, 0xFE, 0xD9, 0x86, 0x59, 0x86, 0x19, 0xFE, 0x19, 0xFE, 0x19, 0x86}}, // 相
  {0x77E5, {0x30, 0x00, 0x70, 0xFE, 0x7F, 0xFE, 0x7F, 0xE6, 0xFC, 0xE6, 0x7C, 0xE6, 0x1C, 0xE6, 0xFF, 0xE6, 0xFF, 0xE6, 0x1C, 0xE6, 0x1C, 0xE6, 0x3E, 0xE6, 0x37, 0xFE, 0x73, 0xFE, 0xE0, 0xE6, 0x40, 0x40}}, // 知
  {0x7840, {0x00, 0x30, 0x7E, 0x30, 0x7D, 0xB6, 0x31, 0xB6, 0x31, 0xB6, 0x31, 0xB6, 0x7D, 0xFE, 0x7D, 0xFE, 0xEF, 0xB6, 0xEF, 0xB6, 0x6F, 0xB6, 0x6F, 0xB6, 0x7F, 0xB6, 0x7F, 0xFE, 0x63, 0xFE, 0x00, 0x06}}, // 础
  {0x786E, {0x7E, 0xF8, 0x7E, 0xFC, 0x31, 0x98, 0x33, 0xFE, 0x31, 0xFE, 0x7F, 0xB6, 0x7F, 0xFE, 0x77, 0xFE, 0xF7, 0xB6, 0x77, 0xFE, 0x77, 0xFE, 0x3F, 0x86, 0x3F, 0x86, 0x33, 0x1E, 0x03, 0x1E, 0x00, 0x00}}, // 确
  {0x793A, {0x3F, 0xFC, 0x3F, 0xFC, 0x00, 0x00, 0x00, 0x00, 0x7F, 0xFE, 0x7F, 0xFE, 0x01, 0x80, 0x11, 0x88, 0x39, 0x98, 0x39, 0x9C, 0x71, 0x8C, 0x71, 0x8E, 0x61, 0x86, 0x0F, 0x84, 0x07, 0x80, 0x00, 0x00}}, // 示
  {0x79D2, {0x0C, 0x30, 0x7E, 0x30, 0x78, 0x30, 0x19, 0xBC, 0x19, 0xB6, 0xFF, 0xB6, 0x7F, 0xB7, 0x3B, 0xB3, 0x3F, 0x34, 0x7E, 0xFE, 0x7C, 0x7C, 0xD8, 0x1C, 0x58, 0x38, 0x18, 0xF0, 0x1B, 0xE0, 0x19, 0x80}}, // 秒
  {0x79F0, {0x0C, 0xC0, 0x7E, 0xC0, 0x79, 0xC0, 0x19, 0xFE, 0x19, 0xFE, 0xFF, 0x30, 0x7F, 0x30, 0x39, 0xFE, 0x3D, 0xF6, 0x7F, 0xB6, 0x7F, 0xB6, 0xDB, 0xB7, 0x5B, 0x33, 0x18, 0x30, 0x18, 0xF0, 0x18, 0x70}}, // 称
  {0x7A0B, {0x7F, 0xFE, 0xFD, 0xFE, 0x59, 0x86, 0x19, 0x86, 0xFF, 0xFE, 0x7F, 0xFE, 0x39, 0xFE, 0x3F, 0xFE, 0x7E, 0x30, 0xFD, 0xFE, 0xD9, 0xFE, 0x58, 0x30, 0x18, 0x30, 0x1B, 0xFF, 0x1B, 0xFF, 0x00, 0x00}}, // 程
  {0x7A33, {0x1C, 0xC0, 0x7D, 0xFC, 0x7B, 0xFC, 0x1F, 0x18, 0x1B, 0xFE, 0x7F, 0xFE, 0x7D, 0xFE, 0x39, 0xFE, 0x3C, 0x06, 0x7F, 0xFE, 0x7F, 0xFC, 0xFA, 0xF4, 0x5F, 0xF6, 0x1F, 0xCE, 0x1E, 0xFE, 0x18, 0x78}}, // 稳
  {0x7ACB, {0x01, 0x80, 0x01, 0x80, 0x01, 0x80, 0x7F, 0xFE, 0x7F, 0xFE, 0x00, 0x00, 0x1C, 0x38, 0x0C, 0x38, 0x0C, 0x30, 0x0C, 0x30, 0x0E, 0x70, 0x0E, 0x60, 0x04, 0x60, 0x7F, 0xFE, 0x7F, 0xFE, 0x00, 0x00}}, // 立
  {0x7B2C, {0x30, 0x60, 0x3F, 0xFE, 0x7F, 0xFE, 0xFD, 0xD8, 0x4C, 0x98, 0x3F, 0xFC, 0x3F, 0xFC, 0x1F, 0xFC, 0x3F, 0xFC, 0x31, 0x80, 0x3F, 0xFE, 0x3F, 0xFE, 0x0F, 0x8E, 0x3D, 0xBE, 0x79, 0xBC, 0x21, 0x80}}, // 第
  {0x7B97, {0x30, 0x60, 0x3F, 0xFE, 0x7F, 0xFE, 0x6D, 0x98, 0x3F, 0xFC, 0x38, 0x1C, 0x3F, 0xFC, 0x3F, 0xFC, 0x3F, 0xFC, 0x3F, 0xFC, 0x1F, 0xFC, 0x7F, 0xFE, 0x7F, 0xFE, 0x1C, 0x30, 0x78, 0x30, 0x30, 0x30}}, // 算
  {0x7EC3, {0x30, 0xE0, 0x30, 0xE0, 0x33, 0xFE, 0x73, 0xFE, 0x6C, 0xC0, 0xFF, 0xF0, 0xFB, 0xF0, 0x31, 0xB0, 0x33, 0xFE, 0x7F, 0xFE, 0x79, 0x30, 0x41, 0xBC, 0x3F, 0x36, 0xFF, 0x36, 0x66, 0xF6, 0x00, 0xE0}}, // 练
  {0x7EC4, {0x18, 0x00, 0x19, 0xFC, 0x39, 0xFC, 0x31, 0xCC, 0x77, 0xCC, 0x7D, 0xFC, 0x7D, 0xFC, 0x59, 0xCC, 0x31, 0xCC, 0x7F, 0xFC, 0x79, 0xFC, 0x01, 0xCC, 0x1F, 0xCC, 0x7D, 0xCC, 0x73, 0xFF, 0x03, 0xFF}}, // 组
  {0x7ED3, {0x18, 0x30, 0x38, 0x30, 0x3B, 0xFE, 0x37, 0xFE, 0x7E, 0x30, 0xFC, 0x30, 0xFD, 0xFE, 0x39, 0xFE, 0x3C, 0x00, 0xFF, 0xFE, 0x7D, 0xFE, 0x01, 0x8E, 0x1F, 0x8E, 0xFF, 0xFE, 0xF1, 0xFE, 0x01, 0x86}}, // 结
  {0x7EE7, {0x00, 0x00, 0x19, 0x18, 0x3B, 0x5A, 0x33, 0xDE, 0x33, 0x7E, 0x6F, 0x7C, 0xFF, 0xFE, 0xFB, 0xFE, 0x3B, 0x38, 0x33, 0x3C, 0x7F, 0x7E, 0x7B, 0xDE, 0x03, 0x58, 0x3F, 0x18, 0xFF, 0xFF, 0x63, 0xFF}}, // 继
  {0x7EED, {0x19, 0xFC, 0x31, 0xFE, 0x30, 0x70, 0x6F, 0xFE, 0x7F, 0xFE, 0xF8, 0x86, 0x39, 0xF6, 0x3F, 0xF0, 0x7D, 0xB0, 0x7B, 0xFE, 0x47, 0xFE, 0x3C, 0xF8, 0xF9, 0xDC, 0x63, 0x86, 0x03, 0x02, 0x00, 0x00}}, // 续
  {0x7F6E, {0x7F, 0xFE, 0x76, 0x6E, 0x7F, 0xFE, 0x01, 0x80, 0x7F, 0xFE, 0x7F, 0xFE, 0x37, 0xFC, 0x36, 0x0C, 0x37, 0xFC, 0x37, 0xFC, 0x37, 0xFC, 0x37, 0xFC, 0x30, 0x00, 0x3F, 0xFF, 0x3F, 0xFE, 0x00, 0x00}}, // 置
  {0x8054, {0xFE, 0xCC, 0x7E, 0xFC, 0x65, 0xFE, 0x7D, 0xFE, 0x7C, 0x30, 0x64, 0x30, 0x64, 0x30, 0x7D, 0xFF, 0x7D, 0xFE, 0x64, 0x70, 0x7F, 0x78, 0xFF, 0x78, 0x7C, 0xEE, 0x07, 0xCF, 0x07, 0x82, 0x00, 0x00}}, // 联
  {0x8098, {0x00, 0x0C, 0x7E, 0x1C, 0x7E, 0x1C, 0x66, 0x1C, 0x7F, 0xFF, 0x7F, 0xFF, 0x66, 0x1C, 0x66, 0xDC, 0x7E, 0xDC, 0x7E, 0xFC, 0x66, 0x7C, 0x66, 0x1C, 0x66, 0x1C, 0x66, 0x1C, 0xFE, 0x7C, 0x5C, 0x78}}, // 肘
  {0x80A2, {0x00, 0x30, 0x7E, 0x30, 0x7F, 0xFE, 0x6F, 0xFF, 0x7E, 0x30, 0x7E, 0x30, 0x6F, 0xFE, 0x6F, 0xFE, 0x6E, 0xCE, 0x7E, 0xCC, 0x7E, 0xFC, 0x6E, 0x78, 0x6E, 0x78, 0x6F, 0xFC, 0xFF, 0xCF, 0x5D, 0x86}}, // 肢
  {0x80A9, {0x7F, 0xFE, 0x7F, 0xFE, 0x1F, 0xFC, 0x3F, 0xFC, 0x3F, 0xFC, 0x3F, 0xFC, 0x30, 0x00, 0x3F, 0xFC, 0x3F, 0xFC, 0x3F, 0xFC, 0x7E, 0x0C, 0x7F, 0xFC, 0x6E, 0x0C, 0xEE, 0x3C, 0x4E, 0x3C, 0x00, 0x00}}, // 肩
  {0x80FD, {0x18, 0x60, 0x3E, 0x6C, 0x37, 0x7E, 0xFF, 0x78, 0xFF, 0xE2, 0x01, 0x67, 0x7F, 0x7E, 0x7F, 0x7E, 0x7F, 0x60, 0x7F, 0x6E, 0x67, 0x7E, 0x7F, 0x78, 0x7F, 0x62, 0x67, 0x63, 0x6F, 0x7E, 0x6E, 0x3E}}, // 能
  {0x8155, {0x7F, 0xFE, 0x7F, 0xFE, 0x6F, 0x06, 0x7F, 0x86, 0x7D, 0xFE, 0x6D, 0xFE, 0x6F, 0x7E, 0x7F, 0x7E, 0x7F, 0xFE, 0x6E, 0xDE, 0x6C, 0xDE, 0x6C, 0xD8, 0x6D, 0x9B, 0xFF, 0x9F, 0x5F, 0x1E, 0x00, 0x00}}, // 腕
  {0x817F, {0x01, 0x00, 0x7F, 0xFE, 0x7D, 0xFE, 0x6C, 0x7E, 0x7F, 0x7E, 0x7F, 0xE6, 0x6D, 0x7E, 0x6C, 0x7E, 0x7F, 0xEE, 0x7F, 0xEE, 0x6D, 0xEC, 0x6D, 0xFE, 0x6D, 0xF6, 0x6F, 0xC0, 0xFF, 0xFF, 0x5A, 0x7E}}, // 腿
  {0x819D, {0x00, 0x30, 0x7F, 0xFE, 0x7F, 0xFE, 0x6C, 0xFC, 0x7F, 0xFE, 0x7F, 0x76, 0x6C, 0xF8, 0x6F, 0xCE, 0x7F, 0xB6, 0x7D, 0xB6, 0x6D, 0xFC, 0x6C, 0xBC, 0x6C, 0xFC, 0x6F, 0xFE, 0xFF, 0xF6, 0x5C, 0xE0}}, // 膝
  {0x81C2, {0x7F, 0xFE, 0x63, 0x6C, 0x7F, 0xFF, 0x60, 0xF8, 0x7F, 0x38, 0xF3, 0xFE, 0x7F, 0x18, 0x00, 0x38, 0x1F, 0xF8, 0x18, 0x18, 0x1F, 0xF8, 0x18, 0x18, 0x1F, 0xF8, 0x18, 0x78, 0x18, 0x78, 0x00, 0x00}}, // 臂
  {0x81EA, {0x01, 0x80, 0x03, 0x80, 0x3F, 0xFC, 0x3F, 0xFC, 0x30, 0x0C, 0x30, 0x0C, 0x3F, 0xFC, 0x3F, 0xFC, 0x30, 0x0C, 0x30, 0x0C, 0x3F, 0xFC, 0x3F, 0xFC, 0x30, 0x0C, 0x3F, 0xFC, 0x3F, 0xFC, 0x30, 0x0C}}, // 自
  {0x822A, {0x18, 0x30, 0x18, 0x30, 0x3F, 0xFE, 0x7F, 0xFF, 0x7E, 0x00, 0x7E, 0x00, 0x6E, 0xFC, 0x7F, 0xFC, 0xFE, 0xCC, 0x7E, 0xCC, 0x7E, 0xCC, 0x7E, 0xCC, 0x7E, 0xCF, 0x66, 0xCF, 0xEF, 0x8F, 0x4E, 0x86}}, // 航
  {0x8282, {0x0C, 0x30, 0x0C, 0x30, 0x7F, 0xFE, 0x7F, 0xFE, 0x0C, 0x30, 0x04, 0x30, 0x7F, 0xFC, 0x7F, 0xFC, 0x07, 0x0C, 0x07, 0x0C, 0x07, 0x0C, 0x07, 0x0C, 0x07, 0x7C, 0x07, 0x78, 0x07, 0x00, 0x07, 0x00}}, // 节
  {0x83DC, {0x0E, 0x30, 0x7F, 0xFE, 0x7F, 0xFE, 0x0C, 0x78, 0x7F, 0xFC, 0x7F, 0xF8, 0x33, 0x8C, 0x39, 0x9C, 0x19, 0x98, 0x11, 0x80, 0x7F, 0xFE, 0x7F, 0xFE, 0x0F, 0xF0, 0x7D, 0xBE, 0x79, 0x9E, 0x41, 0x80}}, // 菜
  {0x84DD, {0x0C, 0x30, 0x7F, 0xFE, 0x7F, 0xFE, 0x0C, 0x70, 0x36, 0x60, 0x36, 0xFE, 0x36, 0xFE, 0x37, 0xF8, 0x37, 0x98, 0x06, 0x08, 0x3F, 0xFC, 0x3F, 0xFC, 0x36, 0x6C, 0x36, 0x6C, 0xFF, 0xFF, 0x7F, 0xFE}}, // 蓝
  {0x865A, {0x01, 0x80, 0x01, 0xFC, 0x01, 0xFC, 0x3F, 0xFE, 0x3F, 0xFE, 0x31, 0x9E, 0x3F, 0xF4, 0x31, 0xFE, 0x31, 0xFE, 0x33, 0x60, 0x3F, 0x6C, 0x7F, 0x6C, 0x6F, 0x78, 0x67, 0x68, 0xFF, 0xFE, 0x7F, 0xFE}}, // 虚
  {0x89C4, {0x18, 0x00, 0x19, 0xFE, 0x19, 0xFE, 0x7F, 0x86, 0x7F, 0xB6, 0x19, 0xB6, 0x19, 0xB6, 0xFF, 0xB6, 0xFF, 0xB6, 0x39, 0xF6, 0x3C, 0x78, 0x3E, 0x78, 0x36, 0xFB, 0x61, 0xFB, 0xE3, 0x9E, 0x43, 0x0C}}, // 规
  {0x89D2, {0x0E, 0x00, 0x0F, 0xE0, 0x1F, 0xE0, 0x38, 0xE0, 0x7F, 0xFC, 0x7F, 0xFC, 0x39, 0xCC, 0x1F, 0xFC, 0x1F, 0xFC, 0x19, 0xCC, 0x1F, 0xFC, 0x3F, 0xFC, 0x30, 0x0C, 0x70, 0x0C, 0xE0, 0x7C, 0x40, 0x7C}}, // 角
  {0x8BA4, {0x10, 0x70, 0x38, 0x70, 0x1C, 0x70, 0x08, 0x70, 0x00, 0x70, 0xF8, 0x70, 0xF8, 0x70, 0x18, 0x70, 0x18, 0x70, 0x18, 0x78, 0x18, 0xF8, 0x1E, 0xD8, 0x1F, 0xCC, 0x3F, 0x8E, 0x33, 0x87, 0x01, 0x02}}, // 认
  {0x8BAD, {0x21, 0x06, 0x73, 0xB6, 0x3B, 0xB6, 0x1B, 0xB6, 0x03, 0xB6, 0xF3, 0xB6, 0xF3, 0xB6, 0x33, 0xB6, 0x33, 0xB6, 0x33, 0x36, 0x33, 0x36, 0x3F, 0x36, 0x3F, 0x36, 0x3F, 0x36, 0x37, 0x06, 0x22, 0x06}}, // 训
  {0x8BB0, {0x20, 0x00, 0x33, 0xFE, 0x3B, 0xFE, 0x18, 0x0E, 0x00, 0x0E, 0xF8, 0x0E, 0xF8, 0x0E, 0x19, 0xFE, 0x19, 0xFE, 0x19, 0x86, 0x19, 0x80, 0x1D, 0x80, 0x1F, 0x87, 0x1D, 0x87, 0x39, 0xFE, 0x10, 0xFE}}, // 记
  {0x8BBE, {0x00, 0x00, 0x31, 0xF8, 0x39, 0xF8, 0x19, 0x98, 0x01, 0x98, 0xF7, 0x9E, 0xF3, 0x0E, 0x33, 0xFC, 0x33, 0xFC, 0x31, 0x8C, 0x3D, 0xDC, 0x3C, 0xF8, 0x3C, 0xF0, 0x3B, 0xFC, 0x37, 0xDE, 0x03, 0x06}}, // 设
  {0x8BD5, {0x20, 0x3C, 0x30, 0x3E, 0x38, 0x3E, 0x1F, 0xFF, 0x07, 0xFF, 0x78, 0x38, 0x78, 0x18, 0x3B, 0xF8, 0x3B, 0xF8, 0x39, 0x98, 0x39, 0x98, 0x3D, 0x9A, 0x3F, 0xFF, 0x3F, 0xEE, 0x33, 0x0E, 0x00, 0x04}}, // 试
  {0x8C03, {0x73, 0xFE, 0x3B, 0xFE, 0x1B, 0x66, 0x03, 0xFE, 0xF3, 0x76, 0xF3, 0xF6, 0x33, 0xFE, 0x33, 0x06, 0x33, 0xFE, 0x3B, 0xFE, 0x3E, 0xFE, 0x3E, 0xFE, 0x3E, 0xC6, 0x76, 0x1E, 0x24, 0x0E, 0x00, 0x00}}, // 调
  {0x8D28, {0x01, 0xFC, 0x3F, 0xFC, 0x3F, 0xE0, 0x3F, 0xFE, 0x3F, 0xFF, 0x30, 0xC0, 0x30, 0xC0, 0x37, 0xFE, 0x37, 0xFE, 0x36, 0x6E, 0x36, 0xEE, 0x76, 0xEE, 0x66, 0xCC, 0x63, 0xF8, 0xEF, 0x1E, 0x4E, 0x06}}, // 质
  {0x8D56, {0x18, 0x7C, 0x7F, 0x7E, 0x7F, 0xCC, 0x7F, 0xFE, 0x7F, 0xFE, 0x7B, 0xD6, 0x7B, 0xDE, 0x7F, 0xDE, 0x7E, 0xD6, 0x3E, 0xF6, 0x3F, 0xF6, 0x7B, 0x7E, 0x78, 0x7C, 0x59, 0xE7, 0x19, 0x82, 0x00, 0x00}}, // 赖
  {0x8DB3, {0x3F, 0xFC, 0x3F, 0xFC, 0x38, 0x0C, 0x38, 0x0C, 0x3F, 0xFC, 0x3F, 0xFC, 0x01, 0x80, 0x19, 0x80, 0x19, 0xFE, 0x39, 0xFC, 0x3D, 0x80, 0x3F, 0x80, 0x77, 0x80, 0x63, 0xFF, 0x40, 0xFE, 0x00, 0x00}}, // 足
  {0x8E1D, {0x7D, 0xFE, 0x7D, 0xFE, 0x6D, 0xB6, 0x6D, 0xFE, 0x7D, 0xFE, 0x7D, 0xB6, 0x19, 0xFE, 0x79, 0xFE, 0x7C, 0x30, 0x7F, 0xFE, 0x7B, 0xFE, 0x7C, 0xF8, 0x7D, 0xFC, 0xFF, 0xBE, 0xE3, 0x37, 0x02, 0x32}}, // 踝
  {0x8EAB, {0x01, 0x80, 0x03, 0x80, 0x1F, 0xF8, 0x1F, 0xF8, 0x1F, 0xF8, 0x1F, 0xFC, 0x18, 0x3E, 0x1F, 0xFE, 0x1F, 0xFC, 0x1F, 0xF8, 0x7F, 0xF8, 0x7F, 0xF8, 0x07, 0xB8, 0x3F, 0x38, 0x7C, 0xF8, 0x60, 0xF0}}, // 身
  {0x8F91, {0x18, 0xFC, 0x39, 0xFE, 0xFF, 0x8E, 0xFF, 0xFE, 0x38, 0xFC, 0x7F, 0xFE, 0x7D, 0xFE, 0x7D, 0xFC, 0x7F, 0xFC, 0x1D, 0x8C, 0x1F, 0xFC, 0xFF, 0xFC, 0xFD, 0xDF, 0x1F, 0xFE, 0x1D, 0xEC, 0x1C, 0x0C}}, // 辑
  {0x8FD4, {0x63, 0xFE, 0x73, 0xFE, 0x3B, 0x00, 0x03, 0xFC, 0x03, 0xFE, 0x03, 0xCC, 0x7B, 0xDC, 0x7B, 0xF8, 0x3F, 0x78, 0x3E, 0x78, 0x3F, 0xFE, 0x3F, 0xCE, 0x7C, 0x86, 0xFF, 0xFF, 0x47, 0xFE, 0x00, 0x00}}, // 返
  {0x8FDB, {0x71, 0xD8, 0x39, 0xD8, 0x1F, 0xFE, 0x07, 0xFE, 0x01, 0xD8, 0x79, 0xD8, 0x7F, 0xFE, 0x3F, 0xFE, 0x39, 0x98, 0x3B, 0x98, 0x3B, 0x18, 0x3B, 0x18, 0x7E, 0x08, 0xEF, 0xFF, 0x43, 0xFE, 0x00, 0x00}}, // 进
  {0x9009, {0x63, 0xF0, 0x73, 0xFE, 0x3F, 0xFE, 0x06, 0x70, 0x02, 0x70, 0x07, 0xFF, 0x7F, 0xFE, 0x79, 0xF8, 0x39, 0xB8, 0x3B, 0xBB, 0x3F, 0x1F, 0x3E, 0x1E, 0x7E, 0x00, 0x77, 0xFE, 0x61, 0xFE, 0x00, 0x00}}, // 选
  {0x903B, {0x77, 0xFE, 0x3E, 0xD6, 0x17, 0xFE, 0x07, 0xFE, 0x00, 0xC0, 0xF9, 0xFC, 0x7B, 0xFC, 0x3F, 0x9C, 0x3F, 0xF8, 0x38, 0xF0, 0x39, 0xE0, 0x3F, 0xC0, 0x7F, 0x00, 0xEF, 0xFF, 0x43, 0xFE, 0x00, 0x00}}, // 逻
  {0x90E8, {0x0C, 0x00, 0x0C, 0x7E, 0x7F, 0xFE, 0x7F, 0xE6, 0x33, 0x6E, 0x36, 0x6C, 0xFF, 0xEC, 0xFF, 0xEC, 0x00, 0x66, 0x7F, 0x66, 0x7F, 0x66, 0x63, 0x66, 0x63, 0x7E, 0x7F, 0x6C, 0x7F, 0x60, 0x63, 0x60}}, // 部
  {0x91CF, {0x1F, 0xF8, 0x18, 0x18, 0x1F, 0xF8, 0x18, 0x18, 0x1F, 0xF8, 0x7F, 0xFE, 0x7F, 0xFE, 0x3F, 0xFC, 0x31, 0x8C, 0x3F, 0xFC, 0x31, 0x8C, 0x3F, 0xFC, 0x3F, 0xFC, 0x3F, 0xFC, 0x7F, 0xFF, 0x7F, 0xFE}}, // 量
  {0x952E, {0x37, 0xFE, 0x7F, 0xBE, 0x79, 0xFE, 0x43, 0x7F, 0x7F, 0x1E, 0x7F, 0xFE, 0x33, 0xB8, 0x7D, 0xFE, 0x7F, 0xBE, 0x33, 0xFE, 0x33, 0xFE, 0x3F, 0x98, 0x3F, 0xF0, 0x36, 0xFF, 0x24, 0x1E, 0x00, 0x00}}, // 键
  {0x95F4, {0x20, 0x00, 0x77, 0xFE, 0x3F, 0xFE, 0x10, 0x06, 0x60, 0x06, 0x67, 0xF6, 0x67, 0xF6, 0x66, 0x76, 0x67, 0xF6, 0x67, 0xF6, 0x66, 0x76, 0x67, 0xF6, 0x67, 0xF6, 0x60, 0x06, 0x60, 0x1E, 0x60, 0x0C}}, // 间
  {0x9635, {0x00, 0x60, 0x7C, 0x60, 0x7F, 0xFE, 0x6F, 0xFE, 0x7C, 0xC0, 0x79, 0xF8, 0x79, 0xB8, 0x6F, 0xFE, 0x6D, 0xFE, 0x6C, 0x38, 0x7C, 0x38, 0x7F, 0xFF, 0x63, 0xFF, 0x60, 0x38, 0x60, 0x38, 0x60, 0x30}}, // 阵
  {0x9762, {0x7F, 0xFE, 0x7F, 0xFE, 0x03, 0x80, 0x7F, 0xFE, 0x7F, 0xFE, 0x66, 0x6E, 0x67, 0xEE, 0x67, 0xEE, 0x66, 0x6E, 0x67, 0xEE, 0x67, 0xEE, 0x66, 0x6E, 0x7F, 0xFE, 0x7F, 0xFE, 0x60, 0x0E, 0x00, 0x00}}, // 面
  {0x9875, {0x7F, 0xFE, 0x7F, 0xFE, 0x03, 0x80, 0x3F, 0xFC, 0x3F, 0xFC, 0x38, 0x0C, 0x39, 0x8C, 0x39, 0x8C, 0x39, 0x8C, 0x39, 0x8C, 0x3B, 0x8C, 0x07, 0xF0, 0x1F, 0x3C, 0x7C, 0x1E, 0x70, 0x06, 0x00, 0x00}}, // 页
  {0x9879, {0x03, 0xFF, 0xFF, 0xFE, 0xFC, 0x60, 0x3B, 0xFE, 0x3B, 0xFE, 0x3B, 0xBE, 0x3B, 0xFE, 0x3B, 0xFE, 0x3F, 0xFE, 0x7F, 0xFE, 0xFF, 0xE6, 0xE0, 0xE8, 0x01, 0xDE, 0x07, 0x8F, 0x07, 0x02, 0x00, 0x00}}, // 项
  {0x9884, {0x00, 0x00, 0x7F, 0xFF, 0x7F, 0xFF, 0x0C, 0x30, 0x78, 0xFE, 0x3C, 0xFE, 0xFF, 0xF6, 0xFF, 0xF6, 0x1E, 0xF6, 0x1E, 0xF6, 0x18, 0xF6, 0x18, 0xF6, 0x18, 0x7C, 0x18, 0xEE, 0x79, 0xC7, 0x71, 0x82}}, // 预
  {0x9996, {0x0C, 0x38, 0x0C, 0x30, 0x7F, 0xFE, 0x7F, 0xFE, 0x01, 0x80, 0x3F, 0xFC, 0x3F, 0xFC, 0x30, 0x1C, 0x3F, 0xFC, 0x3F, 0xFC, 0x3F, 0xFC, 0x3F, 0xFC, 0x30, 0x1C, 0x3F, 0xFC, 0x3F, 0xFC, 0x30, 0x1C}}, // 首
  {0x9AA4, {0x7F, 0xFE, 0x7F, 0xE6, 0x6F, 0x7C, 0x6F, 0xFC, 0x6B, 0xEE, 0x7B, 0xFE, 0x7B, 0xFC, 0x7F, 0xFC, 0x7C, 0x30, 0x0D, 0xFE, 0x7F, 0xB8, 0xFD, 0xFC, 0x0F, 0xFE, 0x3F, 0x36, 0x38, 0x30, 0x00, 0x00}}, // 骤
  {0x9ACB, {0x3E, 0x30, 0x7F, 0xFE, 0x67, 0xFF, 0x7F, 0x83, 0x7F, 0xFE, 0x7F, 0xFE, 0x7F, 0x68, 0x7F, 0xFE, 0x7E, 0xFE, 0x66, 0xF6, 0x7E, 0xF6, 0x66, 0xFE, 0x7E, 0x7A, 0x66, 0x7B, 0x6F, 0xFF, 0x6F, 0x9E}}, // 髋
  {0x4E09, {0x00, 0x00, 0x3F, 0xF8, 0x3F, 0xF8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F, 0xF0, 0x1F, 0xF0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7F, 0xFC, 0x7F, 0xFC, 0x00, 0x00, 0x00, 0x00}}, // 三
  {0x503C, {0x18, 0x60, 0x1F, 0xFC, 0x37, 0xFC, 0x30, 0xC0, 0x73, 0xF8, 0x73, 0x18, 0xF3, 0x18, 0x73, 0xF8, 0x33, 0x18, 0x33, 0xF8, 0x32, 0x08, 0x33, 0xF8, 0x33, 0x18, 0x3F, 0xFE, 0x37, 0xFC, 0x00, 0x00}}, // 值
  {0x504F, {0x18, 0xC0, 0x1F, 0xF8, 0x3F, 0xFC, 0x36, 0x0C, 0x77, 0xFC, 0xF7, 0xFC, 0xF6, 0x00, 0x77, 0xFC, 0x37, 0xFC, 0x37, 0x5C, 0x37, 0xFC, 0x37, 0xFC, 0x3D, 0x5C, 0x3D, 0x5C, 0x31, 0x0C, 0x00, 0x00}}, // 偏
  {0x518D, {0x00, 0x00, 0x7F, 0xFC, 0x7F, 0xFC, 0x03, 0x00, 0x3F, 0xF8, 0x3F, 0xF8, 0x3F, 0xF8, 0x3F, 0xF8, 0x33, 0x18, 0x7F, 0xFC, 0xFF, 0xFE, 0x30, 0x18, 0x30, 0x18, 0x30, 0x78, 0x30, 0x70, 0x00, 0x00}}, // 再
  {0x5224, {0x0C, 0x0C, 0x6D, 0x8C, 0x6F, 0x6C, 0x3F, 0x6C, 0x3F, 0x6C, 0x7F, 0x6C, 0x7F, 0x6C, 0x0C, 0x6C, 0x7F, 0x6C, 0xFF, 0xEC, 0x18, 0x6C, 0x18, 0x0C, 0x38, 0x0C, 0x70, 0x3C, 0x20, 0x38, 0x00, 0x00}}, // 判
  {0x5411, {0x03, 0x00, 0x03, 0x00, 0x07, 0x00, 0x7F, 0xFC, 0x7F, 0xFC, 0x60, 0x0C, 0x6F, 0xEC, 0x6F, 0xEC, 0x6C, 0x6C, 0x6C, 0x6C, 0x6F, 0xEC, 0x6F, 0xCC, 0x64, 0x0C, 0x60, 0x7C, 0x60, 0x38, 0x00, 0x00}}, // 向
  {0x5750, {0x03, 0x00, 0x1B, 0xB0, 0x1B, 0xB0, 0x3B, 0xB0, 0x3F, 0xF8, 0x7F, 0xFC, 0x67, 0xCC, 0x43, 0x80, 0x3F, 0xF8, 0x3F, 0xF8, 0x03, 0x80, 0x03, 0x80, 0x03, 0x80, 0x7F, 0xFC, 0x7F, 0xFC, 0x00, 0x00}}, // 坐
  {0x5934, {0x00, 0xC0, 0x1C, 0xC0, 0x0E, 0xC0, 0x04, 0xC0, 0x71, 0xC0, 0x3D, 0x80, 0x09, 0x80, 0x7F, 0xFC, 0x7F, 0xFC, 0x03, 0x80, 0x03, 0x40, 0x07, 0xF0, 0x1E, 0x38, 0x78, 0x1C, 0x60, 0x04, 0x00, 0x00}}, // 头
  {0x5C55, {0x3F, 0xF8, 0x3F, 0xFC, 0x30, 0x0C, 0x3F, 0xFC, 0x3F, 0xFC, 0x33, 0x30, 0x3F, 0xFC, 0x3F, 0xF8, 0x33, 0x30, 0x3F, 0xFC, 0x7F, 0xFC, 0x66, 0x7C, 0x66, 0xF8, 0xEF, 0xBC, 0x4E, 0x0C, 0x00, 0x00}}, // 展
  {0x5CF0, {0x00, 0x00, 0x10, 0xC0, 0x10, 0xFC, 0x71, 0xF8, 0x7F, 0xF8, 0x7F, 0xF0, 0x7C, 0xFC, 0x7F, 0xFE, 0x7F, 0x6C, 0x7D, 0xFC, 0x7C, 0x70, 0x7D, 0xFC, 0x7C, 0x70, 0x7F, 0xFC, 0x05, 0xFC, 0x00, 0x60}}, // 峰
  {0x6162, {0x33, 0xF8, 0x33, 0x18, 0x3B, 0xF8, 0x7B, 0x18, 0x7F, 0xF8, 0x77, 0xFC, 0xF7, 0xBC, 0xF7, 0xBC, 0x37, 0xFC, 0x37, 0xF8, 0x37, 0xFC, 0x33, 0xB8, 0x31, 0xF0, 0x37, 0xFC, 0x37, 0x1C, 0x00, 0x00}}, // 慢
  {0x62AC, {0x30, 0xC0, 0x30, 0xC0, 0x31, 0x90, 0xF9, 0x98, 0x7B, 0x1C, 0x37, 0xFC, 0x37, 0xFE, 0x38, 0x04, 0xFB, 0xF8, 0xF3, 0xFC, 0x33, 0x1C, 0x33, 0x1C, 0x33, 0x1C, 0x73, 0xFC, 0x63, 0xFC, 0x00, 0x00}}, // 抬
  {0x65C1, {0x03, 0x00, 0x7F, 0xFC, 0x7F, 0xFC, 0x0C, 0x60, 0x7F, 0xFC, 0x7F, 0xFC, 0x61, 0x0C, 0x7F, 0xFC, 0x7F, 0xFC, 0x0E, 0x00, 0x0F, 0xF8, 0x0F, 0xF8, 0x38, 0x38, 0x78, 0xF0, 0x60, 0xF0, 0x00, 0x00}}, // 旁
  {0x66F2, {0x06, 0xC0, 0x06, 0xC0, 0x06, 0xC0, 0x7F, 0xFC, 0x7F, 0xFC, 0x66, 0xCC, 0x66, 0xCC, 0x66, 0xCC, 0x7F, 0xFC, 0x7F, 0xFC, 0x66, 0xCC, 0x66, 0xCC, 0x7F, 0xFC, 0x7F, 0xFC, 0x60, 0x0C, 0x00, 0x00}}, // 曲
  {0x79BB, {0x00, 0x00, 0x03, 0x00, 0x7F, 0xFC, 0x7F, 0xFC, 0x16, 0xD8, 0x33, 0xD8, 0x36, 0xD8, 0x3F, 0xF8, 0x03, 0x80, 0x3F, 0xF8, 0x7F, 0xFC, 0x66, 0xCC, 0x6F, 0xEC, 0x6F, 0xEC, 0x60, 0x3C, 0x60, 0x38}}, // 离
  {0x7F13, {0x10, 0x3C, 0x33, 0xFC, 0x33, 0x6C, 0x73, 0x6C, 0x6F, 0xFC, 0xFF, 0xFC, 0x7B, 0xFC, 0x37, 0xFC, 0x7D, 0x80, 0xF9, 0xFC, 0x43, 0xD8, 0x1F, 0xF8, 0xFF, 0x78, 0xEF, 0xFE, 0x05, 0x8C, 0x00, 0x00}}, // 缓
  {0x808C, {0x7D, 0xF8, 0x7D, 0xF8, 0x6D, 0x98, 0x6D, 0x98, 0x7D, 0x98, 0x7D, 0x98, 0x6D, 0x98, 0x6D, 0x98, 0x7D, 0x98, 0x7D, 0x98, 0x6D, 0x9C, 0x6D, 0x9E, 0x6F, 0x9E, 0xDF, 0x1E, 0x59, 0x1C, 0x00, 0x00}}, // 肌
  {0x80B1, {0x7C, 0xC0, 0x7C, 0xC0, 0x6F, 0xFC, 0x6F, 0xFE, 0x7D, 0x80, 0x7D, 0x90, 0x6D, 0xB0, 0x6D, 0xB0, 0x7D, 0xB0, 0x7D, 0xB8, 0x6D, 0xEC, 0x6F, 0xEC, 0x6F, 0xFC, 0xDF, 0xFE, 0x5A, 0x04, 0x00, 0x00}}, // 肱
  {0x4E60, {0x00, 0x00, 0x7F, 0xFC, 0x3F, 0xFC, 0x00, 0x0C, 0x1C, 0x0C, 0x0E, 0x0C, 0x07, 0x0C, 0x03, 0x0C, 0x00, 0xDC, 0x07, 0xDC, 0x1F, 0x98, 0x7C, 0x18, 0x20, 0x18, 0x00, 0xF8, 0x00, 0xF0, 0x00, 0x00}}, // 习
  {0x4EE5, {0x00, 0x00, 0x30, 0x18, 0x32, 0x18, 0x33, 0x18, 0x33, 0x98, 0x31, 0x98, 0x30, 0x30, 0x30, 0x30, 0x32, 0x30, 0x3E, 0x70, 0x3E, 0xF8, 0x78, 0xF8, 0x63, 0xCC, 0x03, 0x0C, 0x00, 0x00, 0x00, 0x00}}, // 以
  {0x4FE1, {0x18, 0xC0, 0x18, 0xE0, 0x3F, 0xFC, 0x37, 0xFE, 0x70, 0x00, 0x73, 0xF8, 0xF3, 0xF8, 0x33, 0xF8, 0x33, 0xF8, 0x33, 0xF8, 0x37, 0xFC, 0x36, 0x0C, 0x36, 0x0C, 0x37, 0xFC, 0x37, 0xFC, 0x00, 0x00}}, // 信
  {0x505A, {0x1B, 0x30, 0x3B, 0x30, 0x33, 0x30, 0x3F, 0xFE, 0x7F, 0xFC, 0xF3, 0x6C, 0xF3, 0xEC, 0x77, 0xFC, 0x3F, 0xB8, 0x3D, 0xB8, 0x3D, 0x98, 0x3F, 0xB8, 0x3F, 0xFC, 0x3C, 0xEC, 0x30, 0x44, 0x00, 0x00}}, // 做
  {0x5185, {0x03, 0x80, 0x03, 0x80, 0x03, 0x80, 0x7F, 0xFC, 0x7F, 0xFC, 0x63, 0x0C, 0x63, 0x0C, 0x63, 0x8C, 0x67, 0xCC, 0x6E, 0xEC, 0x7C, 0x7C, 0x68, 0x2C, 0x60, 0x0C, 0x60, 0x7C, 0x60, 0x38, 0x00, 0x00}}, // 内
  {0x52FF, {0x00, 0x00, 0x18, 0x00, 0x18, 0x00, 0x1F, 0xFC, 0x3F, 0xFC, 0x36, 0x6C, 0x76, 0x6C, 0xE6, 0xCC, 0x4C, 0xCC, 0x0C, 0xCC, 0x19, 0x8C, 0x39, 0x8C, 0x73, 0x9C, 0x27, 0x18, 0x06, 0x78, 0x00, 0x70}}, // 勿
  {0x539F, {0x00, 0x00, 0x3F, 0xFC, 0x3F, 0xFC, 0x31, 0xC0, 0x3F, 0xF8, 0x3C, 0x18, 0x3F, 0xF8, 0x6E, 0x18, 0x6E, 0x18, 0x6F, 0xF8, 0x60, 0xC0, 0x66, 0xD8, 0x6C, 0xDC, 0xDB, 0xCC, 0x43, 0x80, 0x00, 0x00}}, // 原
  {0x56E0, {0x7F, 0xFC, 0x7F, 0xFC, 0x60, 0x0C, 0x61, 0x8C, 0x61, 0x8C, 0x7F, 0xFC, 0x6F, 0xEC, 0x63, 0x0C, 0x63, 0x8C, 0x67, 0xCC, 0x6E, 0x6C, 0x6C, 0x2C, 0x68, 0x0C, 0x7F, 0xFC, 0x7F, 0xFC, 0x00, 0x00}}, // 因
  {0x5728, {0x06, 0x00, 0x06, 0x00, 0x7F, 0xFC, 0x7F, 0xFC, 0x0C, 0x00, 0x18, 0x60, 0x38, 0x60, 0x7B, 0xFC, 0xF3, 0xFC, 0x70, 0x60, 0x30, 0x60, 0x30, 0x60, 0x30, 0x60, 0x37, 0xFC, 0x37, 0xFC, 0x00, 0x00}}, // 在
  {0x5747, {0x30, 0xC0, 0x30, 0xC0, 0x31, 0xFC, 0x31, 0xFC, 0xFF, 0x0C, 0x7E, 0x0C, 0x33, 0xCC, 0x30, 0xEC, 0x30, 0x5C, 0x3C, 0x7C, 0x39, 0xEC, 0xF3, 0x8C, 0x43, 0x0C, 0x00, 0x7C, 0x00, 0x78, 0x00, 0x00}}, // 均
  {0x591A, {0x03, 0x00, 0x07, 0xF0, 0x1F, 0xF0, 0x38, 0x60, 0x3F, 0xE0, 0x07, 0x80, 0x3F, 0xE0, 0x7D, 0xFC, 0x23, 0xFC, 0x1F, 0x1C, 0x1F, 0x38, 0x01, 0xF0, 0x07, 0xC0, 0x7F, 0x80, 0x7C, 0x00, 0x00, 0x00}}, // 多
  {0x5931, {0x1B, 0x00, 0x3B, 0x00, 0x3F, 0xF8, 0x3F, 0xF8, 0x63, 0x00, 0x63, 0x00, 0x03, 0x00, 0x7F, 0xFC, 0x7F, 0xFC, 0x07, 0xC0, 0x06, 0xC0, 0x0E, 0xE0, 0x3C, 0x78, 0x78, 0x3E, 0x60, 0x0C, 0x00, 0x00}}, // 失
  {0x597D, {0x30, 0x00, 0x31, 0xFC, 0x31, 0xFC, 0x7C, 0x18, 0xFE, 0x30, 0x66, 0x30, 0x6F, 0xFE, 0x6F, 0xFE, 0x7C, 0x30, 0x3C, 0x30, 0x1C, 0x30, 0x3C, 0x30, 0x7E, 0x30, 0xE4, 0xF0, 0x40, 0xE0, 0x00, 0x00}}, // 好
  {0x5B66, {0x00, 0x00, 0x13, 0x18, 0x19, 0xB8, 0x7F, 0xFC, 0x7F, 0xFC, 0x60, 0x0C, 0x6F, 0xEC, 0x1F, 0xF0, 0x01, 0xC0, 0x03, 0x80, 0x7F, 0xFC, 0x7F, 0xFC, 0x03, 0x00, 0x03, 0x00, 0x0F, 0x00, 0x0F, 0x00}}, // 学
  {0x5B9E, {0x00, 0x00, 0x03, 0x00, 0x7F, 0xFC, 0x7F, 0xFC, 0x68, 0x0C, 0x6D, 0x8C, 0x05, 0x80, 0x31, 0x80, 0x39, 0x80, 0x01, 0x80, 0x7F, 0xFC, 0x7F, 0xFC, 0x07, 0xE0, 0x1E, 0xF8, 0x7C, 0x3C, 0x30, 0x08}}, // 实
  {0x5BF8, {0x00, 0x60, 0x00, 0x60, 0x00, 0x60, 0x7F, 0xFC, 0x7F, 0xFC, 0x00, 0x60, 0x10, 0x60, 0x38, 0x60, 0x1C, 0x60, 0x0C, 0x60, 0x0C, 0x60, 0x00, 0x60, 0x00, 0x60, 0x03, 0xE0, 0x03, 0xC0, 0x00, 0x00}}, // 寸
  {0x5C1D, {0x13, 0x90, 0x1B, 0xB0, 0x7F, 0xFC, 0x7F, 0xFC, 0x60, 0x0C, 0x7F, 0xFC, 0x3F, 0xF0, 0x00, 0x00, 0x7F, 0xFC, 0x7F, 0xFC, 0x0E, 0x60, 0x1C, 0x70, 0x3F, 0xF8, 0x3F, 0xF8, 0x38, 0x08, 0x00, 0x00}}, // 尝
  {0x5E73, {0x7F, 0xFC, 0x7F, 0xFC, 0x13, 0x90, 0x3B, 0xB8, 0x1B, 0xB0, 0x1B, 0xB0, 0x03, 0x80, 0x7F, 0xFC, 0x7F, 0xFC, 0x03, 0x80, 0x03, 0x80, 0x03, 0x80, 0x03, 0x80, 0x03, 0x80, 0x00, 0x00, 0x00, 0x00}}, // 平
  {0x5E8F, {0x01, 0x80, 0x3F, 0xFC, 0x3F, 0xFC, 0x20, 0x00, 0x2F, 0xF8, 0x67, 0xF8, 0x63, 0x70, 0x63, 0xE0, 0x6F, 0xFC, 0x7F, 0xFC, 0x60, 0xDC, 0x60, 0xD8, 0x60, 0xC0, 0xE7, 0xC0, 0x43, 0x80, 0x00, 0x00}}, // 序
  {0x5F02, {0x3F, 0xF0, 0x3F, 0xF8, 0x30, 0x18, 0x3F, 0xF8, 0x3F, 0xF8, 0x30, 0x0C, 0x3F, 0xFC, 0x1F, 0xF8, 0x08, 0x20, 0x0C, 0x60, 0x7F, 0xFC, 0x7F, 0xFC, 0x38, 0x60, 0x70, 0x60, 0x60, 0x60, 0x00, 0x00}}, // 异
  {0x5F85, {0x18, 0x60, 0x38, 0x60, 0x73, 0xFC, 0xE3, 0xFC, 0x58, 0x60, 0x3F, 0xFE, 0x77, 0xFC, 0xF0, 0x18, 0x77, 0xFE, 0x37, 0xFC, 0x33, 0x18, 0x33, 0x98, 0x31, 0x98, 0x30, 0x78, 0x30, 0x70, 0x00, 0x00}}, // 待
  {0x627F, {0x1F, 0xF0, 0x1F, 0xF0, 0x00, 0xE0, 0x01, 0xC0, 0x79, 0xBC, 0x7F, 0xFC, 0x1F, 0xF8, 0x3F, 0xF8, 0x37, 0xD8, 0x31, 0x98, 0x7F, 0xFC, 0x6F, 0xEE, 0x43, 0x84, 0x0F, 0x00, 0x07, 0x00, 0x00, 0x00}}, // 承
  {0x672C, {0x03, 0x80, 0x03, 0x80, 0x03, 0x80, 0x7F, 0xFC, 0x7F, 0xFC, 0x0F, 0xE0, 0x0F, 0xE0, 0x1B, 0xF0, 0x1B, 0xB0, 0x33, 0x98, 0x7F, 0xFC, 0x7F, 0xFC, 0x03, 0x80, 0x03, 0x80, 0x03, 0x80, 0x00, 0x00}}, // 本
  {0x67B6, {0x18, 0x00, 0x7E, 0xFC, 0x7F, 0xFC, 0x33, 0xCC, 0x33, 0xCC, 0x36, 0xFC, 0x6E, 0xFC, 0x4F, 0x00, 0x7F, 0xFC, 0x7F, 0xFC, 0x07, 0xC0, 0x1F, 0xF0, 0x3B, 0xBC, 0x73, 0x9C, 0x43, 0x80, 0x00, 0x00}}, // 架
  {0x67E5, {0x03, 0x00, 0x03, 0x00, 0x7F, 0xFC, 0x7F, 0xFC, 0x1F, 0xE0, 0x3B, 0x38, 0x71, 0x1C, 0x5F, 0xF4, 0x18, 0x30, 0x1F, 0xF0, 0x18, 0x30, 0x1F, 0xF0, 0x00, 0x00, 0x7F, 0xFC, 0x7F, 0xFC, 0x00, 0x00}}, // 查
  {0x6837, {0x00, 0x00, 0x31, 0x0C, 0x31, 0x98, 0x31, 0x98, 0x7F, 0xFC, 0x7B, 0xFC, 0x30, 0x60, 0x39, 0xFC, 0x7F, 0xFC, 0x7C, 0x60, 0xF3, 0xFC, 0xF7, 0xFE, 0x30, 0x60, 0x30, 0x60, 0x30, 0x60, 0x30, 0x60}}, // 样
  {0x6B62, {0x00, 0x00, 0x01, 0x80, 0x01, 0x80, 0x01, 0x80, 0x11, 0x80, 0x39, 0x80, 0x39, 0xFC, 0x39, 0xFC, 0x39, 0x80, 0x39, 0x80, 0x39, 0x80, 0x39, 0x80, 0x39, 0x80, 0x7F, 0xFC, 0xFF, 0xFC, 0x00, 0x00}}, // 止
  {0x6CA1, {0x21, 0xF0, 0x7B, 0xF0, 0x13, 0x30, 0x03, 0x30, 0x07, 0x3E, 0xF6, 0x1E, 0x74, 0x00, 0x07, 0xFC, 0x07, 0xF8, 0x1B, 0x38, 0x39, 0xB0, 0x31, 0xE0, 0x73, 0xF8, 0x6F, 0xFE, 0x26, 0x0C, 0x00, 0x00}}, // 没
  {0x7387, {0x00, 0x00, 0x03, 0x00, 0x7F, 0xFC, 0x7F, 0xFC, 0x03, 0x48, 0x77, 0xDC, 0x37, 0x98, 0x03, 0xD0, 0x7F, 0xF8, 0x7F, 0xFC, 0x03, 0x00, 0x7F, 0xFC, 0x7F, 0xFC, 0x03, 0x80, 0x03, 0x80, 0x03, 0x80}}, // 率
  {0x770B, {0x07, 0xF8, 0x3F, 0xF8, 0x27, 0x00, 0x3F, 0xF8, 0x06, 0x00, 0x7F, 0xFC, 0x7F, 0xFC, 0x1F, 0xF8, 0x3C, 0x38, 0x7F, 0xF8, 0x6C, 0x18, 0x0F, 0xF8, 0x0C, 0x18, 0x0F, 0xF8, 0x0C, 0x38, 0x00, 0x00}}, // 看
  {0x771F, {0x01, 0x80, 0x7F, 0xFC, 0x7F, 0xFC, 0x1F, 0xF0, 0x18, 0x30, 0x1F, 0xF0, 0x18, 0x30, 0x1F, 0xF0, 0x18, 0x30, 0x1F, 0xF0, 0x7F, 0xFC, 0x7F, 0xFC, 0x0C, 0x70, 0x7C, 0x7C, 0x70, 0x0C, 0x00, 0x00}}, // 真
  {0x87BA, {0x30, 0x00, 0x33, 0xFC, 0x33, 0x6C, 0x7F, 0xFC, 0x7F, 0x6C, 0x5F, 0xFC, 0x5C, 0xD0, 0x7D, 0xF8, 0x7D, 0xF8, 0x71, 0xEC, 0x3F, 0xFC, 0x3D, 0x6C, 0xFF, 0xFC, 0x67, 0xEC, 0x02, 0xE4, 0x00, 0x00}}, // 螺
  {0x8BF7, {0x20, 0x60, 0x73, 0xFC, 0x38, 0x60, 0x13, 0xFC, 0x00, 0x60, 0xF7, 0xFE, 0x70, 0x00, 0x33, 0xFC, 0x33, 0x1C, 0x33, 0xFC, 0x37, 0x0C, 0x3F, 0x0C, 0x3F, 0xFC, 0x33, 0x3C, 0x23, 0x38, 0x00, 0x00}}, // 请
  {0x8D25, {0x00, 0x40, 0x7E, 0x60, 0x7E, 0x60, 0x7E, 0xFC, 0x7E, 0xFC, 0x7F, 0xC8, 0x7F, 0xC8, 0x7F, 0xD8, 0x7E, 0x78, 0x7E, 0x78, 0x7A, 0x70, 0x3C, 0x30, 0x36, 0x78, 0x67, 0xDC, 0x40, 0x84, 0x00, 0x00}}, // 败
  {0x8D77, {0x18, 0xFC, 0x7E, 0xFC, 0x7E, 0x0C, 0x18, 0x0C, 0x18, 0x0C, 0x7F, 0xFC, 0x7E, 0xFC, 0x68, 0xC0, 0x6F, 0xC4, 0x6E, 0xC6, 0x78, 0xFC, 0x78, 0x7C, 0x7C, 0x00, 0xDF, 0xFE, 0xC7, 0xFC, 0x00, 0x00}}, // 起
  {0x8F74, {0x10, 0x60, 0x30, 0x60, 0x7C, 0x60, 0x7D, 0xFC, 0x39, 0xFC, 0x79, 0xEC, 0x79, 0xEC, 0x7F, 0xEC, 0x7F, 0xFC, 0x19, 0xFC, 0x7F, 0xEC, 0xFF, 0xEC, 0x19, 0xFC, 0x19, 0xFC, 0x19, 0x8C, 0x00, 0x00}}, // 轴
  {0x8F93, {0x00, 0x00, 0x30, 0x60, 0x30, 0xF0, 0xFD, 0xF8, 0x7F, 0x9C, 0x7F, 0xFC, 0x79, 0xFC, 0x5B, 0xFC, 0xFF, 0xFC, 0x7B, 0xFC, 0x1B, 0x7C, 0x7F, 0x7C, 0xFB, 0xFC, 0x1B, 0x74, 0x1B, 0xCC, 0x1B, 0xDC}}, // 输
  {0x8FBE, {0x20, 0xC0, 0x60, 0xC0, 0x30, 0xC0, 0x37, 0xFC, 0x07, 0xFC, 0x00, 0xC0, 0xF0, 0xE0, 0x71, 0xF0, 0x31, 0xB0, 0x33, 0x98, 0x37, 0x1C, 0x36, 0x0C, 0x7C, 0x00, 0xEF, 0xFC, 0x43, 0xFC, 0x00, 0x00}}, // 达
  {0x8FD8, {0x20, 0x00, 0x77, 0xFC, 0x3F, 0xFC, 0x10, 0xE0, 0x01, 0xC0, 0xF1, 0xD0, 0xF3, 0xF8, 0x37, 0xDC, 0x3E, 0xCC, 0x34, 0xC0, 0x30, 0xC0, 0x30, 0xC0, 0x7C, 0x40, 0xEF, 0xFE, 0x43, 0xFC, 0x00, 0x00}}, // 还
  {0x901A, {0x00, 0x08, 0x67, 0xFC, 0x71, 0xB8, 0x31, 0xF0, 0x07, 0xFC, 0x06, 0xEC, 0xF7, 0xFC, 0x76, 0xEC, 0x36, 0xEC, 0x37, 0xFC, 0x36, 0x6C, 0x36, 0x7C, 0x7E, 0x18, 0xEF, 0xFE, 0x41, 0xFC, 0x00, 0x00}}, // 通
  {0x901F, {0x00, 0xC0, 0x67, 0xFC, 0x77, 0xFC, 0x30, 0xC0, 0x07, 0xFC, 0x07, 0xFC, 0xF6, 0xFC, 0x77, 0xFC, 0x31, 0xF0, 0x33, 0xF8, 0x3F, 0xDC, 0x36, 0xC4, 0x7C, 0x40, 0xEF, 0xFC, 0x41, 0xFC, 0x00, 0x00}}, // 速
  {0x91CD, {0x00, 0x00, 0x07, 0xF0, 0x3F, 0xF0, 0x03, 0x00, 0x7F, 0xFC, 0x7F, 0xFC, 0x3F, 0xF8, 0x33, 0x98, 0x3F, 0xF8, 0x33, 0x98, 0x3F, 0xF8, 0x03, 0x80, 0x3F, 0xF8, 0x03, 0x00, 0x7F, 0xFC, 0x7F, 0xFC}}, // 重
  {0x9608, {0x33, 0xFC, 0x3F, 0xFC, 0x18, 0x0C, 0x61, 0xEC, 0x61, 0xEC, 0x7F, 0xFC, 0x60, 0xAC, 0x6E, 0xEC, 0x6A, 0xEC, 0x6E, 0xCC, 0x62, 0xDC, 0x7F, 0xFC, 0x61, 0xEC, 0x60, 0x3C, 0x60, 0x38, 0x00, 0x00}}, // 阈
  {0x9640, {0x00, 0x00, 0x7C, 0x60, 0x7C, 0x60, 0x6F, 0xFC, 0x6F, 0xFC, 0x7B, 0x04, 0x7B, 0x84, 0x6D, 0x88, 0x6D, 0xBC, 0x6D, 0xF0, 0x6D, 0xC0, 0x7D, 0x80, 0x79, 0x84, 0x61, 0x8C, 0x61, 0xFC, 0x60, 0xF8}}, // 陀
  {0x9645, {0x78, 0x00, 0x7D, 0xFC, 0x6D, 0xFC, 0x6C, 0x00, 0x78, 0x00, 0x7B, 0xFE, 0x7F, 0xFC, 0x6C, 0x60, 0x6D, 0x68, 0x6F, 0x6C, 0x7F, 0x6C, 0x7B, 0x6C, 0x66, 0x66, 0x60, 0xE4, 0x60, 0xE0, 0x00, 0x00}}, // 际
  {0x9759, {0x18, 0xC0, 0x7E, 0xE0, 0x19, 0xF8, 0x7F, 0xB0, 0x19, 0xF8, 0xFF, 0xFC, 0x00, 0x6C, 0x7F, 0xFE, 0x67, 0xFC, 0x7E, 0x6C, 0x67, 0xFC, 0x7E, 0xFC, 0x66, 0x60, 0x6E, 0xE0, 0x6C, 0xE0, 0x00, 0x00}}, // 静
  {0x989D, {0x18, 0x00, 0x7F, 0xFE, 0x7F, 0xFC, 0x53, 0x30, 0x7F, 0xFC, 0x7E, 0xFC, 0xFC, 0xBC, 0x3C, 0xBC, 0x7F, 0xBC, 0x62, 0xFC, 0x7E, 0xEC, 0x3E, 0xE8, 0x26, 0x7C, 0x3F, 0xCE, 0x3F, 0x84, 0x00, 0x00}}, // 额
  {0x5012, {0x10, 0x04, 0x3F, 0xFC, 0x37, 0xFC, 0x76, 0xBC, 0x67, 0xFC, 0xEF, 0xFC, 0xEE, 0x7C, 0x63, 0x3C, 0x2F, 0xBC, 0x2F, 0xFC, 0x23, 0x3C, 0x23, 0xCC, 0x3F, 0xCC, 0x3F, 0x1C, 0x20, 0x1C, 0x00, 0x00}}, // 倒
  {0x52BF, {0x18, 0xC0, 0x7D, 0xF8, 0x7F, 0xF8, 0x18, 0xD8, 0x7F, 0xD8, 0x7D, 0xDC, 0x1B, 0xFE, 0x73, 0x0C, 0x23, 0x00, 0x7F, 0xF8, 0x7F, 0xF8, 0x06, 0x18, 0x1C, 0x18, 0x78, 0xF8, 0x70, 0xF0, 0x00, 0x00}}, // 势
  {0x624B, {0x00, 0x70, 0x3F, 0xF8, 0x3F, 0xC0, 0x03, 0x80, 0x3F, 0xF8, 0x3F, 0xFC, 0x03, 0x80, 0x03, 0x80, 0x7F, 0xFC, 0x7F, 0xFE, 0x03, 0x80, 0x03, 0x80, 0x03, 0x80, 0x0F, 0x00, 0x0F, 0x00, 0x00, 0x00}}, // 手
  {0x8BA1, {0x20, 0x60, 0x30, 0x60, 0x18, 0x60, 0x08, 0x60, 0x00, 0x60, 0xFF, 0xFE, 0x7F, 0xFE, 0x18, 0x60, 0x18, 0x60, 0x18, 0x60, 0x1C, 0x60, 0x1E, 0x60, 0x3C, 0x60, 0x38, 0x60, 0x00, 0x60, 0x00, 0x00}}, // 计
};

static const int CN_FONT_COUNT = sizeof(CN_FONT) / sizeof(CN_FONT[0]);

uint32_t readUtf8(const char *&p) {
  uint8_t c = (uint8_t)*p;
  if (c < 0x80) { p++; return c; }
  if ((c & 0xE0) == 0xC0) {
    uint32_t cp = ((uint32_t)(p[0] & 0x1F) << 6) | (uint32_t)(p[1] & 0x3F);
    p += 2; return cp;
  }
  if ((c & 0xF0) == 0xE0) {
    uint32_t cp = ((uint32_t)(p[0] & 0x0F) << 12) | ((uint32_t)(p[1] & 0x3F) << 6) | (uint32_t)(p[2] & 0x3F);
    p += 3; return cp;
  }
  p++;
  return '?';
}

int findCnGlyph(uint32_t cp) {
  for (int i = 0; i < CN_FONT_COUNT; ++i) {
    if (pgm_read_dword(&CN_FONT[i].code) == cp) return i;
  }
  return -1;
}

void drawGlyph16(int x, int y, int idx, uint16_t color, int scale) {
  for (int row = 0; row < 16; ++row) {
    uint8_t hi = pgm_read_byte(&CN_FONT[idx].data[row * 2]);
    uint8_t lo = pgm_read_byte(&CN_FONT[idx].data[row * 2 + 1]);
    uint16_t bits = ((uint16_t)hi << 8) | lo;
    for (int col = 0; col < 16; ++col) {
      if (bits & (1 << (15 - col))) {
        if (scale == 1) {
          tft.drawPixel(x + col, y + row, color);
        } else {
          tft.fillRect(x + col * scale, y + row * scale, scale, scale, color);
        }
      }
    }
  }
}

int cnTextWidth(const char *s, int scale = 1) {
  int x = 0;
  const char *p = s;
  while (*p) {
    uint32_t cp = readUtf8(p);
    if (cp < 128) {
      if (cp == '\n') continue;
      char tmp[2] = {(char)cp, 0};
      tft.setTextFont(scale >= 2 ? 4 : 2);
      x += tft.textWidth(tmp) + 1;
    } else {
      x += 16 * scale + 2;
    }
  }
  return x;
}

void drawCnText(int x, int y, const char *s, uint16_t color, int scale = 1) {
  const char *p = s;
  int cx = x;
  while (*p) {
    const char *old = p;
    uint32_t cp = readUtf8(p);
    if (cp < 128) {
      if (cp == '\n') { cx = x; y += 18 * scale; continue; }
      char tmp[2] = {(char)cp, 0};
      tft.setTextFont(scale >= 2 ? 4 : 2);
      tft.setTextColor(color);
      tft.setCursor(cx, y + (scale == 1 ? 1 : 2));
      tft.print(tmp);
      cx += tft.textWidth(tmp) + 1;
    } else {
      int idx = findCnGlyph(cp);
      if (idx >= 0) {
        drawGlyph16(cx, y, idx, color, scale);
      } else {
        // 缺字时画一个小方框，方便发现。
        tft.drawRect(cx, y, 16 * scale, 16 * scale, color);
      }
      cx += 16 * scale + 2;
    }
  }
}

void drawCnCenter(int x, int y, int w, const char *s, uint16_t color, int scale = 1) {
  int tw = cnTextWidth(s, scale);
  drawCnText(x + (w - tw) / 2, y, s, color, scale);
}

// =====================================================
// UI 状态
// =====================================================

enum UiPage {
  PAGE_HOME,
  PAGE_BODY_SELECT,
  PAGE_EXERCISE_SELECT,
  PAGE_MODE_SELECT,
  PAGE_PARAM_SET,
  PAGE_CALIBRATION,
  PAGE_TRAINING,
  PAGE_REST,
  PAGE_RESULT,
  PAGE_COUNT
};

UiPage page = PAGE_HOME;
UiPage lastPage = PAGE_COUNT;

int bodyIndex = 0;     // V4.4 fixed single arm: A left-upper + B left-lower; E = waist/abdomen torso reference; C/D mapped to right side
int exerciseIndex = 0; // 0..7 = RehabMotion eight-action catalog
int modeIndex = 0;     // 0 标准，1 游戏，2 自定义
int paramField = 0;    // 0 组数，1 次数，2 目标角度，3 休息时间
int trainView = 0;     // formal training view
int resultView = 0;    // K12 result pages: 0 overview, 1 ROM/tempo details
bool bilateralLeftSlotWaiting = false;
bool bilateralRightSlotWaiting = false;

int targetSets = 3;
int targetReps = 10;
int targetAngle = 80;
int validAngle = 40;  // K12.2 default: 50% of 80-degree target
int returnAngle = 20;
int restSec = 30;

bool trainingRunning = false;
bool trainingPaused = false;
unsigned long trainingStartMs = 0;
unsigned long lastFakeUpdateMs = 0;

int setIndex = 1;
int leftCount = 0;
int rightCount = 0;
float leftAngle = 0.0f;
float rightAngle = 0.0f;
float leftRom = 0.0f;
float rightRom = 0.0f;
int completionPercent = 0;

bool uiDirty = true;
unsigned long lastBlinkMs = 0;
bool blinkOn = true;

bool systemReady = false;
bool imuReady = false;
bool trainingFinishHandled = false;

// K11 phase feedback. Screen and buzzer observe the SAME gyro algorithm state.
K11GyroPhase k11LastFeedbackPhase = K11_IDLE;
bool k11ReadyFeedbackDone = false;
bool k11FailFeedbackDone = false;

// V4.4 K11 dynamic calibration acceptance.
// V4.4.16: BODY FRONT defines semantic directions; K11 uses per-sensor semantic frames. BODY_AZ is continuous verticalGyro(A)-verticalGyro(E).
bool k11MotionCalibrationAccepted = false;
float k11CalObservedUpperMaxDeg = 0.0f;
float k11CalObservedPlaneMaxDeg = 0.0f;
float k11CalObservedTorsoMaxDeg = 0.0f;
static constexpr float V44_K11_CAL_UPPER_MAX_DEG = 30.0f;
// Keep K11 plane acceptance consistent with the formal-training hard plane limit.
// V4.4.4 accidentally retained the legacy 20 deg gate, causing clean hinge-axis
// calibrations around 25-30 deg plane offset to be rejected forever.
static constexpr float V44_K11_CAL_PLANE_MAX_DEG = 35.0f;
static constexpr float V44_K11_CAL_TORSO_MAX_DEG = 20.0f;

// V4.2.2 explicit semantic start-pose gate for BOTH action 1 and action 2.
// Stage 1 waits indefinitely until the calibrated body-frame says the selected
// arm(s) are actually in the exercise start pose. Stage 2 is now only a short
// breathing-tolerant stability window; K11 itself performs the real A/B gyro bias sampling.
enum A2PreCalPhase : uint8_t {
  A2_PRECAL_OFF = 0,
  A2_PRECAL_PREPARE,
  A2_PRECAL_STILL_CHECK
};
A2PreCalPhase a2PreCalPhase = A2_PRECAL_OFF;
unsigned long a2PreCalPhaseStartMs = 0;
unsigned long a2PreCalStillHoldStartMs = 0;
unsigned long startPoseLastWaitLogMs = 0;
static constexpr unsigned long START_POSE_WAIT_LOG_MS = 1000UL; // diagnostics only; semantic wait has no timeout
static constexpr unsigned long A2_START_STILL_HOLD_MS = 600UL;
static constexpr unsigned long A2_START_SAMPLE_FRESH_MS = 600UL;
static constexpr float A2_START_ARM_STILL_MAX_GYRO_DEG_S = 12.0f;

// Explicit forward declarations keep cross-toolchain compilation aligned with Arduino preprocessing.
static float a2AbsMaxGyro(const WT901EulerData &d);
static const char *a2EffectiveCalPhaseName();
static bool a2PreCalibrationActive();
static float a2PrepareProgress();
static int a2PrepareRemainingSec();
static float a2StartStillProgress();
static bool a2BothImuFreshAndStill();
static void a2DrainPreCalibrationQueues();
static void startA2PreCalibrationGate();
static void updateA2PreCalibrationGate();
static void resetV44K11CalibrationObservation();
static void observeV44K11CalibrationMotion();
static bool validateV44K11CalibrationReady();

// K2.2 group reacquisition: after the first group we keep the calibrated hinge axis,
// but a confirm press only ARMS the next-group gate.  Training remains paused until
// BOTH A (upper arm) and B (forearm) return near the saved first-group start-pose
// gravity directions briefly. E breathing does not block the next group.
struct A2GravityPoseRef {
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
  bool valid = false;
};
A2GravityPoseRef a2SessionStartPoseRef[2];
bool a2GroupReacquireActive = false;
unsigned long a2GroupReacquireHoldStartMs = 0;
float a2GroupPoseDevDeg[2] = {999.0f, 999.0f};
static constexpr float A2_GROUP_POSE_TOL_DEG = 35.0f; // coarse engineering gate, not a clinical threshold
static constexpr unsigned long A2_GROUP_REACQUIRE_HOLD_MS = 500UL;
static constexpr float A2_GROUP_ACCEL_MIN_G = 0.72f;
static constexpr float A2_GROUP_ACCEL_MAX_G = 1.28f;

static bool a2NormalizeLatestGravity(int slotIndex, float &x, float &y, float &z);
static float a2GravityAngleDeg(float ax, float ay, float az, float bx, float by, float bz);
static bool a2CaptureSessionStartPoseReference();
static bool a2GroupPoseMatchesReference();
static void beginA2GroupReacquireGate();
static void updateA2GroupReacquireGate();

unsigned long lastImuStatusTime = 0;
unsigned long lastImuWarnTime = 0;
unsigned long lastRealUiUpdateMs = 0;
unsigned long lastSerialStatusMs = 0;
unsigned long restStartMs = 0;
int restRemainingSec = 0;

float lastLeftSignedAngle = 0.0f;
float lastRightSignedAngle = 0.0f;
float leftSpeedDegS = 0.0f;
float rightSpeedDegS = 0.0f;
float speedPrevLeftAngle = 0.0f;
float speedPrevRightAngle = 0.0f;
unsigned long speedPrevMs = 0;
unsigned long lastJsonTime = 0;

static constexpr unsigned long JSON_INTERVAL_MS = 1000; // K12.1: full debug JSON no longer saturates 115200 baud
static constexpr unsigned long SPEED_SAMPLE_INTERVAL_MS = 80;
static constexpr float SPEED_FILTER_ALPHA = 0.30f;
static constexpr float SPEED_MAX_DEG_S = 500.0f;
static constexpr float SPEED_DEAD_ZONE_DEG_S = 2.0f;

uint32_t gameSeq = 0;
const char *pendingRepEvent = "none";


// =====================================================
// RehabMotion v5 power-loss resume checkpoint (ESP32 NVS)
// -----------------------------------------------------
// Only COMPLETED planned repetitions are persisted.  If power is lost mid-rep
// or during a retry, that unfinished slot is intentionally repeated after boot.
// Motion calibration / K11 reference is NEVER restored from flash: after a reboot
// the user performs the normal BODY-FRONT + K11 calibration again, then the saved
// plan and completed progress are injected into the fresh training state.
// This keeps persistence completely outside the motion-recognition algorithm.
// =====================================================
Preferences powerResumePrefs;
PowerResumeCheckpoint powerResumeCheckpoint;
bool powerResumePrefsReady = false;
bool powerResumeAvailable = false;
bool powerResumeApplyPending = false;
bool powerResumeAppliedThisBoot = false;

// Video evidence UI only: these flags never participate in motion recognition.
static bool screenResumeNoticePending = false;
static bool screenResumeNoticeShown = false;
static unsigned long screenResumeNoticeUntilMs = 0;

// Forward declarations used by the persistence layer.
void applyTrainingParamsToLogic();
void syncUiDataFromTraining();

static uint32_t powerResumeChecksum(const PowerResumeCheckpoint &cp) {
  const uint8_t *p = reinterpret_cast<const uint8_t*>(&cp);
  const size_t n = offsetof(PowerResumeCheckpoint, checksum);
  uint32_t h = 2166136261UL; // FNV-1a
  for (size_t i = 0; i < n; ++i) { h ^= p[i]; h *= 16777619UL; }
  return h;
}

static bool powerResumeCheckpointSane(const PowerResumeCheckpoint &cp) {
  if (cp.magic != POWER_RESUME_MAGIC || cp.version != POWER_RESUME_VERSION || cp.size != sizeof(PowerResumeCheckpoint)) return false;
  if (cp.active != 1) return false;
  if (cp.checksum != powerResumeChecksum(cp)) return false;
  if (cp.exercise >= RM_ACTION_COUNT || cp.mode > 2) return false;
  if (cp.targetSets < 1 || cp.targetSets > 5) return false;
  if (cp.targetReps < 1 || cp.targetReps > 30) return false;
  if (cp.targetAngle < 30 || cp.targetAngle > 120) return false;
  if (cp.validAngle < 5 || cp.validAngle > 120) return false;
  if (cp.returnAngle < 5 || cp.returnAngle > 40) return false;
  if (cp.restSec < 0 || cp.restSec > 120) return false;
  const int totalTarget = cp.targetSets * cp.targetReps;
  if (cp.totalCompletedSlots < 0 || cp.totalCompletedSlots >= totalTarget) return false;
  return true;
}

static void powerResumeClear(const char *reason) {
  powerResumeAvailable = false;
  powerResumeApplyPending = false;
  powerResumeAppliedThisBoot = false;
  powerResumeCheckpoint = PowerResumeCheckpoint();
  if (powerResumePrefsReady) powerResumePrefs.remove(POWER_RESUME_KEY);
  Serial.printf("POWER_RESUME_CLEARED reason=%s\n", reason ? reason : "UNKNOWN");
}

static bool powerResumeLoadAtBoot() {
  powerResumeAvailable = false;
  powerResumeApplyPending = false;
  if (!powerResumePrefsReady) return false;
  const size_t len = powerResumePrefs.getBytesLength(POWER_RESUME_KEY);
  if (len != sizeof(PowerResumeCheckpoint)) {
    if (len > 0) powerResumePrefs.remove(POWER_RESUME_KEY);
    Serial.println("POWER_RESUME_NONE");
    return false;
  }
  PowerResumeCheckpoint cp;
  memset(&cp, 0, sizeof(cp));
  if (powerResumePrefs.getBytes(POWER_RESUME_KEY, &cp, sizeof(cp)) != sizeof(cp) || !powerResumeCheckpointSane(cp)) {
    powerResumePrefs.remove(POWER_RESUME_KEY);
    Serial.println("POWER_RESUME_INVALID: checkpoint discarded");
    return false;
  }
  powerResumeCheckpoint = cp;
  powerResumeAvailable = true;
  powerResumeApplyPending = true;

  // Restore the PLAN immediately so the 7-inch screen reports the same targets
  // before calibration. Progress itself is injected only after fresh calibration.
  exerciseIndex = cp.exercise;
  modeIndex = cp.mode;
  targetSets = cp.targetSets;
  targetReps = cp.targetReps;
  targetAngle = cp.targetAngle;
  validAngle = cp.validAngle;
  returnAngle = cp.returnAngle;
  restSec = cp.restSec;

  const int nextSet = cp.totalCompletedSlots / max(1, (int)cp.targetReps) + 1;
  const int inSet = cp.totalCompletedSlots % max(1, (int)cp.targetReps);
  Serial.printf("POWER_RESUME_FOUND completed=%d/%d next_set=%d in_set=%d plan=%dx%d angle=%d. Fresh calibration required before resume.\n",
                cp.totalCompletedSlots, cp.targetSets * cp.targetReps, nextSet, inSet,
                cp.targetSets, cp.targetReps, cp.targetAngle);
  return true;
}

static bool powerResumeSave(const char *reason) {
  if (!powerResumePrefsReady) return false;
  const int totalTarget = max(1, targetSets * targetReps);
  int totalDone = (max(1, setIndex) - 1) * targetReps + leftTraining.data.currentCount;
  totalDone = constrain(totalDone, 0, totalTarget);
  if (totalDone >= totalTarget) {
    powerResumeClear("SESSION_ALREADY_COMPLETE");
    return true;
  }

  PowerResumeCheckpoint cp;
  memset(&cp, 0, sizeof(cp));
  cp.magic = POWER_RESUME_MAGIC;
  cp.version = POWER_RESUME_VERSION;
  cp.size = sizeof(PowerResumeCheckpoint);
  cp.active = 1;
  cp.exercise = (uint8_t)constrain(exerciseIndex, 0, (int)RM_ACTION_COUNT - 1);
  cp.mode = (uint8_t)constrain(modeIndex, 0, 2);
  cp.targetSets = (int16_t)targetSets;
  cp.targetReps = (int16_t)targetReps;
  cp.targetAngle = (int16_t)targetAngle;
  cp.validAngle = (int16_t)validAngle;
  cp.returnAngle = (int16_t)returnAngle;
  cp.restSec = (int16_t)restSec;
  cp.totalCompletedSlots = (int16_t)totalDone;
  cp.assessment = v4Assessment;
  cp.checksum = powerResumeChecksum(cp);

  const size_t written = powerResumePrefs.putBytes(POWER_RESUME_KEY, &cp, sizeof(cp));
  if (written != sizeof(cp)) {
    Serial.printf("POWER_RESUME_SAVE_FAIL reason=%s wrote=%u expected=%u\n",
                  reason ? reason : "UNKNOWN", (unsigned)written, (unsigned)sizeof(cp));
    return false;
  }
  powerResumeCheckpoint = cp;
  powerResumeAvailable = true;
  Serial.printf("POWER_RESUME_SAVED reason=%s completed=%d/%d set=%d count=%d\n",
                reason ? reason : "UNKNOWN", totalDone, totalTarget, setIndex, leftTraining.data.currentCount);
  return true;
}

static void powerResumeForceSavedPlan() {
  if (!powerResumeAvailable) return;
  const PowerResumeCheckpoint &cp = powerResumeCheckpoint;
  exerciseIndex = cp.exercise;
  modeIndex = cp.mode;
  targetSets = cp.targetSets;
  targetReps = cp.targetReps;
  targetAngle = cp.targetAngle;
  validAngle = cp.validAngle;
  returnAngle = cp.returnAngle;
  restSec = cp.restSec;
  applyTrainingParamsToLogic();
}

static bool powerResumeApplyAfterFreshCalibration() {
  if (!powerResumeApplyPending || !powerResumeAvailable) return false;
  const PowerResumeCheckpoint &cp = powerResumeCheckpoint;
  if (!powerResumeCheckpointSane(cp)) {
    powerResumeClear("APPLY_INVALID");
    return false;
  }

  powerResumeForceSavedPlan();
  const int totalTarget = targetSets * targetReps;
  const int totalDone = constrain((int)cp.totalCompletedSlots, 0, totalTarget - 1);
  const int restoredSet = totalDone / targetReps + 1;
  const int restoredCount = totalDone % targetReps;

  setIndex = constrain(restoredSet, 1, targetSets);
  leftTraining.data.currentCount = restoredCount;
  leftTraining.data.state = RUNNING;
  leftTraining.data.retryPending = false; // an interrupted/retry slot restarts cleanly
  leftTraining.data.lastSlotCompleted = false;
  leftTraining.data.lastAttemptPassed = false;
  leftTraining.data.readyForRep = true;
  rightTraining.data.state = IDLE;

  // Preserve already-finished-session statistics when the checkpoint is internally
  // consistent.  The current in-progress slot is always restarted from scratch.
  v4Assessment = cp.assessment;
  if (v4Assessment.completedSlots != totalDone || !v4Assessment.accountingOk()) {
    v4Assessment.reset(millis());
    v4Assessment.completedSlots = totalDone;
    v4Assessment.passedSlots = totalDone; // conservative fallback for display continuity
    v4Assessment.totalAttempts = totalDone;
  }
  v4Assessment.sessionStartMs = millis();

  powerResumeApplyPending = false;
  powerResumeAppliedThisBoot = true;
  screenResumeNoticePending = true;
  screenResumeNoticeShown = false;
  screenResumeNoticeUntilMs = 0;
  syncUiDataFromTraining();
  powerResumeSave("RESUME_APPLIED");
  Serial.printf("POWER_RESUME_APPLIED completed=%d/%d -> set=%d/%d count=%d/%d. Current unfinished rep is repeated.\n",
                totalDone, totalTarget, setIndex, targetSets, restoredCount, targetReps);
  if (logger.isActive()) {
    char marker[72];
    snprintf(marker, sizeof(marker), "POWER_RESUME_APPLIED_TOTAL_%d_SET_%d_COUNT_%d", totalDone, setIndex, restoredCount);
    logger.logMarker(marker);
  }
  return true;
}

// =====================================================
// 文本
// =====================================================

const char *pageName(int p) {
  switch (p) {
    case PAGE_HOME: return "首页";
    case PAGE_BODY_SELECT: return "选择部位";
    case PAGE_EXERCISE_SELECT: return "选择动作";
    case PAGE_MODE_SELECT: return "选择模式";
    case PAGE_PARAM_SET: return "训练参数";
    case PAGE_CALIBRATION: return "佩戴校准";
    case PAGE_TRAINING: return "训练中";
    case PAGE_REST: return "休息中";
    case PAGE_RESULT: return "训练结果";
    default: return "未知";
  }
}

const char *bodyName() { return rehabV5ActionUsesUpperPair(exerciseIndex) ? "上肢 A+B+E" : "下肢 C+D+E"; }
static const char *bodyCode() { return rehabV5ActionUsesUpperPair(exerciseIndex) ? "upper_ab_torso_e" : "lower_cd_torso_e"; }
static const char *bodyCodeUpper() { return rehabV5ActionUsesUpperPair(exerciseIndex) ? "UPPER_AB_TORSO_E" : "LOWER_CD_TORSO_E"; }
static bool bilateralMode() { return false; }
static bool leftSideActive() { return true; }
static bool rightSideActive() { return false; }

const char *exerciseName() { return rehabV5Action(exerciseIndex).nameZh; }

static const char *exerciseCode() { return rehabV5Action(exerciseIndex).code; }
static int selectedFixedSlot() { return rehabV5Action(exerciseIndex).fixedSlot; }
static int selectedMovingSlot() { return rehabV5Action(exerciseIndex).movingSlot; }
static float currentUpperThresholdDeg() { return rehabV5Action(exerciseIndex).upperHardDeg; }
static float currentPlaneThresholdDeg() { return rehabV5Action(exerciseIndex).planeHardDeg; }
static float currentTorsoThresholdDeg() { return rehabV5Action(exerciseIndex).torsoHardDeg; }
static float currentUpperSoftDeg() { return max(10.0f, currentUpperThresholdDeg() - 10.0f); }
static float currentPlaneSoftDeg() { return max(10.0f, currentPlaneThresholdDeg() - 10.0f); }
static float currentTorsoSoftDeg() { return max(8.0f, currentTorsoThresholdDeg() - 15.0f); }
static const char *targetMetricName() { return rehabV5Action(exerciseIndex).metricZh; }
static void selectActiveImuPair() {
  wt901K11SelectPair(selectedFixedSlot(), selectedMovingSlot());
}

static bool selectedPairReceiving() {
  const RehabV5ActionProfile &p = rehabV5Action(exerciseIndex);
  return wt901SlotReceiving(p.fixedSlot) && wt901SlotReceiving(p.movingSlot) && wt901SlotReceiving(p.torsoSlot);
}

static void applySelectedActionDefaults() {
  const RehabV5ActionProfile &p = rehabV5Action(exerciseIndex);
  targetSets = p.defaultSets;
  targetReps = p.defaultReps;
  targetAngle = p.defaultTargetDeg;
  validAngle = p.validDeg;
  returnAngle = p.returnDeg;
  productOrchestrator.selectExercise(static_cast<rehab::ExerciseId>(rehabV5NormalizeActionIndex(exerciseIndex)));
  Serial.printf("ACTION_PROFILE_APPLIED: index=%d code=%s sets=%d reps=%d target=%d valid=%d return=%d pair=%d/%d torso=%d\n",
    exerciseIndex, p.code, targetSets, targetReps, targetAngle, validAngle, returnAngle, p.fixedSlot, p.movingSlot, p.torsoSlot);
}

static void updateVoiceSensorHealth() {
  const bool monitored = systemReady &&
    (page == PAGE_CALIBRATION || page == PAGE_TRAINING || page == PAGE_REST);
  if (!monitored) {
    voiceSensorMonitoringActive = false;
    return;
  }

  const bool ready = selectedPairReceiving();
  if (!voiceSensorMonitoringActive) {
    voiceSensorMonitoringActive = true;
    voiceSensorsPreviouslyReady = ready;
    if (!ready) queueVoiceEvent("SENSOR_ERROR", rehabVoice.queueSensorError());
    return;
  }

  if (voiceSensorsPreviouslyReady && !ready) {
    queueVoiceEvent("SENSOR_ERROR", rehabVoice.queueSensorError());
  }
  voiceSensorsPreviouslyReady = ready;
}

static bool selectedBodyFrameReady() { return bodyFrameReady(0); }
static bool selectedK11Ready() { return wt901K11Ready(); }
static bool selectedK11TrainingReady() { return wt901K11Ready() && k11MotionCalibrationAccepted; }
static bool selectedK11Failed() { return wt901K11Failed(); }
static bool selectedK11CalibrationActive() { return wt901K11CalibrationActive(); }
static void selectedK11SetPaused(bool p) { wt901K11SetPaused(p); }
static void selectedK11RezeroFormal() { wt901K11RezeroFormal(); }

static bool exerciseStartPoseValidForSide(int side, bool verbose) {
  (void)side;
  if (!bodyFrameReady(0)) { if (verbose) Serial.println("START_POSE_INVALID: torso reference not calibrated."); return false; }
  if (!selectedPairReceiving()) { if (verbose) Serial.println("START_POSE_INVALID: selected IMU chain incomplete."); return false; }

  // Eight-action start-pose gate. Upper-limb actions use the calibrated A/B body
  // frame; lower-limb functional actions use C/D availability plus torso posture.
  // The dedicated motion-engine detector performs the detailed action-specific
  // validation after this coarse safety gate releases the training state machine.
  const float upperNeutralDelta = torsoRefUpperNeutralElevationDeg();
  const float foreNeutralDelta  = torsoRefForeNeutralElevationDeg();
  const float torsoTilt = torsoRefTorsoTiltDeviationDeg();
  BFSemanticVec upper, fore;
  const bool semanticReady = bodyFrameSemanticVector(0, 0, upper) && bodyFrameSemanticVector(0, 1, fore);
  const float upperElev = semanticReady ? bodyFrameElevationFromDownDeg(upper) : 999.0f;
  const float segmentAngle = semanticReady ? bodyFrameSemanticAngleDeg(upper, fore) : 999.0f;

  bool ok = false;
  switch (exerciseIndex) {
    case RM_ACTION_DUMBBELL_CURL:
      ok = isfinite(upperNeutralDelta) && isfinite(foreNeutralDelta) && upperNeutralDelta <= 30.0f && foreNeutralDelta <= 35.0f;
      break;
    case RM_ACTION_TRICEPS_EXTENSION:
      ok = semanticReady && upperElev >= 100.0f && segmentAngle >= 55.0f;
      break;
    case RM_ACTION_SCAPTION_RAISE:
      ok = isfinite(upperNeutralDelta) && upperNeutralDelta <= 35.0f && isfinite(torsoTilt) && torsoTilt <= 20.0f;
      break;
    case RM_ACTION_WALL_CRAWL:
      ok = isfinite(upperNeutralDelta) && upperNeutralDelta <= 45.0f && isfinite(torsoTilt) && torsoTilt <= 20.0f;
      break;
    case RM_ACTION_KNEE_FLEX_EXTEND:
      ok = isfinite(torsoTilt) && torsoTilt <= 25.0f;
      break;
    case RM_ACTION_SIT_TO_STAND:
      ok = isfinite(torsoTilt) && torsoTilt <= 35.0f;
      break;
    case RM_ACTION_BOX_SQUAT:
      ok = isfinite(torsoTilt) && torsoTilt <= 30.0f;
      break;
    case RM_ACTION_STEP_UP:
      ok = isfinite(torsoTilt) && torsoTilt <= 30.0f;
      break;
    default:
      ok = false;
      break;
  }
  if (verbose) Serial.printf("START_POSE_CHECK_8ACTION: ex=%d code=%s neutralA=%.1f neutralB=%.1f upperElev=%.1f segment=%.1f torso=%.1f result=%s\n",
    exerciseIndex+1, exerciseCode(), upperNeutralDelta, foreNeutralDelta, upperElev, segmentAngle, torsoTilt, ok?"PASS":"WAIT");
  return ok;
}

static bool exerciseStartPoseValid(bool verbose) {
  return exerciseStartPoseValidForSide(0,verbose);
}

static const char *exerciseStartPoseInstruction() {
  static const char *kText[RM_ACTION_COUNT] = {
    "手臂自然下垂伸直，保持躯干稳定",
    "上臂举到耳旁，屈肘保持头后起始位",
    "双肩放松，训练臂自然下垂",
    "面向墙站稳，训练手置于墙面低位",
    "坐姿/站姿稳定，膝关节回到起始角",
    "坐稳椅面，双脚踩实地面",
    "站在训练箱前，双脚与肩同宽",
    "站在台阶前，训练侧脚准备上台阶"
  };
  return kText[rehabV5NormalizeActionIndex(exerciseIndex)];
}

static const char *exerciseCalibrationMotionInstruction() {
  static const char *kText[RM_ACTION_COUNT] = {
    "缓慢完成一次屈肘并返回，学习功能性屈伸轴",
    "缓慢完成一次伸肘并返回，学习功能性伸展轴",
    "沿肩胛平面抬手并返回，建立动作平面",
    "沿墙向上爬手后返回，建立抬举范围",
    "缓慢屈膝再伸膝，建立膝关节功能轴",
    "完成一次坐到站再坐下，建立功能动作模板",
    "完成一次箱式下蹲再站起，建立下蹲模板",
    "完成一次上台阶再返回，建立踩踏模板"
  };
  return kText[rehabV5NormalizeActionIndex(exerciseIndex)];
}

static const char *exerciseReturnInstruction() {
  static const char *kText[RM_ACTION_COUNT] = {
    "回到自然下垂伸直位",
    "回到头上屈肘起始位",
    "回到手臂自然下垂位",
    "回到墙面低位起始点",
    "回到膝关节起始角",
    "回到稳定坐姿",
    "回到直立站姿",
    "回到台阶前准备位"
  };
  return kText[rehabV5NormalizeActionIndex(exerciseIndex)];
}

static const char *exerciseLiveMetricLabel() {
  static const char *kText[RM_ACTION_COUNT] = {
    "屈肘幅度", "伸肘幅度", "肩胛抬举", "爬墙高度",
    "膝屈伸幅度", "起立幅度", "下蹲幅度", "踩踏幅度"
  };
  return kText[rehabV5NormalizeActionIndex(exerciseIndex)];
}

static const char *exerciseCalibrationCode() {
  static const char *kCode[RM_ACTION_COUNT] = {
    "CAL_CURL_AXIS", "CAL_TRICEPS_AXIS", "CAL_SCAPTION_PLANE", "CAL_WALL_CRAWL_RANGE",
    "CAL_KNEE_AXIS", "CAL_SIT_STAND_TEMPLATE", "CAL_BOX_SQUAT_TEMPLATE", "CAL_STEP_UP_TEMPLATE"
  };
  return kCode[rehabV5NormalizeActionIndex(exerciseIndex)];
}

static const char *exerciseQualityProfileCode() {
  static const char *kCode[RM_ACTION_COUNT] = {
    "CURL_ROM_PLANE_COMP_TORSO_TEMPO",
    "TRICEPS_ROM_PLANE_COMP_TORSO_TEMPO",
    "SCAPTION_ROM_PLANE_SHRUG_TORSO_STABILITY",
    "WALL_CRAWL_ROM_PLANE_TORSO_SMOOTHNESS",
    "KNEE_ROM_PLANE_TORSO_STABILITY_TEMPO",
    "SIT_STAND_ROM_LEAN_ASYMMETRY_STABILITY",
    "BOX_SQUAT_ROM_LEAN_ASYMMETRY_TEMPO",
    "STEP_UP_ROM_TORSO_ASYMMETRY_CADENCE"
  };
  return kCode[rehabV5NormalizeActionIndex(exerciseIndex)];
}


const char *modeName() {
  switch (modeIndex) {
    case 0: return "标准模式";
    case 1: return "游戏模式";
    case 2: return "自定义模式";
    default: return "标准模式";
  }
}

const TrainingData &displayTrainingData() { return bodyIndex==1 ? rightTraining.data : leftTraining.data; }

const char *planeSourceTextCn(PlaneDeviationSource src) {
  switch(src) {
    case PLANE_SOURCE_BODY_AZIMUTH: return "方向积分仅作诊断";
    case PLANE_SOURCE_UPPER_SIDE: return "上臂侧向抬起";
    case PLANE_SOURCE_FOREARM_QUAT_PLANE: return "前臂轨迹偏离";
    case PLANE_SOURCE_HINGE_OFF_AXIS: return "肘关节离轴运动";
    default: return "动作平面偏离";
  }
}

const char *qualityText() {
  const TrainingData &d=displayTrainingData();
  if(d.completedMotionCount<=0)return "准备";
  switch(d.lastRepQuality){
    case REP_QUALITY_BASELINE:return "基准"; case REP_QUALITY_GOOD:return "标准完成"; case REP_QUALITY_ROM_LOW:return "幅度不足";
    case REP_QUALITY_UPPER_ARM_EXCESS:return "上臂代偿"; case REP_QUALITY_PLANE_DEVIATION:return "动作平面偏离";
    case REP_QUALITY_TORSO_COMPENSATION:return "躯干代偿"; case REP_QUALITY_MOVEMENT_UNSTABLE:return "动作不稳";
    case REP_QUALITY_MULTI_ISSUE:return "多项偏差"; default:return "准备";
  }
}

uint16_t qualityColor() {
  const TrainingData &d=displayTrainingData();
  switch(d.lastRepQuality){
    case REP_QUALITY_GOOD:return C_OK; case REP_QUALITY_BASELINE: case REP_QUALITY_NONE:return C_ACCENT;
    case REP_QUALITY_ROM_LOW: case REP_QUALITY_UPPER_ARM_EXCESS: case REP_QUALITY_PLANE_DEVIATION:
    case REP_QUALITY_TORSO_COMPENSATION: case REP_QUALITY_MOVEMENT_UNSTABLE:return C_WARN;
    case REP_QUALITY_MULTI_ISSUE:return C_DANGER; default:return C_MUTED;
  }
}

void printStatusToSerial() {
  Serial.print("页面="); Serial.print(pageName(page));
  Serial.print(" | 部位="); Serial.print(bodyName());
  Serial.print(" | 动作="); Serial.print(exerciseName());
  Serial.print(" | 模式="); Serial.print(modeName());
  Serial.print(" | 组数="); Serial.print(targetSets);
  Serial.print(" | 次数="); Serial.print(targetReps);
  Serial.print(" | 目标角度="); Serial.print(targetAngle);
  Serial.print(" | 休息="); Serial.println(restSec);
}

// =====================================================
// 绘图基础
// =====================================================

void fillRound(int x, int y, int w, int h, int r, uint16_t color) {
  tft.fillRoundRect(x, y, w, h, r, color);
}

void strokeRound(int x, int y, int w, int h, int r, uint16_t color) {
  tft.drawRoundRect(x, y, w, h, r, color);
}

void drawHeader(const char *title, int step) {
  tft.fillRect(0, 0, SCREEN_W, 54, C_PANEL_2);
  tft.fillRect(0, 0, 8, 54, C_ACCENT);
  drawCnText(18, 14, title, C_TEXT, 1);
  drawCnText(350, 10, "真实数据版", C_MUTED, 1);
  char buf[24];
  snprintf(buf, sizeof(buf), "步骤 %d/8", step);
  drawCnText(350, 31, buf, C_MUTED, 1);
}

void drawFooter(const char *hint) {
  tft.fillRect(0, 292, SCREEN_W, 28, C_PANEL_2);
  drawCnText(14, 300, hint, C_MUTED, 1);
}

void drawProgressBar(int step) {
  const int startX = 38;
  const int y = 72;
  const int gap = 55;
  tft.drawLine(startX, y, startX + gap * 7, y, C_LINE);
  for (int i = 0; i < 8; ++i) {
    int x = startX + gap * i;
    uint16_t color = (i + 1 < step) ? C_OK : ((i + 1 == step) ? C_ACCENT : C_LINE);
    tft.fillCircle(x, y, 7, color);
    tft.drawCircle(x, y, 8, color);
  }
}

void drawOptionCard(int x, int y, int w, int h, const char *label, const char *sub, bool selected, uint16_t accent) {
  uint16_t fill = selected ? C_PANEL_2 : C_PANEL;
  uint16_t line = selected ? accent : C_LINE;
  fillRound(x, y, w, h, 12, fill);
  strokeRound(x, y, w, h, 12, line);
  if (selected) tft.fillRoundRect(x + 8, y + 8, 8, h - 16, 4, accent);
  drawCnText(x + 26, y + 18, label, selected ? accent : C_TEXT, 1);
  if (sub && sub[0]) drawCnText(x + 26, y + 50, sub, C_MUTED, 1);
}

void drawInfoRow(int y, const char *name, const char *value, bool selected) {
  uint16_t fill = selected ? C_PANEL_2 : C_PANEL;
  uint16_t line = selected ? C_ACCENT : C_LINE;
  fillRound(38, y, 404, 42, 8, fill);
  strokeRound(38, y, 404, 42, 8, line);
  drawCnText(56, y + 13, name, C_MUTED, 1);
  drawCnText(286, y + 13, value, selected ? C_ACCENT : C_TEXT, 1);
}

void drawMetricCard(int x, int y, int w, int h, const char *label, float value, const char *unit, uint16_t color) {
  fillRound(x, y, w, h, 10, C_PANEL);
  strokeRound(x, y, w, h, 10, C_LINE);
  drawCnText(x + 12, y + 9, label, C_MUTED, 1);
  char buf[24];
  snprintf(buf, sizeof(buf), "%.1f%s", value, unit);
  drawCnText(x + 12, y + 40, buf, color, 1);
}

void drawMetricCardInt(int x, int y, int w, int h, const char *label, int value, int total, uint16_t color) {
  fillRound(x, y, w, h, 10, C_PANEL);
  strokeRound(x, y, w, h, 10, C_LINE);
  drawCnText(x + 12, y + 9, label, C_MUTED, 1);
  char buf[24];
  snprintf(buf, sizeof(buf), "%d/%d", value, total);
  drawCnText(x + 12, y + 40, buf, color, 1);
}

// =====================================================
// 页面绘制
// =====================================================

void drawHome() {
  tft.fillScreen(C_BG);
  drawHeader("首页", 1);
  drawProgressBar(1);
  drawOptionCard(38, 96, 404, 72, "开始训练", "主控盒独立操作", true, C_ACCENT);
  drawCnText(46, 188, "当前设置", C_MUTED, 1);

  fillRound(38, 212, 192, 50, 10, C_PANEL);
  strokeRound(38, 212, 192, 50, 10, C_LINE);
  drawCnText(54, 222, "部位", C_ACCENT, 1);
  drawCnText(54, 242, bodyName(), C_TEXT, 1);

  fillRound(250, 212, 192, 50, 10, C_PANEL);
  strokeRound(250, 212, 192, 50, 10, C_LINE);
  drawCnText(266, 222, "模式", C_PURPLE, 1);
  drawCnText(266, 242, modeName(), C_TEXT, 1);
  drawFooter("o 确认    l/r 选择    b 返回");
}

void drawBodySelect() {
  tft.fillScreen(C_BG);
  drawHeader("传感器配置", 2);
  drawProgressBar(2);
  const bool upper = rehabV5ActionUsesUpperPair(exerciseIndex);
  drawOptionCard(38, 96, 404, 118, upper ? "上肢三节点链" : "下肢三节点链",
                 upper ? "A上臂 + B前臂 + E腰腹参考" : "C大腿 + D小腿 + E腰腹参考", true, C_ACCENT);
  drawCnText(42, 235, upper ? "当前动作自动启用 A/B/E" : "当前动作自动启用 C/D/E", C_MUTED, 1);
  drawFooter("o 继续    b 首页");
}

void drawExerciseSelect() {
  tft.fillScreen(C_BG);
  drawHeader("选择动作 · 8项训练库", 3);
  drawProgressBar(3);
  static const char *shortName[RM_ACTION_COUNT] = {
    "1 哑铃弯举", "2 肱三头伸展", "3 肩胛平面抬手", "4 墙面爬手",
    "5 膝关节屈伸", "6 坐到站", "7 箱式深蹲", "8 台阶踩踏"
  };
  for (int i = 0; i < RM_ACTION_COUNT; ++i) {
    const int col = i % 2;
    const int row = i / 2;
    const int x = 20 + col * 230;
    const int y = 88 + row * 47;
    const bool selected = exerciseIndex == i;
    fillRound(x, y, 214, 39, 8, selected ? C_PANEL_2 : C_PANEL);
    strokeRound(x, y, 214, 39, 8, selected ? (i < 4 ? C_ACCENT : C_PURPLE) : C_LINE);
    if (selected) tft.fillRoundRect(x + 7, y + 7, 6, 25, 3, i < 4 ? C_ACCENT : C_PURPLE);
    drawCnText(x + 20, y + 12, shortName[i], selected ? (i < 4 ? C_ACCENT : C_PURPLE) : C_TEXT, 1);
  }
  drawCnText(28, 278, rehabV5ActionUsesUpperPair(exerciseIndex) ? "传感器链 A/B/E · 上肢" : "传感器链 C/D/E · 下肢", C_MUTED, 1);
  drawFooter("l/r 选择 8动作    o 确认    b 首页");
}

void drawModeSelect() {
  tft.fillScreen(C_BG);
  drawHeader("选择模式", 4);
  drawProgressBar(4);
  drawOptionCard(28, 100, 136, 112, "标准", "预设参数", modeIndex == 0, C_ACCENT);
  drawOptionCard(172, 100, 136, 112, "游戏", "规则相同", modeIndex == 1, C_PURPLE);
  drawOptionCard(316, 100, 136, 112, "自定义", "参数可调", modeIndex == 2, C_OK);
  drawCnText(42, 235, "游戏模式不改变训练算法", C_MUTED, 1);
  drawFooter("l/r 选择    o 确认    b 选择动作");
}

void drawParamSet() {
  tft.fillScreen(C_BG);
  drawHeader("训练参数", 5);
  drawProgressBar(5);
  char v0[20], v1[20], v2[20], v3[20];
  snprintf(v0, sizeof(v0), "%d", targetSets);
  snprintf(v1, sizeof(v1), "%d", targetReps);
  snprintf(v2, sizeof(v2), "%d度", targetAngle);
  snprintf(v3, sizeof(v3), "%d秒", restSec);
  drawInfoRow(92,  "组数",     v0, paramField == 0);
  drawInfoRow(139, "每组次数", v1, paramField == 1);
  drawInfoRow(186, targetMetricName(), v2, paramField == 2);
  drawInfoRow(233, "休息时间", v3, paramField == 3);
  drawFooter("l/r 增减    o 下一项/确认    b 选择模式");
}

void drawCalibrationBlinkOnly() {
  tft.fillRect(38, 238, 404, 46, C_BG);
  if (bodyFramePhase() == BF_OFF) drawCnText(48, 250, "o 开始人体方向校准", blinkOn ? C_WARN : C_MUTED, 1);
  else if (bodyFramePhase() == BF_DOWN_STILL) drawCnText(48, 250, "第1步 坐直 手臂下垂伸直 静止", C_ACCENT, 1);
  else if (bodyFramePhase() == BF_FRONT_RAISES) {
    if (!bodyFrameCal[0].frontArmed) drawCnText(48, 250, "第2步准备 先继续保持手臂下垂", C_ACCENT, 1);
    else if (!bodyFrameCal[0].frontRaiseSeen) drawCnText(48, 250, "现在开始 整条手臂伸直向正前方抬", C_WARN, 1);
    else drawCnText(48, 250, "已检测到抬臂 保持正前方姿势", C_WARN, 1);
  }
  else if (bodyFramePhase() == BF_READY) drawCnText(48, 250, "人体前方/标准平面完成 o 继续", C_OK, 1);
  else drawCnText(48, 250, "校准失败 o 重试", C_DANGER, 1);
}

void drawCalibration() {
  tft.fillScreen(C_BG);
  drawHeader("人体方向校准", 6);
  drawProgressBar(6);
  drawOptionCard(38, 88, 404, 62, "A左上+B左下+E腰腹", "A/B管左侧；E只管躯干", true, C_ACCENT);
  if (bodyFramePhase() == BF_DOWN_STILL || bodyFramePhase() == BF_OFF) {
    drawOptionCard(38, 158, 404, 70, "第1步 中立位", "坐直 + 手臂自然下垂伸直 + 静止1.5秒", true, C_OK);
  } else if (bodyFramePhase() == BF_FRONT_RAISES) {
    if (!bodyFrameCal[0].frontArmed)
      drawOptionCard(38, 158, 404, 70, "第2步 准备", "先别动，继续保持手臂自然下垂，等待系统就绪", true, C_ACCENT);
    else
      drawOptionCard(38, 158, 404, 70, "第2步 正前方", "A/B整条手臂伸直向正前方抬约60度并停稳", true, C_WARN);
  } else {
    drawOptionCard(38, 158, 404, 70, "标准平面已建立", "人体前方 + 重力上下 = 屈肘矢状面", true, C_OK);
  }
  drawCalibrationBlinkOnly();
  drawFooter("o 校准/开始    b 返回");
}

void drawTrainingDynamic() {
  if (a2PreCalPhase == A2_PRECAL_PREPARE) {
    drawMetricCard(20, 88, 210, 72, "起始姿势", exerciseStartPoseValid(false) ? 100.0f : 0.0f, "%", C_ACCENT);
    drawMetricCard(250, 88, 210, 72, "静止计时", 0.0f, "%", C_PURPLE);
    drawMetricCard(20, 174, 210, 72, "基线采样", 0.0f, "%", C_OK);
    drawMetricCard(250, 174, 210, 72, "当前阶段", 1.0f, "POSE", C_OK);
    tft.fillRect(20, 254, 440, 30, C_BG);
    drawCnText(24, 262, exerciseStartPoseInstruction(), C_ACCENT, 1);
    return;
  }
  if (a2PreCalPhase == A2_PRECAL_STILL_CHECK) {
    float ga = a2AbsMaxGyro(wt901Slots[selectedFixedSlot()].latest);
    float gb = a2AbsMaxGyro(wt901Slots[selectedMovingSlot()].latest);
    drawMetricCard(20, 88, 210, 72, "稳定进度", a2StartStillProgress()*100.0f, "%", C_ACCENT);
    drawMetricCard(250, 88, 210, 72, "手臂稳定阈值", A2_START_ARM_STILL_MAX_GYRO_DEG_S, "d/s", C_PURPLE);
    drawMetricCard(20, 174, 210, 72, "A GYRO", ga, "d/s", C_OK);
    drawMetricCard(250, 174, 210, 72, "B GYRO", gb, "d/s", C_OK);
    tft.fillRect(20, 254, 440, 30, C_BG);
    drawCnText(24, 262, exerciseStartPoseInstruction(), C_ACCENT, 1);
    return;
  }
  if (wt901K11BiasActive()) {
    drawMetricCard(20, 88, 210, 72, "静止进度", wt901K11BiasProgress()*100.0f, "%", C_ACCENT);
    drawMetricCardInt(250, 88, 210, 72, "A/B样本", min((int)wt901K11BiasSamplesA(), (int)wt901K11BiasSamplesB()), (int)K11_BIAS_MIN_SAMPLES, C_PURPLE);
    drawMetricCard(20, 174, 210, 72, "A GYRO", sqrtf(wt901Slots[selectedFixedSlot()].latest.gx*wt901Slots[selectedFixedSlot()].latest.gx + wt901Slots[selectedFixedSlot()].latest.gy*wt901Slots[selectedFixedSlot()].latest.gy + wt901Slots[selectedFixedSlot()].latest.gz*wt901Slots[selectedFixedSlot()].latest.gz), "d/s", C_OK);
    drawMetricCard(250, 174, 210, 72, "B GYRO", sqrtf(wt901Slots[selectedMovingSlot()].latest.gx*wt901Slots[selectedMovingSlot()].latest.gx + wt901Slots[selectedMovingSlot()].latest.gy*wt901Slots[selectedMovingSlot()].latest.gy + wt901Slots[selectedMovingSlot()].latest.gz*wt901Slots[selectedMovingSlot()].latest.gz), "d/s", C_OK);
    tft.fillRect(20, 254, 440, 30, C_BG);
    drawCnText(30, 262, exerciseStartPoseInstruction(), C_ACCENT, 1);
    return;
  }
  if (wt901K11AxisFlexActive()) {
    drawMetricCard(20, 88, 210, 72, "K11校准角", wt901K11CalAngleDeg(), "度", C_ACCENT);
    drawMetricCard(250, 88, 210, 72, "平面偏离", v449BodyPlaneDeviationDeg(), "度", C_PURPLE);
    drawMetricCard(20, 174, 210, 72, "上臂代偿", torsoRefUpperDeviationDeg(), "度", C_OK);
    drawMetricCard(250, 174, 210, 72, "躯干倾斜", torsoRefTorsoTiltDeviationDeg(), "度", C_OK);
    tft.fillRect(20, 254, 440, 30, C_BG);
    drawCnText(24, 262, exerciseCalibrationMotionInstruction(), C_ACCENT, 1);
    return;
  }
  if (wt901K11AxisReturnActive()) {
    drawMetricCard(20, 88, 210, 72, "回位角", wt901K11CalAngleDeg(), "度", C_ACCENT);
    drawMetricCard(250, 88, 210, 72, "平面最大", k11CalObservedPlaneMaxDeg, "度", C_PURPLE);
    drawMetricCard(20, 174, 210, 72, "上臂最大", k11CalObservedUpperMaxDeg, "度", C_OK);
    drawMetricCard(250, 174, 210, 72, "躯干最大", k11CalObservedTorsoMaxDeg, "度", C_OK);
    tft.fillRect(20, 254, 440, 30, C_BG);
    drawCnText(24, 262, exerciseReturnInstruction(), C_ACCENT, 1);
    return;
  }
  if (wt901K11Failed()) {
    fillRound(20, 96, 440, 140, 12, C_PANEL);
    strokeRound(20, 96, 440, 140, 12, C_DANGER);
    drawCnText(45, 122, "陀螺校准失败", C_DANGER, 1);
    drawCnText(45, 158, "请保持A/B同方向佩戴后重试", C_MUTED, 1);
    return;
  }

  // Formal training. Bilateral mode deliberately shows BOTH sides; neither is hidden behind an average.
  tft.fillRect(20, 82, 440, 204, C_BG);
  if(bilateralMode()) {
    drawMetricCard(20, 88, 210, 72, exerciseLiveMetricLabel(), leftAngle, "度", C_ACCENT);
    drawMetricCard(250, 88, 210, 72, exerciseLiveMetricLabel(), rightAngle, "度", C_PURPLE);
    drawMetricCardInt(20,174,210,72,"左侧进度",leftCount,targetReps,C_OK);
    drawMetricCardInt(250,174,210,72,"右侧进度",rightCount,targetReps,C_OK);
    char s[150];
    if(leftTraining.data.retryPending||rightTraining.data.retryPending)
      snprintf(s,sizeof(s),"重做：%s%s",leftTraining.data.retryPending?"左":"",rightTraining.data.retryPending?"右":"");
    else if(bilateralLeftSlotWaiting||bilateralRightSlotWaiting)
      snprintf(s,sizeof(s),"当前位次等待另一侧完成");
    else snprintf(s,sizeof(s),"双手同时完成；左右侧分别判定");
    drawCnText(24,262,s,(leftTraining.data.retryPending||rightTraining.data.retryPending)?C_WARN:C_MUTED,1);
  } else {
    TrainingLogic &logic = bodyIndex==0 ? leftTraining : rightTraining;
    float angle=bodyIndex==0?leftAngle:rightAngle;
    float upperNow=torsoRefUpperDeviationDeg();
    float planeNow=v449FormalPlaneDeviationDeg();
    float torsoNow=torsoRefTorsoTiltDeviationDeg();
    int count=bodyIndex==0?leftCount:rightCount;
    drawMetricCard(20,88,210,72,exerciseLiveMetricLabel(),angle,"度",C_ACCENT);
    drawMetricCard(250,88,210,72,"动作平面偏离",planeNow,"度",C_PURPLE);
    drawMetricCardInt(20,174,210,72,"训练进度",count,targetReps,C_OK);
    drawMetricCard(250,174,210,72,"上臂代偿",upperNow,"度",C_MUTED);
    char status[160];
    if(logic.data.retryPending) {
      switch(logic.data.lastRepQuality) {
        case REP_QUALITY_ROM_LOW: snprintf(status,sizeof(status),"幅度不足 请重做1次"); break;
        case REP_QUALITY_PLANE_DEVIATION: snprintf(status,sizeof(status),"%s 请重做1次",planeSourceTextCn(logic.data.lastPlaneSource)); break;
        case REP_QUALITY_UPPER_ARM_EXCESS: snprintf(status,sizeof(status),"上臂代偿过大 请重做1次"); break;
        case REP_QUALITY_TORSO_COMPENSATION: snprintf(status,sizeof(status),"躯干代偿过大 请重做1次"); break;
        default: snprintf(status,sizeof(status),"存在多项持续偏差 请重做1次"); break;
      }
    } else {
      snprintf(status,sizeof(status),"提醒/失败 平面%.0f/%.0f 上臂%.0f/%.0f 躯干%.0f/%.0f",
        currentPlaneSoftDeg(),currentPlaneThresholdDeg(),currentUpperSoftDeg(),currentUpperThresholdDeg(),currentTorsoSoftDeg(),currentTorsoThresholdDeg());
    }
    drawCnText(24,262,status,logic.data.retryPending?C_WARN:C_MUTED,1);
  }
}


void drawTraining() {
  tft.fillScreen(C_BG);
  drawHeader(trainingPaused ? "已暂停" : "训练中", 7);
  drawProgressBar(7);
  drawTrainingDynamic();
  if (a2PreCalibrationActive()) drawFooter("免手准备/稳定检测中    b/x 可结束");
  else drawFooter(selectedK11TrainingReady() ? "o/p 暂停/继续    b/x 结束训练" : "K11轴标定/验收中    b/x 结束");
}

void drawRestDynamic() {
  tft.fillRect(54, 132, 372, 118, C_PANEL);
  char b1[64];
  char b2[96];
  char b3[96];
  snprintf(b1, sizeof(b1), "第%d/%d组完成", setIndex, targetSets);
  if (restRemainingSec > 0) {
    snprintf(b2, sizeof(b2), "休息%d秒 手臂可以放下", restRemainingSec);
    drawCnCenter(54, 142, 372, b1, C_OK, 1);
    drawCnCenter(54, 178, 372, b2, C_WARN, 1);
    drawCnCenter(54, 220, 372, exerciseReturnInstruction(), C_MUTED, 1);
  } else if (a2GroupReacquireActive) {
    int holdPct = 0;
    if (a2GroupReacquireHoldStartMs != 0) {
      unsigned long held = millis() - a2GroupReacquireHoldStartMs;
      holdPct = (int)min(100UL, (held * 100UL) / A2_GROUP_REACQUIRE_HOLD_MS);
    }
    snprintf(b2, sizeof(b2), "%s", exerciseReturnInstruction());
    snprintf(b3, sizeof(b3), "姿势匹配后稳定1.5秒 自动开始 %d%%", holdPct);
    drawCnCenter(54, 142, 372, b1, C_OK, 1);
    drawCnCenter(54, 178, 372, b2, C_WARN, 1);
    drawCnCenter(54, 220, 372, b3, C_ACCENT, 1);
  } else {
    snprintf(b2, sizeof(b2), "休息结束 可准备下一组");
    drawCnCenter(54, 142, 372, b1, C_OK, 1);
    drawCnCenter(54, 178, 372, b2, C_WARN, 1);
    drawCnCenter(54, 220, 372, "按确认后回到起始位 系统自动检测", C_ACCENT, 1);
  }
}
void drawRest() {
  tft.fillScreen(C_BG);
  drawHeader(a2GroupReacquireActive ? "起始位检测" : "休息中", 7);
  drawProgressBar(7);
  fillRound(38, 96, 404, 180, 14, C_PANEL);
  strokeRound(38, 96, 404, 180, 14, C_OK);
  drawRestDynamic();
  drawFooter(a2GroupReacquireActive ? "回到起始位并保持    b/x结束" : "倒计时结束后 o准备下一组    b/x结束");
}

void drawResult() {
  tft.fillScreen(C_BG); drawHeader("训练结果",8); drawProgressBar(8);
  fillRound(28,92,424,192,14,C_PANEL); strokeRound(28,92,424,192,14,C_OK);
  const int targetTotal=max(1,targetSets*targetReps);
  char b1[96],b2[96],b3[96],b4[96],b5[96];
  if(bilateralMode()){
    if(resultView==0){
      snprintf(b1,sizeof(b1),"左：%d/%d 达标%.1f%%",v4Assessment.completedSlots,targetTotal,v4Assessment.passRatePct());
      snprintf(b2,sizeof(b2),"右：%d/%d 达标%.1f%%",v4AssessmentRight.completedSlots,targetTotal,v4AssessmentRight.passRatePct());
      snprintf(b3,sizeof(b3),"左 达标/未达标：%d/%d",v4Assessment.passedSlots,v4Assessment.failedSlots);
      snprintf(b4,sizeof(b4),"右 达标/未达标：%d/%d",v4AssessmentRight.passedSlots,v4AssessmentRight.failedSlots);
      snprintf(b5,sizeof(b5),"重做 左%d次 / 右%d次",v4Assessment.retryAttempts,v4AssessmentRight.retryAttempts);
      drawCnText(48,108,"左右臂分别评估",C_OK,1);
    } else {
      snprintf(b1,sizeof(b1),"左 幅度不足/上臂：%d/%d",v4Assessment.finalRomLowSlots,v4Assessment.finalUpperArmExcessSlots);
      snprintf(b2,sizeof(b2),"右 幅度不足/上臂：%d/%d",v4AssessmentRight.finalRomLowSlots,v4AssessmentRight.finalUpperArmExcessSlots);
      snprintf(b3,sizeof(b3),"左 平均达标ROM：%.1f度",v4Assessment.passRomAvgDeg());
      snprintf(b4,sizeof(b4),"右 平均达标ROM：%.1f度",v4AssessmentRight.passRomAvgDeg());
      snprintf(b5,sizeof(b5),"左右数据分别写入同一训练记录");
      drawCnText(48,108,"双侧详细结果",C_ACCENT,1);
    }
  } else {
    const V4SessionAssessment &a=bodyIndex==1?v4AssessmentRight:v4Assessment;
    if(resultView==0){
      snprintf(b1,sizeof(b1),"训练进度：%d/%d",a.completedSlots,targetTotal);
      snprintf(b2,sizeof(b2),"达标 / 未达标：%d / %d",a.passedSlots,a.failedSlots);
      snprintf(b3,sizeof(b3),"动作达标率：%.1f%%",a.passRatePct());
      snprintf(b4,sizeof(b4),"实际尝试：%d次",a.totalAttempts);
      snprintf(b5,sizeof(b5),"额外重做：%d次",a.retryAttempts);
      drawCnText(48,108,"训练已保存",C_OK,1);
    } else {
      snprintf(b1,sizeof(b1),"幅度不足：%d次",a.finalRomLowSlots);
      snprintf(b2,sizeof(b2),"上臂偏离过大：%d次",a.finalUpperArmExcessSlots);
      snprintf(b3,sizeof(b3),"重做后达标：%d/%d",a.recoveredOnRetry,a.retryAttempts);
      if(a.hasPassRomStats()){snprintf(b4,sizeof(b4),"平均达标ROM：%.1f度",a.passRomAvgDeg());snprintf(b5,sizeof(b5),"达标ROM SD：%.1f度",a.passRomSdDeg());}
      else {snprintf(b4,sizeof(b4),"暂无达标ROM数据");snprintf(b5,sizeof(b5),"请查看未达标原因");}
      drawCnText(48,108,"未达标原因 / 重做 / ROM",C_ACCENT,1);
    }
  }
  drawCnText(48,138,b1,C_TEXT,1);drawCnText(48,166,b2,C_TEXT,1);drawCnText(48,194,b3,C_TEXT,1);drawCnText(48,222,b4,C_TEXT,1);drawCnText(48,250,b5,C_TEXT,1);
  drawFooter("l/r 切换结果    o/b 首页");
}

void drawPage() {
  switch (page) {
    case PAGE_HOME: drawHome(); break;
    case PAGE_BODY_SELECT: drawBodySelect(); break;
    case PAGE_EXERCISE_SELECT: drawExerciseSelect(); break;
    case PAGE_MODE_SELECT: drawModeSelect(); break;
    case PAGE_PARAM_SET: drawParamSet(); break;
    case PAGE_CALIBRATION: drawCalibration(); break;
    case PAGE_TRAINING: drawTraining(); break;
    case PAGE_REST: drawRest(); break;
    case PAGE_RESULT: drawResult(); break;
    default: drawHome(); break;
  }
  lastPage = page;
  uiDirty = false;
}

// =====================================================
// v2 P真实 IMU 数据接入
// =====================================================


const char *currentModeCode() { return "upper"; }

const char *currentExerciseCode() { return exerciseCode(); }

const char *repEventFromCountChange(bool leftCountIncreased, bool rightCountIncreased) {
  if (leftCountIncreased && rightCountIncreased) return "both_rep_done";
  if (leftCountIncreased) return "left_rep_done";
  if (rightCountIncreased) return "right_rep_done";
  return "none";
}

void setPendingRepEvent(bool leftCountIncreased, bool rightCountIncreased) {
  const char *newEvent = repEventFromCountChange(leftCountIncreased, rightCountIncreased);
  if (strcmp(newEvent, "none") == 0) return;
  if (strcmp(pendingRepEvent, "none") == 0) {
    pendingRepEvent = newEvent;
  } else if (strcmp(pendingRepEvent, newEvent) != 0) {
    pendingRepEvent = "both_rep_done";
  }
}

void resetGameOutputState() {
  pendingRepEvent = "none";
  leftSpeedDegS = 0.0f;
  rightSpeedDegS = 0.0f;
  speedPrevLeftAngle = 0.0f;
  speedPrevRightAngle = 0.0f;
  speedPrevMs = 0;
  lastJsonTime = 0;
}

void resetSpeedReference(float leftA, float rightA) {
  leftSpeedDegS = 0.0f;
  rightSpeedDegS = 0.0f;
  speedPrevLeftAngle = leftA;
  speedPrevRightAngle = rightA;
  speedPrevMs = millis();
}

float sanitizeSpeed(float rawSpeed, float oldSpeed) {
  if (!isfinite(rawSpeed) || rawSpeed < SPEED_DEAD_ZONE_DEG_S) rawSpeed = 0.0f;
  if (rawSpeed > SPEED_MAX_DEG_S) rawSpeed = SPEED_MAX_DEG_S;
  return oldSpeed * (1.0f - SPEED_FILTER_ALPHA) + rawSpeed * SPEED_FILTER_ALPHA;
}

void updateMotionSpeed(float leftA, float rightA) {
  unsigned long nowMs = millis();
  if (speedPrevMs == 0) {
    resetSpeedReference(leftA, rightA);
    return;
  }
  unsigned long elapsedMs = nowMs - speedPrevMs;
  if (elapsedMs < SPEED_SAMPLE_INTERVAL_MS) return;
  if (elapsedMs > 1200) {
    resetSpeedReference(leftA, rightA);
    return;
  }
  float dt = elapsedMs / 1000.0f;
  float rawLeftSpeed = fabsf((leftA - speedPrevLeftAngle) / dt);
  float rawRightSpeed = fabsf((rightA - speedPrevRightAngle) / dt);
  leftSpeedDegS = sanitizeSpeed(rawLeftSpeed, leftSpeedDegS);
  rightSpeedDegS = sanitizeSpeed(rawRightSpeed, rightSpeedDegS);
  speedPrevLeftAngle = leftA;
  speedPrevRightAngle = rightA;
  speedPrevMs = nowMs;
}

static void updateProductApplicationBridge() {
  if (!systemReady || page != PAGE_TRAINING || integratedCountdownActive || !selectedK11TrainingReady()) return;
  const unsigned long now = millis();
  if (now - productOrchestratorLastMs < 40UL) return;
  productOrchestratorLastMs = now;

  rehab::V5MotionSnapshot snap{};
  snap.timestampMs = now;
  snap.primaryAngleDeg = leftAngle;
  snap.secondaryAngleDeg = rightAngle;
  snap.upperDeviationDeg = torsoRefUpperDeviationDeg();
  snap.planeDeviationDeg = v449FormalPlaneDeviationDeg();
  snap.torsoTiltDeg = torsoRefTorsoTiltDeviationDeg();
  snap.torsoYawRelativeDeg = v449BodyRelativeAzimuthDeg();
  snap.angularSpeedDegS = leftSpeedDegS;
  const float deviationLoad = fabsf(snap.upperDeviationDeg) + fabsf(snap.planeDeviationDeg) + fabsf(snap.torsoTiltDeg);
  snap.stabilityScore = constrain(1.0f - deviationLoad / 180.0f, 0.0f, 1.0f);
  snap.leftRightDifferenceDeg = fabsf(leftAngle - rightAngle);
  snap.verticalExcursionDeg = leftAngle;
  snap.forwardLeanDeg = snap.torsoTiltDeg;
  snap.cadenceRpm = 0.0f;
  snap.imuMask = currentImuMask();

  rehab::RehabSystemOutput out = productOrchestrator.update(snap);
  if (out.runtime.feedback.repCompleted) {
    Serial.printf("PRODUCT_PIPELINE_REP: action=%s accepted=%d score=%.1f sync_queue=%u\n",
      exerciseCode(), out.runtime.feedback.repAccepted ? 1 : 0, out.runtime.feedback.qualityScore,
      (unsigned)productOrchestrator.pendingSyncCount());
    if (logger.isActive()) logger.logMarker(out.runtime.feedback.repAccepted ? "PRODUCT_PIPELINE_REP_ACCEPTED" : "PRODUCT_PIPELINE_REP_REJECTED");
  }
  if (out.emitGameEvent) {
    Serial.printf("PRODUCT_GAME_EVENT: action=%s trigger=1\n", exerciseCode());
  }
  if (out.sessionFinished && !out.reportJson.empty()) {
    Serial.print("PRODUCT_SESSION_REPORT:");
    Serial.println(out.reportJson.c_str());
  }
}

const char *currentTrainModeCode() {
  switch (modeIndex) {
    case 0: return "standard";
    case 1: return "game";
    case 2: return "custom";
    default: return "standard";
  }
}

const char *currentJsonTrainingState(const char *legacyState) {
  if (page == PAGE_REST) return "REST";
  return legacyState ? legacyState : "UNKNOWN";
}

const char *lastRepQualityCode() {
  const TrainingData &d=displayTrainingData();
  if(d.completedMotionCount<=0)return "NONE";
  return repQualityCodeName(d.lastRepQuality);
}

static bool formalQualityEvaluatedNow() {
  return page == PAGE_TRAINING && (leftTraining.data.state == RUNNING || rightTraining.data.state == RUNNING) && !trainingFinishHandled && selectedK11TrainingReady();
}

const char *currentQualityCode() {
  if (page != PAGE_TRAINING) return "NOT_EVALUATED";
  if (!selectedPairReceiving()) return "SIGNAL_LOST";
  if (a2PreCalPhase == A2_PRECAL_PREPARE) return "WAIT_START_POSE";
  if (a2PreCalPhase == A2_PRECAL_STILL_CHECK) return "START_POSE_STILL_CHECK";
  if (wt901K11BiasActive()) return "GYRO_BIAS";
  if (wt901K11AxisFlexActive()) return exerciseCalibrationCode();
  if (wt901K11AxisReturnActive()) return "CAL_RETURN";
  if (wt901K11Failed()) return "GYRO_CAL_FAILED";
  if (wt901K11Ready() && !k11MotionCalibrationAccepted) return "K11_MOTION_VALIDATION";
  if (!selectedK11TrainingReady()) return "WAIT";
  if (!formalQualityEvaluatedNow()) return "NOT_EVALUATED";
  return exerciseQualityProfileCode();
}

const char *currentWarningCode() {
  if (!selectedPairReceiving()) return "imu_signal_lost";
  if (wt901K11Failed()) return "gyro_cal_failed";
  return "none";
}


void outputJsonFrame() {
  MotionOutput out = buildMotionOutput(
    leftTraining.data,
    rightTraining.data,
    gameSeq++,
    millis(),
    pendingRepEvent,
    leftSpeedDegS,
    rightSpeedDegS,
    currentModeCode(),
    currentExerciseCode()
  );

  out.trainingState = currentJsonTrainingState(out.trainingState);
  String json = motionOutputToJson(out);
  if (json.endsWith("}")) json.remove(json.length() - 1);

  json += ",\"body_mode\":\"" + String(currentModeCode()) + "\"";
  json += ",\"train_mode\":\"" + String(currentTrainModeCode()) + "\"";
  json += ",\"set_index\":" + String(setIndex);
  json += ",\"target_sets\":" + String(targetSets);
  json += ",\"rest_remaining_sec\":" + String(page == PAGE_REST ? restRemainingSec : 0);
  json += ",\"target_extension_deg\":" + String(targetAngle);
  json += ",\"attempt_min_extension_deg\":" + String(validAngle);
  json += ",\"attempt_threshold_deg\":" + String(validAngle);
  json += ",\"return_to_start_max_deg\":" + String(returnAngle);
  json += ",\"overall_completion_percent\":" + String(completionPercent);
  json += ",\"quality\":\"" + String(currentQualityCode()) + "\"";
  json += ",\"warning\":\"" + String(currentWarningCode()) + "\"";
  json += ",\"quality_evaluation_state\":\"" + String(formalQualityEvaluatedNow() ? "ACTIVE" : "NOT_EVALUATED") + "\"";
  json += ",\"formal_metrics_valid\":" + String(formalQualityEvaluatedNow() ? "true" : "false");

  {
    json += ",\"training_side\":\"" + String(bodyCode()) + "\"";
    json += ",\"sensor_role_a\":\"left_upper\"";
    json += ",\"sensor_role_b\":\"left_lower\"";
    json += ",\"sensor_role_c\":\"right_upper\"";
    json += ",\"sensor_role_d\":\"right_lower\"";
    json += ",\"sensor_role_e\":\"waist_abdomen_WT901BLE67\"";
    json += ",\"exercise_index\":" + String(exerciseIndex+1);
    json += ",\"torso_reference_ready\":" + String(selectedBodyFrameReady()?"true":"false");
    json += ",\"body_front_ready\":" + String(torsoRefFrontReady()?"true":"false");
    json += ",\"body_front_axis_e\":[" + String(torsoRef.frontAxisTorso.x,4) + "," + String(torsoRef.frontAxisTorso.y,4) + "," + String(torsoRef.frontAxisTorso.z,4) + "]";
    json += ",\"body_side_axis_e\":[" + String(torsoRef.sideAxisTorso.x,4) + "," + String(torsoRef.sideAxisTorso.y,4) + "," + String(torsoRef.sideAxisTorso.z,4) + "]";
    json += ",\"body_front_axis_e_valid\":false";
    json += ",\"arm_reference_uses_e\":true";
    json += ",\"d_role\":\"right_lower_distal_sensor\"";
    json += ",\"e_role\":\"torso_tilt_plus_continuous_differential_vertical_gyro_body_reference\"";
    json += ",\"arm_plane_reference\":\"per_attempt_delta(A_vertical_gyro_heading-E_vertical_gyro_heading); reset after each completed attempt; B/absolute-yaw/hinge diagnostic only\"";
    BFSemanticVec jUpper, jFore;
    if (bodyFrameSemanticVector(bodyIndex,0,jUpper)) {
      json += ",\"upper_body_azimuth_deg\":" + String(bodyFrameAzimuthDeg(jUpper),1);
      json += ",\"upper_elevation_from_down_deg\":" + String(bodyFrameElevationFromDownDeg(jUpper),1);
      json += ",\"upper_sagittal_plane_deviation_deg\":" + String(bodyFrameSagittalPlaneDeviationDeg(jUpper),1);
    }
    if (bodyFrameSemanticVector(bodyIndex,1,jFore)) {
      json += ",\"fore_body_azimuth_deg\":" + String(bodyFrameAzimuthDeg(jFore),1);
      json += ",\"fore_elevation_from_down_deg\":" + String(bodyFrameElevationFromDownDeg(jFore),1);
      json += ",\"fore_sagittal_plane_deviation_deg\":" + String(bodyFrameSagittalPlaneDeviationDeg(jFore),1);
      json += ",\"fore_front_component\":" + String(jFore.front,3);
      json += ",\"fore_side_component\":" + String(jFore.side,3);
    }
    json += ",\"k11_gyro_differential\":true";
    json += ",\"joint_angle_source\":\"per_sensor_semantic_frame_hinge_projection_difference_integral\"";
    json += ",\"extension_excursion_deg\":" + String(wt901K11ElbowAngleDeg(), 1);
    json += ",\"extension_raw_projected_deg\":" + String(wt901K11RawProjectedDeg(), 1);
    json += ",\"k11_phase\":\"" + String(a2EffectiveCalPhaseName()) + "\"";
    json += ",\"k11_cal_active\":" + String((a2PreCalibrationActive() || wt901K11CalibrationActive()) ? "true" : "false");
    json += ",\"k11_training_ready\":" + String(selectedK11TrainingReady() ? "true" : "false");
    json += ",\"k11_motion_calibration_accepted\":" + String(k11MotionCalibrationAccepted ? "true" : "false");
    json += ",\"a2_prepare_remaining_sec\":" + String(a2PrepareRemainingSec());
    json += ",\"a2_start_still_progress_pct\":" + String(a2StartStillProgress()*100.0f, 0);
    json += ",\"a2_precal_no_baseline_sampling\":" + String(a2PreCalibrationActive() ? "true" : "false");
    json += ",\"a2_group_reacquire_active\":" + String(a2GroupReacquireActive ? "true" : "false");
    json += ",\"group_reacquire_rule\":\"same_start_pose_short_breathing_tolerant_window_no_per_rep_gate\"";
    json += ",\"k11_axis_x\":" + String(wt901K11AxisX(), 5);
    json += ",\"k11_axis_y\":" + String(wt901K11AxisY(), 5);
    json += ",\"k11_axis_z\":" + String(wt901K11AxisZ(), 5);
    json += ",\"k11_axis_dominance\":" + String(wt901K11AxisDominance(), 3);
    json += ",\"k11_semantic_frame\":" + String(wt901K11UsesSemanticFrame()?"true":"false");
    json += ",\"k11_learned_axis_semantic_front\":" + String(wt901K11LearnedAxisFront(),3);
    json += ",\"k11_learned_axis_semantic_side\":" + String(wt901K11LearnedAxisSide(),3);
    json += ",\"k11_learned_axis_semantic_down\":" + String(wt901K11LearnedAxisDown(),3);
    json += ",\"k11_cal_peak_deg\":" + String(wt901K11PeakDeg(), 1);
    json += ",\"k11_cal_angle_deg\":" + String(wt901K11CalAngleDeg(), 1);
    json += ",\"k11_cal_observed_upper_max_deg\":" + String(k11CalObservedUpperMaxDeg, 1);
    json += ",\"k11_cal_observed_plane_max_deg\":" + String(k11CalObservedPlaneMaxDeg, 1);
    json += ",\"k11_cal_observed_torso_max_deg\":" + String(k11CalObservedTorsoMaxDeg, 1);
    json += ",\"extension_speed_deg_s\":" + String(wt901K11ElbowSpeedDegS(), 1);
    json += ",\"k11_bias_samples_a\":" + String(wt901K11BiasSamplesA());
    json += ",\"k11_bias_samples_b\":" + String(wt901K11BiasSamplesB());
    json += ",\"k11_processed_a\":" + String(wt901K11ProcessedA());
    json += ",\"k11_processed_b\":" + String(wt901K11ProcessedB());
    json += ",\"k11_rejected_dt_a\":" + String(wt901K11RejectedDtA());
    json += ",\"k11_rejected_dt_b\":" + String(wt901K11RejectedDtB());
    json += ",\"k11_bridged_gap_a\":" + String(wt901K11BridgedGapA());
    json += ",\"k11_bridged_gap_b\":" + String(wt901K11BridgedGapB());
    json += ",\"k11_queue_depth_a\":" + String(wt901K11QueueDepthA());
    json += ",\"k11_queue_depth_b\":" + String(wt901K11QueueDepthB());
    json += ",\"k11_queue_drop_a\":" + String(wt901K11QueueDroppedA());
    json += ",\"k11_queue_drop_b\":" + String(wt901K11QueueDroppedB());
    json += ",\"upper_arm_rel_torso_deg\":" + String(torsoRefUpperDeviationDeg(), 1); // compatibility alias now means sagittal U
    json += ",\"upper_arm_a_only_deviation_deg\":" + String(torsoRefUpperDeviationDeg(), 1);
    json += ",\"upper_inplane_participation_deg\":" + String(torsoRefUpperDeviationDeg(), 1);
    json += ",\"upper_total_tilt_diagnostic_deg\":" + String(torsoRefUpperTotalTiltDiagnosticDeg(), 1);
    json += ",\"upper_side_tilt_deg\":" + String(v449UpperSidePlaneDeg(), 1);
    json += ",\"fore_side_tilt_deg\":" + String(v449ForeSidePlaneDeg(), 1);
    json += ",\"fore_side_tilt_role\":\"diagnostic_only_dynamic_accel_sensitive_not_formal_P\"";
    json += ",\"fore_quaternion_plane_deviation_deg\":" + String(v449ForeQuatPlaneDeg(), 1);
    json += ",\"fore_quaternion_plane_raw_deg\":" + String(v449ForeQuatPlaneRawDeg(), 1);
    json += ",\"fore_quaternion_plane_role\":\"diagnostic_only_absolute_heading; raw field used only for K11 calibration acceptance\"";
    json += ",\"fore_motion_plane_deg\":" + String(v4430ForearmMotionPlaneDeg(), 1);
    json += ",\"upper_body_relative_z_gyro_deg\":" + String(torsoRefUpperarmRelativeBodyYawDeg(), 1);
    json += ",\"fore_body_relative_z_gyro_deg\":" + String(torsoRefForearmRelativeBodyYawDeg(), 1);
    json += ",\"fore_motion_plane_ref_front\":" + String(wt901K11LearnedAxisFront(), 3);
    json += ",\"fore_motion_plane_ref_side\":" + String(wt901K11LearnedAxisSide(), 3);
    json += ",\"body_relative_azimuth_deg\":" + String(v449BodyRelativeAzimuthDeg(), 1);
    json += ",\"plane_per_rep_ae_delta_deg\":" + String(v4431fPerRepPlaneDeg(), 1);
    json += ",\"plane_per_rep_ae_baseline_deg\":" + String(v4431fPlane.baselineAeDeg, 1);
    json += ",\"torso_yaw_from_front_deg\":" + String(torsoRefTorsoYawFromFrontDeg(), 1);
    json += ",\"torso_body_turn_gyro_deg\":" + String(torsoRef.torsoBodyTurnYawDeg, 1);
    json += ",\"body_plane_reference_valid\":" + String(torsoRefBodyPlaneReferenceValid()?"true":"false");
    json += ",\"arm_vertical_twist_delta_deg\":" + String(torsoRefArmVerticalTwistDeltaDeg(), 1);
    json += ",\"torso_vertical_twist_delta_deg\":" + String(torsoRefTorsoVerticalTwistDeltaDeg(), 1);
    json += ",\"arm_vertical_yaw_integral_deg\":" + String(torsoRefArmVerticalYawIntegralDeg(), 1);
    json += ",\"torso_vertical_yaw_integral_deg\":" + String(torsoRefTorsoVerticalYawIntegralDeg(), 1);
    json += ",\"legacy_yaw_integral_fields_role\":\"continuous_vertical_gyro_integrals\"";
    json += ",\"sagittal_plane_deviation_deg\":" + String(v449FormalPlaneDeviationDeg(), 1);
    json += ",\"body_front_plane_deviation_deg\":" + String(v449BodyPlaneDeviationDeg(), 1);
    json += ",\"k11_hinge_axis_deviation_deg\":" + String(v449HingePlaneDeviationDeg(), 1);
    json += ",\"k11_hinge_off_axis_rotation_deg\":" + String(wt901K11HingeOffAxisRotationDeg(), 1);
    json += ",\"torso_deviation_from_neutral_deg\":" + String(torsoRefTorsoDeviationDeg(), 1);
    json += ",\"torso_deviation_from_group_start_deg\":" + String(torsoRefTorsoDeviationDeg(), 1);
    json += ",\"torso_tilt_quality_deg\":" + String(torsoRefTorsoTiltDeviationDeg(), 1);
    json += ",\"torso_quality_metric\":\"filtered_gravity_tilt_yaw_ignored\"";
    json += ",\"upper_elevation_from_neutral_down_deg\":" + String(torsoRefUpperNeutralElevationDeg(), 1);
    json += ",\"fore_elevation_from_neutral_down_deg\":" + String(torsoRefForeNeutralElevationDeg(), 1);
    json += ",\"elbow_segment_proxy_deg\":" + String(torsoRefElbowProxyDeg(), 1);
    json += ",\"torso_reference_ready\":" + String(bodyFrameReady(0) ? "true" : "false");
    // V4.4.16: report-facing maxima come ONLY from the meaningful evaluation window.
    // Whole-attempt peaks remain explicit diagnostics and must not drive reports.
    json += ",\"current_rep_upper_arm_rel_torso_max_deg\":" + String(leftTraining.data.currentRepEvalMaxUpperArmDev, 1);
    json += ",\"current_rep_plane_deviation_max_deg\":" + String(leftTraining.data.currentRepEvalMaxPlaneDev, 1);
    json += ",\"current_rep_torso_deviation_max_deg\":" + String(leftTraining.data.currentRepEvalMaxTorsoDev, 1);
    json += ",\"last_rep_upper_arm_rel_torso_max_deg\":" + String(leftTraining.data.lastCompletedRepEvalMaxUpperArmDev, 1);
    json += ",\"last_rep_plane_deviation_max_deg\":" + String(leftTraining.data.lastCompletedRepEvalMaxPlaneDev, 1);
    json += ",\"last_rep_torso_deviation_max_deg\":" + String(leftTraining.data.lastCompletedRepEvalMaxTorsoDev, 1);
    json += ",\"last_rep_eval_upper_max_deg\":" + String(leftTraining.data.lastCompletedRepEvalMaxUpperArmDev, 1);
    json += ",\"last_rep_eval_plane_max_deg\":" + String(leftTraining.data.lastCompletedRepEvalMaxPlaneDev, 1);
    json += ",\"last_rep_eval_torso_max_deg\":" + String(leftTraining.data.lastCompletedRepEvalMaxTorsoDev, 1);
    json += ",\"current_rep_upper_raw_diag_max_deg\":" + String(leftTraining.data.currentRepMaxUpperArmDev, 1);
    json += ",\"current_rep_plane_raw_diag_max_deg\":" + String(leftTraining.data.currentRepMaxPlaneDev, 1);
    json += ",\"current_rep_torso_raw_diag_max_deg\":" + String(leftTraining.data.currentRepMaxTorsoDev, 1);
    json += ",\"last_rep_upper_raw_diag_max_deg\":" + String(leftTraining.data.lastCompletedRepMaxUpperArmDev, 1);
    json += ",\"last_rep_plane_raw_diag_max_deg\":" + String(leftTraining.data.lastCompletedRepMaxPlaneDev, 1);
    json += ",\"last_rep_torso_raw_diag_max_deg\":" + String(leftTraining.data.lastCompletedRepMaxTorsoDev, 1);
    json += ",\"last_rep_plane_source\":\"" + String(planeDeviationSourceName(leftTraining.data.lastPlaneSource)) + "\"";
    json += ",\"last_rep_plane_body_azimuth_eval_max_deg\":" + String(leftTraining.data.lastCompletedRepEvalMaxPlaneBodyAzimuth, 1);
    json += ",\"last_rep_plane_upper_side_eval_max_deg\":" + String(leftTraining.data.lastCompletedRepEvalMaxPlaneUpperSide, 1);
    json += ",\"last_rep_plane_forearm_quat_eval_max_deg\":" + String(leftTraining.data.lastCompletedRepEvalMaxPlaneForearmQuat, 1);
    json += ",\"last_rep_plane_forearm_motion_eval_max_deg\":" + String(leftTraining.data.lastCompletedRepEvalMaxPlaneForearmQuat, 1);
    json += ",\"last_rep_plane_hinge_eval_max_deg\":" + String(leftTraining.data.lastCompletedRepEvalMaxPlaneHinge, 1);
    json += ",\"last_rep_upper_severe\":" + String(leftTraining.data.lastUpperSevere ? "true" : "false");
    json += ",\"last_rep_plane_severe\":" + String(leftTraining.data.lastPlaneSevere ? "true" : "false");
    json += ",\"last_rep_torso_severe\":" + String(leftTraining.data.lastTorsoSevere ? "true" : "false");
    json += ",\"last_rep_upper_arm_rel_torso_mean_deg\":" + String(leftTraining.data.lastCompletedRepMeanUpperArmDev, 1);
    json += ",\"attempt_sequence_count\":" + String(leftTraining.data.completedMotionCount);
    json += ",\"last_completed_attempt_peak_deg\":" + String(leftTraining.data.lastCompletedRepPeakAngle, 1);
    json += ",\"upper_arm_quality_active\":true";
    json += ",\"upper_arm_deviation_role\":\"A_local_gravity_change_relative_to_group_start; D_excluded\"";
    json += ",\"upper_arm_quality_soft_deg\":" + String(currentUpperSoftDeg(), 1);
    json += ",\"plane_quality_soft_deg\":" + String(currentPlaneSoftDeg(), 1);
    json += ",\"torso_quality_soft_deg\":" + String(currentTorsoSoftDeg(), 1);
    json += ",\"upper_arm_quality_threshold_deg\":" + String(currentUpperThresholdDeg(), 1);
    json += ",\"plane_quality_threshold_deg\":" + String(currentPlaneThresholdDeg(), 1);
    json += ",\"torso_quality_threshold_deg\":" + String(currentTorsoThresholdDeg(), 1);
    json += ",\"relaxed_return_max_deg\":" + String(returnAngle + K11_RELAXED_RETURN_MARGIN_DEG, 1);
    json += ",\"last_rep_quality\":\"" + String(lastRepQualityCode()) + "\"";
    json += ",\"last_rep_rom_low\":" + String(leftTraining.data.lastRomLow ? "true" : "false");
    json += ",\"last_rep_upper_arm_compensation\":" + String(leftTraining.data.lastUpperArmExcess ? "true" : "false");
    json += ",\"last_rep_plane_deviation\":" + String(leftTraining.data.lastPlaneDeviation ? "true" : "false");
    json += ",\"last_rep_torso_compensation\":" + String(leftTraining.data.lastTorsoCompensation ? "true" : "false");
    json += ",\"last_rep_quality_eval_ms\":" + String(leftTraining.data.lastCompletedRepQualityEvalMs);
    json += ",\"last_rep_upper_bad_pct\":" + String(leftTraining.data.lastCompletedRepUpperBadPct,1);
    json += ",\"last_rep_plane_bad_pct\":" + String(leftTraining.data.lastCompletedRepPlaneBadPct,1);
    json += ",\"last_rep_torso_bad_pct\":" + String(leftTraining.data.lastCompletedRepTorsoBadPct,1);
    json += ",\"last_rep_upper_bad_run_ms\":" + String(leftTraining.data.lastCompletedRepUpperMaxBadRunMs);
    json += ",\"last_rep_plane_bad_run_ms\":" + String(leftTraining.data.lastCompletedRepPlaneMaxBadRunMs);
    json += ",\"last_rep_torso_bad_run_ms\":" + String(leftTraining.data.lastCompletedRepTorsoMaxBadRunMs);
    json += ",\"last_rep_upper_soft_warning\":" + String(leftTraining.data.lastUpperArmSoftWarning ? "true" : "false");
    json += ",\"last_rep_plane_soft_warning\":" + String(leftTraining.data.lastPlaneSoftWarning ? "true" : "false");
    json += ",\"last_rep_torso_soft_warning\":" + String(leftTraining.data.lastTorsoSoftWarning ? "true" : "false");
    json += ",\"last_rep_upper_arm_deviation_max_deg\":" + String(leftTraining.data.lastCompletedRepEvalMaxUpperArmDev, 1);
    json += ",\"attempt_good_count\":" + String(leftTraining.data.goodRepCount);
    json += ",\"attempt_issue_count\":" + String(leftTraining.data.issueRepCount);
    json += ",\"slot_progress_count\":" + String(leftTraining.data.currentCount);
    json += ",\"formal_attempt_count\":" + String(leftTraining.data.totalAttemptCount);
    json += ",\"retry_pending\":" + String(leftTraining.data.retryPending ? "true" : "false");
    json += ",\"last_attempt_was_retry\":" + String(leftTraining.data.lastAttemptWasRetry ? "true" : "false");
    json += ",\"last_attempt_passed\":" + String(leftTraining.data.lastAttemptPassed ? "true" : "false");
    json += ",\"last_slot_completed\":" + String(leftTraining.data.lastSlotCompleted ? "true" : "false");
    json += ",\"last_slot_passed\":" + String(leftTraining.data.lastSlotPassed ? "true" : "false");
    json += ",\"quality_rule\":\"ROM target with 3deg tolerance; U=upper sagittal-in-plane participation; P=per-attempt delta(A-E vertical gyro heading), reset after every completed attempt; B/absolute yaw/hinge diagnostic only; T=E filtered gravity tilt; no per-rep recalibration; normal breathing allowed; evaluated above 30deg elbow; one_retry_max; formal report uses EVAL_MAX\"";
    int v4TargetTotal = max(1, targetSets * targetReps);
    // V4 A2 K2 slot-vs-attempt report contract.
    json += ",\"v4_session_completed_slots\":" + String(v4Assessment.completedSlots);
    json += ",\"v4_session_target_slots\":" + String(v4TargetTotal);
    json += ",\"v4_session_passed_slots\":" + String(v4Assessment.passedSlots);
    json += ",\"v4_session_failed_slots\":" + String(v4Assessment.failedSlots);
    json += ",\"v4_pass_rate_pct\":" + String(v4Assessment.passRatePct(), 1);
    json += ",\"v4_total_attempts\":" + String(v4Assessment.totalAttempts);
    json += ",\"v4_retry_attempts\":" + String(v4Assessment.retryAttempts);
    json += ",\"v4_recovered_on_retry\":" + String(v4Assessment.recoveredOnRetry);
    json += ",\"v4_first_try_passed_slots\":" + String(v4Assessment.firstTryPassedSlots());
    json += ",\"v4_first_try_pass_rate_pct\":" + String(v4Assessment.firstTryPassRatePct(), 1);
    json += ",\"v4_retry_recovery_rate_pct\":" + String(v4Assessment.retryRecoveryRatePct(), 1);
    json += ",\"v4_accounting_ok\":" + String(v4Assessment.accountingOk() ? "true" : "false");
    json += ",\"v4_final_rom_low_slots\":" + String(v4Assessment.finalRomLowSlots);
    json += ",\"v4_final_upper_arm_excess_slots\":" + String(v4Assessment.finalUpperArmExcessSlots);
    json += ",\"v4_final_plane_deviation_slots\":" + String(v4Assessment.finalPlaneDeviationSlots);
    json += ",\"v4_final_torso_compensation_slots\":" + String(v4Assessment.finalTorsoCompensationSlots);
    json += ",\"v4_final_multi_issue_slots\":" + String(v4Assessment.finalMultiIssueSlots);
    json += ",\"v4_rom_low_attempts\":" + String(v4Assessment.romLowAttempts);
    json += ",\"v4_upper_arm_excess_attempts\":" + String(v4Assessment.upperArmExcessAttempts);
    json += ",\"v4_plane_deviation_attempts\":" + String(v4Assessment.planeDeviationAttempts);
    json += ",\"v4_torso_compensation_attempts\":" + String(v4Assessment.torsoCompensationAttempts);
    json += ",\"v4_multi_issue_attempts\":" + String(v4Assessment.multiIssueAttempts);
    json += ",\"v4_completion_rate_pct\":" + String(v4Assessment.completionRatePct(v4TargetTotal), 1);
    json += ",\"v4_pass_rom_avg_deg\":" + String(v4Assessment.passRomAvgDeg(), 1);
    json += ",\"v4_pass_rom_min_deg\":" + String(v4Assessment.passRomMin, 1);
    json += ",\"v4_pass_rom_max_deg\":" + String(v4Assessment.passRomMax, 1);
    json += ",\"v4_pass_rom_sd_deg\":" + String(v4Assessment.passRomSdDeg(), 1);
    json += ",\"v4_avg_attempt_speed_deg_s\":" + String(v4Assessment.avgAttemptMeanSpeedDegS(), 1);
    json += ",\"v4_avg_attempt_duration_sec\":" + String(v4Assessment.avgAttemptDurationSec(), 2);
    json += ",\"v4_avg_attempt_upper_arm_max_deg\":" + String(v4Assessment.avgAttemptUpperArmMaxDeg(), 1);
    json += ",\"v4_avg_attempt_plane_max_deg\":" + String(v4Assessment.avgAttemptPlaneMaxDeg(), 1);
    json += ",\"v4_avg_attempt_torso_max_deg\":" + String(v4Assessment.avgAttemptTorsoMaxDeg(), 1);
    json += ",\"v4_summary_code\":\"" + String(v4Assessment.summaryCode(v4TargetTotal)) + "\"";
    json += ",\"v4_assessment_scope\":\"training_support_not_diagnosis\"";
  }

  json += "}";
  Serial.print("JSON:");
  Serial.println(json);
  pendingRepEvent = "none";
}

const char *stateName(TrainingState state) {
  switch (state) {
    case IDLE: return "IDLE";
    case RUNNING: return "RUNNING";
    case PAUSED: return "PAUSED";
    case FINISHED: return "FINISHED";
    case STOPPED: return "STOPPED";
    default: return "UNKNOWN";
  }
}

void prepareSpiBusBeforeInit() {
  // Old 3.5-inch TFT is no longer initialized. Keep its former CS high so it can
  // never interfere with the SD bus if an old connector is still present.
  pinMode(PIN_TFT_CS, OUTPUT);
  digitalWrite(PIN_TFT_CS, HIGH);
  pinMode(PIN_SD_CS, OUTPUT);
  digitalWrite(PIN_SD_CS, HIGH);
  sharedSPI.begin(PIN_SPI_SCK, PIN_SPI_MISO, PIN_SPI_MOSI, PIN_SD_CS);
  delay(120);
}

bool anyRunning() {
  if (bodyIndex==0) return leftTraining.data.state==RUNNING;
  if (bodyIndex==1) return rightTraining.data.state==RUNNING;
  return leftTraining.data.state==RUNNING || rightTraining.data.state==RUNNING;
}

bool anyPaused() {
  if (bodyIndex==0) return leftTraining.data.state==PAUSED;
  if (bodyIndex==1) return rightTraining.data.state==PAUSED;
  return leftTraining.data.state==PAUSED || rightTraining.data.state==PAUSED;
}

bool bothFinished() {
  if (bodyIndex==0) return leftTraining.data.currentCount >= leftTraining.data.targetCount;
  if (bodyIndex==1) return rightTraining.data.currentCount >= rightTraining.data.targetCount;
  return leftTraining.data.currentCount >= leftTraining.data.targetCount && rightTraining.data.currentCount >= rightTraining.data.targetCount;
}

void applyTrainingParamsToLogic() {
  leftTraining.configureMotionQuality(true, currentUpperThresholdDeg(), true, currentPlaneThresholdDeg(), true, currentTorsoThresholdDeg());
  rightTraining.configureMotionQuality(true, currentUpperThresholdDeg(), true, currentPlaneThresholdDeg(), true, currentTorsoThresholdDeg());
  leftTraining.data.targetAngle = targetAngle;
  rightTraining.data.targetAngle = targetAngle;
  leftTraining.data.validThreshold = validAngle;
  rightTraining.data.validThreshold = validAngle;
  leftTraining.data.resetThreshold = returnAngle;
  rightTraining.data.resetThreshold = returnAngle;
  leftTraining.data.targetCount = targetReps;
  rightTraining.data.targetCount = targetReps;
}

void syncUiDataFromTraining() {
  leftAngle = leftTraining.data.currentAngle; rightAngle = rightTraining.data.currentAngle;
  leftRom = leftTraining.data.maxAngle; rightRom = rightTraining.data.maxAngle;
  leftCount = leftTraining.data.currentCount; rightCount = rightTraining.data.currentCount;
  int totalTarget = max(1, targetSets * targetReps);
  int groupDone = bodyIndex==0 ? leftCount : (bodyIndex==1 ? rightCount : min(leftCount,rightCount));
  int totalDone = (setIndex - 1) * targetReps + groupDone;
  completionPercent = constrain((totalDone * 100) / totalTarget, 0, 100);
  trainingRunning = anyRunning(); trainingPaused = anyPaused();
}

void resetCurrentGroupCountersAndStart(bool keepBaseline = false) {
  if (keepBaseline) leftTraining.resetForNextGroupKeepBaseline();
  else leftTraining.reset();
  rightTraining.reset();

  // K9.1 FIX: do NOT call startOrPause() from IDLE here. startOrPause() resets
  // TrainingData again and silently restores targetAngle=90, which made an 80-degree
  // UI target become an internal 81-degree ROM pass line. Set RUNNING directly,
  // then apply the UI parameters once and keep them.
  leftTraining.data.state = leftSideActive() ? RUNNING : IDLE;
  rightTraining.data.state = rightSideActive() ? RUNNING : IDLE;
  applyTrainingParamsToLogic();
  if (leftSideActive()) leftTraining.armForNextRepFromCurrentAngle(0.0f, torsoRefUpperDeviationDeg());
  if (rightSideActive()) rightTraining.armForNextRepFromCurrentAngle(0.0f, 0.0f);

  lastLeftSignedAngle = 0.0f;
  lastRightSignedAngle = 0.0f;
  leftAngle = 0.0f;
  rightAngle = 0.0f;
  leftRom = 0.0f;
  rightRom = 0.0f;
  leftCount = 0;
  rightCount = 0;
  bilateralLeftSlotWaiting = false;
  bilateralRightSlotWaiting = false;
  resetGameOutputState();
  resetSpeedReference(0.0f, 0.0f);

  syncUiDataFromTraining();
}

void enterRestAfterGroup() {
  a2GroupReacquireActive = false;
  a2GroupReacquireHoldStartMs = 0;
  restStartMs = millis();
  restRemainingSec = restSec;
  syncUiDataFromTraining();
  if (logger.isActive()) {
    char marker[40];
    snprintf(marker, sizeof(marker), "GROUP_%d_FINISHED", setIndex);
    logger.logMarker(marker);
  }
  Serial.print("GROUP "); Serial.print(setIndex); Serial.print('/'); Serial.print(targetSets);
  Serial.print(" FINISHED. REST "); Serial.print(restSec); Serial.println("s.");
  queueVoiceEvent("SET_DONE", rehabVoice.queueSetDone());
  buzzerBeep(0); // quiet single tap: group finished
  page = PAGE_REST;
  lastPage = PAGE_COUNT;
  uiDirty = true;
  powerResumeSave("GROUP_REST");
}

static void startNextGroupDirect(const char *reason) {
  if (page != PAGE_REST || setIndex >= targetSets) return;

  // V4.4.19: group rest is only a rest timer. Do NOT add a hidden second
  // start-pose/recalibration gate after the user presses "Skip rest" (or when
  // the timer reaches 0). The session already has one accepted K11/body-frame
  // calibration. Keep that calibration and start the next group immediately.
  a2GroupReacquireActive = false;
  a2GroupReacquireHoldStartMs = 0;
  startPoseLastWaitLogMs = 0;

  setIndex++;
  resetCurrentGroupCountersAndStart(true);
  selectedK11SetPaused(false);
  trainingFinishHandled = false;
  restStartMs = 0;
  restRemainingSec = 0;
  page = PAGE_TRAINING;
  lastPage = PAGE_COUNT;
  uiDirty = true;

  if (logger.isActive()) {
    char marker[40];
    snprintf(marker, sizeof(marker), "START_GROUP_%d", setIndex);
    logger.logMarker(marker);
  }
  Serial.printf("V4_4_19_NEXT_GROUP_DIRECT: reason=%s -> GROUP %d/%d STARTED; session calibration kept.\n",
                reason ? reason : "REST_DONE", setIndex, targetSets);
  queueVoiceEvent("NEXT_SET", rehabVoice.queueNextSet());
  sendScreenLiveFrame(true);
}

void startNextGroupAfterRest() {
  if (setIndex >= targetSets) return;
  if (restRemainingSec > 0) {
    Serial.printf("V4_A2_REST_ACTIVE: %d sec remaining; next group not started.\n", restRemainingSec);
    buzzerBeep(0);
    return;
  }
  startNextGroupDirect("REST_DONE_OR_CONFIRM");
}

void updateRestState() {
  if (page != PAGE_REST) return;
  unsigned long elapsedSec = (millis() - restStartMs) / 1000UL;
  int remain = restSec - (int)elapsedSec;
  if (remain < 0) remain = 0;
  if (remain != restRemainingSec) {
    restRemainingSec = remain;
    uiDirty = true;
  }
  // V4.4.19: normal countdown completion advances directly to the next group.
  // This restores the intended multi-set UX: REST -> next group, with no hidden
  // pose gate that can leave the rest page stuck at 0 seconds.
  if (remain == 0 && setIndex < targetSets) {
    startNextGroupDirect("REST_TIMER_ZERO");
  }
}

static float a2AbsMaxGyro(const WT901EulerData &d) {
  float m = fabsf(d.gx);
  if (fabsf(d.gy) > m) m = fabsf(d.gy);
  if (fabsf(d.gz) > m) m = fabsf(d.gz);
  return m;
}

static const char *a2EffectiveCalPhaseName() {
  if (a2PreCalPhase == A2_PRECAL_PREPARE) return "START_POSE_WAIT";
  if (a2PreCalPhase == A2_PRECAL_STILL_CHECK) return "START_STILL_CHECK";
  return wt901K11PhaseName();
}

static bool a2PreCalibrationActive() {
  return a2PreCalPhase != A2_PRECAL_OFF;
}

static float a2PrepareProgress() {
  // V4.2.2: there is intentionally no countdown. The user may take as long as
  // needed to reach the semantic start pose.
  return a2PreCalPhase == A2_PRECAL_STILL_CHECK ? 1.0f : 0.0f;
}

static int a2PrepareRemainingSec() {
  return 0;
}

static float a2StartStillProgress() {
  if (a2PreCalPhase != A2_PRECAL_STILL_CHECK || a2PreCalStillHoldStartMs == 0) return 0.0f;
  float p = (millis() - a2PreCalStillHoldStartMs) / (float)A2_START_STILL_HOLD_MS;
  if (p < 0.0f) p = 0.0f;
  if (p > 1.0f) p = 1.0f;
  return p;
}

static bool a2BothImuFreshAndStill() {
  // One-time K11/group-start stability only; NEVER used between repetitions.
  // A/B must be quiet enough for the elbow gyro bias. E only has to be fresh: normal
  // abdominal breathing must never restart or block calibration/training. Gross torso
  // posture is handled by the torso/start-pose geometry, not by an E gyro-still gate.
  unsigned long now = millis();
  const int slots[3] = {0,1,4};
  for (int i=0; i<3; ++i) {
    const WT901EulerData &d = wt901Slots[slots[i]].latest;
    if (!d.valid || d.sampleMs == 0) return false;
    if ((now - d.sampleMs) > A2_START_SAMPLE_FRESH_MS) return false;
    if (slots[i] != 4 && a2AbsMaxGyro(d) > A2_START_ARM_STILL_MAX_GYRO_DEG_S) return false;
  }
  return exerciseStartPoseValid(false);
}

static bool a2NormalizeLatestGravity(int slotIndex, float &x, float &y, float &z) {
  if (slotIndex < 0 || slotIndex > 1) return false;
  int globalSlot = slotIndex == 0 ? selectedFixedSlot() : selectedMovingSlot();
  const WT901EulerData &d = wt901Slots[globalSlot].latest;
  unsigned long now = millis();
  if (!d.valid || d.sampleMs == 0 || (now - d.sampleMs) > A2_START_SAMPLE_FRESH_MS) return false;
  float n = sqrtf(d.ax*d.ax + d.ay*d.ay + d.az*d.az);
  if (!isfinite(n) || n < A2_GROUP_ACCEL_MIN_G || n > A2_GROUP_ACCEL_MAX_G) return false;
  x = d.ax / n; y = d.ay / n; z = d.az / n;
  return true;
}

static float a2GravityAngleDeg(float ax, float ay, float az, float bx, float by, float bz) {
  float dot = ax*bx + ay*by + az*bz;
  if (dot > 1.0f) dot = 1.0f;
  if (dot < -1.0f) dot = -1.0f;
  return acosf(dot) * 57.2957795f;
}

static bool a2CaptureSessionStartPoseReference() {
  A2GravityPoseRef tmp[2];
  for (int i=0; i<2; ++i) {
    if (!a2NormalizeLatestGravity(i, tmp[i].x, tmp[i].y, tmp[i].z)) return false;
    tmp[i].valid = true;
  }
  a2SessionStartPoseRef[0] = tmp[0];
  a2SessionStartPoseRef[1] = tmp[1];
  Serial.printf("V4_A2_SESSION_START_POSE_REF_SAVED: A=(%.4f,%.4f,%.4f) B=(%.4f,%.4f,%.4f) tol=%.1fdeg\n",
    tmp[0].x,tmp[0].y,tmp[0].z,tmp[1].x,tmp[1].y,tmp[1].z,A2_GROUP_POSE_TOL_DEG);
  if (logger.isActive()) logger.logMarker("V4_A2_SESSION_START_POSE_REF_SAVED_AB");
  return true;
}

static bool a2GroupPoseMatchesReference() {
  if (!a2SessionStartPoseRef[0].valid || !a2SessionStartPoseRef[1].valid) return false;
  if (!exerciseStartPoseValid(false)) return false;
  bool matched = true;
  for (int i=0; i<2; ++i) {
    float x,y,z;
    if (!a2NormalizeLatestGravity(i,x,y,z)) {
      a2GroupPoseDevDeg[i] = 999.0f;
      matched = false;
      continue;
    }
    a2GroupPoseDevDeg[i] = a2GravityAngleDeg(x,y,z,
      a2SessionStartPoseRef[i].x,a2SessionStartPoseRef[i].y,a2SessionStartPoseRef[i].z);
    if (a2GroupPoseDevDeg[i] > A2_GROUP_POSE_TOL_DEG) matched = false;
  }
  return matched;
}

static void beginA2GroupReacquireGate() {
  if (a2GroupReacquireActive) {
    Serial.println("V4_4_GROUP_REACQUIRE_ALREADY_ACTIVE: return A/B to the exercise start pose; normal breathing is allowed.");
    return;
  }
  a2GroupReacquireActive = true;
  a2GroupReacquireHoldStartMs = 0;
  startPoseLastWaitLogMs = 0;
  selectedK11SetPaused(true);
  Serial.println("V4_4_GROUP_REACQUIRE_ARMED: after rest, return to the same start-pose region briefly; normal breathing is allowed.");
  if (logger.isActive()) logger.logMarker("V4_4_GROUP_REACQUIRE_ARMED");
  lastPage = PAGE_COUNT;
  uiDirty = true;
}

static void updateA2GroupReacquireGate() {
  if (!a2GroupReacquireActive || page != PAGE_REST || restRemainingSec > 0) return;
  // V4.4: the next group must return near the stable A/B pose saved before
  // group 1, not merely enter any generic exercise start-pose region.
  bool poseMatch = a2GroupPoseMatchesReference();
  bool still = a2BothImuFreshAndStill();
  bool ok = poseMatch && still;
  unsigned long now = millis();
  if (!ok) {
    if (a2GroupReacquireHoldStartMs != 0) Serial.println("V4_4_GROUP_REACQUIRE_RESET: saved pose/motion lost; short start-pose window restarted.");
    a2GroupReacquireHoldStartMs = 0;
    if (startPoseLastWaitLogMs == 0 || (now - startPoseLastWaitLogMs) >= START_POSE_WAIT_LOG_MS) {
      startPoseLastWaitLogMs = now;
      Serial.printf("V4_4_GROUP_REACQUIRE_WAIT: ref=%d pose_match=%d still=%d A_dev=%.1fdeg B_dev=%.1fdeg tol=%.1fdeg\n",
        (a2SessionStartPoseRef[0].valid && a2SessionStartPoseRef[1].valid) ? 1 : 0,
        poseMatch ? 1 : 0, still ? 1 : 0, a2GroupPoseDevDeg[0], a2GroupPoseDevDeg[1], A2_GROUP_POSE_TOL_DEG);
    }
    return;
  }
  if (a2GroupReacquireHoldStartMs == 0) {
    a2GroupReacquireHoldStartMs = now;
    Serial.printf("V4_4_GROUP_REACQUIRE_HOLD: saved pose matched (A_dev=%.1fdeg B_dev=%.1fdeg); briefly keep the arm in start pose; normal breathing is allowed.\n",
      a2GroupPoseDevDeg[0], a2GroupPoseDevDeg[1]);
    uiDirty = true;
  }
  if ((now - a2GroupReacquireHoldStartMs) < A2_GROUP_REACQUIRE_HOLD_MS) { uiDirty = true; return; }

  selectedK11RezeroFormal();
  if (!torsoRefRebaselineUpperFromLatest() || !torsoRefRebaselineTorsoFromLatest()) {
    Serial.println("V4_4_GROUP_REACQUIRE_REBASELINE_RETRY: A or E group baseline unavailable.");
    a2GroupReacquireHoldStartMs = 0;
    return;
  }
  a2GroupReacquireActive = false;
  a2GroupReacquireHoldStartMs = 0;
  setIndex++;
  resetCurrentGroupCountersAndStart(true);
  selectedK11SetPaused(false);
  trainingFinishHandled = false;
  restStartMs = 0;
  restRemainingSec = 0;
  page = PAGE_TRAINING;
  lastPage = PAGE_COUNT;
  uiDirty = true;
  if (logger.isActive()) logger.logMarker("V4_4_GROUP_REACQUIRE_PASSED");
  Serial.printf("V4_4_GROUP_REACQUIRE_PASSED -> GROUP %d/%d STARTED.\n", setIndex, targetSets);
  queueVoiceEvent("NEXT_SET", rehabVoice.queueNextSet());
}

static void a2DrainPreCalibrationQueues() {
  // PREPARE/STILL_CHECK samples must never leak into K11 bias or gravity baseline.
  wt901ClearSampleQueue(0);
  wt901ClearSampleQueue(1);
  wt901ClearSampleQueue(3);
}

static void startA2PreCalibrationGate() {
  // Keep K11 and upper-tilt estimator completely stopped until the gate passes.
  a2PreCalPhase = A2_PRECAL_PREPARE;
  a2PreCalPhaseStartMs = millis();
  a2PreCalStillHoldStartMs = 0;
  startPoseLastWaitLogMs = 0;
  a2DrainPreCalibrationQueues();
  Serial.printf("V4_4_START_POSE_WAIT: action=%d; K11 remains OFF until A/B local semantic start pose is valid. E is checked for torso and later body-plane yaw reference.\n", exerciseIndex+1);
}

static void updateA2PreCalibrationGate() {
  if (!a2PreCalibrationActive()) return;

  // Discard every notification received before true K11 calibration starts.
  a2DrainPreCalibrationQueues();
  unsigned long now = millis();

  if (a2PreCalPhase == A2_PRECAL_PREPARE) {
    // Explicit semantic gate: first confirm the calibrated start pose, then use a short stability window.
    // selected arm(s) are in the correct start-pose region. No timeout.
    bool poseOk = exerciseStartPoseValid(false);
    if (!poseOk) {
      if (startPoseLastWaitLogMs == 0 || (now - startPoseLastWaitLogMs) >= START_POSE_WAIT_LOG_MS) {
        startPoseLastWaitLogMs = now;
        exerciseStartPoseValid(true);
        Serial.println("V4_4_START_POSE_BLOCKED: waiting; K11/baseline sampling is still OFF.");
      }
      return;
    }

    a2PreCalPhase = A2_PRECAL_STILL_CHECK;
    a2PreCalPhaseStartMs = now;
    a2PreCalStillHoldStartMs = 0;
    Serial.println("V4_4_START_POSE_VALID: keep the arm in this pose briefly; normal breathing is allowed.");
    if (logger.isActive()) logger.logMarker("V4_4_START_POSE_VALID_BEGIN_SHORT_STABILITY");
    uiDirty = true;
    return;
  }

  if (a2PreCalPhase == A2_PRECAL_STILL_CHECK) {
    bool poseOk = exerciseStartPoseValid(false);
    bool still = a2BothImuFreshAndStill();
    if (!poseOk || !still) {
      if (a2PreCalStillHoldStartMs != 0) {
        Serial.printf("V4_4_START_STABILITY_RESET: %s; short stability window restarted.\n", poseOk ? "motion/freshness" : "start pose left valid region");
      }
      a2PreCalStillHoldStartMs = 0;
      return;
    }

    if (a2PreCalStillHoldStartMs == 0) {
      a2PreCalStillHoldStartMs = now;
      Serial.println("V4_4_START_STABILITY_HOLD: A/B start pose detected; short stability window. E breathing is tolerated.");
    }
    if ((now - a2PreCalStillHoldStartMs) < A2_START_STILL_HOLD_MS) return;

    // Only NOW may the original validated K11 bias/gravity-baseline calibration begin.
    // V4.4: save the first group's stable start pose once. Later groups must
    // return near this reference before their K11 zero/baseline is refreshed.
    if (!a2SessionStartPoseRef[0].valid || !a2SessionStartPoseRef[1].valid) {
      if (!a2CaptureSessionStartPoseReference()) {
        Serial.println("V4_4_START_POSE_REF_RETRY: stable pose detected but A/B gravity reference capture failed; keep still and retry.");
        a2PreCalStillHoldStartMs = 0;
        return;
      }
    }
    a2DrainPreCalibrationQueues();
    resetV44K11CalibrationObservation();
    wt901K11Start();
    torsoRefRebaselineUpperFromLatest();
    torsoRefRebaselineTorsoFromLatest();
    a2PreCalPhase = A2_PRECAL_OFF;
    a2PreCalStillHoldStartMs = 0;
    k11LastFeedbackPhase = K11_BIAS_STILL;
    Serial.println("V4_4_START_POSE_GATE_PASSED: first-group reference saved; K11 gyro bias + upper-arm baseline sampling begins now.");
    if (logger.isActive()) logger.logMarker("V4_4_START_POSE_GATE_PASSED_BIAS_BEGIN");
    buzzerBeep(0);
    uiDirty = true;
  }
}


static void resetV44K11CalibrationObservation() {
  k11MotionCalibrationAccepted = false;
  k11CalObservedUpperMaxDeg = 0.0f;
  k11CalObservedPlaneMaxDeg = 0.0f;
  k11CalObservedTorsoMaxDeg = 0.0f;
}

static void observeV44K11CalibrationMotion() {
  if (!(wt901K11AxisFlexActive() || wt901K11AxisReturnActive())) return;
  float upper = torsoRefUpperDeviationDeg();
  // V4.4.25 calibration gate MUST NOT use the V4.4.24 E-yaw-compensated
  // formal plane. During K11 calibration the waist IMU absolute yaw can jump/drift
  // tens of degrees even while the torso gravity/tilt is stationary, which falsely
  // turns a clean X-Z curl into a 60-90 deg plane error and restarts CAL1 forever.
  // The BODY-FRONT plane is already explicitly calibrated before K11, so for this
  // one calibration movement use the raw B forearm plane against that fixed plane,
  // plus A-side as a shoulder-side safety cue. Formal training P remains V4.4.24.
  float rawForePlane = v449ForeQuatPlaneRawDeg();
  float upperSide = v449UpperSidePlaneDeg();
  float plane = rawForePlane > upperSide ? rawForePlane : upperSide;
  float torso = torsoRefTorsoTiltDeviationDeg();
  if (isfinite(upper) && upper > k11CalObservedUpperMaxDeg) k11CalObservedUpperMaxDeg = upper;
  if (isfinite(plane) && plane >= 0.0f && plane < 900.0f && plane > k11CalObservedPlaneMaxDeg) k11CalObservedPlaneMaxDeg = plane;
  if (isfinite(torso) && torso > k11CalObservedTorsoMaxDeg) k11CalObservedTorsoMaxDeg = torso;
}

// Called only after the low-level K11 state machine has reached READY.
// V4.4.31: K11 calibration is ONLY for learning the elbow hinge/motion axis.
// Do not reject that learned axis with the absolute B quaternion plane. V4.4.30
// deliberately removed absolute B heading from formal P because it can sit at
// 50-60 deg even for a clean X-O-Z curl. Reusing that discarded metric here
// created an endless CAL -> READY -> REJECT -> CAL loop.
//
// BODY FRONT has already been learned in the explicit arm-front calibration.
// The STANDARD motion-plane reference is the K11 learned differential axis itself;
// therefore the calibration acceptance gate only keeps gross upper-arm and torso
// safety checks. rawPlaneMax remains in the log as DIAGNOSTIC ONLY.
static bool validateV44K11CalibrationReady() {
  if (!wt901K11Ready()) return false;
  if (k11MotionCalibrationAccepted) return true;

  bool upperBad = k11CalObservedUpperMaxDeg > V44_K11_CAL_UPPER_MAX_DEG;
  bool torsoBad = k11CalObservedTorsoMaxDeg > V44_K11_CAL_TORSO_MAX_DEG;

  if (upperBad || torsoBad) {
    Serial.printf(
      "V4_4_31_K11_CAL_REJECTED: upperMax=%.1f/%.1f torsoMax=%.1f/%.1f rawPlaneDiag=%.1f reasons=%s%s. "
      "raw B quaternion plane is DIAGNOSTIC_ONLY and cannot restart calibration. Return to start and repeat.\n",
      k11CalObservedUpperMaxDeg,V44_K11_CAL_UPPER_MAX_DEG,
      k11CalObservedTorsoMaxDeg,V44_K11_CAL_TORSO_MAX_DEG,
      k11CalObservedPlaneMaxDeg,
      upperBad?"UPPER_ARM ":"",torsoBad?"TORSO ":"");
    if (logger.isActive()) logger.logMarker("V4_4_31_K11_CAL_REJECTED_GROSS_MOTION_ONLY");
    buzzerDoubleBeep(0,100);
    resetV44K11CalibrationObservation();
    // User has already returned near K11 zero. Refresh only per-attempt/group baselines;
    // BODY FRONT / motion-plane reference stays fixed from the earlier explicit calibration.
    torsoRefRebaselineUpperFromLatest();
    torsoRefRebaselineTorsoFromLatest();
    wt901K11Start();
    k11LastFeedbackPhase = K11_BIAS_STILL;
    k11ReadyFeedbackDone = false;
    return false;
  }

  k11MotionCalibrationAccepted = true;
  // The accepted K11 movement is itself the one explicit STANDARD X-Z motion.
  // Re-zero A/B/E vertical-gyro headings here so the learned K11 horizontal axis
  // and the BODY relative-yaw trackers share exactly the same reference instant.
  wt901FusedVerticalYawReset(TORSO_REF_UPPER_SLOT);
  wt901FusedVerticalYawReset(TORSO_REF_FORE_SLOT);
  wt901FusedVerticalYawReset(TORSO_REF_SLOT);
  torsoRef.upperarmRelativeBodyYawDeg = 0.0f;
  torsoRef.forearmRelativeBodyYawDeg = 0.0f;
  Serial.printf(
    "V4_4_31_K11_CAL_ACCEPTED: hinge axis accepted; upperMax=%.1f rawPlaneDiag=%.1f torsoMax=%.1f. "
    "Calibration plane gate removed; formal P uses BODY-mapped A/B differential motion axis from A-E/B-E fused vertical gyro + A side tilt + K11 hinge; absolute B quaternion/Euler yaw is diagnostic only.\n",
    k11CalObservedUpperMaxDeg,k11CalObservedPlaneMaxDeg,k11CalObservedTorsoMaxDeg);
  if (logger.isActive()) logger.logMarker("V4_4_16_K11_CAL_ACCEPTED_AXIS_ONLY");
  return true;
}

bool waitRealImuReceiving() {
  for (int i = 0; i < 30; i++) {
    if (selectedPairReceiving()) return true;
    delay(50);
  }
  return selectedPairReceiving();
}

bool startRealTraining() {
  selectActiveImuPair();
  if (powerResumeAvailable) {
    powerResumeForceSavedPlan();
    powerResumeApplyPending = true;
    Serial.printf("POWER_RESUME_ARMED: saved progress will be restored after fresh K11 calibration (%d completed).\n",
                  powerResumeCheckpoint.totalCompletedSlots);
  } else {
    powerResumeApplyPending = false;
  }
  Serial.printf("UI_START V4.4.17: U/P separated; E-referenced body plane; E excluded from elbow ROM exercise=%d %s upper=%.1fdeg\n", exerciseIndex+1, exerciseName(), currentUpperThresholdDeg());
  if (!selectedBodyFrameReady()) {
    Serial.println("START_BLOCKED: torso reference E neutral calibration is not ready.");
    buzzerDoubleBeep(0,90); return false;
  }
  if (!waitRealImuReceiving()) {
    Serial.println("START_BLOCKED: one or more selected IMUs are not receiving.");
    wt901PrintStatus(); buzzerDoubleBeep(0,90); return false;
  }

  a2PreCalPhase=A2_PRECAL_OFF; a2GroupReacquireActive=false; a2GroupReacquireHoldStartMs=0;
  for(int i=0;i<2;++i){a2SessionStartPoseRef[i]=A2GravityPoseRef();a2GroupPoseDevDeg[i]=999.0f;}
  setIndex=1; trainingFinishHandled=false; trainingStartMs=millis(); lastRealUiUpdateMs=0; restStartMs=0; restRemainingSec=0;
  sessionBaselineRepCount=0; v4Assessment.reset(trainingStartMs); v4AssessmentRight.reset(trainingStartMs);
  resultView=0; k11LastFeedbackPhase=K11_IDLE; k11ReadyFeedbackDone=false; k11FailFeedbackDone=false;
  resetV44K11CalibrationObservation();
  resetCurrentGroupCountersAndStart();

  if (logger.startSession()) {
    logger.logMarker(exerciseCode());
    logger.logMarker("SENSORS_A_UPPER_B_FORE_D_TORSO");
    logger.logMarker("TORSO_REFERENCE_NEUTRAL_READY"); logger.logMarker("START_GROUP_1");
  } else Serial.println("V4_SD_WARNING: session continues without CSV persistence.");

  // New session gets a durable zero-progress checkpoint immediately.  On a resume
  // path keep the old checkpoint untouched until fresh calibration has succeeded.
  if (!powerResumeAvailable) powerResumeSave("SESSION_START");

  startA2PreCalibrationGate();
  if(logger.isActive()) logger.logMarker("V4_4_START_POSE_GATE_K11_OFF_UNTIL_VALID");
  Serial.printf("V4.4 training armed: action=%d; sagittal plane comes from E+BODY_FRONT; K11 movement learns hinge axis only.\n",exerciseIndex+1);
  return true;
}

void pauseRealTraining() {
  if (!selectedK11TrainingReady()) { Serial.println("K11_CALIBRATION_ACTIVE: pause ignored until K11 axis calibration is accepted."); return; }
  if (!anyRunning()) return;
  if (leftTraining.data.state==RUNNING) leftTraining.startOrPause();
  if (rightTraining.data.state==RUNNING) rightTraining.startOrPause();
  selectedK11SetPaused(true); syncUiDataFromTraining();
  productOrchestrator.pause();
  if(logger.isActive())logger.logMarker("PAUSE"); buzzerBeep(0);
  powerResumeSave("PAUSE");
  Serial.println("PAUSED. Return A/B to the exercise start pose before resume; normal breathing is allowed.");
  queueVoiceEvent("PAUSE", rehabVoice.queuePause());
}

void resumeRealTraining() {
  if(!anyPaused())return;
  if(!exerciseStartPoseValid(true)||!a2BothImuFreshAndStill()){
    Serial.println("V4_4_RESUME_BLOCKED: A/B must return to the start pose and A/B must be reasonably steady; E breathing does not block resume.");
    buzzerDoubleBeep(0,100);return;
  }
  selectedK11RezeroFormal();
  bool lbase=torsoRefRebaselineUpperFromLatest();
  bool tbase=torsoRefRebaselineTorsoFromLatest();
  if(!lbase||!tbase){Serial.println("V4_4_RESUME_BLOCKED: A/E rebaseline failed.");buzzerDoubleBeep(0,100);return;}
  if(leftTraining.data.state==PAUSED)leftTraining.startOrPause();
  if(rightTraining.data.state==PAUSED)rightTraining.startOrPause();
  if(leftSideActive())leftTraining.armForNextRepFromCurrentAngle(0.0f,0.0f);
  if(rightSideActive())rightTraining.armForNextRepFromCurrentAngle(0.0f,0.0f);
  bilateralLeftSlotWaiting=bilateralRightSlotWaiting=false;
  resetSpeedReference(0.0f,0.0f);syncUiDataFromTraining();selectedK11SetPaused(false);
  productOrchestrator.resume();
  if(logger.isActive())logger.logMarker("RESUME_FROM_START_POSE_REZERO_REBASELINE");buzzerBeep(0);
  Serial.println("RESUMED at confirmed start pose; partial pre-pause motion discarded.");
  queueVoiceEvent("RESUME", rehabVoice.queueResume());
}

void buildV4SummaryLine(char *buf, size_t bufSize) {
  int targetTotal = max(1, targetSets * targetReps);
  snprintf(buf, bufSize,
    "SLOTS=%d/%d,PASS=%d,FAIL=%d,PASS_RATE=%.1f,FIRST_TRY_PASS=%d,FIRST_TRY_RATE=%.1f,ATTEMPTS=%d,RETRIES=%d,RECOVERED=%d,RETRY_RECOVERY_RATE=%.1f,"
    "FINAL_ROM_LOW=%d,FINAL_UPPER=%d,FINAL_PLANE=%d,FINAL_TORSO=%d,FINAL_MULTI=%d,"
    "THRESH_UPPER=%.1f,THRESH_PLANE=%.1f,THRESH_TORSO=%.1f,AVG_UPPER=%.1f,AVG_PLANE=%.1f,AVG_TORSO=%.1f,"
    "PASS_ROM_AVG=%.1f,PASS_ROM_MIN=%.1f,PASS_ROM_MAX=%.1f,PASS_ROM_SD=%.1f,AVG_ATTEMPT_SPEED_EST=%.1f,AVG_ATTEMPT_TIME=%.2f,ACCOUNTING_OK=%d,SUMMARY=%s",
    v4Assessment.completedSlots,targetTotal,v4Assessment.passedSlots,v4Assessment.failedSlots,
    v4Assessment.passRatePct(),v4Assessment.firstTryPassedSlots(),v4Assessment.firstTryPassRatePct(),
    v4Assessment.totalAttempts,v4Assessment.retryAttempts,v4Assessment.recoveredOnRetry,v4Assessment.retryRecoveryRatePct(),
    v4Assessment.finalRomLowSlots,v4Assessment.finalUpperArmExcessSlots,v4Assessment.finalPlaneDeviationSlots,
    v4Assessment.finalTorsoCompensationSlots,v4Assessment.finalMultiIssueSlots,
    currentUpperThresholdDeg(),currentPlaneThresholdDeg(),currentTorsoThresholdDeg(),
    v4Assessment.avgAttemptUpperArmMaxDeg(),v4Assessment.avgAttemptPlaneMaxDeg(),v4Assessment.avgAttemptTorsoMaxDeg(),
    v4Assessment.passRomAvgDeg(),v4Assessment.passRomMin,v4Assessment.passRomMax,v4Assessment.passRomSdDeg(),
    v4Assessment.avgAttemptMeanSpeedDegS(),v4Assessment.avgAttemptDurationSec(),
    v4Assessment.accountingOk()?1:0,v4Assessment.summaryCode(targetTotal));
}

void buildV4SummaryLineFor(const V4SessionAssessment &a, const char *sideLabel, char *buf, size_t bufSize) {
  int targetTotal=max(1,targetSets*targetReps);
  snprintf(buf,bufSize,
    "%s:SLOTS=%d/%d,PASS=%d,FAIL=%d,PASS_RATE=%.1f,FIRST_TRY_PASS=%d,FIRST_TRY_RATE=%.1f,ATTEMPTS=%d,RETRIES=%d,RECOVERED=%d,RETRY_RECOVERY_RATE=%.1f,"
    "FINAL_ROM_LOW=%d,FINAL_UPPER=%d,FINAL_PLANE=%d,FINAL_TORSO=%d,FINAL_MULTI=%d,"
    "THRESH_UPPER=%.1f,THRESH_PLANE=%.1f,THRESH_TORSO=%.1f,AVG_UPPER=%.1f,AVG_PLANE=%.1f,AVG_TORSO=%.1f,"
    "PASS_ROM_AVG=%.1f,PASS_ROM_MIN=%.1f,PASS_ROM_MAX=%.1f,PASS_ROM_SD=%.1f,ACCOUNTING_OK=%d,SUMMARY=%s",
    sideLabel,a.completedSlots,targetTotal,a.passedSlots,a.failedSlots,a.passRatePct(),a.firstTryPassedSlots(),a.firstTryPassRatePct(),
    a.totalAttempts,a.retryAttempts,a.recoveredOnRetry,a.retryRecoveryRatePct(),
    a.finalRomLowSlots,a.finalUpperArmExcessSlots,a.finalPlaneDeviationSlots,a.finalTorsoCompensationSlots,a.finalMultiIssueSlots,
    currentUpperThresholdDeg(),currentPlaneThresholdDeg(),currentTorsoThresholdDeg(),
    a.avgAttemptUpperArmMaxDeg(),a.avgAttemptPlaneMaxDeg(),a.avgAttemptTorsoMaxDeg(),
    a.passRomAvgDeg(),a.passRomMin,a.passRomMax,a.passRomSdDeg(),a.accountingOk()?1:0,a.summaryCode(targetTotal));
}

void printV4SessionSummary() {
  char line[512];
  if(bodyIndex==1) buildV4SummaryLineFor(v4AssessmentRight,"RIGHT",line,sizeof(line));
  else buildV4SummaryLineFor(v4Assessment,"LEFT",line,sizeof(line));
  Serial.print("V4_4_SESSION_SUMMARY: "); Serial.println(line);
  if(bilateralMode()){ char r[512]; buildV4SummaryLineFor(v4AssessmentRight,"RIGHT",r,sizeof(r)); Serial.print("V4_4_SESSION_SUMMARY: "); Serial.println(r); }
}

void stopRealTraining() {
  const bool shouldAnnounceStop = !trainingFinishHandled &&
    (page == PAGE_TRAINING || page == PAGE_REST);
  if(logger.isActive()){
    logger.logNow(lastLeftSignedAngle,leftTraining.data.currentAngle,leftTraining.data,lastRightSignedAngle,rightTraining.data.currentAngle,rightTraining.data,
      "none",leftSpeedDegS,rightSpeedDegS,currentModeCode(),currentExerciseCode());
    char l[512]; if(bodyIndex==1)buildV4SummaryLineFor(v4AssessmentRight,"RIGHT",l,sizeof(l));else buildV4SummaryLineFor(v4Assessment,"LEFT",l,sizeof(l)); logger.logSummary(l);
    if(bilateralMode()){char r[512];buildV4SummaryLineFor(v4AssessmentRight,"RIGHT",r,sizeof(r));logger.logSummary(r);}
    logger.endSession("STOPPED");
  }
  printV4SessionSummary();
  if(leftTraining.data.state==RUNNING||leftTraining.data.state==PAUSED)leftTraining.stop();
  if(rightTraining.data.state==RUNNING||rightTraining.data.state==PAUSED)rightTraining.stop();
  selectedK11SetPaused(true);trainingFinishHandled=true;syncUiDataFromTraining();buzzerDoubleBeep(0,90);
  productOrchestrator.stop();
  Serial.println("STOPPED from V4.4.16 UI.");
  // Explicit STOP/HOME means the user intentionally ended the session; do not
  // offer it as a power-loss resume on the next boot.
  powerResumeClear("USER_STOP");
  if (shouldAnnounceStop) queueVoiceEvent("STOP", rehabVoice.queueStop());
}

void updateRealTraining() {
  if (!systemReady) return;

  if (page==PAGE_TRAINING && anyRunning() && !trainingFinishHandled) updateA2PreCalibrationGate();

  // V4.4.24: keep BODY/E reference tracking alive through REST as well.
  // V4.4.22 could miss an entire X-Z -> Y-Z plane change if the user rotated
  // the arm during rest, because the A-E tracker was frozen and intentionally
  // refused to bridge the sample gap when the next group started.
  bool torsoTrackingWindow = !trainingFinishHandled && !a2PreCalibrationActive() &&
    (selectedK11CalibrationActive() || page==PAGE_TRAINING || page==PAGE_REST);
  if (torsoTrackingWindow) torsoRefUpdateMetrics();

  bool estimatorWindow = !trainingFinishHandled && !a2PreCalibrationActive() &&
    (selectedK11CalibrationActive() || (page==PAGE_TRAINING && anyRunning()));
  if (estimatorWindow) {
    observeV44K11CalibrationMotion();
  }
  wt901K11Update();
  if (bilateralMode()) K11R::rWt901K11Update();

  if (millis()-lastImuStatusTime>=5000) { lastImuStatusTime=millis(); if(!wt901AllReceiving())wt901PrintStatus(); }
  updateRestState();
  updateA2GroupReacquireGate();

  if (page==PAGE_TRAINING && !trainingFinishHandled) {
    if (selectedK11Failed() && !k11FailFeedbackDone) {
      k11FailFeedbackDone=true; buzzerDoubleBeep(0,120);
      if(logger.isActive())logger.logMarker("V4_4_K11_CAL_FAILED");

      // V4.4.17 screen-flow recovery: older builds mapped FAILED back to CAL1
      // without restarting K11, so the display looked like a retry but the main
      // controller was permanently stuck in FAILED. In integrated screen flow,
      // genuinely failed calibration now re-arms the real bias/calibration state
      // machine so CAL1 is an actual retry rather than a dead-end page.
      if (integratedScreenFlowActive) {
        Serial.println("SCREEN_CAL_RETRY: K11 failed -> re-arm real calibration at CAL1");
        resetV44K11CalibrationObservation();
        torsoRefRebaselineUpperFromLatest();
        torsoRefRebaselineTorsoFromLatest();
        wt901K11Start();
        k11LastFeedbackPhase = K11_BIAS_STILL;
        k11ReadyFeedbackDone = false;
        k11FailFeedbackDone = false;
        sendScreenLiveFrame(true);
      }
    }
    if (wt901K11Ready() && !k11MotionCalibrationAccepted) {
      validateV44K11CalibrationReady();
    }
    if (selectedK11TrainingReady() && !k11ReadyFeedbackDone) {
      k11ReadyFeedbackDone=true;
      bool lbase=torsoRefRebaselineUpperFromLatest();
      bool tbase=torsoRefRebaselineTorsoFromLatest();
      v4431fPlane.baselineReady=false;
      v4431fPlaneResetBaseline("TRAINING_READY");
      // Power-loss resume is injected ONLY after a brand-new body/K11 calibration.
      // This restores counters/plan, never stale sensor references.
      powerResumeApplyAfterFreshCalibration();
      if(leftSideActive())leftTraining.armForNextRepFromCurrentAngle(0.0f,torsoRefUpperDeviationDeg());
      bilateralLeftSlotWaiting=bilateralRightSlotWaiting=false;
      resetSpeedReference(0.0f,0.0f);
      buzzerDoubleBeep(0,90);
      Serial.printf("V4_4_29_TRAINING_READY: CONTINUOUS=1 NO_PER_REP_ANCHOR=1 U=UPPER_SAGITTAL_INPLANE P=MAX(B_BODY_PLANE_E_GYRO_TURN,A_SIDE,K11_HINGE); BODY_AZ_DIAG_ONLY B_BODY_PLANE=%.1f SIDE_B_DIAG_ONLY=%.1f baseline=%d torsoTilt=%.1f bodyAz=%.1f sideA=%.1f hinge=%.1f BODY_REF=%d E_USED_FOR_PLANE=1 E_USED_FOR_ELBOW=0\n",
        v449ForeQuatPlaneDeg(),v449ForeSidePlaneDeg(),(lbase&&tbase)?1:0,torsoRefTorsoTiltDeviationDeg(),v449BodyRelativeAzimuthDeg(),v449UpperSidePlaneDeg(),v449HingePlaneDeviationDeg(),torsoRefBodyPlaneReferenceValid()?1:0);
      if(logger.isActive())logger.logMarker("V4_4_17_TRAINING_READY");
      if (integratedScreenFlowActive) {
        integratedCountdownActive = true;
        integratedCountdownStartedMs = millis();
        selectedK11SetPaused(true);
        Serial.println("SCREEN_STATE: COUNTDOWN armed; waiting for COUNTDOWN_DONE");
      } else {
        queueVoiceEvent("TRAINING_START", rehabVoice.queueTrainingStart());
      }
      uiDirty=true;
      sendScreenLiveFrame(true);
    }
  }

  if (page==PAGE_TRAINING && anyRunning() && !trainingFinishHandled && !selectedK11TrainingReady()) {
    logger.update(0.0f,0.0f,leftTraining.data,0.0f,0.0f,rightTraining.data,
      "none",0.0f,0.0f,currentModeCode(),currentExerciseCode());
  }

  if (page==PAGE_TRAINING && anyRunning() && !trainingFinishHandled && selectedK11TrainingReady() && !integratedCountdownActive) {
    float lAngle=wt901K11ElbowAngleDeg();
    float lSpeed=wt901K11ElbowSpeedDegS();
    float lUpper=torsoRefUpperDeviationDeg();
    float lPlaneBodyAz=v449BodyRelativeAzimuthDeg();            // diagnostic only
    float lPlaneUpperSide=v4431fPerRepPlaneDeg();               // ONLY formal P component
    float lPlaneForeQuat=v4430ForearmMotionPlaneDeg();          // diagnostic only
    float lPlaneHinge=v449HingePlaneDeviationDeg();             // diagnostic only
    float lPlane=lPlaneUpperSide;                               // ONLY formal P
    float lTorso=torsoRefTorsoTiltDeviationDeg();
    float rAngle=0,rSpeed=0,rUpper=0,rPlane=0,rTorso=0;
    lastLeftSignedAngle=lAngle; lastRightSignedAngle=rAngle; leftSpeedDegS=lSpeed; rightSpeedDegS=rSpeed;

    bool leftRepDone=false,rightRepDone=false;
    if(leftSideActive() && !bilateralLeftSlotWaiting) {
      int old=leftTraining.data.completedMotionCount;
      leftTraining.update(lAngle,lUpper,lPlane,lTorso,lSpeed,millis(),lPlaneBodyAz,lPlaneUpperSide,lPlaneForeQuat,lPlaneHinge);
      leftRepDone=leftTraining.data.completedMotionCount>old;
      if(leftRepDone){
        v4431fPlaneResetBaseline("ATTEMPT_DONE");
      }
    }
    if(rightSideActive() && !bilateralRightSlotWaiting) {
      int old=rightTraining.data.completedMotionCount;
      rightTraining.update(rAngle,rUpper,rPlane,rTorso,rSpeed,millis());
      rightRepDone=rightTraining.data.completedMotionCount>old;
    }

    auto finishAttemptSide = [&](int side, TrainingLogic &logic, V4SessionAssessment &assess, float angle, float upper) {
      assess.recordAttempt(logic.data);
      // V4.4.21: only now, after TrainingLogic::finalizeAttempt() has closed the motion,
      // may the screen show an error result.  No in-motion popup is generated.
      latchCompletedAttemptFeedback(logic.data);
      const char *sideName=side==0?"LEFT":"RIGHT";
      // currentCount advances only when a planned slot closes. On a first-failed
      // attempt it therefore still points to the previous completed slot.
      const int plannedSlot = logic.data.lastSlotCompleted ? logic.data.currentCount : (logic.data.currentCount + 1);
      Serial.printf("V4_4_16_ATTEMPT_%s: SLOT=%d/%d TRY=%d QUALITY=%s PEAK=%.1f EVAL_MAX[U=%.1f P=%.1f T=%.1f] P_SOURCE=%s P_PARTS[AZ=%.1f A_SIDE=%.1f MOTION_PLANE=%.1f HINGE=%.1f] RAW_DIAG_MAX[U=%.1f P=%.1f T=%.1f] SEVERE[U=%d P=%d T=%d] FLAGS[R=%d U=%d P=%d T=%d] BAD_PCT[U=%.0f P=%.0f T=%.0f] MAX_RUN_MS[U=%lu P=%lu T=%lu] EVAL_MS=%lu RETRY=%d SLOT_DONE=%d PASS=%d\n",
        sideName,plannedSlot,logic.data.targetCount,logic.data.lastAttemptWasRetry?2:1,
        repQualityCodeName(logic.data.lastRepQuality),logic.data.lastCompletedRepPeakAngle,
        logic.data.lastCompletedRepEvalMaxUpperArmDev,logic.data.lastCompletedRepEvalMaxPlaneDev,logic.data.lastCompletedRepEvalMaxTorsoDev,
        planeDeviationSourceName(logic.data.lastPlaneSource),
        logic.data.lastCompletedRepEvalMaxPlaneBodyAzimuth,logic.data.lastCompletedRepEvalMaxPlaneUpperSide,
        logic.data.lastCompletedRepEvalMaxPlaneForearmQuat,logic.data.lastCompletedRepEvalMaxPlaneHinge,
        logic.data.lastCompletedRepMaxUpperArmDev,logic.data.lastCompletedRepMaxPlaneDev,logic.data.lastCompletedRepMaxTorsoDev,
        logic.data.lastUpperSevere?1:0,logic.data.lastPlaneSevere?1:0,logic.data.lastTorsoSevere?1:0,
        logic.data.lastRomLow?1:0,logic.data.lastUpperArmExcess?1:0,logic.data.lastPlaneDeviation?1:0,logic.data.lastTorsoCompensation?1:0,
        logic.data.lastCompletedRepUpperBadPct,logic.data.lastCompletedRepPlaneBadPct,logic.data.lastCompletedRepTorsoBadPct,
        logic.data.lastCompletedRepUpperMaxBadRunMs,logic.data.lastCompletedRepPlaneMaxBadRunMs,logic.data.lastCompletedRepTorsoMaxBadRunMs,
        logic.data.lastCompletedRepQualityEvalMs,
        logic.data.retryPending?1:0,logic.data.lastSlotCompleted?1:0,logic.data.lastSlotPassed?1:0);

      if(logic.data.retryPending) {
        buzzerDoubleBeep(0,90);
        if(side==0) { wt901K11RezeroFormal(); leftTraining.armForNextRepFromCurrentAngle(0.0f,upper); }
        else if(bilateralMode()) { K11R::rWt901K11RezeroFormal(); rightTraining.armForNextRepFromCurrentAngle(0.0f,upper); }
        else { wt901K11RezeroFormal(); rightTraining.armForNextRepFromCurrentAngle(0.0f,upper); }
      } else if(logic.data.lastSlotCompleted) {
        if(logic.data.lastSlotPassed)buzzerBeep(0);else buzzerDoubleBeep(0,120);
        if(bilateralMode()) {
          if(side==0){bilateralLeftSlotWaiting=true;wt901K11SetPaused(true);}
          else {bilateralRightSlotWaiting=true;K11R::rWt901K11SetPaused(true);}
        } else if(!bothFinished()) {
          wt901K11RezeroFormal(); logic.armForNextRepFromCurrentAngle(0.0f,upper);
        }
      }
    };

    if(leftRepDone)finishAttemptSide(0,leftTraining,v4Assessment,lAngle,lUpper);
    if(rightRepDone)finishAttemptSide(1,rightTraining,v4AssessmentRight,rAngle,rUpper);
    // Persist only at a CLOSED planned slot. Mid-rep power loss repeats that slot,
    // so a sudden outage can never create a phantom completed repetition.
    if (leftRepDone && leftTraining.data.lastSlotCompleted) powerResumeSave("SLOT_DONE");

    // Bilateral synchronization contract: a planned repetition advances only after BOTH sides
    // have closed that same slot. A side that already closed its slot stays frozen while the
    // other side performs its one allowed retry.
    if(bilateralMode() && bilateralLeftSlotWaiting && bilateralRightSlotWaiting && !bothFinished()) {
      wt901K11RezeroFormal(); K11R::rWt901K11RezeroFormal();
      leftTraining.armForNextRepFromCurrentAngle(0.0f,lUpper);
      rightTraining.armForNextRepFromCurrentAngle(0.0f,rUpper);
      bilateralLeftSlotWaiting=bilateralRightSlotWaiting=false;
      wt901K11SetPaused(false); K11R::rWt901K11SetPaused(false);
      Serial.printf("V4_4_LEGACY_BILATERAL_SLOT_SYNCED_UNUSED: next planned slot=%d\n",min(leftTraining.data.currentCount,rightTraining.data.currentCount)+1);
    }

    syncUiDataFromTraining();
    const char *event=leftRepDone&&rightRepDone?"bilateral_attempts_closed":(leftRepDone?"left_attempt_closed":(rightRepDone?"right_attempt_closed":"none"));
    logger.update(lAngle,lAngle,leftTraining.data,rAngle,rAngle,rightTraining.data,event,lSpeed,rSpeed,currentModeCode(),currentExerciseCode());

    if(bothFinished() && !trainingFinishHandled) {
      syncUiDataFromTraining();
      if(setIndex<targetSets){selectedK11SetPaused(true);enterRestAfterGroup();return;}
      if(logger.isActive()) {
        logger.logMarker("ALL_GROUPS_FINISHED");
        char line[512];
        if(bodyIndex==1) buildV4SummaryLineFor(v4AssessmentRight,"RIGHT",line,sizeof(line));
        else buildV4SummaryLineFor(v4Assessment,"LEFT",line,sizeof(line));
        logger.logSummary(line);
        if(bilateralMode()) { char rightLine[512]; buildV4SummaryLineFor(v4AssessmentRight,"RIGHT",rightLine,sizeof(rightLine)); logger.logSummary(rightLine); }
        logger.endSession("ALL_FINISHED");
      }
      printV4SessionSummary();
      queueVoiceEvent("TRAINING_DONE", rehabVoice.queueTrainingDone());
      powerResumeClear("ALL_FINISHED");
      selectedK11SetPaused(true);trainingFinishHandled=true;buzzerDoubleBeep(0,100);
      page=PAGE_RESULT;lastPage=PAGE_COUNT;uiDirty=true;return;
    }
  }

  if(page==PAGE_TRAINING && millis()-lastRealUiUpdateMs>=200){lastRealUiUpdateMs=millis();syncUiDataFromTraining();uiDirty=true;}
  if(page==PAGE_TRAINING && millis()-lastSerialStatusMs>=1000){
    lastSerialStatusMs=millis();
    Serial.printf("REAL_UI V4_4_16 READY=%d ANG=%.1f COUNT=%d RETRY=%d PLANE_DEV=%.1f AZIMUTH_A_E=%.1f A_TWIST=%.1f E_TWIST=%.1f SIDE_A=%.1f MOTION_PLANE=%.1f SIDE_B_DIAG=%.1f HINGE_PLANE=%.1f UPPER_INPLANE=%.1f TORSO_E_TILT=%.1f BODY_REF=%d SD=",
      selectedK11TrainingReady()?1:0,leftAngle,leftCount,leftTraining.data.retryPending?1:0,
      v449FormalPlaneDeviationDeg(),v449BodyRelativeAzimuthDeg(),torsoRefArmVerticalTwistDeltaDeg(),torsoRefTorsoVerticalTwistDeltaDeg(),
      v449UpperSidePlaneDeg(),v4430ForearmMotionPlaneDeg(),v449ForeSidePlaneDeg(),v449HingePlaneDeviationDeg(),
      torsoRefUpperDeviationDeg(),torsoRefTorsoTiltDeviationDeg(),torsoRefBodyPlaneReferenceValid()?1:0);
    if(!logger.isReady())Serial.println("ERROR");else if(logger.isActive()){Serial.print("REC ");Serial.println(logger.fileName());}else Serial.println("READY");
  }
}

void drawBootStatus(const char *line1, const char *line2) {
  tft.fillScreen(C_BG);
  drawHeader("首页", 1);
  drawOptionCard(38, 105, 404, 80, line1, line2, true, C_ACCENT);
  drawCnText(52, 215, "AB左 / CD右 / E腰腹", C_MUTED, 1);
  drawFooter("WAIT  BLE IMU");
}

void goPage(int next) {
  page = (UiPage)next;
  uiDirty = true;
  printStatusToSerial();
}

// Explicit forward declaration keeps both Arduino preprocessing and cross-toolchain builds deterministic.
void applyTrainingParamsToLogic();

void applyDefaultsForBody() {
  targetAngle = 80;
  validAngle = max(10, targetAngle / 2);  // attempt-recognition line = 50% target extension
  returnAngle = 20;                      // return to flexed-behind-head start pose
  applyTrainingParamsToLogic();
}

void handleLeftRight(int dir) {
  switch (page) {
    case PAGE_BODY_SELECT:
      bodyIndex = 0;
      uiDirty = true;
      break;
    case PAGE_EXERCISE_SELECT:
      exerciseIndex += dir;
      if (exerciseIndex < 0) exerciseIndex = RM_ACTION_COUNT - 1;
      if (exerciseIndex >= RM_ACTION_COUNT) exerciseIndex = 0;
      applySelectedActionDefaults();
      applyDefaultsForBody();
      uiDirty = true;
      break;
    case PAGE_MODE_SELECT:
      modeIndex += dir;
      if (modeIndex < 0) modeIndex = 2;
      if (modeIndex > 2) modeIndex = 0;
      uiDirty = true;
      break;
    case PAGE_PARAM_SET:
      if (paramField == 0) {
        targetSets += dir;
        targetSets = constrain(targetSets, 1, 5);
      } else if (paramField == 1) {
        targetReps += dir;
        targetReps = constrain(targetReps, 5, 30);
      } else if (paramField == 2) {
        targetAngle += dir * 5;
        targetAngle = constrain(targetAngle, 30, 120);
        validAngle = max(10, targetAngle / 2);  // K12.2: 50% target = attempt recognition, not success
        returnAngle = 20;  // V4 A2: accepted return is relative to the calibrated flexed start pose
      } else if (paramField == 3) {
        restSec += dir * 5;
        restSec = constrain(restSec, 10, 120);
      }
      // Keep IDLE JSON target_count/angle aligned with the parameter page.
      applyTrainingParamsToLogic();
      uiDirty = true;
      break;
    case PAGE_TRAINING:
      trainView = 1 - trainView;
      lastPage = PAGE_COUNT;   // 切换数据/质量页时需要重画结构
      uiDirty = true;
      break;
    case PAGE_RESULT:
      resultView = 1 - resultView;
      lastPage = PAGE_COUNT;
      uiDirty = true;
      break;
    default:
      break;
  }
}

void handleOk() {
  switch (page) {
    case PAGE_HOME:
      goPage(PAGE_EXERCISE_SELECT);
      break;
    case PAGE_BODY_SELECT:
      goPage(PAGE_EXERCISE_SELECT);
      break;
    case PAGE_EXERCISE_SELECT:
      goPage(PAGE_MODE_SELECT);
      break;
    case PAGE_MODE_SELECT:
      paramField = 0;
      goPage(PAGE_PARAM_SET);
      break;
    case PAGE_PARAM_SET:
      paramField++;
      if (paramField >= 4) {
        paramField = 0;
        BodyFrameSideCal freshBf; bodyFrameCal[0] = freshBf; bodyFrameActiveSide = 0; torsoRef = TorsoReferenceState();
        goPage(PAGE_CALIBRATION);
      } else {
        uiDirty = true;
      }
      break;
    case PAGE_CALIBRATION:
      bodyFrameActiveSide = 0;
      if (bodyFramePhase() == BF_OFF || bodyFramePhase() == BF_FAILED) {
        bodyFrameStart(0, 0, 1);
        queueVoiceEvent("CALIBRATION_START", rehabVoice.queueCalibrationStart());
        lastPage=PAGE_COUNT; uiDirty=true;
      } else if (bodyFramePhase() == BF_READY) {
        if (startRealTraining()) goPage(PAGE_TRAINING); else {lastPage=PAGE_COUNT;uiDirty=true;}
      } else {
        Serial.println("TORSO_REF_ACTIVE: finish the current neutral/front calibration step first."); buzzerBeep(0);
      }
      break;
    case PAGE_TRAINING:
      if (!selectedK11TrainingReady()) {
        Serial.println("K11_CALIBRATION_ACTIVE: confirm key ignored until K11 axis calibration is accepted.");
        buzzerBeep(0);
      } else if (anyRunning()) {
        pauseRealTraining();
      } else if (anyPaused()) {
        resumeRealTraining();
      }
      lastPage = PAGE_COUNT;   // 暂停标题变化，需要重画页头
      uiDirty = true;
      break;
    case PAGE_REST:
      startNextGroupAfterRest();
      break;
    case PAGE_RESULT:
      goPage(PAGE_HOME);
      break;
    default:
      break;
  }
}

void handleBack() {
  switch (page) {
    case PAGE_HOME:
      break;
    case PAGE_BODY_SELECT:
      goPage(PAGE_HOME);
      break;
    case PAGE_EXERCISE_SELECT:
      goPage(PAGE_HOME);
      break;
    case PAGE_MODE_SELECT:
      goPage(PAGE_EXERCISE_SELECT);
      break;
    case PAGE_PARAM_SET:
      goPage(PAGE_MODE_SELECT);
      break;
    case PAGE_CALIBRATION:
      goPage(PAGE_PARAM_SET);
      break;
    case PAGE_TRAINING:
      stopRealTraining();
      goPage(PAGE_RESULT);
      break;
    case PAGE_REST:
      stopRealTraining();
      goPage(PAGE_RESULT);
      break;
    case PAGE_RESULT:
      goPage(PAGE_HOME);
      break;
    default:
      break;
  }
}

void handleSerialKey(char c) {
  if (c == '\r' || c == '\n' || c == ' ') return;
  c = (char)tolower(c);
  Serial.print("按键=");
  Serial.println(c);
  if (c == 'l') {
    handleLeftRight(-1);
  } else if (c == 'r') {
    handleLeftRight(1);
  } else if (c == 'o' || c == 's') {
    handleOk();
  } else if (c == 'b') {
    handleBack();
  } else if (c == 'p') {
    if (page == PAGE_TRAINING) {
      if (anyRunning()) pauseRealTraining();
      else if (anyPaused()) resumeRealTraining();
      lastPage = PAGE_COUNT;
      uiDirty = true;
    }
  } else if (c == 'x') {
    if (page == PAGE_TRAINING || page == PAGE_REST) {
      stopRealTraining();
      goPage(PAGE_RESULT);
    }
  } else if (c == 'i') {
    wt901UpdateAngles();
    wt901PrintStatus();
  } else if (c == 'z') {
    powerResumeClear("SERIAL_CLEAR");
    sendScreenLiveFrame(true);
  } else if (c == 'h' || c == '?') {
    Serial.println("按键: l=左 r=右 o=确认 b=返回 p=暂停 x=结束 i=IMU状态 z=清除断电续训记录");
    printStatusToSerial();
  }
}


// =====================================================
// V14 7-inch screen integration
// =====================================================
enum ScreenUiState : uint8_t {
  SCREEN_HOME = 0,
  SCREEN_WEAR = 1,
  SCREEN_BODY1 = 2,
  SCREEN_BODY2 = 3,
  SCREEN_CAL1 = 4,
  SCREEN_CAL2 = 5,
  SCREEN_CAL3 = 6,
  SCREEN_COUNTDOWN = 7,
  SCREEN_TRAIN = 8,
  SCREEN_PAUSE = 9,
  SCREEN_REST = 10,
  SCREEN_RESULT = 11
};

static uint8_t currentImuMask() {
  uint8_t mask = 0;
  for (int i = 0; i < 5; ++i) if (wt901SlotReceiving(i)) mask |= (uint8_t)(1U << i);
  return mask;
}

static uint8_t currentScreenState() {
  if (page == PAGE_HOME) return SCREEN_HOME;
  if (page == PAGE_BODY_SELECT) return SCREEN_WEAR;
  if (page == PAGE_CALIBRATION) {
    BodyFrameCalPhase p = bodyFramePhase();
    if (p == BF_DOWN_STILL || p == BF_OFF || p == BF_FAILED) return SCREEN_BODY1;
    if (p == BF_FRONT_RAISES) return SCREEN_BODY2;
    return SCREEN_CAL1;
  }
  if (page == PAGE_REST) return SCREEN_REST;
  if (page == PAGE_RESULT) return SCREEN_RESULT;
  if (page == PAGE_TRAINING) {
    if (integratedCountdownActive) return SCREEN_COUNTDOWN;
    if (anyPaused()) return SCREEN_PAUSE;
    if (a2PreCalibrationActive() || wt901K11BiasActive()) return SCREEN_CAL1;
    if (wt901K11AxisFlexActive()) return SCREEN_CAL2;
    if (wt901K11AxisReturnActive()) return SCREEN_CAL3;
    if (selectedK11TrainingReady()) return SCREEN_TRAIN;
    return SCREEN_CAL1;
  }
  return SCREEN_HOME;
}

static int currentScreenProgressPercent(uint8_t st) {
  // V4.4.20: during positioning/calibration the same LIVE "progress" field carries
  // the REAL detector/hold progress instead of sitting at the training completion value.
  float p = 0.0f;
  if (st == SCREEN_BODY1 || st == SCREEN_BODY2) {
    p = bodyFrameDownStillProgress();
  } else if (st == SCREEN_CAL1) {
    // CAL1 is two consecutive internal steps: semantic start-pose stillness,
    // then A/B gyro-bias sampling. Map them into one monotonic 0-100% bar.
    if (a2PreCalibrationActive()) p = 0.40f * a2StartStillProgress();
    else if (wt901K11BiasActive()) p = 0.40f + 0.60f * wt901K11BiasProgress();
  } else if (st == SCREEN_CAL2) {
    p = wt901K11AxisFlexActive() ? (wt901K11PeakDeg() / K11_CAL_FLEX_MIN_DEG) : 0.0f;
  } else if (st == SCREEN_CAL3) {
    if (wt901K11AxisReturnActive()) {
      const float peak = max(wt901K11PeakDeg(), K11_CAL_FLEX_MIN_DEG);
      const float denom = max(1.0f, peak - K11_CAL_RETURN_DEG);
      p = (peak - wt901K11CalAngleDeg()) / denom;
    }
  } else {
    return constrain(completionPercent, 0, 100);
  }
  if (!isfinite(p)) p = 0.0f;
  if (p < 0.0f) p = 0.0f;
  if (p > 1.0f) p = 1.0f;
  return constrain((int)lroundf(p * 100.0f), 0, 100);
}

// V4.4.21: user-facing error feedback is a RESULT, never a live pre-warning.
// The previous build ORed instantaneous U/P/T threshold crossings into the screen mask,
// so a transient sample could pop up while the repetition was still in progress.  That
// was both distracting and semantically wrong: the robust quality engine only decides a
// repetition in finalizeAttempt().  Latch the completed-attempt result briefly instead.
static constexpr unsigned long SCREEN_RESULT_FEEDBACK_MS = 1700UL;
static uint16_t screenResultFeedbackMask = 0;
static unsigned long screenResultFeedbackUntilMs = 0;

static uint16_t feedbackMaskFromCompletedAttempt(const TrainingData &d) {
  // bit0=ROM low, bit1=upper-arm excess, bit2=plane, bit3=torso, bit4=retry
  // bit5=completed GOOD repetition; bit6=power-loss session restored (UI only)
  uint16_t m = 0;
  if (d.lastRomLow) m |= 0x01;
  if (d.lastUpperArmExcess) m |= 0x02;
  if (d.lastPlaneDeviation) m |= 0x04;
  if (d.lastTorsoCompensation) m |= 0x08;
  if (d.retryPending) m |= 0x10;
  if (m == 0 && d.lastAttemptPassed) m |= 0x20;
  return m;
}

static void latchCompletedAttemptFeedback(const TrainingData &d) {
  screenResultFeedbackMask = feedbackMaskFromCompletedAttempt(d);
  if (screenResultFeedbackMask != 0) {
    screenResultFeedbackUntilMs = millis() + SCREEN_RESULT_FEEDBACK_MS;
  } else {
    screenResultFeedbackUntilMs = 0;
  }
}

static uint16_t currentFeedbackMask() {
  const unsigned long now = millis();
  if (screenResumeNoticeUntilMs != 0) {
    if ((int32_t)(screenResumeNoticeUntilMs - now) > 0) return 0x40;
    screenResumeNoticeUntilMs = 0;
  }
  if (screenResultFeedbackMask == 0) return 0;
  if ((int32_t)(screenResultFeedbackUntilMs - now) > 0) return screenResultFeedbackMask;
  screenResultFeedbackMask = 0;
  screenResultFeedbackUntilMs = 0;
  return 0;
}

static void sendScreenLiveFrame(bool force) {
  const unsigned long now = millis();
  if (!force && now - screenLastLiveTxMs < SCREEN_LIVE_INTERVAL_MS) return;
  screenLastLiveTxMs = now;

  syncUiDataFromTraining();
  const uint8_t st = currentScreenState();
  const uint8_t imu = currentImuMask();
  const int screenProgress = currentScreenProgressPercent(st);
  const int displayCount = leftCount;
  float displayAngle = leftAngle;
  if (!isfinite(displayAngle)) displayAngle = 0.0f;
  const int angle10 = (int)lroundf(displayAngle * 10.0f);
  const int targetAngle10 = targetAngle * 10;
  const int totalTarget = max(1, targetSets * targetReps);
  int totalDone = (setIndex - 1) * targetReps + displayCount;
  if (page == PAGE_RESULT) totalDone = max(totalDone, v4Assessment.completedSlots);
  totalDone = constrain(totalDone, 0, totalTarget);
  const int passPct10 = (int)lroundf(v4Assessment.passRatePct() * 10.0f);
  float romMax = v4Assessment.passRomMax;
  if (!isfinite(romMax) || romMax < 0.0f) romMax = leftRom;
  const int rom10 = (int)lroundf(max(0.0f, romMax) * 10.0f);
  const unsigned long durationSec = trainingStartMs ? (now - trainingStartMs) / 1000UL : 0UL;

  char payload[220];
  snprintf(payload, sizeof(payload),
    "R,%lu,%u,%u,%d,%d,%d,%d,%d,%d,%d,%d,%u,%d,%d,%d,%d,%lu,%d,%d",
    (unsigned long)++screenTxSeq,
    (unsigned)st,
    (unsigned)imu,
    setIndex,
    targetSets,
    displayCount,
    targetReps,
    angle10,
    targetAngle10,
    screenProgress,
    page == PAGE_REST ? restRemainingSec : 0,
    (unsigned)currentFeedbackMask(),
    totalDone,
    totalTarget,
    passPct10,
    rom10,
    durationSec,
    exerciseIndex,
    modeIndex);
  screenWritePacket(payload);
}

static void integratedBeginPositioning() {
  BodyFrameSideCal freshBf;
  bodyFrameCal[0] = freshBf;
  bodyFrameActiveSide = 0;
  torsoRef = TorsoReferenceState();
  integratedAutoTrainingStarted = false;
  integratedCountdownActive = false;
  integratedCountdownStartedMs = 0;
  page = PAGE_CALIBRATION;
  lastPage = PAGE_COUNT;
  uiDirty = false;
  bodyFrameStart(0, 0, 1);
  Serial.println("SCREEN_CMD: BEGIN_POSITION -> body-frame calibration started");
  queueVoiceEvent("CALIBRATION_START", rehabVoice.queueCalibrationStart());
  sendScreenLiveFrame(true);
}

static void integratedReleaseCountdown() {
  if (!integratedCountdownActive || !selectedK11TrainingReady()) return;
  selectedK11RezeroFormal();
  torsoRefRebaselineUpperFromLatest();
  torsoRefRebaselineTorsoFromLatest();
  if (leftSideActive()) leftTraining.armForNextRepFromCurrentAngle(0.0f, torsoRefUpperDeviationDeg());
  bilateralLeftSlotWaiting = bilateralRightSlotWaiting = false;
  resetSpeedReference(0.0f, 0.0f);
  integratedCountdownActive = false;
  integratedCountdownStartedMs = 0;
  selectedK11SetPaused(false);
  productOrchestrator.selectExercise(static_cast<rehab::ExerciseId>(rehabV5NormalizeActionIndex(exerciseIndex)));
  productOrchestrator.start();
  if (screenResumeNoticePending && !screenResumeNoticeShown) {
    screenResumeNoticePending = false;
    screenResumeNoticeShown = true;
    screenResumeNoticeUntilMs = millis() + 2300UL;
    Serial.println("SCREEN_RESUME_NOTICE: training restored overlay armed");
  }
  Serial.println("SCREEN_CMD: COUNTDOWN_DONE -> formal training released");
  queueVoiceEvent("TRAINING_START", rehabVoice.queueTrainingStart());
  if (logger.isActive()) logger.logMarker("SCREEN_COUNTDOWN_DONE_TRAINING_RELEASED");
  sendScreenLiveFrame(true);
}

static void integratedSkipRest() {
  if (page != PAGE_REST || setIndex >= targetSets) return;
  restRemainingSec = 0;
  restStartMs = millis() - (unsigned long)restSec * 1000UL;
  Serial.println("SCREEN_CMD: SKIP_REST -> next group direct");
  startNextGroupDirect("SKIP_REST");
}

static bool handleScreenCommand(const char *cmd) {
  if (!cmd || !*cmd) return false;

  // Unified eight-action selection. The active profile controls IMU pair, targets,
  // start-pose gate, quality vocabulary, screen telemetry and downstream detector routing.
  if (strncmp(cmd, "SELECT_EXERCISE:", 16) == 0) {
    if (page == PAGE_TRAINING || page == PAGE_REST) return false;
    int idx = atoi(cmd + 16);
    if (idx < 0 || idx >= RM_ACTION_COUNT) return false;
    exerciseIndex = idx;
    applySelectedActionDefaults();
    selectActiveImuPair();
    lastPage = PAGE_COUNT;
    uiDirty = true;
    Serial.printf("SCREEN_CMD: SELECT_EXERCISE -> %d %s (%s)\n", exerciseIndex + 1, exerciseName(), exerciseCode());
    sendScreenLiveFrame(true);
    return true;
  }
  if (strncmp(cmd, "SELECT_MODE:", 12) == 0) {
    if (page == PAGE_TRAINING || page == PAGE_REST) return false;
    int idx = atoi(cmd + 12);
    if (idx < 0 || idx > 2) return false;
    modeIndex = idx;
    Serial.printf("SCREEN_CMD: SELECT_MODE -> %d %s\n", modeIndex, modeName());
    sendScreenLiveFrame(true);
    return true;
  }
  if (strcmp(cmd, "START_FLOW") == 0) {
    if (page == PAGE_TRAINING || page == PAGE_REST) return false;
    integratedScreenFlowActive = true;
    integratedAutoTrainingStarted = false;
    integratedCountdownActive = false;
    page = PAGE_BODY_SELECT; // reused internally as the WEAR state
    lastPage = PAGE_COUNT;
    uiDirty = false;
    queueVoiceEvent("START_FLOW", rehabVoice.queueStartFlow());
    sendScreenLiveFrame(true);
    return true;
  }
  if (strcmp(cmd, "BEGIN_POSITION") == 0) {
    integratedScreenFlowActive = true;
    integratedBeginPositioning();
    return true;
  }
  if (strcmp(cmd, "COUNTDOWN_DONE") == 0) {
    integratedReleaseCountdown();
    return true;
  }
  if (strcmp(cmd, "PAUSE") == 0) {
    pauseRealTraining();
    sendScreenLiveFrame(true);
    return true;
  }
  if (strcmp(cmd, "RESUME") == 0) {
    resumeRealTraining();
    sendScreenLiveFrame(true);
    return true;
  }
  if (strcmp(cmd, "STOP") == 0) {
    if (page == PAGE_TRAINING || page == PAGE_REST) stopRealTraining();
    page = PAGE_RESULT;
    lastPage = PAGE_COUNT;
    uiDirty = false;
    sendScreenLiveFrame(true);
    return true;
  }
  if (strcmp(cmd, "SKIP_REST") == 0) {
    integratedSkipRest();
    return true;
  }
  if (strcmp(cmd, "HOME") == 0) {
    if (page == PAGE_TRAINING || page == PAGE_REST) stopRealTraining();
    integratedScreenFlowActive = false;
    integratedAutoTrainingStarted = false;
    integratedCountdownActive = false;
    page = PAGE_HOME;
    lastPage = PAGE_COUNT;
    uiDirty = false;
    sendScreenLiveFrame(true);
    return true;
  }
  if (strcmp(cmd, "CALIBRATION_CONTINUE") == 0) {
    // Product UI alias: release the calibrated countdown through the same path
    // used by the established 7-inch training flow.
    integratedReleaseCountdown();
    return true;
  }
  if (strcmp(cmd, "RETRY") == 0) {
    // Restart positioning/calibration without forcing a full session reset.
    integratedScreenFlowActive = true;
    integratedBeginPositioning();
    return true;
  }
  if (strcmp(cmd, "SYNC_NOW") == 0) {
    // Acknowledge a manual sync request immediately; cloud/miniprogram transport
    // consumes the next live/report envelope without blocking the motion loop.
    Serial.println("SCREEN_CMD: SYNC_NOW -> telemetry/report sync requested");
    sendScreenLiveFrame(true);
    return true;
  }
  if (strcmp(cmd, "PING") == 0) {
    sendScreenLiveFrame(true);
    return true;
  }
  return false;
}

static void updateScreenLinkRx() {
  static char line[128];
  static size_t n = 0;
  static bool collecting = false;

  while (ScreenLink.available() > 0) {
    const char c = (char)ScreenLink.read();
    if (!collecting) {
      if (c == '@') { collecting = true; n = 0; }
      continue; // ignore ESP-ROM / library logs and any non-protocol text
    }
    if (c == '\r') continue;
    if (c == '\n') {
      line[n] = 0;
      collecting = false;
      char *star = strrchr(line, '*');
      if (!star || strlen(star + 1) < 2) { n = 0; continue; }
      *star = 0;
      const uint8_t gotCrc = (uint8_t)strtoul(star + 1, nullptr, 16);
      const uint8_t calc = screenCrc8((const uint8_t*)line, strlen(line));
      if (gotCrc != calc) { n = 0; continue; }
      // C,<seq>,<command>
      if (line[0] == 'C' && line[1] == ',') {
        char *save = nullptr;
        strtok_r(line, ",", &save);
        char *seqText = strtok_r(nullptr, ",", &save);
        char *cmd = strtok_r(nullptr, ",", &save);
        if (seqText && cmd) {
          const uint32_t seq = strtoul(seqText, nullptr, 10);
          // Duplicate command packets are ACKed but not executed twice.
          bool ok = true;
          if (seq != screenLastCommandSeq) {
            screenLastCommandSeq = seq;
            ok = handleScreenCommand(cmd);
          }
          screenSendCommandAck(seq, ok);
        }
      }
      n = 0;
      continue;
    }
    if (n + 1 < sizeof(line)) line[n++] = c;
    else { collecting = false; n = 0; }
  }
}

static void updateIntegratedScreenFlow() {
  // Automatically start the K11/start-pose pipeline when BODY FRONT is ready.
  if (integratedScreenFlowActive && page == PAGE_CALIBRATION && bodyFramePhase() == BF_READY && !integratedAutoTrainingStarted) {
    if (startRealTraining()) {
      page = PAGE_TRAINING;
      lastPage = PAGE_COUNT;
      uiDirty = false;
      integratedAutoTrainingStarted = true;
      sendScreenLiveFrame(true);
    }
  }

  // Fail-safe only: normal release comes from the screen COUNTDOWN_DONE command.
  // If that command is lost, do not leave the user permanently stuck.
  if (integratedCountdownActive && integratedCountdownStartedMs != 0 &&
      millis() - integratedCountdownStartedMs > SCREEN_COUNTDOWN_TIMEOUT_MS) {
    Serial.println("SCREEN_COUNTDOWN_TIMEOUT: releasing training fail-safe");
    integratedReleaseCountdown();
  }
}

// V4.2.2: K11 FIFO is only meaningful once the training pipeline owns it.
// Menus, body-frame calibration and result pages use WT901Slot::latest directly,
// so queued gyro packets there are stale by definition. Drain them continuously
// to keep QDROP diagnostics scoped to the actual K11 session.
static void drainQueuesOutsideTrainingPipeline() {
  if (page == PAGE_TRAINING || page == PAGE_REST) return;
  wt901ClearSampleQueue(0); wt901ClearSampleQueue(1); wt901ClearSampleQueue(3);
}

// =====================================================
// Arduino setup / loop
// =====================================================

void setup() {
  Serial.begin(115200);
  delay(500);

  ScreenLink.begin(SCREEN_LINK_BAUD, SERIAL_8N1, PIN_SCREEN_RX, PIN_SCREEN_TX);
  rehabVoice.begin(RehabVoiceLink::kDefaultBaud, PIN_VOICE_RX, PIN_VOICE_TX);
  prepareSpiBusBeforeInit();
  buzzerInit();

  Serial.println();
  Serial.println("========================================");
  Serial.println(RM_BUILD_VERSION);
  Serial.println("V4.4.16：连续训练；一次校准。K11将A/B各自XYZ映射到人体FRONT/SIDE/DOWN后求肘角；P使用A-E竖直轴差分旋转+A侧倾+K11离轴；E持续参与P/T；无每次动作等待/重标定。 ");
  Serial.println("校准流程：手臂自然下垂约1.5s（可正常呼吸） -> 保持下垂直到FRONT_ARMED -> A/B整条手臂抬向真实身体正前方约40-100度并短暂停稳 -> K11慢速屈肘/返回一次。 ");
  Serial.println("A/B分别建立自己的人体FRONT/SIDE/DOWN语义坐标；E持续跟踪人体相对转向和躯干倾斜。K11慢动作只验证肘轴方向/符号，正式训练不再重标定。 ");
  Serial.println("断电续训：NVS保存训练参数+已完成计划次数；断电中途的一次动作不计数，重启后重新校准再续训。");
  Serial.println("========================================");

  // The legacy 3.5-inch ILI9488 is intentionally NOT initialized.
  // All visual UI now lives on the VIEWE 7-inch screen; GPIO8/9 are UART.
  digitalWrite(PIN_TFT_CS, HIGH);
  digitalWrite(PIN_SD_CS, HIGH);
  delay(30);

  bool sdOK = logger.begin(sharedSPI);
  productOrchestrator.setConnectivity(false, sdOK);
  productOrchestrator.setDeviceIdentity("patient-local", "RM-Core-01");
  if (!sdOK) {
    Serial.println("SD unavailable. Training will continue without CSV logging.");
  }

  bodyIndex = 0; // fixed single-arm A/B + torso E test; C/D remain mapped as the right-side pair
  applyDefaultsForBody();

  powerResumePrefsReady = powerResumePrefs.begin(POWER_RESUME_NAMESPACE, false);
  if (!powerResumePrefsReady) Serial.println("POWER_RESUME_NVS_INIT_FAIL: training still works, but outage resume is unavailable.");
  else powerResumeLoadAtBoot();

  leftTraining.reset();
  rightTraining.reset();
  applyTrainingParamsToLogic();
  resetGameOutputState();
  syncUiDataFromTraining();

  // RehabMotion_v5 IMU-screen boot-state fix:
  // The 7-inch screen is a separate ESP32 and can stay powered while this main
  // controller resets/reflashes. In that case its last LIVE frame may still say
  // 0x1F (all five IMUs connected), even though this freshly booted controller
  // has not started BLE discovery yet. Push several fresh LIVE frames BEFORE
  // wt901Begin() so the screen explicitly invalidates the previous IMU mask.
  // currentImuMask() is 0 here because wt901InitSlotsAndPairs() has not run yet.
  // Repeating the frame makes this robust to the screen finishing its own boot a
  // little later than the main controller. No training/calibration state is changed.
  Serial.println("SCREEN_IMU_BOOT_CLEAR: sending fresh mask=0x00 before BLE scan");
  for (uint8_t i = 0; i < 4; ++i) {
    sendScreenLiveFrame(true);
    delay(120);
  }

  imuReady = wt901Begin();

  // Publish the real receiving mask immediately after the initial BLE connect
  // sequence, instead of leaving the screen on the boot-clear state until the
  // later 1.2 s startup delay expires. Partial connection masks are preserved.
  Serial.printf("SCREEN_IMU_BOOT_RESULT: mask=0x%02X\n", currentImuMask());
  sendScreenLiveFrame(true);

  if (imuReady) {
    // K11.3.1: no boot success beep; keep startup quiet.
    Serial.println("A/B/C/D/E IMU connect OK. AB=left pair, CD=right pair, E=waist/abdomen torso reference. E is REQUIRED for body-relative plane P/T; E never enters elbow ROM.");
  } else {
    buzzerDoubleBeep(0, 100);
    Serial.println("WARNING: A/B/C/D/E not all ready. Current left-arm training specifically requires A, B and E; C/D are the mapped right-side pair.");
  }

  delay(1200);
  systemReady = true;
  printStatusToSerial();
  uiDirty = false;
  sendScreenLiveFrame(true);
}

void loop() {
  buzzerUpdate();

  // USB debug console remains independent from the screen UART.
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    handleSerialKey(c);
  }

  // Screen commands are parsed without blocking the IMU/training pipeline.
  updateScreenLinkRx();

  // V4.4.27: while the user is on HOME / IMU wear-check, keep looking for any
  // sensor that was off at boot or was power-cycled. Do NOT run this blocking
  // recovery scan during calibration/formal training because reconnecting there
  // would invalidate the current motion reference.
  if (systemReady && (page == PAGE_HOME || page == PAGE_BODY_SELECT) && !wt901AllReceiving()) {
    const uint8_t maskBeforeReconnect = currentImuMask();
    const bool reconnectChanged = wt901ServiceReconnect();
    imuReady = wt901AllReceiving();
    const uint8_t maskAfterReconnect = currentImuMask();
    if (reconnectChanged || maskAfterReconnect != maskBeforeReconnect) {
      Serial.printf("SCREEN_IMU_MASK_REFRESH: 0x%02X -> 0x%02X\n",
                    maskBeforeReconnect, maskAfterReconnect);
      sendScreenLiveFrame(true);
    }
  }

  const unsigned long now = millis();
  if (page == PAGE_CALIBRATION) {
    static BodyFrameCalPhase prevBfPhase = BF_OFF;
    BodyFrameCalPhase before = bodyFramePhase();
    if (before != BF_OFF && before != BF_READY && before != BF_FAILED) bodyFrameUpdate();
    BodyFrameCalPhase after = bodyFramePhase();
    if (after != prevBfPhase) {
      prevBfPhase = after;
      sendScreenLiveFrame(true);
    }
  }

  drainQueuesOutsideTrainingPipeline();
  updateRealTraining();
  updateProductApplicationBridge();
  updateIntegratedScreenFlow();
  updateVoiceSensorHealth();
  rehabVoice.update();


  // Main->screen LIVE state is 25 Hz. UART writes are skipped rather than blocked
  // if the hardware TX FIFO is temporarily full.
  sendScreenLiveFrame(false);

  if (systemReady && millis() - lastJsonTime >= JSON_INTERVAL_MS) {
    lastJsonTime = millis();
    outputJsonFrame();
  }

  // Legacy ILI9488 rendering is disabled. Keep the flag from accumulating stale
  // redraw requests while retaining the mature internal page/state machine.
  uiDirty = false;

  delay(1);
}
