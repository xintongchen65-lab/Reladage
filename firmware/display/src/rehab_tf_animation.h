#pragma once

#include <lvgl.h>

// 校准 2/3：180° -> 30°，正放一次，停在终点
bool rehab_motion_animation_start_forward(lv_obj_t *parent, int x, int y);

// 校准 3/3：30° -> 180°，倒放一次，停在起点
bool rehab_motion_animation_start_reverse(lv_obj_t *parent, int x, int y);

// 正式训练：正放 -> 终点停顿 -> 倒放 -> 起点停顿，持续循环
bool rehab_motion_animation_start_training(lv_obj_t *parent, int x, int y);

// 保留旧接口，等价于 start_forward
bool rehab_motion_animation_start(lv_obj_t *parent, int x, int y);

void rehab_motion_animation_stop();
bool rehab_motion_animation_sd_ready();
// Preload the entire 48-frame file into PSRAM once for smooth 30 FPS playback.
bool rehab_motion_animation_preload();
