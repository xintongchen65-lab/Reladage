#pragma once
#include "rehab_product_pages.h"
#include "rehab_ui_model.h"

void rehab_ui_router_init(lv_obj_t* root);
void rehab_ui_router_set_model(const RehabUiModel& model);
const RehabUiModel& rehab_ui_router_model();
void rehab_ui_router_open(RehabProductPage page);
bool rehab_ui_router_is_active();
void rehab_ui_router_leave();
void rehab_ui_router_handle_event(const char* eventLine);
