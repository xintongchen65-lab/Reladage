#include "rehab_tf_animation.h"

#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <esp_heap_caps.h>
#include <string.h>

// VIEWE UEDX80480070E-WB-A onboard TF/SD slot.
static constexpr int REHAB_SD_CS   = 10;
static constexpr int REHAB_SD_SCK  = 12;
static constexpr int REHAB_SD_MOSI = 11;
static constexpr int REHAB_SD_MISO = 13;
static constexpr uint32_t REHAB_SD_FREQ_HZ = 20000000;

static constexpr const char *REHAB_ANIM_PATH =
    "/rehabmotion/animations/elbow_curl/elbow_curl_112x304_rgb565.bin";

static constexpr uint16_t REHAB_FRAME_W = 112;
static constexpr uint16_t REHAB_FRAME_H = 304;
static constexpr uint16_t REHAB_FRAME_COUNT = 48;
static constexpr uint32_t REHAB_FRAME_BYTES =
    (uint32_t)REHAB_FRAME_W * (uint32_t)REHAB_FRAME_H * 2U;
static constexpr uint32_t REHAB_EXPECTED_FILE_BYTES =
    REHAB_FRAME_BYTES * (uint32_t)REHAB_FRAME_COUNT;

// 33 ms ≈ 30 FPS.
static constexpr uint32_t REHAB_FRAME_PERIOD_MS = 33;

// 正式训练在屈肘终点和自然下垂起点各停约 0.4 秒。
static constexpr uint8_t REHAB_TRAIN_HOLD_TICKS = 12;

enum RehabAnimationMode {
    REHAB_ANIM_FORWARD_ONCE = 0,
    REHAB_ANIM_REVERSE_ONCE,
    REHAB_ANIM_TRAINING_PINGPONG
};

static bool g_sd_ready = false;
static uint8_t *g_anim_cache = nullptr;
static lv_timer_t *g_anim_timer = nullptr;
static lv_obj_t *g_anim_img = nullptr;

static uint8_t *g_frame_buf[2] = {nullptr, nullptr};
static lv_img_dsc_t g_frame_dsc[2] = {};
static uint8_t g_front_buf = 0;

static RehabAnimationMode g_mode = REHAB_ANIM_FORWARD_ONCE;
static int16_t g_current_frame = 0;
static int8_t g_direction = 1;
static uint8_t g_hold_ticks = 0;

static void setup_descriptor(lv_img_dsc_t &dsc, const uint8_t *data)
{
    memset(&dsc, 0, sizeof(dsc));
    dsc.header.always_zero = 0;
    dsc.header.w = REHAB_FRAME_W;
    dsc.header.h = REHAB_FRAME_H;
    dsc.header.cf = LV_IMG_CF_TRUE_COLOR;
    dsc.data_size = REHAB_FRAME_BYTES;
    dsc.data = data;
}

static bool ensure_frame_buffers()
{
    for (int i = 0; i < 2; ++i) {
        if (g_frame_buf[i]) continue;

        g_frame_buf[i] = static_cast<uint8_t *>(
            heap_caps_malloc(REHAB_FRAME_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
        );
        if (!g_frame_buf[i]) {
            g_frame_buf[i] = static_cast<uint8_t *>(
                heap_caps_malloc(REHAB_FRAME_BYTES, MALLOC_CAP_8BIT)
            );
        }
        if (!g_frame_buf[i]) {
            Serial.printf("ANIM: buffer %d allocation failed (%lu bytes)\n",
                          i, (unsigned long)REHAB_FRAME_BYTES);
            return false;
        }
        setup_descriptor(g_frame_dsc[i], g_frame_buf[i]);
    }
    return true;
}

static bool mount_sd_if_needed()
{
    if (g_sd_ready) return true;

    SPI.begin(REHAB_SD_SCK, REHAB_SD_MISO, REHAB_SD_MOSI, REHAB_SD_CS);
    if (!SD.begin(REHAB_SD_CS, SPI, REHAB_SD_FREQ_HZ)) {
        Serial.println("ANIM: TF card mount failed");
        return false;
    }

    if (SD.cardType() == CARD_NONE) {
        Serial.println("ANIM: no TF card detected");
        SD.end();
        return false;
    }

    g_sd_ready = true;
    Serial.println("ANIM: TF card ready");
    return true;
}

static bool ensure_animation_cached()
{
    if (g_anim_cache) return true;
    if (!mount_sd_if_needed()) return false;

    File f = SD.open(REHAB_ANIM_PATH, FILE_READ);
    if (!f) {
        Serial.print("ANIM: file missing: ");
        Serial.println(REHAB_ANIM_PATH);
        return false;
    }
    const uint32_t file_size = (uint32_t)f.size();
    if (file_size != REHAB_EXPECTED_FILE_BYTES) {
        Serial.printf("ANIM: wrong file size: %lu, expected %lu\n",
                      (unsigned long)file_size,
                      (unsigned long)REHAB_EXPECTED_FILE_BYTES);
        f.close();
        return false;
    }

    g_anim_cache = static_cast<uint8_t *>(
        heap_caps_malloc(REHAB_EXPECTED_FILE_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!g_anim_cache) {
        Serial.printf("ANIM: PSRAM cache allocation failed (%lu bytes)\n",
                      (unsigned long)REHAB_EXPECTED_FILE_BYTES);
        f.close();
        return false;
    }

    uint32_t offset = 0;
    static constexpr size_t CHUNK = 32 * 1024;
    while (offset < REHAB_EXPECTED_FILE_BYTES) {
        const size_t want = min((uint32_t)CHUNK, REHAB_EXPECTED_FILE_BYTES - offset);
        const size_t got = f.read(g_anim_cache + offset, want);
        if (got != want) {
            Serial.printf("ANIM: cache short read at %lu (%u/%u)\n",
                          (unsigned long)offset, (unsigned)got, (unsigned)want);
            f.close();
            heap_caps_free(g_anim_cache);
            g_anim_cache = nullptr;
            return false;
        }
        offset += got;
        delay(0); // yield while loading once
    }
    f.close();
    Serial.printf("ANIM: cached %lu bytes in PSRAM\n", (unsigned long)REHAB_EXPECTED_FILE_BYTES);
    return true;
}

static bool read_frame_index_into(uint16_t frame_index, uint8_t buf_index)
{
    if (!g_anim_cache || frame_index >= REHAB_FRAME_COUNT) return false;
    const uint8_t *src = g_anim_cache + (uint32_t)frame_index * REHAB_FRAME_BYTES;
    memcpy(g_frame_buf[buf_index], src, REHAB_FRAME_BYTES);
    return true;
}

static bool show_frame(uint16_t frame_index)
{
    const uint8_t back = (uint8_t)(1U - g_front_buf);

    if (!read_frame_index_into(frame_index, back)) {
        return false;
    }

    lv_img_set_src(g_anim_img, &g_frame_dsc[back]);
    lv_obj_invalidate(g_anim_img);

    g_front_buf = back;
    g_current_frame = (int16_t)frame_index;
    return true;
}

static void stop_timer_keep_current_frame(const char *message)
{
    if (g_anim_timer) {
        lv_timer_del(g_anim_timer);
        g_anim_timer = nullptr;
    }
    if (message) Serial.println(message);
}

static void animation_tick(lv_timer_t *timer)
{
    (void)timer;

    if (!g_anim_img || !g_anim_cache) {
        rehab_motion_animation_stop();
        return;
    }

    // ---------------- 校准 2/3：正放一次 ----------------
    if (g_mode == REHAB_ANIM_FORWARD_ONCE) {
        if (g_current_frame >= (int16_t)REHAB_FRAME_COUNT - 1) {
            stop_timer_keep_current_frame("ANIM: calibration flex reached final pose");
            return;
        }

        const uint16_t next = (uint16_t)(g_current_frame + 1);
        if (!show_frame(next)) {
            rehab_motion_animation_stop();
        }
        return;
    }

    // ---------------- 校准 3/3：倒放一次 ----------------
    if (g_mode == REHAB_ANIM_REVERSE_ONCE) {
        if (g_current_frame <= 0) {
            stop_timer_keep_current_frame("ANIM: calibration return reached start pose");
            return;
        }

        const uint16_t next = (uint16_t)(g_current_frame - 1);
        if (!show_frame(next)) {
            rehab_motion_animation_stop();
        }
        return;
    }

    // ---------------- 正式训练：正放 + 倒放循环 ----------------
    if (g_hold_ticks > 0) {
        g_hold_ticks--;
        return;
    }

    int16_t next = g_current_frame + g_direction;

    if (next >= (int16_t)REHAB_FRAME_COUNT) {
        // 已经停在第48帧，反向开始伸肘。
        g_direction = -1;
        next = (int16_t)REHAB_FRAME_COUNT - 2;
    } else if (next < 0) {
        // 已经停在第1帧，重新开始下一次屈肘。
        g_direction = 1;
        next = 1;
    }

    if (!show_frame((uint16_t)next)) {
        rehab_motion_animation_stop();
        return;
    }

    // 到达端点后停约0.4秒。
    if (g_current_frame == (int16_t)REHAB_FRAME_COUNT - 1) {
        g_hold_ticks = REHAB_TRAIN_HOLD_TICKS;
        Serial.println("ANIM TRAIN: final pose hold");
    } else if (g_current_frame == 0) {
        g_hold_ticks = REHAB_TRAIN_HOLD_TICKS;
        Serial.println("ANIM TRAIN: start pose hold");
    }
}

static bool start_animation(lv_obj_t *parent, int x, int y, RehabAnimationMode mode)
{
    rehab_motion_animation_stop();

    if (!parent) return false;
    if (!mount_sd_if_needed()) return false;
    if (!ensure_frame_buffers()) return false;
    if (!ensure_animation_cached()) return false;

    g_mode = mode;
    g_hold_ticks = 0;

    // 正放/训练从第1帧开始；倒放从第48帧开始。
    const uint16_t first_frame =
        (mode == REHAB_ANIM_REVERSE_ONCE) ? (REHAB_FRAME_COUNT - 1) : 0;

    g_front_buf = 0;

    if (!read_frame_index_into(first_frame, g_front_buf)) {
        return false;
    }

    g_current_frame = (int16_t)first_frame;
    g_direction = (mode == REHAB_ANIM_REVERSE_ONCE) ? -1 : 1;

    g_anim_img = lv_img_create(parent);
    lv_img_set_src(g_anim_img, &g_frame_dsc[g_front_buf]);
    lv_obj_set_pos(g_anim_img, x, y);
    lv_obj_clear_flag(g_anim_img, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(g_anim_img, LV_OBJ_FLAG_SCROLLABLE);

    g_anim_timer = lv_timer_create(animation_tick, REHAB_FRAME_PERIOD_MS, nullptr);

    if (mode == REHAB_ANIM_FORWARD_ONCE) {
        Serial.println("ANIM: CAL 2 forward 1->48 started (~30 FPS)");
    } else if (mode == REHAB_ANIM_REVERSE_ONCE) {
        Serial.println("ANIM: CAL 3 reverse 48->1 started (~30 FPS)");
    } else {
        Serial.println("ANIM: TRAINING ping-pong 1<->48 started (~30 FPS)");
    }

    return true;
}

bool rehab_motion_animation_start_forward(lv_obj_t *parent, int x, int y)
{
    return start_animation(parent, x, y, REHAB_ANIM_FORWARD_ONCE);
}

bool rehab_motion_animation_start_reverse(lv_obj_t *parent, int x, int y)
{
    return start_animation(parent, x, y, REHAB_ANIM_REVERSE_ONCE);
}

bool rehab_motion_animation_start_training(lv_obj_t *parent, int x, int y)
{
    return start_animation(parent, x, y, REHAB_ANIM_TRAINING_PINGPONG);
}

// Backward compatibility with v8 call sites.
bool rehab_motion_animation_start(lv_obj_t *parent, int x, int y)
{
    return rehab_motion_animation_start_forward(parent, x, y);
}

bool rehab_motion_animation_sd_ready()
{
    return mount_sd_if_needed();
}

bool rehab_motion_animation_preload()
{
    return ensure_animation_cached();
}

void rehab_motion_animation_stop()
{
    if (g_anim_timer) {
        lv_timer_del(g_anim_timer);
        g_anim_timer = nullptr;
    }

    g_anim_img = nullptr;
    g_current_frame = 0;
    g_direction = 1;
    g_hold_ticks = 0;
}
