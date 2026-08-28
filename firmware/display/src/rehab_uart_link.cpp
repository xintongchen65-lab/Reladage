#include "rehab_uart_link.h"
#include "rehab_ui_flow.h"
#include "product_ui/rehab_ui_router.h"

#include <Arduino.h>
#include <stdlib.h>
#include <string.h>

// VIEWE external TX/RX connector uses UART0. With
// ARDUINO_USB_CDC_ON_BOOT=0, Serial is the external hardware UART.
static constexpr uint32_t REHAB_UART_BAUD = 115200;
static constexpr uint32_t UI_APPLY_INTERVAL_MS = 50;   // 20 Hz visual refresh
static constexpr uint32_t CMD_RETRY_INTERVAL_MS = 120; // reliable UI commands
static constexpr uint8_t CMD_MAX_RETRIES = 240; // ~28.8 s: main boot does a 12 s BLE scan before it can ACK UI commands

static uint32_t g_cmd_seq = 0;
static int g_last_state = -1;
static int g_last_imu_mask = -1;
static unsigned long g_last_apply_ms = 0;

// V4.4.27 screen: when positioning/calibration succeeds, hold the completed
// 100% visual very briefly before creating the next page. Previously the main
// controller could change state between two 20-Hz UI samples, so the page could
// disappear at 82/91/etc percent even though the detector had actually passed.
static constexpr uint32_t STAGE_COMPLETE_HOLD_MS = 160;
static int g_pending_state = -1;
static int g_pending_rest_seconds = 0;
static unsigned long g_pending_state_due_ms = 0;

// LIVE telemetry is "latest wins". Commands are different: they are retried until
// the main controller ACKs them, so a brief UART busy period cannot lose PAUSE/STOP/etc.
static bool g_pending_cmd = false;
static uint32_t g_pending_cmd_seq = 0;
static char g_pending_cmd_payload[96] = {};
static uint8_t g_pending_cmd_retries = 0;
static unsigned long g_pending_cmd_last_tx_ms = 0;

// V4.4.28: START_FLOW is optimistic on the screen: the wear/IMU page is shown
// immediately, while the main controller may still be inside its blocking boot BLE
// scan and unable to parse UART commands.  During that interval the main can emit
// one stale HOME snapshot at the end of setup.  Do not let that stale HOME undo
// the user's tap while START_FLOW is still waiting for an ACK.
static bool g_start_flow_waiting_ack = false;

struct RemoteSnapshot {
    uint32_t seq = 0;
    int state = 0;
    int imuMask = 0;
    int group = 1;
    int groups = 3;
    int count = 0;
    int targetReps = 10;
    int angle10 = 0;
    int targetAngle10 = 800;
    int progress = 0;
    int rest = 0;
    uint16_t feedback = 0;
    int completed = 0;
    int targetTotal = 30;
    int passPct10 = 0;
    int rom10 = 0;
    unsigned long durationSec = 0;
    int exerciseIndex = 0;
    int modeIndex = 0;
};

static RemoteSnapshot g_snap;
static bool g_snap_dirty = false;

static uint8_t crc8(const uint8_t *data, size_t len) {
    uint8_t crc = 0;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (uint8_t b = 0; b < 8; ++b)
            crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x07) : (uint8_t)(crc << 1);
    }
    return crc;
}

static bool writePacket(const char *payload) {
    if (!payload) return false;
    const size_t n = strlen(payload);
    const uint8_t c = crc8((const uint8_t*)payload, n);
    char line[128];
    const int len = snprintf(line, sizeof(line), "@%s*%02X\n", payload, c);
    if (len <= 0 || len >= (int)sizeof(line)) return false;
    if (Serial.availableForWrite() < len) return false;
    Serial.write((const uint8_t*)line, (size_t)len);
    return true;
}

static void tryPendingCommandNow() {
    if (!g_pending_cmd) return;
    if (writePacket(g_pending_cmd_payload)) {
        g_pending_cmd_last_tx_ms = millis();
        if (g_pending_cmd_retries < 255) g_pending_cmd_retries++;
    }
}

bool rehab_uart_link_send_command(const char *command) {
    if (!command || !*command) return false;

    // A fresh user action supersedes any older still-unacknowledged command.
    // START_FLOW gets special protection against the main controller's one stale
    // boot-time HOME frame; any other fresh command cancels that protection.
    g_start_flow_waiting_ack = (strcmp(command, "START_FLOW") == 0);
    g_pending_cmd_seq = ++g_cmd_seq;
    snprintf(g_pending_cmd_payload, sizeof(g_pending_cmd_payload),
             "C,%lu,%s", (unsigned long)g_pending_cmd_seq, command);
    g_pending_cmd = true;
    g_pending_cmd_retries = 0;
    g_pending_cmd_last_tx_ms = 0;
    tryPendingCommandNow();
    return true;
}


static void commitStateTransition(int state, int restSeconds) {
    g_last_state = state;
    switch (state) {
        case 0: rehab_ui_show_home(); break;
        case 1: rehab_ui_start_training_flow(); break;
        case 2: rehab_ui_wear_ready(); break;
        case 3: rehab_ui_body_down_positioning_success(); break;
        case 4: rehab_ui_body_front_positioning_success(); break;
        case 5: rehab_ui_calibration_start_pose_ready(); break;
        case 6: rehab_ui_calibration_flex_success(); break;
        case 7: rehab_ui_calibration_return_success(); break;
        case 8: rehab_ui_set_training_paused(false); break;
        case 9: rehab_ui_set_training_paused(true); break;
        case 10: rehab_ui_group_finished(g_snap.group, restSeconds); break;
        case 11: rehab_ui_training_finished(); break;
        default: break;
    }
}

// Returns true only when the visible page already matches the incoming state.
// A false return means the old detector page is intentionally being held at
// 100% for STAGE_COMPLETE_HOLD_MS.
static bool applyStateTransition(int state, int restSeconds) {
    const unsigned long now = millis();

    if (g_pending_state >= 0) {
        if (state != g_pending_state) {
            // Controller changed its mind/reset while we were holding a page.
            g_pending_state = -1;
            g_pending_state_due_ms = 0;
        } else if ((int32_t)(now - g_pending_state_due_ms) >= 0) {
            const int target = g_pending_state;
            const int rest = g_pending_rest_seconds;
            g_pending_state = -1;
            g_pending_state_due_ms = 0;
            commitStateTransition(target, rest);
            return true;
        } else {
            return false;
        }
    }

    if (state == g_last_state) return true;

    // Progress-driven pages are states 2..6. On a successful forward transition,
    // finish the old visual first instead of immediately cleaning the screen.
    if (g_last_state >= 2 && g_last_state <= 6 && state > g_last_state) {
        rehab_ui_detection_progress(g_last_state, 100);
        g_pending_state = state;
        g_pending_rest_seconds = restSeconds;
        g_pending_state_due_ms = now + STAGE_COMPLETE_HOLD_MS;
        return false;
    }

    commitStateTransition(state, restSeconds);
    return true;
}

static void applySnapshot(bool force) {
    const unsigned long now = millis();

    // Keep the extended product-page model synchronized with the same controller
    // LIVE packet used by the approved training flow. Product pages therefore
    // consume real plan/device/session state when opened.
    RehabUiModel model = rehab_ui_router_model();
    model.exerciseIndex = (uint8_t)constrain(g_snap.exerciseIndex, 0, 7);
    model.trainMode = (uint8_t)constrain(g_snap.modeIndex, 0, 2);
    model.currentExercise = rehab_ui_exercise_name(model.exerciseIndex);
    model.currentAngleDeg = g_snap.angle10 / 10.0f;
    model.targetAngleDeg = g_snap.targetAngle10 / 10.0f;
    model.currentRep = (uint8_t)constrain(g_snap.count, 0, 255);
    model.targetReps = (uint8_t)constrain(g_snap.targetReps, 0, 255);
    model.currentSet = (uint8_t)constrain(g_snap.group, 0, 255);
    model.targetSets = (uint8_t)constrain(g_snap.groups, 0, 255);
    model.imuMask = (uint8_t)(g_snap.imuMask & 0x1F);
    model.weeklyPassRate = g_snap.passPct10 / 10.0f;
    model.weeklyMaxRomDeg = g_snap.rom10 / 10.0f;
    model.peakRomDeg = g_snap.rom10 / 10.0f;
    model.completedTotal = (uint16_t)constrain(g_snap.completed, 0, 65535);
    model.targetTotal = (uint16_t)constrain(g_snap.targetTotal, 0, 65535);
    model.restSeconds = (uint16_t)constrain(g_snap.rest, 0, 65535);
    model.elapsedSeconds = g_snap.durationSec;
    model.qualityFlags = g_snap.feedback;
    model.qualityText = "动作正常";
    model.qualityScore = 92.0f;
    if (g_snap.feedback & 0x40) { model.qualityText = "动作达标"; model.qualityScore = 96.0f; }
    else if (g_snap.feedback & 0x20) { model.qualityText = "动作未达标"; model.qualityScore = 58.0f; }
    else if (g_snap.feedback & 0x01) { model.qualityText = "幅度不足"; model.qualityScore = 68.0f; }
    else if (g_snap.feedback & 0x02) { model.qualityText = "近端代偿"; model.qualityScore = 64.0f; }
    else if (g_snap.feedback & 0x04) { model.qualityText = "平面偏移"; model.qualityScore = 62.0f; }
    else if (g_snap.feedback & 0x08) { model.qualityText = "躯干代偿"; model.qualityScore = 60.0f; }
    rehab_ui_router_set_model(model);

    // Plan values are loaded BEFORE a transition creates a new page, preventing
    // one-frame flashes of stale defaults.
    rehab_ui_set_plan(g_snap.groups, g_snap.targetReps, g_snap.targetAngle10 / 10.0f);

    if (g_snap.imuMask != g_last_imu_mask) {
        g_last_imu_mask = g_snap.imuMask;
        rehab_ui_set_imu_connections(
            (g_snap.imuMask & 0x01) != 0,
            (g_snap.imuMask & 0x02) != 0,
            (g_snap.imuMask & 0x04) != 0,
            (g_snap.imuMask & 0x08) != 0,
            (g_snap.imuMask & 0x10) != 0);
    }

    // V4.4.28 boot race guard: if the user already tapped Start and the screen is
    // waiting for START_FLOW to be ACKed, a HOME snapshot is stale boot telemetry,
    // not a user-requested navigation event.  Keep the wear page visible.  IMU
    // connection bits above are still applied live while we wait.
    if (g_snap.state == 0 && g_start_flow_waiting_ack) {
        return;
    }

    // Repository product pages (history/report/device/settings/action library) are
    // local browsing pages. HOME telemetry must not immediately kick the user
    // back to the dashboard. A real training-state transition still takes
    // authority and closes the product shell.
    if (rehab_ui_router_is_active()) {
        if (g_snap.state == 0) return;
        rehab_ui_router_leave();
    }

    // Finish a successful detector page at a visible 100% before turning it.
    // While that short hold is active, do not draw the *next* state's progress
    // onto the old page.
    const bool visibleStateMatches = applyStateTransition(g_snap.state, g_snap.rest);
    if (!visibleStateMatches) return;

    if (!force && now - g_last_apply_ms < UI_APPLY_INTERVAL_MS) return;
    g_last_apply_ms = now;

    // States 2..6 reuse LIVE.progress as real positioning/calibration detector
    // progress. This drives the "姿势识别中 / 方向识别中 / 校准中" visuals.
    if (g_snap.state >= 2 && g_snap.state <= 6) {
        rehab_ui_detection_progress(g_snap.state, g_snap.progress);
    }

    if (g_snap.state == 8 || g_snap.state == 9) {
        rehab_ui_training_update_remote(
            g_snap.group, g_snap.groups, g_snap.count, g_snap.targetReps,
            g_snap.angle10 / 10.0f, g_snap.targetAngle10 / 10.0f,
            g_snap.progress);
        rehab_ui_training_feedback(g_snap.feedback);
    } else if (g_snap.state == 10) {
        rehab_ui_rest_update(g_snap.group, g_snap.rest);
    } else if (g_snap.state == 11) {
        rehab_ui_result_update(g_snap.completed, g_snap.targetTotal,
                               g_snap.passPct10 / 10.0f,
                               g_snap.rom10 / 10.0f,
                               g_snap.durationSec);
    }
}

static bool parseLive(char *payload) {
    // R,seq,state,imu,group,groups,count,targetReps,angle10,targetAngle10,
    // progress,rest,feedback,completed,total,passPct10,rom10,durationSec,exerciseIndex,modeIndex
    char *save = nullptr;
    char *tok = strtok_r(payload, ",", &save);
    if (!tok || strcmp(tok, "R") != 0) return false;
    long vals[19] = {};
    int valueCount = 0;
    while (valueCount < 19) {
        tok = strtok_r(nullptr, ",", &save);
        if (!tok) break;
        vals[valueCount++] = strtol(tok, nullptr, 10);
    }
    // V5 repository packets append exerciseIndex/modeIndex. Accept the older
    // 17-value packet too so the display can still boot against the preceding
    // controller build during firmware rollout.
    if (valueCount < 17) return false;

    // Serial ordering prevents reordering, but duplicate LIVE frames are unnecessary.
    const uint32_t incomingSeq = (uint32_t)vals[0];
    if (incomingSeq == g_snap.seq && g_snap.seq != 0) return true;

    g_snap.seq = incomingSeq;
    g_snap.state = (int)vals[1];
    g_snap.imuMask = (int)vals[2];
    g_snap.group = (int)vals[3];
    g_snap.groups = (int)vals[4];
    g_snap.count = (int)vals[5];
    g_snap.targetReps = (int)vals[6];
    g_snap.angle10 = (int)vals[7];
    g_snap.targetAngle10 = (int)vals[8];
    g_snap.progress = (int)vals[9];
    g_snap.rest = (int)vals[10];
    g_snap.feedback = (uint16_t)vals[11];
    g_snap.completed = (int)vals[12];
    g_snap.targetTotal = (int)vals[13];
    g_snap.passPct10 = (int)vals[14];
    g_snap.rom10 = (int)vals[15];
    g_snap.durationSec = (unsigned long)vals[16];
    if (valueCount >= 18) g_snap.exerciseIndex = (int)vals[17];
    if (valueCount >= 19) g_snap.modeIndex = (int)vals[18];
    g_snap_dirty = true;
    return true;
}

static void parseAck(char *payload) {
    // A,<command_seq>,<ok>
    char *save = nullptr;
    char *tok = strtok_r(payload, ",", &save);
    if (!tok || strcmp(tok, "A") != 0) return;
    char *seqText = strtok_r(nullptr, ",", &save);
    char *okText = strtok_r(nullptr, ",", &save);
    if (!seqText || !okText) return;
    const uint32_t seq = (uint32_t)strtoul(seqText, nullptr, 10);
    if (g_pending_cmd && seq == g_pending_cmd_seq) {
        // ACK means the main controller received the command. ok=0 is still an ACK;
        // current LIVE state then remains authoritative for the UI.
        g_pending_cmd = false;
        g_start_flow_waiting_ack = false;
    }
}

void rehab_uart_link_begin() {
    Serial.begin(REHAB_UART_BAUD);
    rehab_uart_link_send_command("PING");
}

void rehab_uart_link_update() {
    static char line[240];
    static size_t n = 0;
    static bool collecting = false;

    // Drain UART bytes first. No LVGL calls happen inside this loop, so an expensive
    // redraw can never cause the hardware RX FIFO to be serviced late.
    while (Serial.available() > 0) {
        const char c = (char)Serial.read();
        if (!collecting) {
            if (c == '@') { collecting = true; n = 0; }
            continue; // ignore ROM/panel/debug text outside framed protocol
        }
        if (c == '\r') continue;
        if (c == '\n') {
            line[n] = 0;
            collecting = false;
            char *star = strrchr(line, '*');
            if (!star || strlen(star + 1) < 2) { n = 0; continue; }
            *star = 0;
            const uint8_t got = (uint8_t)strtoul(star + 1, nullptr, 16);
            const uint8_t calc = crc8((const uint8_t*)line, strlen(line));
            if (got == calc) {
                if (line[0] == 'R' && line[1] == ',') parseLive(line);
                else if (line[0] == 'A' && line[1] == ',') parseAck(line);
            }
            n = 0;
            continue;
        }
        if (n + 1 < sizeof(line)) line[n++] = c;
        else { collecting = false; n = 0; }
    }

    // Reliable command retry. Duplicate command packets are safe because the main
    // controller de-duplicates by sequence and merely re-ACKs them.
    if (g_pending_cmd) {
        const unsigned long now = millis();
        if (g_pending_cmd_last_tx_ms == 0 || now - g_pending_cmd_last_tx_ms >= CMD_RETRY_INTERVAL_MS) {
            if (g_pending_cmd_retries < CMD_MAX_RETRIES) tryPendingCommandNow();
            else {
                g_pending_cmd = false; // keep UI governed by main LIVE state; user may retry
                g_start_flow_waiting_ack = false;
            }
        }
    }

    if (g_snap_dirty) {
        const bool force = g_snap.state != g_last_state;
        applySnapshot(force);
        g_snap_dirty = false;
    }
}
