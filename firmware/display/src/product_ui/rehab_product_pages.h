#pragma once
#include <lvgl.h>

// Complete product-facing page library for the 7-inch terminal. The approved
// guided training flow in main.cpp can coexist with these pages; both share the
// same controller snapshot model and UART command vocabulary.
enum RehabProductPage {
  RP_HOME = 0,
  RP_EXERCISE_LIBRARY,
  RP_PLAN_DETAIL,
  RP_PRECHECK,
  RP_WEAR_GUIDE,
  RP_BODY_POSITION,
  RP_MOTION_CALIBRATION,
  RP_LIVE_TRAINING,
  RP_REST,
  RP_SESSION_RESULT,
  RP_DIGITAL_TWIN,
  RP_GAME_HUB,
  RP_HISTORY,
  RP_REPORT,
  RP_AI_COACH,
  RP_DEVICE_CENTER,
  RP_SETTINGS,
  RP_PRESCRIPTION_SYNC,
  RP_OFFLINE_SYNC,
  RP_ABOUT,
  RP_PAGE_COUNT
};

void rehab_product_pages_init(lv_obj_t *root);
void rehab_product_pages_bind_root(lv_obj_t *root);
void rehab_product_show_page(RehabProductPage page);
RehabProductPage rehab_product_current_page();
