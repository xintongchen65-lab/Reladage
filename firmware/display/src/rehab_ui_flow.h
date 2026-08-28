#pragma once
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void rehab_ui_show_home(void);
void rehab_ui_start_training_flow(void);
void rehab_ui_set_imu_connections(bool a, bool b, bool c, bool d, bool e);
void rehab_ui_wear_ready(void);
void rehab_ui_body_down_positioning_success(void);
void rehab_ui_body_front_positioning_success(void);
void rehab_ui_calibration_start_pose_ready(void);
void rehab_ui_calibration_flex_success(void);
void rehab_ui_calibration_return_success(void);
void rehab_ui_detection_progress(int state, int progress_percent);

void rehab_ui_set_plan(int target_groups, int target_reps, float target_angle_deg);
void rehab_ui_training_update_remote(int group_index, int target_groups,
                                     int count, int target_reps,
                                     float angle_deg, float target_angle_deg,
                                     int progress_percent);
void rehab_ui_training_feedback(uint16_t feedback_mask);
void rehab_ui_set_training_paused(bool paused);
void rehab_ui_group_finished(int group_index, int rest_seconds);
void rehab_ui_rest_update(int group_index, int rest_seconds);
void rehab_ui_training_finished(void);
void rehab_ui_result_update(int completed, int target_total,
                            float pass_rate_pct, float rom_max_deg,
                            unsigned long duration_sec);

#ifdef __cplusplus
}
#endif
