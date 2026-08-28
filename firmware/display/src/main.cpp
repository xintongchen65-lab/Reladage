#include <Arduino.h>
#include <math.h>
#include <esp_display_panel.hpp>
#include <lvgl.h>
#include "lvgl_v8_port.h"
#include "rehab_ui_text_assets.h"
#include "body_position_page.h"
#include "body_position_front_page.h"
#include "rehab_flow_text_assets.h"
#include "rehab_feedback_cn_assets.h"
#include "rehab_feedback_big_assets.h"
#include "rehab_video_assets.h"
#include "rehab_person_assets.h"
#include "rehab_pause_text_assets.h"
#include "rehab_wear_diagram.h"
#include "rehab_tf_animation.h"
#include "rehab_uart_link.h"
#include "product_ui/rehab_ui_router.h"
#include "rehab_ui_flow.h"
#include "rehab_prescription_sync_assets.h"
#include "rehab_voice_assistant_assets.h"

using namespace esp_panel::drivers;
using namespace esp_panel::board;

// RehabMotion Home + Body Positioning V1
// Target: VIEWE UEDX80480070E-WB-A, 800 x 480, LVGL 8.4
// Static Chinese labels are compiled into firmware.
// V8: 动作校准 2/3 streams the approved elbow-curl animation from the onboard TF card.
// Other approved pages remain unchanged.

static const lv_color_t C_BG      = lv_color_hex(0xFAFCF9);
static const lv_color_t C_CARD    = lv_color_hex(0xFFFFFF);
static const lv_color_t C_DARK    = lv_color_hex(0x202420);
static const lv_color_t C_TEAL    = lv_color_hex(0x31B270);
static const lv_color_t C_TEAL_2  = lv_color_hex(0x3DB77A);
static const lv_color_t C_SOFT    = lv_color_hex(0xEEF7F1);
static const lv_color_t C_LINE    = lv_color_hex(0xDCE9E0);
static const lv_color_t C_MUTED   = lv_color_hex(0x68716B);
static const lv_color_t C_GREEN   = lv_color_hex(0x31B270);

// Prescription synchronization UI surface.
// Transport events are exposed through the screen command interface.
#define DEMO_PRESCRIPTION_SYNC 1

// Voice assistant UI surface.
// Hidden long-press in the top-left corner starts a timed interaction overlay, then
// enters the exact same IMU wear/connection page used by the real START button.
#define DEMO_VOICE_ASSISTANT 1

static lv_obj_t *g_prescription_sync_mask = nullptr;
static lv_obj_t *g_prescription_sync_panel = nullptr;

static lv_obj_t *g_voice_assistant_mask = nullptr;
static lv_obj_t *g_voice_assistant_panel = nullptr;
static lv_obj_t *g_voice_assistant_state_img = nullptr;
static lv_obj_t *g_voice_assistant_pulse = nullptr;
static lv_obj_t *g_voice_assistant_wave[5] = {nullptr, nullptr, nullptr, nullptr, nullptr};
static lv_timer_t *g_voice_assistant_timer = nullptr;
static uint32_t g_voice_assistant_started_at = 0;
static int g_voice_assistant_stage = 0;

static lv_point_t orbit1_pts[64];
static lv_point_t orbit2_pts[64];
static lv_point_t orbit3_pts[64];
static lv_point_t chart_pts[6];

static void create_home_ui();
static void show_home_ui();
static void product_nav_clicked(lv_event_t *e);


static void show_body_positioning_ui();
static void show_body_position_front_ui();

static void show_home_async(void *user_data);
static void show_body_positioning_async(void *user_data);
static void show_body_position_front_async(void *user_data);
static void show_wear_async(void *user_data);

static lv_obj_t *make_card(lv_obj_t *parent, int x, int y, int w, int h, int radius = 18)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(o, 0, 0);
    lv_obj_set_style_radius(o, radius, 0);
    lv_obj_set_style_bg_color(o, C_CARD, 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(o, 1, 0);
    lv_obj_set_style_border_color(o, lv_color_hex(0xE1EAE3), 0);
    lv_obj_set_style_shadow_width(o, 12, 0);
    lv_obj_set_style_shadow_opa(o, LV_OPA_10, 0);
    lv_obj_set_style_shadow_color(o, lv_color_hex(0x728078), 0);
    lv_obj_set_style_shadow_ofs_y(o, 4, 0);
    return o;
}

static lv_obj_t *make_img(lv_obj_t *parent, const lv_img_dsc_t *src, int x, int y)
{
    lv_obj_t *img = lv_img_create(parent);
    lv_img_set_src(img, src);
    lv_obj_set_pos(img, x, y);
    return img;
}

static lv_obj_t *make_text(lv_obj_t *parent, const char *txt, const lv_font_t *font,
                           lv_color_t color, int x, int y)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, txt);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, color, 0);
    lv_obj_set_pos(label, x, y);
    return label;
}

static lv_obj_t *make_circle(lv_obj_t *parent, int x, int y, int size, lv_color_t color)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, size, size);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(o, 0, 0);
    lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(o, color, 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(o, 0, 0);
    return o;
}

static void fill_ellipse(lv_point_t *pts, int n, int cx, int cy, int rx, int ry, float deg)
{
    float a = deg * 3.14159265f / 180.0f;
    float ca = cosf(a);
    float sa = sinf(a);
    for (int i = 0; i < n; ++i) {
        float t = 2.0f * 3.14159265f * (float)i / (float)(n - 1);
        float x = rx * cosf(t);
        float y = ry * sinf(t);
        pts[i].x = (lv_coord_t)(cx + x * ca - y * sa);
        pts[i].y = (lv_coord_t)(cy + x * sa + y * ca);
    }
}

static lv_obj_t *make_orbit(lv_obj_t *parent, lv_point_t *pts, lv_color_t color, lv_opa_t opa, int width)
{
    lv_obj_t *line = lv_line_create(parent);
    lv_obj_set_pos(line, 0, 0);
    lv_obj_set_size(line, 440, 244);
    lv_line_set_points(line, pts, 64);
    lv_obj_set_style_line_color(line, color, 0);
    lv_obj_set_style_line_opa(line, opa, 0);
    lv_obj_set_style_line_width(line, width, 0);
    return line;
}

static void add_orbit_dot(lv_obj_t *parent, int x, int y, int size)
{
    lv_obj_t *dot = make_circle(parent, x - size / 2, y - size / 2, size, C_TEAL_2);
    lv_obj_set_style_shadow_width(dot, 5, 0);
    lv_obj_set_style_shadow_opa(dot, LV_OPA_20, 0);
    lv_obj_set_style_shadow_color(dot, C_TEAL_2, 0);
}

static void start_clicked(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

    rehab_uart_link_send_command("START_FLOW");
    // Immediate local response; the main controller will confirm WEAR in its next LIVE frame.
    lv_async_call(show_wear_async, nullptr);
}

static void body_back_clicked(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

    rehab_uart_link_send_command("HOME");
    lv_async_call(show_home_async, nullptr);
}


static void create_logo(lv_obj_t *screen)
{
    lv_obj_t *logo = lv_obj_create(screen);
    lv_obj_set_pos(logo, 22, 14);
    lv_obj_set_size(logo, 36, 36);
    lv_obj_clear_flag(logo, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(logo, 0, 0);
    lv_obj_set_style_radius(logo, 10, 0);
    lv_obj_set_style_border_width(logo, 0, 0);
    lv_obj_set_style_bg_color(logo, lv_color_hex(0x35B47A), 0);
    lv_obj_set_style_bg_grad_color(logo, lv_color_hex(0x78D1A3), 0);
    lv_obj_set_style_bg_grad_dir(logo, LV_GRAD_DIR_VER, 0);

    lv_obj_t *inner = lv_obj_create(logo);
    lv_obj_set_pos(inner, 9, 7);
    lv_obj_set_size(inner, 18, 22);
    lv_obj_clear_flag(inner, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(inner, 0, 0);
    lv_obj_set_style_radius(inner, 6, 0);
    lv_obj_set_style_border_width(inner, 0, 0);
    lv_obj_set_style_bg_color(inner, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(inner, LV_OPA_80, 0);
}

static void create_calendar_icon(lv_obj_t *parent, int x, int y)
{
    lv_obj_t *bg = make_circle(parent, x, y, 38, C_SOFT);
    lv_obj_t *body = lv_obj_create(bg);
    lv_obj_set_pos(body, 10, 11);
    lv_obj_set_size(body, 18, 17);
    lv_obj_clear_flag(body, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(body, 0, 0);
    lv_obj_set_style_radius(body, 3, 0);
    lv_obj_set_style_bg_opa(body, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(body, 2, 0);
    lv_obj_set_style_border_color(body, C_TEAL_2, 0);

    for (int i = 0; i < 2; ++i) {
        lv_obj_t *tab = lv_obj_create(bg);
        lv_obj_set_pos(tab, 13 + i * 9, 7);
        lv_obj_set_size(tab, 2, 7);
        lv_obj_set_style_pad_all(tab, 0, 0);
        lv_obj_set_style_radius(tab, 1, 0);
        lv_obj_set_style_border_width(tab, 0, 0);
        lv_obj_set_style_bg_color(tab, C_TEAL_2, 0);
    }
    for (int r = 0; r < 2; ++r) {
        for (int c = 0; c < 2; ++c) {
            lv_obj_t *dot = make_circle(bg, 14 + c * 7, 17 + r * 6, 3, C_TEAL_2);
            (void)dot;
        }
    }
}

static void create_bars_icon(lv_obj_t *parent, int x, int y)
{
    lv_obj_t *bg = make_circle(parent, x, y, 38, C_SOFT);
    const int heights[3] = {9, 15, 22};
    for (int i = 0; i < 3; ++i) {
        lv_obj_t *bar = lv_obj_create(bg);
        lv_obj_set_pos(bar, 10 + i * 7, 28 - heights[i]);
        lv_obj_set_size(bar, 4, heights[i]);
        lv_obj_set_style_pad_all(bar, 0, 0);
        lv_obj_set_style_radius(bar, 2, 0);
        lv_obj_set_style_border_width(bar, 0, 0);
        lv_obj_set_style_bg_color(bar, C_TEAL_2, 0);
    }
}

static void create_main_card(lv_obj_t *screen)
{
    lv_obj_t *card = make_card(screen, 20, 76, 440, 244, 20);
    lv_obj_set_style_bg_color(card, lv_color_hex(0xFBFDFB), 0);
    lv_obj_set_style_bg_grad_color(card, lv_color_hex(0xECF7F0), 0);
    lv_obj_set_style_bg_grad_dir(card, LV_GRAD_DIR_HOR, 0);

    // The exact decorative choice we settled on: light orbital curves and dots only.
    fill_ellipse(orbit1_pts, 64, 346, 121, 108, 76, -18.0f);
    fill_ellipse(orbit2_pts, 64, 344, 124, 89, 59, 20.0f);
    fill_ellipse(orbit3_pts, 64, 345, 128, 70, 43, -28.0f);
    make_orbit(card, orbit1_pts, lv_color_hex(0x8FD7AF), LV_OPA_60, 2);
    make_orbit(card, orbit2_pts, lv_color_hex(0x4EBE82), LV_OPA_50, 2);
    make_orbit(card, orbit3_pts, lv_color_hex(0xCDEAD8), LV_OPA_80, 1);
    add_orbit_dot(card, 378, 35, 9);
    add_orbit_dot(card, 403, 87, 11);
    add_orbit_dot(card, 286, 56, 7);
    add_orbit_dot(card, 304, 174, 9);
    add_orbit_dot(card, 391, 187, 7);

    lv_obj_t *pill = lv_obj_create(card);
    lv_obj_set_pos(pill, 18, 14);
    lv_obj_set_size(pill, 124, 30);
    lv_obj_clear_flag(pill, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(pill, 0, 0);
    lv_obj_set_style_radius(pill, 15, 0);
    lv_obj_set_style_border_width(pill, 0, 0);
    lv_obj_set_style_bg_color(pill, lv_color_hex(0xE2F3E8), 0);
    make_img(pill, &zh_today_plan, 14, 4);
    lv_obj_add_flag(pill, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(pill, product_nav_clicked, LV_EVENT_CLICKED, (void*)(intptr_t)RP_EXERCISE_LIBRARY);

    make_img(card, &rx_fullbody, 18, 53);
    make_img(card, &rx_three_tasks, 20, 107);
    make_img(card, &zh_about18, 153, 107);

    lv_obj_t *sep = lv_obj_create(card);
    lv_obj_set_pos(sep, 18, 139);
    lv_obj_set_size(sep, 220, 1);
    lv_obj_set_style_pad_all(sep, 0, 0);
    lv_obj_set_style_border_width(sep, 0, 0);
    lv_obj_set_style_bg_color(sep, C_LINE, 0);

    make_img(card, &zh_completed, 18, 152);
    make_text(card, "0", &lv_font_montserrat_30, C_TEAL, 92, 143);
    make_text(card, "/ 3", &lv_font_montserrat_18, C_DARK, 118, 153);

    lv_obj_t *bar = lv_bar_create(card);
    lv_obj_set_pos(bar, 18, 184);
    lv_obj_set_size(bar, 225, 8);
    lv_bar_set_range(bar, 0, 3);
    lv_bar_set_value(bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_radius(bar, 4, LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0xE3EBE5), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(bar, 4, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(bar, C_TEAL_2, LV_PART_INDICATOR);

    lv_obj_t *btn = lv_btn_create(card);
    lv_obj_set_pos(btn, 18, 202);
    lv_obj_set_size(btn, 225, 34);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(btn, 0, 0);
    lv_obj_set_style_radius(btn, 17, 0);
    lv_obj_set_style_bg_color(btn, C_TEAL, 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x27945C), LV_STATE_PRESSED);
    lv_obj_set_style_shadow_width(btn, 8, 0);
    lv_obj_set_style_shadow_opa(btn, LV_OPA_20, 0);
    lv_obj_set_style_shadow_color(btn, C_TEAL, 0);
    lv_obj_add_event_cb(btn, start_clicked, LV_EVENT_CLICKED, nullptr);
    make_img(btn, &zh_start_today, 42, 5);

    lv_obj_t *arrow = make_circle(btn, 188, 3, 28, lv_color_white());
    make_text(arrow, ">", &lv_font_montserrat_18, C_TEAL, 8, 3);
}

static void create_right_cards(lv_obj_t *screen)
{
    lv_obj_t *week = make_card(screen, 472, 76, 148, 104, 18);
    lv_obj_t *total = make_card(screen, 632, 76, 148, 104, 18);
    lv_obj_t *record = make_card(screen, 472, 190, 308, 130, 18);

    make_img(week, &zh_week_persist, 18, 14);
    create_calendar_icon(week, 18, 42);
    make_text(week, "5", &lv_font_montserrat_30, C_TEAL, 70, 39);
    make_img(week, &zh_day, 104, 58);
    make_img(week, &zh_continuous, 70, 78);

    make_img(total, &zh_total_training, 18, 14);
    create_bars_icon(total, 18, 42);
    make_text(total, "128", &lv_font_montserrat_30, C_TEAL, 62, 39);
    make_img(total, &zh_minutes, 111, 57);
    make_img(total, &zh_week_total, 62, 78);

    make_img(record, &zh_record, 18, 15);
    make_img(record, &zh_more, 253, 16);
    make_text(record, ">", &lv_font_montserrat_18, C_MUTED, 283, 12);

    make_img(record, &zh_last, 24, 57);
    make_img(record, &zh_qualified, 164, 57);
    make_text(record, "16", &lv_font_montserrat_30, C_DARK, 24, 72);
    make_img(record, &zh_minutes, 66, 92);
    make_text(record, "89", &lv_font_montserrat_30, C_DARK, 164, 72);
    make_text(record, "%", &lv_font_montserrat_16, C_DARK, 207, 89);

    lv_obj_t *vline = lv_obj_create(record);
    lv_obj_set_pos(vline, 139, 52);
    lv_obj_set_size(vline, 1, 56);
    lv_obj_set_style_pad_all(vline, 0, 0);
    lv_obj_set_style_border_width(vline, 0, 0);
    lv_obj_set_style_bg_color(vline, C_LINE, 0);

    chart_pts[0] = {0, 31};
    chart_pts[1] = {16, 21};
    chart_pts[2] = {32, 25};
    chart_pts[3] = {48, 12};
    chart_pts[4] = {63, 18};
    chart_pts[5] = {78, 0};
    lv_obj_t *chart = lv_line_create(record);
    lv_obj_set_pos(chart, 218, 78);
    lv_obj_set_size(chart, 82, 35);
    lv_line_set_points(chart, chart_pts, 6);
    lv_obj_set_style_line_color(chart, lv_color_hex(0x63C58E), 0);
    lv_obj_set_style_line_width(chart, 2, 0);
    add_orbit_dot(record, 296, 78, 7);
    lv_obj_add_flag(week, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(week, product_nav_clicked, LV_EVENT_CLICKED, (void*)(intptr_t)RP_REPORT);
    lv_obj_add_flag(total, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(total, product_nav_clicked, LV_EVENT_CLICKED, (void*)(intptr_t)RP_HISTORY);
    lv_obj_add_flag(record, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(record, product_nav_clicked, LV_EVENT_CLICKED, (void*)(intptr_t)RP_REPORT);
}

static void create_flow(lv_obj_t *screen)
{
    lv_obj_t *flow = make_card(screen, 20, 330, 760, 68, 18);
    make_img(flow, &zh_flow, 18, 23);

    // Prescription item 1: 哑铃弯举 3组×10次
    lv_obj_t *sel = lv_obj_create(flow);
    lv_obj_set_pos(sel, 136, 8);
    lv_obj_set_size(sel, 184, 50);
    lv_obj_clear_flag(sel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(sel, 0, 0);
    lv_obj_set_style_radius(sel, 16, 0);
    lv_obj_set_style_border_width(sel, 0, 0);
    lv_obj_set_style_bg_color(sel, lv_color_hex(0xEEF7F1), 0);

    lv_obj_t *c1 = make_circle(sel, 12, 8, 34, C_TEAL_2);
    make_text(c1, "1", &lv_font_montserrat_18, lv_color_white(), 11, 5);
    make_img(sel, &rx_curl, 56, 4);
    lv_obj_t *tag = lv_obj_create(sel);
    lv_obj_set_pos(tag, 128, 4);
    lv_obj_set_size(tag, 48, 22);
    lv_obj_clear_flag(tag, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(tag, 0, 0);
    lv_obj_set_style_radius(tag, 11, 0);
    lv_obj_set_style_border_width(tag, 0, 0);
    lv_obj_set_style_bg_color(tag, lv_color_hex(0xDFF2E6), 0);
    make_img(tag, &rx_pending, 3, 1);
    make_img(sel, &rx_3x10, 56, 28);

    make_text(flow, ">", &lv_font_montserrat_20, C_MUTED, 326, 21);

    // Prescription item 2: 肱三头肌伸展 2组×10次
    lv_obj_t *c2 = make_circle(flow, 352, 16, 34, lv_color_hex(0xEFF2F0));
    make_text(c2, "2", &lv_font_montserrat_18, C_DARK, 11, 5);
    make_img(flow, &rx_triceps, 398, 12);
    make_img(flow, &rx_2x10, 398, 36);
    make_text(flow, ">", &lv_font_montserrat_20, C_MUTED, 530, 21);

    // Prescription item 3: 膝关节屈伸 3组×10次
    lv_obj_t *c3 = make_circle(flow, 558, 16, 34, lv_color_hex(0xEFF2F0));
    make_text(c3, "3", &lv_font_montserrat_18, C_DARK, 11, 5);
    make_img(flow, &rx_knee, 604, 12);
    make_img(flow, &rx_3x10, 604, 36);
    lv_obj_add_flag(flow, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(flow, product_nav_clicked, LV_EVENT_CLICKED, (void*)(intptr_t)RP_PLAN_DETAIL);
}

static void product_nav_clicked(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    const intptr_t page = (intptr_t)lv_event_get_user_data(e);
    rehab_ui_router_open((RehabProductPage)page);
}

static void create_nav(lv_obj_t *screen)
{
    lv_obj_t *nav = make_card(screen, 20, 410, 760, 56, 18);

    lv_obj_t *line1 = lv_obj_create(nav);
    lv_obj_set_pos(line1, 250, 10);
    lv_obj_set_size(line1, 1, 34);
    lv_obj_set_style_pad_all(line1, 0, 0);
    lv_obj_set_style_border_width(line1, 0, 0);
    lv_obj_set_style_bg_color(line1, C_LINE, 0);
    lv_obj_t *line2 = lv_obj_create(nav);
    lv_obj_set_pos(line2, 510, 10);
    lv_obj_set_size(line2, 1, 34);
    lv_obj_set_style_pad_all(line2, 0, 0);
    lv_obj_set_style_border_width(line2, 0, 0);
    lv_obj_set_style_bg_color(line2, C_LINE, 0);

    make_text(nav, LV_SYMBOL_HOME, &lv_font_montserrat_20, C_TEAL_2, 82, 13);
    make_img(nav, &zh_home, 118, 16);
    make_text(nav, LV_SYMBOL_LIST, &lv_font_montserrat_20, lv_color_hex(0x89928C), 338, 13);
    make_img(nav, &zh_records, 374, 16);
    make_text(nav, LV_SYMBOL_SETTINGS, &lv_font_montserrat_20, lv_color_hex(0x89928C), 596, 13);
    make_img(nav, &zh_settings, 632, 16);

    lv_obj_t *active = lv_obj_create(nav);
    lv_obj_set_pos(active, 78, 49);
    lv_obj_set_size(active, 104, 3);
    lv_obj_set_style_pad_all(active, 0, 0);
    lv_obj_set_style_radius(active, 2, 0);
    lv_obj_set_style_border_width(active, 0, 0);
    lv_obj_set_style_bg_color(active, C_TEAL_2, 0);

    // Product-shell navigation. HOME remains the approved dashboard; the other
    // tabs open the full repository page set without disturbing the training flow.
    lv_obj_t *records_hit = lv_btn_create(nav);
    lv_obj_set_pos(records_hit, 252, 0); lv_obj_set_size(records_hit, 258, 56);
    lv_obj_set_style_bg_opa(records_hit, LV_OPA_TRANSP, 0); lv_obj_set_style_border_width(records_hit, 0, 0);
    lv_obj_set_style_shadow_width(records_hit, 0, 0);
    lv_obj_add_event_cb(records_hit, product_nav_clicked, LV_EVENT_CLICKED, (void*)(intptr_t)RP_HISTORY);

    lv_obj_t *settings_hit = lv_btn_create(nav);
    lv_obj_set_pos(settings_hit, 512, 0); lv_obj_set_size(settings_hit, 248, 56);
    lv_obj_set_style_bg_opa(settings_hit, LV_OPA_TRANSP, 0); lv_obj_set_style_border_width(settings_hit, 0, 0);
    lv_obj_set_style_shadow_width(settings_hit, 0, 0);
    lv_obj_add_event_cb(settings_hit, product_nav_clicked, LV_EVENT_CLICKED, (void*)(intptr_t)RP_SETTINGS);
}


static void prescription_sync_close(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    if (g_prescription_sync_mask) {
        lv_obj_del(g_prescription_sync_mask);
    }
    g_prescription_sync_mask = nullptr;
    g_prescription_sync_panel = nullptr;
}

static void show_prescription_sync_popup()
{
#if DEMO_PRESCRIPTION_SYNC
    if (g_prescription_sync_mask) return;

    lv_obj_t *screen = lv_scr_act();

    // Full-screen dim layer blocks accidental touches behind the modal.
    g_prescription_sync_mask = lv_obj_create(screen);
    lv_obj_set_pos(g_prescription_sync_mask, 0, 0);
    lv_obj_set_size(g_prescription_sync_mask, 800, 480);
    lv_obj_clear_flag(g_prescription_sync_mask, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(g_prescription_sync_mask, 0, 0);
    lv_obj_set_style_radius(g_prescription_sync_mask, 0, 0);
    lv_obj_set_style_border_width(g_prescription_sync_mask, 0, 0);
    lv_obj_set_style_bg_color(g_prescription_sync_mask, lv_color_hex(0x243029), 0);
    lv_obj_set_style_bg_opa(g_prescription_sync_mask, LV_OPA_40, 0);

    g_prescription_sync_panel = lv_obj_create(g_prescription_sync_mask);
    lv_obj_set_size(g_prescription_sync_panel, 520, 326);
    lv_obj_center(g_prescription_sync_panel);
    lv_obj_clear_flag(g_prescription_sync_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(g_prescription_sync_panel, 0, 0);
    lv_obj_set_style_radius(g_prescription_sync_panel, 24, 0);
    lv_obj_set_style_bg_color(g_prescription_sync_panel, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(g_prescription_sync_panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(g_prescription_sync_panel, 1, 0);
    lv_obj_set_style_border_color(g_prescription_sync_panel, lv_color_hex(0xDCE9E0), 0);
    lv_obj_set_style_shadow_width(g_prescription_sync_panel, 24, 0);
    lv_obj_set_style_shadow_opa(g_prescription_sync_panel, LV_OPA_20, 0);
    lv_obj_set_style_shadow_ofs_y(g_prescription_sync_panel, 8, 0);

    lv_obj_t *ok = make_circle(g_prescription_sync_panel, 34, 28, 46, lv_color_hex(0xDFF3E6));
    make_text(ok, LV_SYMBOL_OK, &lv_font_montserrat_22, lv_color_hex(0x2F8B57), 12, 8);

    make_img(g_prescription_sync_panel, &rx_popup_title, 96, 24);
    make_img(g_prescription_sync_panel, &rx_popup_subtitle, 96, 61);

    lv_obj_t *sync_badge = lv_obj_create(g_prescription_sync_panel);
    lv_obj_set_pos(sync_badge, 386, 24);
    lv_obj_set_size(sync_badge, 112, 24);
    lv_obj_clear_flag(sync_badge, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(sync_badge, 0, 0);
    lv_obj_set_style_radius(sync_badge, 12, 0);
    lv_obj_set_style_border_width(sync_badge, 0, 0);
    lv_obj_set_style_bg_color(sync_badge, lv_color_hex(0xF0F3F1), 0);
    make_img(sync_badge, &rx_popup_sync, 6, 3);

    struct RxRow {
        const lv_img_dsc_t *name;
        const lv_img_dsc_t *dose;
    };
    const RxRow rows[3] = {
        {&rx_curl, &rx_3x10},
        {&rx_triceps, &rx_2x10},
        {&rx_knee, &rx_3x10},
    };

    for (int i = 0; i < 3; ++i) {
        const int y = 104 + i * 55;
        lv_obj_t *row = lv_obj_create(g_prescription_sync_panel);
        lv_obj_set_pos(row, 28, y);
        lv_obj_set_size(row, 464, 45);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_pad_all(row, 0, 0);
        lv_obj_set_style_radius(row, 14, 0);
        lv_obj_set_style_border_width(row, 1, 0);
        lv_obj_set_style_border_color(row, lv_color_hex(0xE5ECE7), 0);
        lv_obj_set_style_bg_color(row, i == 0 ? lv_color_hex(0xF3FAF6) : lv_color_hex(0xFBFCFB), 0);

        lv_obj_t *num = make_circle(row, 12, 8, 29, i == 0 ? C_TEAL_2 : lv_color_hex(0xE9EFEB));
        char nbuf[2] = {(char)('1' + i), '\0'};
        make_text(num, nbuf, &lv_font_montserrat_14,
                  i == 0 ? lv_color_white() : C_DARK, 10, 5);
        make_img(row, rows[i].name, 56, 10);
        make_img(row, rows[i].dose, 357, 13);
    }

    make_img(g_prescription_sync_panel, &rx_from_app, 32, 279);

    lv_obj_t *btn = lv_btn_create(g_prescription_sync_panel);
    lv_obj_set_pos(btn, 382, 270);
    lv_obj_set_size(btn, 110, 42);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(btn, 0, 0);
    lv_obj_set_style_radius(btn, 18, 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_bg_color(btn, C_TEAL, 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x27945C), LV_STATE_PRESSED);
    lv_obj_add_event_cb(btn, prescription_sync_close, LV_EVENT_CLICKED, nullptr);
    make_img(btn, &rx_confirm, 35, 9);

    Serial.println("DEMO_PRESCRIPTION_POPUP: shown");
#endif
}

static void prescription_sync_long_pressed(lv_event_t *e)
{
#if DEMO_PRESCRIPTION_SYNC
    if (lv_event_get_code(e) != LV_EVENT_LONG_PRESSED) return;
    show_prescription_sync_popup();
#else
    (void)e;
#endif
}

// ---------------- Voice assistant interaction surface ----------------
// Timeline after the hidden long-press fires:
//   0.00s  listening overlay + animated voice bars
//   1.30s  recognized command: "开始训练"
//   1.85s  assistant response state: "好的，开始训练"
//   3.35s  overlay closes and the normal IMU wear/connection page opens
// The reply audio itself is intended to be added during video post-production.
static void voice_assistant_clear_objects()
{
    if (g_voice_assistant_mask) {
        lv_obj_del(g_voice_assistant_mask);
    }
    g_voice_assistant_mask = nullptr;
    g_voice_assistant_panel = nullptr;
    g_voice_assistant_state_img = nullptr;
    g_voice_assistant_pulse = nullptr;
    for (int i = 0; i < 5; ++i) g_voice_assistant_wave[i] = nullptr;
    g_voice_assistant_stage = 0;
}

static void voice_assistant_cancel()
{
    if (g_voice_assistant_timer) {
        lv_timer_del(g_voice_assistant_timer);
        g_voice_assistant_timer = nullptr;
    }
    voice_assistant_clear_objects();
}

static void voice_assistant_set_state_image(const lv_img_dsc_t *src)
{
    if (!g_voice_assistant_state_img || !src) return;
    lv_img_set_src(g_voice_assistant_state_img, src);
    // Keep all three Chinese bitmap states visually centered in the text zone.
    lv_obj_align(g_voice_assistant_state_img, LV_ALIGN_TOP_LEFT, 118, 31);
}

static void voice_assistant_timer_cb(lv_timer_t *timer)
{
#if DEMO_VOICE_ASSISTANT
    if (!g_voice_assistant_mask || !g_voice_assistant_panel) {
        g_voice_assistant_timer = nullptr;
        lv_timer_del(timer);
        return;
    }

    const uint32_t elapsed = lv_tick_elaps(g_voice_assistant_started_at);

    // Animate the five voice bars while the user is speaking and while Xiaosi replies.
    static const uint8_t wave_pattern[8][5] = {
        {8, 16, 24, 14, 9},
        {12, 24, 14, 22, 11},
        {20, 12, 27, 15, 23},
        {9, 21, 16, 27, 13},
        {16, 27, 11, 20, 25},
        {24, 14, 22, 10, 18},
        {13, 20, 26, 18, 9},
        {18, 10, 17, 24, 15},
    };
    const bool animate_wave = (g_voice_assistant_stage == 0 || g_voice_assistant_stage == 2);
    const int pat = (elapsed / 90U) % 8U;
    for (int i = 0; i < 5; ++i) {
        if (!g_voice_assistant_wave[i]) continue;
        const int h = animate_wave ? wave_pattern[pat][i] : 7;
        lv_obj_set_height(g_voice_assistant_wave[i], h);
        lv_obj_set_y(g_voice_assistant_wave[i], 108 - h / 2);
    }

    // Soft breathing ring around the microphone.
    if (g_voice_assistant_pulse) {
        const int p = (elapsed / 70U) % 12U;
        const int tri = (p <= 6) ? p : (12 - p);
        const int size = 76 + tri * 2;
        lv_obj_set_size(g_voice_assistant_pulse, size, size);
        lv_obj_set_pos(g_voice_assistant_pulse, 58 - size / 2, 73 - size / 2);
        lv_obj_set_style_border_opa(g_voice_assistant_pulse, (lv_opa_t)(85 - tri * 7), 0);
    }

    if (g_voice_assistant_stage == 0 && elapsed >= 1300U) {
        g_voice_assistant_stage = 1;
        voice_assistant_set_state_image(&voice_recognized);
        Serial.println("DEMO_VOICE: recognized START_TRAINING");
    }

    if (g_voice_assistant_stage == 1 && elapsed >= 1850U) {
        g_voice_assistant_stage = 2;
        voice_assistant_set_state_image(&voice_reply);
        Serial.println("DEMO_VOICE: reply cue - add audio '好的，开始训练'");
    }

    if (elapsed >= 3350U) {
        g_voice_assistant_timer = nullptr;
        lv_timer_del(timer);
        voice_assistant_clear_objects();

        // Reuse the real start path so the next screen is not a separate fake page.
        rehab_uart_link_send_command("START_FLOW");
        lv_async_call(show_wear_async, nullptr);
        Serial.println("DEMO_VOICE: -> IMU wear page");
    }
#else
    (void)timer;
#endif
}

static void show_voice_assistant_popup()
{
#if DEMO_VOICE_ASSISTANT
    if (g_voice_assistant_mask || g_prescription_sync_mask) return;

    voice_assistant_cancel();
    lv_obj_t *screen = lv_scr_act();

    // Slight dimming keeps the existing home page visible while making the voice card
    // easy to read in a phone-shot competition video.
    g_voice_assistant_mask = lv_obj_create(screen);
    lv_obj_set_pos(g_voice_assistant_mask, 0, 0);
    lv_obj_set_size(g_voice_assistant_mask, 800, 480);
    lv_obj_clear_flag(g_voice_assistant_mask, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(g_voice_assistant_mask, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_pad_all(g_voice_assistant_mask, 0, 0);
    lv_obj_set_style_radius(g_voice_assistant_mask, 0, 0);
    lv_obj_set_style_border_width(g_voice_assistant_mask, 0, 0);
    lv_obj_set_style_bg_color(g_voice_assistant_mask, lv_color_hex(0x25312B), 0);
    lv_obj_set_style_bg_opa(g_voice_assistant_mask, LV_OPA_20, 0);

    g_voice_assistant_panel = lv_obj_create(g_voice_assistant_mask);
    lv_obj_set_size(g_voice_assistant_panel, 382, 148);
    lv_obj_align(g_voice_assistant_panel, LV_ALIGN_CENTER, 0, -12);
    lv_obj_clear_flag(g_voice_assistant_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(g_voice_assistant_panel, 0, 0);
    lv_obj_set_style_radius(g_voice_assistant_panel, 26, 0);
    lv_obj_set_style_bg_color(g_voice_assistant_panel, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(g_voice_assistant_panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(g_voice_assistant_panel, 1, 0);
    lv_obj_set_style_border_color(g_voice_assistant_panel, lv_color_hex(0xDCE9E0), 0);
    lv_obj_set_style_shadow_width(g_voice_assistant_panel, 28, 0);
    lv_obj_set_style_shadow_opa(g_voice_assistant_panel, LV_OPA_20, 0);
    lv_obj_set_style_shadow_color(g_voice_assistant_panel, lv_color_hex(0x506158), 0);
    lv_obj_set_style_shadow_ofs_y(g_voice_assistant_panel, 8, 0);

    // Breathing ring.
    g_voice_assistant_pulse = lv_obj_create(g_voice_assistant_panel);
    lv_obj_set_pos(g_voice_assistant_pulse, 20, 35);
    lv_obj_set_size(g_voice_assistant_pulse, 76, 76);
    lv_obj_clear_flag(g_voice_assistant_pulse, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(g_voice_assistant_pulse, 0, 0);
    lv_obj_set_style_radius(g_voice_assistant_pulse, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(g_voice_assistant_pulse, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(g_voice_assistant_pulse, 2, 0);
    lv_obj_set_style_border_color(g_voice_assistant_pulse, lv_color_hex(0x74CFA0), 0);
    lv_obj_set_style_border_opa(g_voice_assistant_pulse, LV_OPA_50, 0);

    lv_obj_t *mic_bg = make_circle(g_voice_assistant_panel, 30, 45, 56, C_TEAL_2);
    lv_obj_set_style_shadow_width(mic_bg, 12, 0);
    lv_obj_set_style_shadow_opa(mic_bg, LV_OPA_20, 0);
    lv_obj_set_style_shadow_color(mic_bg, C_TEAL_2, 0);

    // Simple microphone glyph made only with LVGL primitives (no extra icon font).
    lv_obj_t *mic_head = lv_obj_create(mic_bg);
    lv_obj_set_pos(mic_head, 20, 10);
    lv_obj_set_size(mic_head, 16, 25);
    lv_obj_clear_flag(mic_head, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(mic_head, 0, 0);
    lv_obj_set_style_radius(mic_head, 8, 0);
    lv_obj_set_style_bg_color(mic_head, lv_color_white(), 0);
    lv_obj_set_style_border_width(mic_head, 0, 0);

    lv_obj_t *mic_stem = lv_obj_create(mic_bg);
    lv_obj_set_pos(mic_stem, 27, 35);
    lv_obj_set_size(mic_stem, 3, 10);
    lv_obj_set_style_pad_all(mic_stem, 0, 0);
    lv_obj_set_style_radius(mic_stem, 2, 0);
    lv_obj_set_style_bg_color(mic_stem, lv_color_white(), 0);
    lv_obj_set_style_border_width(mic_stem, 0, 0);

    lv_obj_t *mic_base = lv_obj_create(mic_bg);
    lv_obj_set_pos(mic_base, 20, 44);
    lv_obj_set_size(mic_base, 17, 3);
    lv_obj_set_style_pad_all(mic_base, 0, 0);
    lv_obj_set_style_radius(mic_base, 2, 0);
    lv_obj_set_style_bg_color(mic_base, lv_color_white(), 0);
    lv_obj_set_style_border_width(mic_base, 0, 0);

    g_voice_assistant_state_img = make_img(g_voice_assistant_panel, &voice_listening, 118, 31);

    // Five animated audio bars below the recognition text.
    for (int i = 0; i < 5; ++i) {
        g_voice_assistant_wave[i] = lv_obj_create(g_voice_assistant_panel);
        lv_obj_set_pos(g_voice_assistant_wave[i], 126 + i * 26, 104);
        lv_obj_set_size(g_voice_assistant_wave[i], 7, 9 + i * 2);
        lv_obj_clear_flag(g_voice_assistant_wave[i], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_pad_all(g_voice_assistant_wave[i], 0, 0);
        lv_obj_set_style_radius(g_voice_assistant_wave[i], 4, 0);
        lv_obj_set_style_border_width(g_voice_assistant_wave[i], 0, 0);
        lv_obj_set_style_bg_color(g_voice_assistant_wave[i], C_TEAL_2, 0);
    }

    // Tiny non-CJK helper label avoids adding another bitmap while keeping the card balanced.
    make_text(g_voice_assistant_panel, "XIAOSI  VOICE", &lv_font_montserrat_12,
              lv_color_hex(0x7A867F), 267, 104);

    g_voice_assistant_stage = 0;
    g_voice_assistant_started_at = lv_tick_get();
    g_voice_assistant_timer = lv_timer_create(voice_assistant_timer_cb, 55, nullptr);
    Serial.println("DEMO_VOICE: listening overlay shown");
#endif
}

static void voice_assistant_long_pressed(lv_event_t *e)
{
#if DEMO_VOICE_ASSISTANT
    if (lv_event_get_code(e) != LV_EVENT_LONG_PRESSED) return;
    show_voice_assistant_popup();
#else
    (void)e;
#endif
}

static void create_header(lv_obj_t *screen)
{
    create_logo(screen);
    make_text(screen, "RehabMotion", &lv_font_montserrat_24, lv_color_hex(0x243029), 70, 9);
    make_img(screen, &zh_qingsongbeng, 242, 16);

    make_text(screen, "09:45", &lv_font_montserrat_30, C_DARK, 504, 3);
    make_text(screen, "2026/08/17", &lv_font_montserrat_12, C_MUTED, 508, 39);

    lv_obj_t *ready = make_card(screen, 620, 11, 160, 53, 16);
#if DEMO_PRESCRIPTION_SYNC
    // Hidden video trigger: long-press the top-right device-ready card.
    lv_obj_add_flag(ready, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ready, prescription_sync_long_pressed, LV_EVENT_LONG_PRESSED, nullptr);
#endif
    lv_obj_set_style_shadow_width(ready, 8, 0);
    lv_obj_t *ok = make_circle(ready, 12, 11, 24, C_GREEN);
    make_text(ok, LV_SYMBOL_OK, &lv_font_montserrat_14, lv_color_white(), 5, 4);
    make_img(ready, &zh_device_ready, 43, 7);
    make_img(ready, &zh_start_hint, 43, 29);
    make_text(ready, ">", &lv_font_montserrat_18, C_MUTED, 137, 13);

#if DEMO_VOICE_ASSISTANT
    // Hidden video trigger: long-press the top-left brand area. It is fully transparent
    // and does not change the approved home-page appearance.
    lv_obj_t *voice_hotspot = lv_obj_create(screen);
    lv_obj_set_pos(voice_hotspot, 0, 0);
    lv_obj_set_size(voice_hotspot, 185, 70);
    lv_obj_clear_flag(voice_hotspot, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(voice_hotspot, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_pad_all(voice_hotspot, 0, 0);
    lv_obj_set_style_radius(voice_hotspot, 0, 0);
    lv_obj_set_style_border_width(voice_hotspot, 0, 0);
    lv_obj_set_style_bg_opa(voice_hotspot, LV_OPA_TRANSP, 0);
    lv_obj_add_event_cb(voice_hotspot, voice_assistant_long_pressed, LV_EVENT_LONG_PRESSED, nullptr);
    lv_obj_move_foreground(voice_hotspot);
#endif
}

// ---------------- RehabMotion complete elbow-curl screen flow ----------------
// The normal UI remains LVGL layout + compiled assets. Only 动作校准 2/3 uses TF-card
// animation frames. Other pages keep the approved compiled person assets.

static lv_obj_t *g_imu_status_dot[5] = {nullptr};
static lv_obj_t *g_imu_status_img[5] = {nullptr};
static lv_obj_t *g_imu_status_symbol[5] = {nullptr};
static bool g_imu_connected[5] = {false, false, false, false, false};

static lv_obj_t *g_train_reps_value = nullptr;
static lv_obj_t *g_train_angle_value = nullptr;
static lv_obj_t *g_train_group_value = nullptr;
static lv_obj_t *g_train_progress_bar = nullptr;
static lv_obj_t *g_train_progress_value = nullptr;
static lv_obj_t *g_train_left_value = nullptr;
static lv_obj_t *g_train_right_value = nullptr;
static lv_obj_t *g_train_target_value = nullptr;
static lv_obj_t *g_train_feedback_img = nullptr;

// V4.4.20 screen polish: dynamic positioning/calibration progress + large center feedback.
static lv_obj_t *g_stage_progress_bar = nullptr;
static lv_obj_t *g_body_pose_dots[4] = {nullptr, nullptr, nullptr, nullptr};

// V4.4.27 screen: controller telemetry arrives at 20 Hz. Repeated LV_ANIM_ON
// calls restart LVGL's animation before it reaches the target, which made the
// bar visibly lag and sometimes leave the page at 80-90%. We interpolate the
// displayed value ourselves at 40 Hz and draw with LV_ANIM_OFF.
static lv_timer_t *g_stage_progress_timer = nullptr;
static int g_stage_progress_state = -1;
static int g_stage_progress_target = 0;
static int g_stage_progress_display = 0;

static lv_obj_t *g_feedback_overlay = nullptr;
static lv_obj_t *g_feedback_overlay_img = nullptr;
static lv_timer_t *g_feedback_overlay_timer = nullptr;
static uint16_t g_last_feedback_mask = 0;

static lv_obj_t *g_result_total_value = nullptr;
static lv_obj_t *g_result_completion_value = nullptr;
static lv_obj_t *g_result_rom_value = nullptr;
static lv_obj_t *g_result_duration_value = nullptr;
static lv_obj_t *g_result_summary_value = nullptr;

static lv_obj_t *g_rest_seconds_value = nullptr;
static lv_obj_t *g_rest_group_value = nullptr;
static lv_obj_t *g_rest_arc = nullptr;
static lv_timer_t *g_rest_timer = nullptr;
static int g_rest_remaining = 30;
static int g_current_group = 1;
static int g_target_groups = 3;
static int g_target_reps = 10;
static int g_target_angle = 80;

// Persist the last training values so pausing/resuming does not reset the visible progress.
static int g_last_left_count = 0;
static int g_last_right_count = 0;
static float g_last_left_angle_deg = 0.0f;
static float g_last_right_angle_deg = 0.0f;
static int g_last_progress_percent = 0;
static bool g_training_paused = false;


static lv_timer_t *g_countdown_timer = nullptr;
static lv_obj_t *g_countdown_overlay = nullptr;
static lv_obj_t *g_countdown_number = nullptr;
static int g_countdown_value = 3;


// ---- Shared helpers for the full training-flow pages ----
// These are defined before the page builders so PlatformIO/C++ sees every symbol
// before first use. The approved motion animation is implemented separately in
// rehab_tf_animation.cpp so the page code stays simple.
static void show_wear_ui();
static void show_calibration_1_ui();
static void show_calibration_2_ui();
static void show_calibration_3_ui();
static void show_training_ui();
static void show_paused_ui();
static void show_rest_ui(int seconds = 30);
static void show_result_ui();

static void show_wear_async(void *user_data) { (void)user_data; show_wear_ui(); }
static void show_cal1_async(void *user_data) { (void)user_data; show_calibration_1_ui(); }
static void show_cal2_async(void *user_data) { (void)user_data; show_calibration_2_ui(); }
static void show_cal3_async(void *user_data) { (void)user_data; show_calibration_3_ui(); }
static void show_training_async(void *user_data) { (void)user_data; show_training_ui(); }
static void show_paused_async(void *user_data) { (void)user_data; show_paused_ui(); }
static void show_result_async(void *user_data) { (void)user_data; show_result_ui(); }

static void flow_stop_rest_timer()
{
    if (g_rest_timer) {
        lv_timer_del(g_rest_timer);
        g_rest_timer = nullptr;
    }
}

static void flow_stop_countdown()
{
    if (g_countdown_timer) {
        lv_timer_del(g_countdown_timer);
        g_countdown_timer = nullptr;
    }
    g_countdown_overlay = nullptr;
    g_countdown_number = nullptr;
}

static void flow_stop_feedback_overlay()
{
    if (g_feedback_overlay_timer) {
        lv_timer_del(g_feedback_overlay_timer);
        g_feedback_overlay_timer = nullptr;
    }
    if (g_feedback_overlay) {
        lv_obj_del(g_feedback_overlay);
    }
    g_feedback_overlay = nullptr;
    g_feedback_overlay_img = nullptr;
}

static void feedback_overlay_timeout(lv_timer_t *timer)
{
    (void)timer;
    if (g_feedback_overlay) {
        lv_obj_del(g_feedback_overlay);
    }
    g_feedback_overlay = nullptr;
    g_feedback_overlay_img = nullptr;
    g_feedback_overlay_timer = nullptr;
}

static void show_large_feedback_overlay(const lv_img_dsc_t *src, bool success_style = false, uint32_t hold_ms = 1600)
{
    if (!src) return;
    flow_stop_feedback_overlay();

    lv_obj_t *screen = lv_scr_act();
    g_feedback_overlay = lv_obj_create(screen);
    lv_obj_set_size(g_feedback_overlay, 390, 122);
    lv_obj_align(g_feedback_overlay, LV_ALIGN_CENTER, 0, -8);
    lv_obj_clear_flag(g_feedback_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(g_feedback_overlay, 0, 0);
    lv_obj_set_style_radius(g_feedback_overlay, 24, 0);
    lv_obj_set_style_bg_color(g_feedback_overlay,
                              success_style ? lv_color_hex(0xF1FAF4) : lv_color_hex(0xFFF8F5), 0);
    lv_obj_set_style_bg_opa(g_feedback_overlay, LV_OPA_90, 0);
    lv_obj_set_style_border_width(g_feedback_overlay, 2, 0);
    lv_obj_set_style_border_color(g_feedback_overlay,
                                  success_style ? lv_color_hex(0x9FD8B3) : lv_color_hex(0xE7A397), 0);
    lv_obj_set_style_shadow_width(g_feedback_overlay, 24, 0);
    lv_obj_set_style_shadow_opa(g_feedback_overlay, LV_OPA_20, 0);
    lv_obj_set_style_shadow_color(g_feedback_overlay,
                                  success_style ? lv_color_hex(0x486A55) : lv_color_hex(0x7A584F), 0);
    lv_obj_set_style_shadow_ofs_y(g_feedback_overlay, 7, 0);

    lv_obj_t *icon = make_circle(
        g_feedback_overlay, 28, 31, 60,
        success_style ? lv_color_hex(0xDDF3E5) : lv_color_hex(0xF5D8D1));
    make_text(icon,
              success_style ? LV_SYMBOL_OK : "!",
              success_style ? &lv_font_montserrat_24 : &lv_font_montserrat_36,
              success_style ? lv_color_hex(0x2F8B57) : lv_color_hex(0xB54635),
              success_style ? 16 : 20,
              success_style ? 12 : 7);

    g_feedback_overlay_img = make_img(g_feedback_overlay, src, 0, 0);
    lv_obj_align(g_feedback_overlay_img, LV_ALIGN_CENTER, 38, 0);

    if (hold_ms < 1000) hold_ms = 1000;
    g_feedback_overlay_timer = lv_timer_create(feedback_overlay_timeout, hold_ms, nullptr);
    lv_timer_set_repeat_count(g_feedback_overlay_timer, 1);
}

static void flow_prepare_screen()
{
    flow_stop_rest_timer();
    flow_stop_countdown();
    voice_assistant_cancel();
    rehab_motion_animation_stop();

    for (int i = 0; i < 5; ++i) {
        g_imu_status_dot[i] = nullptr;
        g_imu_status_img[i] = nullptr;
        g_imu_status_symbol[i] = nullptr;
    }
    g_train_reps_value = nullptr;
    g_train_angle_value = nullptr;
    g_train_group_value = nullptr;
    g_train_progress_bar = nullptr;
    g_train_progress_value = nullptr;
    g_train_left_value = nullptr;
    g_train_right_value = nullptr;
    g_train_target_value = nullptr;
    g_train_feedback_img = nullptr;
    g_stage_progress_bar = nullptr;
    for (int i = 0; i < 4; ++i) g_body_pose_dots[i] = nullptr;
    flow_stop_feedback_overlay();
    g_last_feedback_mask = 0;
    g_result_total_value = nullptr;
    g_result_completion_value = nullptr;
    g_result_rom_value = nullptr;
    g_result_duration_value = nullptr;
    g_result_summary_value = nullptr;
    g_rest_seconds_value = nullptr;
    g_rest_group_value = nullptr;
    g_rest_arc = nullptr;

    lv_obj_t *screen = lv_scr_act();
    lv_obj_clean(screen);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(screen, 0, 0);
    lv_obj_set_style_bg_color(screen, C_BG, 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
}

static void flow_create_brand_header(lv_obj_t *screen, int current, int total)
{
    create_logo(screen);
    make_text(screen, "RehabMotion", &lv_font_montserrat_24, lv_color_hex(0x243029), 70, 9);
    make_img(screen, &zh_qingsongbeng, 242, 16);
    // Deliberately no second subtitle line.  Only "RehabMotion 轻松绷" remains.

    char buf[20];
    snprintf(buf, sizeof(buf), "%d / %d", current, total);
    make_text(screen, buf, &lv_font_montserrat_16, C_DARK, 665, 18);

    lv_obj_t *track = lv_obj_create(screen);
    lv_obj_set_pos(track, 710, 24);
    lv_obj_set_size(track, 66, 6);
    lv_obj_clear_flag(track, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(track, 0, 0);
    lv_obj_set_style_radius(track, 3, 0);
    lv_obj_set_style_border_width(track, 0, 0);
    lv_obj_set_style_bg_color(track, lv_color_hex(0xE7ECE8), 0);

    int fill_w = (total > 0) ? (66 * current / total) : 0;
    if (fill_w < 6 && current > 0) fill_w = 6;
    lv_obj_t *fill = lv_obj_create(track);
    lv_obj_set_pos(fill, 0, 0);
    lv_obj_set_size(fill, fill_w, 6);
    lv_obj_clear_flag(fill, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(fill, 0, 0);
    lv_obj_set_style_radius(fill, 3, 0);
    lv_obj_set_style_border_width(fill, 0, 0);
    lv_obj_set_style_bg_color(fill, C_GREEN, 0);
}

static lv_obj_t *flow_make_left_panel(lv_obj_t *screen)
{
    lv_obj_t *p = make_card(screen, 24, 52, 432, 352, 18);
    lv_obj_set_style_bg_color(p, lv_color_hex(0xFBFDFB), 0);
    lv_obj_set_style_bg_grad_color(p, lv_color_hex(0xEDF7F0), 0);
    lv_obj_set_style_bg_grad_dir(p, LV_GRAD_DIR_VER, 0);

    for (int i = 0; i < 3; ++i) {
        lv_obj_t *ring = lv_obj_create(p);
        int size = 270 - i * 45;
        lv_obj_set_pos(ring, 216 - size / 2, 170 - size / 2);
        lv_obj_set_size(ring, size, size);
        lv_obj_clear_flag(ring, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_pad_all(ring, 0, 0);
        lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(ring, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(ring, 1, 0);
        lv_obj_set_style_border_color(ring, lv_color_hex(0xD8EEE0), 0);
        lv_obj_set_style_border_opa(ring, LV_OPA_40, 0);
    }
    return p;
}

static lv_obj_t *flow_make_motion_panel(lv_obj_t *screen)
{
    // Flat #F6FBF8 background exactly matches the pre-rendered RGB565 frames.
    lv_obj_t *p = make_card(screen, 24, 52, 432, 352, 18);
    lv_obj_set_style_bg_color(p, lv_color_hex(0xF6FBF8), 0);
    lv_obj_set_style_bg_grad_dir(p, LV_GRAD_DIR_NONE, 0);
    return p;
}

static lv_obj_t *flow_make_right_panel(lv_obj_t *screen)
{
    return make_card(screen, 470, 52, 306, 352, 18);
}

static lv_obj_t *flow_make_outline_button(lv_obj_t *parent, int x, int y, int w, int h,
                                          const lv_img_dsc_t *label)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_size(btn, w, h);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(btn, 0, 0);
    lv_obj_set_style_radius(btn, 13, 0);
    lv_obj_set_style_bg_color(btn, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_style_border_color(btn, C_GREEN, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_t *im = make_img(btn, label, 0, 0);
    lv_obj_center(im);
    return btn;
}

static lv_obj_t *flow_make_green_button(lv_obj_t *parent, int x, int y, int w, int h,
                                        const lv_img_dsc_t *label)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_size(btn, w, h);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(btn, 0, 0);
    lv_obj_set_style_radius(btn, 13, 0);
    lv_obj_set_style_bg_color(btn, C_GREEN, 0);
    lv_obj_set_style_bg_grad_color(btn, lv_color_hex(0x3FC27D), 0);
    lv_obj_set_style_bg_grad_dir(btn, LV_GRAD_DIR_HOR, 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_shadow_width(btn, 8, 0);
    lv_obj_set_style_shadow_opa(btn, LV_OPA_10, 0);
    lv_obj_t *im = make_img(btn, label, 0, 0);
    lv_obj_center(im);
    return btn;
}

static lv_obj_t *flow_make_person(lv_obj_t *parent, const lv_img_dsc_t *src,
                                  int x, int y, int zoom = 256)
{
    lv_obj_t *im = lv_img_create(parent);
    lv_img_set_src(im, src);
    lv_obj_set_pos(im, x, y);
    lv_img_set_zoom(im, zoom);
    return im;
}

static lv_obj_t *flow_make_sensor_band(lv_obj_t *parent, int x, int y, int w = 24)
{
    lv_obj_t *band = lv_obj_create(parent);
    lv_obj_set_pos(band, x, y);
    lv_obj_set_size(band, w, 11);
    lv_obj_clear_flag(band, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(band, 0, 0);
    lv_obj_set_style_radius(band, 5, 0);
    lv_obj_set_style_bg_color(band, lv_color_hex(0x313735), 0);
    lv_obj_set_style_border_width(band, 0, 0);

    lv_obj_t *module = lv_obj_create(band);
    lv_obj_set_pos(module, w / 2 - 5, 1);
    lv_obj_set_size(module, 10, 9);
    lv_obj_clear_flag(module, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(module, 0, 0);
    lv_obj_set_style_radius(module, 3, 0);
    lv_obj_set_style_bg_color(module, lv_color_hex(0x49514E), 0);
    lv_obj_set_style_border_width(module, 1, 0);
    lv_obj_set_style_border_color(module, lv_color_hex(0x59625E), 0);
    make_circle(module, 4, 3, 3, C_GREEN);
    return band;
}

static void flow_home_back_clicked(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    // Keep controller state synchronized with the locally visible HOME page.
    rehab_uart_link_send_command("HOME");
    lv_async_call(show_home_async, nullptr);
}

static void flow_wear_start_clicked(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    for (int i = 0; i < 5; ++i) {
        if (!g_imu_connected[i]) {
            Serial.println("WEAR_CHECK_BLOCKED: IMU not all connected");
            return;
        }
    }
    rehab_uart_link_send_command("BEGIN_POSITION");
    lv_async_call(show_body_positioning_async, nullptr);
}

static void flow_cal1_back_clicked(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    lv_async_call(show_body_position_front_async, nullptr);
}

static void flow_cal2_back_clicked(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    lv_async_call(show_cal1_async, nullptr);
}

static void flow_cal3_back_clicked(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    lv_async_call(show_cal2_async, nullptr);
}

static void flow_end_training_clicked(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    rehab_uart_link_send_command("STOP");
}

static void flow_pause_training_clicked(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    rehab_uart_link_send_command("PAUSE");
}

static void flow_continue_training_clicked(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    rehab_uart_link_send_command("RESUME");
}

static void flow_skip_rest_clicked(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    rehab_uart_link_send_command("SKIP_REST");
}

static void flow_result_home_clicked(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    rehab_uart_link_send_command("HOME");
}

static void show_wear_ui()
{
    flow_prepare_screen();
    lv_obj_t *screen = lv_scr_act();
    flow_create_brand_header(screen, 1, 6);

    // V7 wear page:
    // The complete front-view character + A/B/C/D/E callouts are baked into ONE
    // 448x402 bitmap.  This removes the previous coordinate-drift problem where
    // the character was zoomed but IMU modules were positioned separately.
    // E is placed on the upper abdomen, just below the chest, with its label to
    // the side instead of directly underneath.
    make_img(screen, &rehab_wear_diagram, 20, 54);

    // Connection status remains live/dynamic on the right side.
    lv_obj_t *right = make_card(screen, 477, 54, 300, 402, 18);
    make_img(right, &flow_wear_title, 20, 28);
    make_img(right, &flow_wear_sub, 20, 65);

    const lv_img_dsc_t *labels[5] = {
        &flow_left_upper,
        &flow_left_forearm,
        &flow_right_upper,
        &flow_right_forearm,
        &flow_waist
    };
    const char letters[5] = {'A','B','C','D','E'};

    for (int i = 0; i < 5; ++i) {
        lv_obj_t *row = make_card(right, 18, 101 + i * 46, 264, 40, 12);
        lv_obj_set_style_shadow_width(row, 0, 0);

        lv_obj_t *lc = make_circle(row, 10, 9, 22, lv_color_hex(0xE1F3E8));
        char b[2] = {letters[i], 0};
        make_text(lc, b, &lv_font_montserrat_14, C_GREEN, 6, 2);

        make_img(row, labels[i], 43, 10);
        // Runtime UI evidence: E is not just another IMU; it is the waist/abdomen
        // body-reference node used to distinguish whole-body turning from a limb
        // moving relative to the body. Make that role visible on the wear page.
        if (i == 4) {
            make_img(row, &rehab_video_ref_tag, 82, 9);
            lv_obj_set_style_border_width(row, 1, 0);
            lv_obj_set_style_border_color(row, lv_color_hex(0x9FD8B3), 0);
            lv_obj_set_style_bg_color(row, lv_color_hex(0xF3FAF5), 0);
        }
        g_imu_status_img[i] = make_img(
            row,
            g_imu_connected[i] ? &flow_connected : &flow_disconnected,
            187,
            10
        );
        g_imu_status_dot[i] = make_circle(
            row,
            241,
            13,
            14,
            g_imu_connected[i] ? C_GREEN : lv_color_hex(0xD05C4E)
        );
        g_imu_status_symbol[i] = make_text(
            g_imu_status_dot[i],
            g_imu_connected[i] ? LV_SYMBOL_OK : "!",
            &lv_font_montserrat_10,
            lv_color_white(),
            3,
            1
        );
    }

    lv_obj_t *start_btn = flow_make_green_button(
        right,
        18,
        341,
        264,
        42,
        &flow_start_positioning
    );
    lv_obj_add_event_cb(start_btn, flow_wear_start_clicked, LV_EVENT_CLICKED, nullptr);
}

static void flow_make_instruction_person(lv_obj_t *left, bool no_forearm, bool with_dumbbell)
{
    (void)no_forearm;
    (void)with_dumbbell;
    // Use the complete full-body sprite extracted from the approved positioning page.
    // Keep the whole figure, including both shoes, inside the 432x352 visual panel.
    flow_make_person(left, &rehab_person_side, 176, 34, 300);
}

static void show_calibration_1_ui()
{
    flow_prepare_screen();
    lv_obj_t *screen = lv_scr_act();
    flow_create_brand_header(screen, 1, 3);
    lv_obj_t *left = flow_make_left_panel(screen);
    lv_obj_t *right = flow_make_right_panel(screen);

    flow_make_instruction_person(left, false, false);

    make_img(right, &flow_calibration, 22, 26);
    make_text(right, "1 / 3", &lv_font_montserrat_16, C_GREEN, 218, 27);
    make_img(right, &flow_cal_start_title, 22, 70);
    make_img(right, &flow_cal_start_sub, 22, 120);

    // Keep calibration 1/3 deliberately simple on the 7-inch screen:
    // the previous explanatory "系统将记录..." card is removed.
    lv_obj_t *state = make_card(right, 22, 220, 262, 48, 14);
    lv_obj_set_style_shadow_width(state, 0, 0);
    make_circle(state, 18, 17, 12, C_GREEN);
    lv_obj_t *st = make_img(state, &flow_preparing, 0, 0);
    lv_obj_align(st, LV_ALIGN_CENTER, 16, -6);
    g_stage_progress_bar = lv_bar_create(state);
    lv_obj_set_pos(g_stage_progress_bar, 34, 35);
    lv_obj_set_size(g_stage_progress_bar, 194, 5);
    lv_bar_set_range(g_stage_progress_bar, 0, 100);
    lv_bar_set_value(g_stage_progress_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(g_stage_progress_bar, lv_color_hex(0xE5ECE7), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g_stage_progress_bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(g_stage_progress_bar, C_GREEN, LV_PART_INDICATOR);

}

static void show_calibration_2_ui()
{
    flow_prepare_screen();
    lv_obj_t *screen = lv_scr_act();
    flow_create_brand_header(screen, 2, 3);

    // This page alone uses a flat motion panel whose background exactly matches
    // the TF animation frames. Other approved pages keep their original panel.
    lv_obj_t *left = flow_make_motion_panel(screen);
    lv_obj_t *right = flow_make_right_panel(screen);

    // 112 x 304 frame, centered in the 432 x 352 panel.
    // Frames are already position-locked: do not recenter or rescale per frame.
    const bool anim_ok = rehab_motion_animation_start_forward(left, 160, 24);

    if (!anim_ok) {
        // Safe fallback when the card/file is missing. No fake extra arm/dumbbell.
        flow_make_instruction_person(left, false, false);

        lv_obj_t *arc = lv_arc_create(left);
        lv_obj_set_pos(arc, 205, 132);
        lv_obj_set_size(arc, 110, 110);
        lv_arc_set_bg_angles(arc, 20, 120);
        lv_arc_set_angles(arc, 20, 120);
        lv_obj_remove_style(arc, nullptr, LV_PART_KNOB);
        lv_obj_set_style_arc_width(arc, 3, LV_PART_MAIN);
        lv_obj_set_style_arc_color(arc, lv_color_hex(0xC9E9D5), LV_PART_MAIN);
        lv_obj_set_style_arc_width(arc, 3, LV_PART_INDICATOR);
        lv_obj_set_style_arc_color(arc, C_GREEN, LV_PART_INDICATOR);
        Serial.println("CAL_2: TF animation unavailable, static fallback shown");
    }

    make_img(right, &flow_calibration, 22, 26);
    make_text(right, "2 / 3", &lv_font_montserrat_16, C_GREEN, 218, 27);
    make_img(right, &flow_cal_flex_title, 22, 70);
    make_img(right, &flow_upper_stable, 22, 120);
    make_img(right, &flow_slow_curl, 22, 153);

    lv_obj_t *info = make_card(right, 22, 210, 262, 82, 16);
    lv_obj_set_style_shadow_width(info, 0, 0);
    lv_obj_set_style_bg_color(info, lv_color_hex(0xF4F9F5), 0);
    make_circle(info, 18, 22, 38, lv_color_hex(0xDFF3E7));
    make_img(info, &flow_recording_track, 70, 21);
    g_stage_progress_bar = lv_bar_create(info);
    lv_obj_set_pos(g_stage_progress_bar, 70, 52);
    lv_obj_set_size(g_stage_progress_bar, 170, 8);
    lv_bar_set_range(g_stage_progress_bar, 0, 100);
    lv_bar_set_value(g_stage_progress_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(g_stage_progress_bar, lv_color_hex(0xE5ECE7), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g_stage_progress_bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(g_stage_progress_bar, C_GREEN, LV_PART_INDICATOR);

    lv_obj_t *state = make_card(right, 22, 305, 262, 34, 12);
    lv_obj_set_style_shadow_width(state, 0, 0);
    make_circle(state, 19, 11, 12, C_GREEN);
    lv_obj_t *st = make_img(state, &flow_calibrating, 0, 0);
    lv_obj_align(st, LV_ALIGN_CENTER, 15, 0);
}

static void show_calibration_3_ui()
{
    flow_prepare_screen();
    lv_obj_t *screen = lv_scr_act();
    flow_create_brand_header(screen, 3, 3);
    // Use the same flat motion panel/background as calibration 2/3,
    // because calibration 3/3 reuses the exact same TF frames in reverse.
    lv_obj_t *left = flow_make_motion_panel(screen);
    lv_obj_t *right = flow_make_right_panel(screen);

    // 48 -> 1: return from the flexed pose to the natural start pose.
    const bool anim_ok = rehab_motion_animation_start_reverse(left, 160, 24);

    if (!anim_ok) {
        flow_make_instruction_person(left, false, false);
        lv_obj_t *arrow = make_text(left, LV_SYMBOL_DOWN, &lv_font_montserrat_28, C_GREEN, 285, 190);
        lv_obj_set_style_text_opa(arrow, LV_OPA_70, 0);
        Serial.println("CAL_3: TF reverse animation unavailable, static fallback shown");
    }

    make_img(right, &flow_calibration, 22, 26);
    make_text(right, "3 / 3", &lv_font_montserrat_16, C_GREEN, 218, 27);
    make_img(right, &flow_cal_return_title, 22, 70);
    make_img(right, &flow_slow_return, 22, 120);

    lv_obj_t *info = make_card(right, 22, 178, 262, 94, 16);
    lv_obj_set_style_shadow_width(info, 0, 0);
    lv_obj_set_style_bg_color(info, lv_color_hex(0xF4F9F5), 0);
    make_circle(info, 18, 27, 38, lv_color_hex(0xDFF3E7));
    make_img(info, &flow_hold_auto_v2, 28, 25);

    lv_obj_t *state = make_card(right, 22, 294, 262, 42, 13);
    lv_obj_set_style_shadow_width(state, 0, 0);
    make_circle(state, 18, 15, 12, C_GREEN);
    lv_obj_t *st = make_img(state, &flow_calibrating, 0, 0);
    lv_obj_align(st, LV_ALIGN_CENTER, 15, -6);
    g_stage_progress_bar = lv_bar_create(state);
    lv_obj_set_pos(g_stage_progress_bar, 34, 31);
    lv_obj_set_size(g_stage_progress_bar, 194, 5);
    lv_bar_set_range(g_stage_progress_bar, 0, 100);
    lv_bar_set_value(g_stage_progress_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(g_stage_progress_bar, lv_color_hex(0xE5ECE7), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g_stage_progress_bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(g_stage_progress_bar, C_GREEN, LV_PART_INDICATOR);

}

static void countdown_tick(lv_timer_t *timer)
{
    (void)timer;
    if (!g_countdown_number) return;
    g_countdown_value--;
    if (g_countdown_value > 0) {
        char b[4]; snprintf(b, sizeof(b), "%d", g_countdown_value);
        lv_label_set_text(g_countdown_number, b);
        return;
    }
    flow_stop_countdown();
    rehab_uart_link_send_command("COUNTDOWN_DONE");
    // Keep the overlay until the controller confirms TRAIN (usually <50 ms).
}

static void flow_start_countdown_overlay()
{
    flow_stop_countdown();
    lv_obj_t *screen = lv_scr_act();
    g_countdown_overlay = lv_obj_create(screen);
    lv_obj_set_pos(g_countdown_overlay, 0, 0);
    lv_obj_set_size(g_countdown_overlay, 800, 480);
    lv_obj_clear_flag(g_countdown_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(g_countdown_overlay, 0, 0);
    lv_obj_set_style_bg_color(g_countdown_overlay, lv_color_hex(0xF7FBF8), 0);
    lv_obj_set_style_bg_opa(g_countdown_overlay, LV_OPA_80, 0);
    lv_obj_set_style_border_width(g_countdown_overlay, 0, 0);

    lv_obj_t *ok = make_circle(g_countdown_overlay, 350, 115, 100, C_GREEN);
    make_text(ok, LV_SYMBOL_OK, &lv_font_montserrat_48, lv_color_white(), 24, 18);
    lv_obj_t *done = make_img(g_countdown_overlay, &flow_cal_done, 0, 0);
    lv_obj_align(done, LV_ALIGN_CENTER, 0, -20);
    g_countdown_number = make_text(g_countdown_overlay, "3", &lv_font_montserrat_48, C_GREEN, 0, 0);
    lv_obj_align(g_countdown_number, LV_ALIGN_CENTER, 0, 58);
    g_countdown_value = 3;
    g_countdown_timer = lv_timer_create(countdown_tick, 700, nullptr);
}

static lv_obj_t *flow_make_metric_row(lv_obj_t *parent, int y, const lv_img_dsc_t *label, lv_obj_t **value_out)
{
    lv_obj_t *row = make_card(parent, 20, y, 266, 54, 13);
    lv_obj_set_style_shadow_width(row, 0, 0);
    make_img(row, label, 18, 17);
    lv_obj_t *v = make_text(row, "0", &lv_font_montserrat_24, C_GREEN, 156, 11);
    lv_obj_set_width(v, 94);
    lv_label_set_long_mode(v, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_align(v, LV_TEXT_ALIGN_RIGHT, 0);
    if (value_out) *value_out = v;
    return row;
}

static void show_training_ui()
{
    flow_prepare_screen();
    lv_obj_t *screen = lv_scr_act();
    flow_create_brand_header(screen, g_current_group, g_target_groups);
    // Formal training reuses the same approved TF animation.
    // 1 -> 48, hold, 47 -> 1, hold, repeat.
    lv_obj_t *left = flow_make_motion_panel(screen);
    lv_obj_t *right = flow_make_right_panel(screen);

    const bool anim_ok = rehab_motion_animation_start_training(left, 160, 24);

    if (!anim_ok) {
        flow_make_instruction_person(left, false, false);

        lv_obj_t *guide = lv_arc_create(left);
        lv_obj_set_pos(guide, 205, 132);
        lv_obj_set_size(guide, 110, 110);
        lv_arc_set_bg_angles(guide, 20, 120);
        lv_arc_set_angles(guide, 20, 120);
        lv_obj_remove_style(guide, nullptr, LV_PART_KNOB);
        lv_obj_set_style_arc_width(guide, 3, LV_PART_MAIN);
        lv_obj_set_style_arc_color(guide, lv_color_hex(0xC9E9D5), LV_PART_MAIN);
        lv_obj_set_style_arc_width(guide, 3, LV_PART_INDICATOR);
        lv_obj_set_style_arc_color(guide, C_GREEN, LV_PART_INDICATOR);
        Serial.println("TRAINING: TF ping-pong animation unavailable, static fallback shown");
    }

    // Left/right raw counts remain available through the update API, but are not
    // drawn as floating badges over the person because that layout was cluttered.
    g_train_left_value = nullptr;
    g_train_right_value = nullptr;

    make_img(right, &flow_exercise, 20, 17);
    make_img(right, &flow_continue_training, 20, 50);

    char gb[24]; snprintf(gb, sizeof(gb), "%d / %d", g_current_group, g_target_groups);
    g_train_group_value = make_text(right, gb, &lv_font_montserrat_14, C_MUTED, 218, 20);

    flow_make_metric_row(right, 96, &flow_current_reps, &g_train_reps_value);
    flow_make_metric_row(right, 154, &flow_current_angle, &g_train_angle_value);

    lv_obj_t *target_row = make_card(right, 20, 212, 266, 48, 13);
    lv_obj_set_style_shadow_width(target_row, 0, 0);
    make_img(target_row, &flow_target_angle, 18, 15);
    char tb[12]; snprintf(tb, sizeof(tb), "%d°", g_target_angle);
    g_train_target_value = make_text(target_row, tb, &lv_font_montserrat_24, C_GREEN, 158, 10);
    lv_obj_set_width(g_train_target_value, 92);
    lv_label_set_long_mode(g_train_target_value, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_align(g_train_target_value, LV_TEXT_ALIGN_RIGHT, 0);

    lv_obj_t *feedback = make_card(right, 20, 268, 266, 44, 13);
    lv_obj_set_style_shadow_width(feedback, 0, 0);
    lv_obj_set_style_bg_color(feedback, lv_color_hex(0xEFF8F2), 0);
    make_img(feedback, &flow_feedback, 18, 13);
    // Chinese feedback is rendered as tiny compiled bitmap assets.  The screen
    // project intentionally does not carry a full CJK LVGL font, so using images
    // keeps flash/RAM cost small while avoiding English-only feedback labels.
    g_train_feedback_img = make_img(feedback, &rehab_feedback_ok, 0, 0);
    lv_obj_align(g_train_feedback_img, LV_ALIGN_RIGHT_MID, -18, 0);

    make_img(right, &flow_training_progress, 20, 327);
    g_train_progress_bar = lv_bar_create(right);
    lv_obj_set_pos(g_train_progress_bar, 105, 331);
    lv_obj_set_size(g_train_progress_bar, 126, 8);
    lv_bar_set_range(g_train_progress_bar, 0, 100);
    lv_bar_set_value(g_train_progress_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(g_train_progress_bar, lv_color_hex(0xE5ECE7), LV_PART_MAIN);
    lv_obj_set_style_bg_color(g_train_progress_bar, C_GREEN, LV_PART_INDICATOR);
    g_train_progress_value = make_text(right, "0%", &lv_font_montserrat_12, C_MUTED, 242, 323);

    // Bottom controls: pause sits immediately to the left of end training.
    lv_obj_t *pause_btn = flow_make_green_button(screen, 24, 419, 170, 45, &flow_pause_training);
    lv_obj_add_event_cb(pause_btn, flow_pause_training_clicked, LV_EVENT_CLICKED, nullptr);

    lv_obj_t *end = flow_make_outline_button(screen, 204, 419, 170, 45, &flow_end_training);
    lv_obj_add_event_cb(end, flow_end_training_clicked, LV_EVENT_CLICKED, nullptr);

    // Restore the last real values after resuming from pause.
    int count = (g_last_left_count < g_last_right_count) ? g_last_left_count : g_last_right_count;
    char rb[24]; snprintf(rb, sizeof(rb), "%d / %d", count, g_target_reps);
    lv_label_set_text(g_train_reps_value, rb);
    float angle = (g_last_left_angle_deg < g_last_right_angle_deg) ? g_last_left_angle_deg : g_last_right_angle_deg;
    snprintf(rb, sizeof(rb), "%.0f°", angle);
    lv_label_set_text(g_train_angle_value, rb);
    if (g_train_progress_bar) lv_bar_set_value(g_train_progress_bar, g_last_progress_percent, LV_ANIM_OFF);
    snprintf(rb, sizeof(rb), "%d%%", g_last_progress_percent);
    if (g_train_progress_value) lv_label_set_text(g_train_progress_value, rb);
}


static void show_paused_ui()
{
    flow_prepare_screen();
    lv_obj_t *screen = lv_scr_act();
    flow_create_brand_header(screen, g_current_group, g_target_groups);

    lv_obj_t *content = make_card(screen, 24, 52, 752, 352, 18);
    lv_obj_set_style_bg_color(content, lv_color_hex(0xFBFDFB), 0);
    lv_obj_set_style_bg_grad_color(content, lv_color_hex(0xEEF7F1), 0);
    lv_obj_set_style_bg_grad_dir(content, LV_GRAD_DIR_HOR, 0);

    // Large, calm pause symbol.
    lv_obj_t *pause_circle = make_circle(content, 86, 86, 132, lv_color_hex(0xE3F4E9));
    lv_obj_t *bar1 = lv_obj_create(pause_circle);
    lv_obj_set_pos(bar1, 38, 34);
    lv_obj_set_size(bar1, 18, 64);
    lv_obj_clear_flag(bar1, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(bar1, 0, 0);
    lv_obj_set_style_radius(bar1, 7, 0);
    lv_obj_set_style_border_width(bar1, 0, 0);
    lv_obj_set_style_bg_color(bar1, C_GREEN, 0);

    lv_obj_t *bar2 = lv_obj_create(pause_circle);
    lv_obj_set_pos(bar2, 76, 34);
    lv_obj_set_size(bar2, 18, 64);
    lv_obj_clear_flag(bar2, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(bar2, 0, 0);
    lv_obj_set_style_radius(bar2, 7, 0);
    lv_obj_set_style_border_width(bar2, 0, 0);
    lv_obj_set_style_bg_color(bar2, C_GREEN, 0);

    make_img(content, &flow_paused_title, 270, 86);
    make_img(content, &flow_paused_sub, 270, 142);

    // Preserve and show progress while paused.
    make_img(content, &flow_training_progress, 270, 197);
    lv_obj_t *bar = lv_bar_create(content);
    lv_obj_set_pos(bar, 270, 228);
    lv_obj_set_size(bar, 360, 10);
    lv_bar_set_range(bar, 0, 100);
    lv_bar_set_value(bar, g_last_progress_percent, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0xE5ECE7), LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar, C_GREEN, LV_PART_INDICATOR);

    char pb[12];
    snprintf(pb, sizeof(pb), "%d%%", g_last_progress_percent);
    make_text(content, pb, &lv_font_montserrat_16, C_MUTED, 642, 220);

    // No "返回" button on this page: only continue or end the current training.
    lv_obj_t *continue_btn = flow_make_green_button(screen, 204, 419, 170, 45, &flow_continue_btn);
    lv_obj_add_event_cb(continue_btn, flow_continue_training_clicked, LV_EVENT_CLICKED, nullptr);

    lv_obj_t *end = flow_make_outline_button(screen, 384, 419, 170, 45, &flow_end_training);
    lv_obj_add_event_cb(end, flow_end_training_clicked, LV_EVENT_CLICKED, nullptr);
}

static void rest_timer_tick(lv_timer_t *timer)
{
    (void)timer;
    if (g_rest_remaining > 0) g_rest_remaining--;
    if (g_rest_seconds_value) {
        char b[8]; snprintf(b, sizeof(b), "%d", g_rest_remaining);
        lv_label_set_text(g_rest_seconds_value, b);
    }
    if (g_rest_arc) lv_arc_set_value(g_rest_arc, g_rest_remaining);
    if (g_rest_remaining <= 0) {
        flow_stop_rest_timer();
        if (g_current_group < g_target_groups) g_current_group++;
        lv_async_call(show_training_async, nullptr);
    }
}

static void show_rest_ui(int seconds)
{
    flow_prepare_screen();
    g_rest_remaining = seconds;
    lv_obj_t *screen = lv_scr_act();
    flow_create_brand_header(screen, g_current_group, g_target_groups);
    lv_obj_t *left = flow_make_left_panel(screen);
    lv_obj_t *right = flow_make_right_panel(screen);

    flow_make_instruction_person(left, false, false);

    make_text(right, "#", &lv_font_montserrat_20, C_GREEN, 24, 18);
    char gb[8]; snprintf(gb, sizeof(gb), "%d", g_current_group);
    g_rest_group_value = make_text(right, gb, &lv_font_montserrat_20, C_GREEN, 48, 17);
    make_img(right, &flow_group_done, 72, 19);

    lv_obj_t *arc = lv_arc_create(right);
    g_rest_arc = arc;
    lv_obj_set_pos(arc, 60, 60);
    lv_obj_set_size(arc, 188, 188);
    lv_arc_set_range(arc, 0, seconds > 0 ? seconds : 30);
    lv_arc_set_value(arc, seconds);
    lv_obj_remove_style(arc, nullptr, LV_PART_KNOB);
    lv_obj_set_style_arc_width(arc, 14, LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc, lv_color_hex(0xDCEFE3), LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, 14, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc, C_GREEN, LV_PART_INDICATOR);

    lv_obj_t *rest_title = make_img(right, &flow_rest, 0, 0);
    lv_obj_align_to(rest_title, arc, LV_ALIGN_CENTER, 0, -38);
    char sb[8]; snprintf(sb, sizeof(sb), "%d", seconds);
    g_rest_seconds_value = make_text(right, sb, &lv_font_montserrat_48, C_GREEN, 0, 0);
    lv_obj_align_to(g_rest_seconds_value, arc, LV_ALIGN_CENTER, 0, 5);
    lv_obj_t *sec = make_img(right, &flow_seconds, 0, 0);
    lv_obj_align_to(sec, arc, LV_ALIGN_CENTER, 0, 55);

    lv_obj_t *relax = make_img(right, &flow_relax, 0, 0);
    lv_obj_align(relax, LV_ALIGN_BOTTOM_MID, 0, -56);

    lv_obj_t *skip = flow_make_outline_button(screen, 470, 419, 306, 45, &flow_skip_rest);
    lv_obj_add_event_cb(skip, flow_skip_rest_clicked, LV_EVENT_CLICKED, nullptr);

    // Rest countdown is controller-owned. No local timer: this prevents visual time
    // from drifting away from the actual training state.
}

static lv_obj_t *flow_make_result_metric(lv_obj_t *parent, int x, int y, int w,
                                         const lv_img_dsc_t *label, const char *value,
                                         const lv_font_t *value_font, lv_obj_t **value_out)
{
    lv_obj_t *card = make_card(parent, x, y, w, 112, 14);
    lv_obj_set_style_shadow_width(card, 5, 0);
    lv_obj_set_style_shadow_opa(card, LV_OPA_10, 0);
    lv_obj_t *lab = make_img(card, label, 0, 0);
    lv_obj_align(lab, LV_ALIGN_TOP_MID, 0, 14);
    lv_obj_t *val = make_text(card, value, value_font, C_GREEN, 0, 0);
    lv_obj_align(val, LV_ALIGN_CENTER, 0, 13);
    if (value_out) *value_out = val;
    return card;
}

static void show_result_ui()
{
    flow_prepare_screen();
    lv_obj_t *screen = lv_scr_act();
    flow_create_brand_header(screen, g_target_groups, g_target_groups);

    // Result page intentionally has no person illustration.
    // Let the summary and metrics use the full 752px content width.
    lv_obj_t *content = make_card(screen, 24, 52, 752, 352, 18);
    lv_obj_set_style_bg_color(content, lv_color_hex(0xFBFDFB), 0);
    lv_obj_set_style_bg_grad_color(content, lv_color_hex(0xF1F8F3), 0);
    lv_obj_set_style_bg_grad_dir(content, LV_GRAD_DIR_HOR, 0);

    lv_obj_t *ok = make_circle(content, 26, 28, 64, C_GREEN);
    make_text(ok, LV_SYMBOL_OK, &lv_font_montserrat_34, lv_color_white(), 15, 11);
    make_img(content, &flow_training_done, 116, 33);
    make_img(content, &flow_keep_it, 116, 78);

    const int metric_y = 132;
    const int metric_w = 130;
    const int gap = 10;
    char rv0[24], rv1[16], rv2[16], rv3[16];
    const int targetTotal = max(1, g_target_groups * g_target_reps);
    snprintf(rv0, sizeof(rv0), "%d/%d", g_last_left_count, targetTotal);
    snprintf(rv1, sizeof(rv1), "%d%%", g_last_progress_percent);
    snprintf(rv2, sizeof(rv2), "%.0f°", g_last_left_angle_deg);
    snprintf(rv3, sizeof(rv3), "00:00");
    flow_make_result_metric(content, 22, metric_y, metric_w, &flow_total_reps, rv0, &lv_font_montserrat_22, &g_result_total_value);
    flow_make_result_metric(content, 22 + (metric_w + gap), metric_y, metric_w, &flow_completion, rv1, &lv_font_montserrat_24, &g_result_completion_value);
    flow_make_result_metric(content, 22 + 2 * (metric_w + gap), metric_y, metric_w, &flow_rom, rv2, &lv_font_montserrat_18, &g_result_rom_value);
    flow_make_result_metric(content, 22 + 3 * (metric_w + gap), metric_y, metric_w, &flow_duration, rv3, &lv_font_montserrat_20, &g_result_duration_value);

    lv_obj_t *data_card = make_card(content, 22 + 4 * (metric_w + gap), metric_y, metric_w, 112, 14);
    lv_obj_set_style_shadow_width(data_card, 5, 0);
    lv_obj_set_style_shadow_opa(data_card, LV_OPA_10, 0);
    lv_obj_t *data_lab = make_img(data_card, &flow_data_status, 0, 0);
    lv_obj_align(data_lab, LV_ALIGN_TOP_MID, 0, 14);
    lv_obj_t *saved = make_img(data_card, &flow_saved, 0, 0);
    lv_obj_align(saved, LV_ALIGN_CENTER, 0, 14);

    lv_obj_t *summary = make_card(content, 22, 267, 690, 58, 14);
    lv_obj_set_style_shadow_width(summary, 0, 0);
    lv_obj_set_style_bg_color(summary, lv_color_hex(0xEAF6EE), 0);
    make_img(summary, &flow_overall, 30, 18);
    g_result_summary_value = make_text(summary, "PASS --", &lv_font_montserrat_16, C_GREEN, 190, 15);

    // Training complete is one of the two pages that keeps a bottom return action.
    lv_obj_t *home = flow_make_green_button(screen, 470, 419, 306, 45, &flow_back_home);
    lv_obj_add_event_cb(home, flow_result_home_clicked, LV_EVENT_CLICKED, nullptr);
}

extern "C" void rehab_ui_show_home()
{
    rehab_ui_router_leave();
    lvgl_port_lock(-1);
    show_home_ui();
    lvgl_port_unlock();
}

extern "C" void rehab_ui_start_training_flow()
{
    Serial.println("APP/TRAINING_TASK -> WEAR_CHECK");
    lvgl_port_lock(-1);
    show_wear_ui();
    lvgl_port_unlock();
}

extern "C" void rehab_ui_set_imu_connections(bool a, bool b, bool c, bool d, bool e)
{
    bool v[5] = {a,b,c,d,e};
    lvgl_port_lock(-1);
    for (int i = 0; i < 5; ++i) {
        g_imu_connected[i] = v[i];
        if (g_imu_status_img[i]) {
            lv_img_set_src(g_imu_status_img[i], v[i] ? &flow_connected : &flow_disconnected);
        }
        if (g_imu_status_dot[i]) {
            lv_obj_set_style_bg_color(g_imu_status_dot[i], v[i] ? C_GREEN : lv_color_hex(0xD05C4E), 0);
        }
        if (g_imu_status_symbol[i]) {
            lv_label_set_text(g_imu_status_symbol[i], v[i] ? LV_SYMBOL_OK : "!");
        }
    }
    lvgl_port_unlock();
}

extern "C" void rehab_ui_wear_ready()
{
    Serial.println("WEAR_READY -> BODY_POSITION_1");
    lvgl_port_lock(-1);
    show_body_positioning_ui();
    lvgl_port_unlock();
}

extern "C" void rehab_ui_calibration_start_pose_ready()
{
    Serial.println("CAL_START_POSE_OK -> CAL_2");
    lvgl_port_lock(-1);
    show_calibration_2_ui();
    lvgl_port_unlock();
}

extern "C" void rehab_ui_calibration_flex_success()
{
    Serial.println("CAL_FLEX_OK -> CAL_3");
    lvgl_port_lock(-1);
    show_calibration_3_ui();
    lvgl_port_unlock();
}

extern "C" void rehab_ui_calibration_return_success()
{
    Serial.println("CAL_RETURN_OK -> COUNTDOWN -> TRAINING");
    lvgl_port_lock(-1);
    flow_start_countdown_overlay();
    lvgl_port_unlock();
}

extern "C" void rehab_ui_set_plan(int target_groups, int target_reps, float target_angle_deg)
{
    if (target_groups > 0) g_target_groups = target_groups;
    if (target_reps > 0) g_target_reps = target_reps;
    if (isfinite(target_angle_deg) && target_angle_deg > 0.0f) g_target_angle = (int)lroundf(target_angle_deg);
}

extern "C" void rehab_ui_training_update_remote(int group_index, int target_groups,
                                                   int count, int target_reps,
                                                   float angle_deg, float target_angle_deg,
                                                   int progress_percent)
{
    g_current_group = group_index;
    g_target_groups = target_groups;
    g_target_reps = target_reps;
    g_target_angle = (int)lroundf(target_angle_deg);
    g_last_left_count = count;
    g_last_right_count = count;
    g_last_left_angle_deg = angle_deg;
    g_last_right_angle_deg = angle_deg;
    if (progress_percent < 0) progress_percent = 0;
    if (progress_percent > 100) progress_percent = 100;
    g_last_progress_percent = progress_percent;

    if (!g_train_reps_value) return;
    lvgl_port_lock(-1);
    char b[24];
    snprintf(b, sizeof(b), "%d / %d", count, target_reps);
    lv_label_set_text(g_train_reps_value, b);
    snprintf(b, sizeof(b), "%.1f°", angle_deg);
    lv_label_set_text(g_train_angle_value, b);
    snprintf(b, sizeof(b), "%d / %d", group_index, target_groups);
    if (g_train_group_value) lv_label_set_text(g_train_group_value, b);
    snprintf(b, sizeof(b), "%.0f°", target_angle_deg);
    if (g_train_target_value) lv_label_set_text(g_train_target_value, b);

    static int lastProgressDrawn = -1;
    if (progress_percent != lastProgressDrawn) {
        lastProgressDrawn = progress_percent;
        if (g_train_progress_bar) lv_bar_set_value(g_train_progress_bar, progress_percent, LV_ANIM_ON);
        snprintf(b, sizeof(b), "%d%%", progress_percent);
        if (g_train_progress_value) lv_label_set_text(g_train_progress_value, b);
    }
    lvgl_port_unlock();
}


static void draw_stage_progress_value(int state, int progress_percent)
{
    if (progress_percent < 0) progress_percent = 0;
    if (progress_percent > 100) progress_percent = 100;

    if (state == 2) {
        // Four dots represent 25/50/75/100% of the real stable-pose hold.
        for (int i = 0; i < 4; ++i) {
            if (!g_body_pose_dots[i]) continue;
            const int threshold = (i + 1) * 25;
            const lv_color_t c = (progress_percent >= threshold) ? C_GREEN : lv_color_hex(0xCDE7D8);
            lv_obj_set_style_bg_color(g_body_pose_dots[i], c, 0);

            const int lower = i * 25;
            const bool active = progress_percent >= lower && progress_percent < threshold;
            lv_obj_set_style_bg_opa(g_body_pose_dots[i], active ? LV_OPA_80 : LV_OPA_COVER, 0);
        }
    } else if (state >= 3 && state <= 6) {
        if (g_stage_progress_bar) {
            // Interpolation is handled by our 40-Hz timer; do not restart an
            // LVGL animation on every 20-Hz UART sample.
            lv_bar_set_value(g_stage_progress_bar, progress_percent, LV_ANIM_OFF);
        }
    }
}

static void stage_progress_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    if (g_stage_progress_display == g_stage_progress_target) return;

    const int diff = g_stage_progress_target - g_stage_progress_display;
    const int mag = abs(diff);
    const int step = (mag > 6) ? 6 : mag;
    g_stage_progress_display += (diff > 0) ? step : -step;
    draw_stage_progress_value(g_stage_progress_state, g_stage_progress_display);
}

extern "C" void rehab_ui_detection_progress(int state, int progress_percent)
{
    if (progress_percent < 0) progress_percent = 0;
    if (progress_percent > 100) progress_percent = 100;

    lvgl_port_lock(-1);

    if (!g_stage_progress_timer) {
        g_stage_progress_timer = lv_timer_create(stage_progress_timer_cb, 25, nullptr);
    }

    if (state != g_stage_progress_state) {
        g_stage_progress_state = state;
        g_stage_progress_target = 0;
        g_stage_progress_display = 0;
        draw_stage_progress_value(state, 0);
    }

    g_stage_progress_target = progress_percent;

    // A completed detector stage must visibly show 100% before the UART state
    // machine is allowed to turn the page.
    if (progress_percent >= 100) {
        g_stage_progress_display = 100;
        draw_stage_progress_value(state, 100);
    }

    lvgl_port_unlock();
}

extern "C" void rehab_ui_training_feedback(uint16_t feedback_mask)
{
    if (!g_train_feedback_img) return;
    const lv_img_dsc_t *src = &rehab_feedback_ok;
    const lv_img_dsc_t *big = nullptr;
    bool successStyle = false;
    uint32_t holdMs = 1600;

    // 0x01 ROM; 0x02 upper-arm compensation; 0x04 plane deviation;
    // 0x08 torso compensation; 0x10 retry; 0x20 completed GOOD rep;
    // 0x40 power-loss training restored after fresh calibration.
    if (feedback_mask & 0x40) {
        src = &rehab_feedback_ok;
        big = &rehab_video_big_resume;
        successStyle = true;
        holdMs = 2300;
    } else if (feedback_mask & 0x20) {
        src = &rehab_feedback_ok;
        big = &rehab_video_big_ok;
        successStyle = true;
        holdMs = 1600;
    } else {
        if (feedback_mask & 0x10) { src = &rehab_feedback_retry; big = &rehab_feedback_big_retry; }
        if (feedback_mask & 0x01) { src = &rehab_feedback_rom_low; big = &rehab_feedback_big_rom_low; }
        if (feedback_mask & 0x02) { src = &rehab_feedback_upper; big = &rehab_feedback_big_upper; }
        if (feedback_mask & 0x04) { src = &rehab_feedback_plane; big = &rehab_feedback_big_plane; }
        if (feedback_mask & 0x08) { src = &rehab_feedback_torso; big = &rehab_feedback_big_torso; }

        // Multiple simultaneous issues use one stable summary instead of flickering.
        const uint16_t issueBits = feedback_mask & 0x0F;
        if (issueBits && (issueBits & (issueBits - 1))) {
            src = &rehab_feedback_check;
            big = &rehab_feedback_big_check;
        }
    }

    const uint16_t feedbackKey = feedback_mask & 0x7F;

    lvgl_port_lock(-1);
    lv_img_set_src(g_train_feedback_img, src);
    lv_obj_align(g_train_feedback_img, LV_ALIGN_RIGHT_MID, -18, 0);

    // Completed correct actions are now positive evidence on camera, not merely
    // "nothing happened". Errors and success both remain visible >=1 s.
    if (feedbackKey != 0 && feedbackKey != g_last_feedback_mask && big) {
        show_large_feedback_overlay(big, successStyle, holdMs);
    }
    g_last_feedback_mask = feedbackKey;
    lvgl_port_unlock();
}

extern "C" void rehab_ui_rest_update(int group_index, int rest_seconds)
{
    g_current_group = group_index;
    if (rest_seconds < 0) rest_seconds = 0;
    g_rest_remaining = rest_seconds;
    if (!g_rest_seconds_value) return;
    lvgl_port_lock(-1);
    char b[16];
    snprintf(b, sizeof(b), "%d", rest_seconds);
    lv_label_set_text(g_rest_seconds_value, b);
    if (g_rest_arc) lv_arc_set_value(g_rest_arc, rest_seconds);
    snprintf(b, sizeof(b), "%d", group_index);
    if (g_rest_group_value) lv_label_set_text(g_rest_group_value, b);
    lvgl_port_unlock();
}

extern "C" void rehab_ui_result_update(int completed, int target_total,
                                         float pass_rate_pct, float rom_max_deg,
                                         unsigned long duration_sec)
{
    if (target_total < 1) target_total = 1;
    g_last_left_count = completed;
    g_last_progress_percent = constrain((completed * 100) / target_total, 0, 100);
    g_last_left_angle_deg = rom_max_deg;
    if (!g_result_total_value) return;

    lvgl_port_lock(-1);
    char b[32];
    snprintf(b, sizeof(b), "%d/%d", completed, target_total);
    lv_label_set_text(g_result_total_value, b);
    snprintf(b, sizeof(b), "%d%%", g_last_progress_percent);
    if (g_result_completion_value) lv_label_set_text(g_result_completion_value, b);
    snprintf(b, sizeof(b), "%.0f°", rom_max_deg);
    if (g_result_rom_value) lv_label_set_text(g_result_rom_value, b);
    const unsigned long mm = duration_sec / 60UL;
    const unsigned long ss = duration_sec % 60UL;
    snprintf(b, sizeof(b), "%02lu:%02lu", mm, ss);
    if (g_result_duration_value) lv_label_set_text(g_result_duration_value, b);
    snprintf(b, sizeof(b), "PASS %.0f%%", pass_rate_pct);
    if (g_result_summary_value) lv_label_set_text(g_result_summary_value, b);
    lvgl_port_unlock();
}

extern "C" void rehab_ui_training_update(int group_index, int target_groups,
                                           int left_count, int right_count, int target_reps,
                                           float left_angle_deg, float right_angle_deg,
                                           int progress_percent)
{
    g_current_group = group_index;
    g_target_groups = target_groups;
    g_target_reps = target_reps;
    g_last_left_count = left_count;
    g_last_right_count = right_count;
    g_last_left_angle_deg = left_angle_deg;
    g_last_right_angle_deg = right_angle_deg;
    if (progress_percent < 0) progress_percent = 0;
    if (progress_percent > 100) progress_percent = 100;
    g_last_progress_percent = progress_percent;
    if (!g_train_reps_value) return;

    lvgl_port_lock(-1);
    int count = (left_count < right_count) ? left_count : right_count;
    char b[24];
    snprintf(b, sizeof(b), "%d / %d", count, target_reps);
    lv_label_set_text(g_train_reps_value, b);
    float angle = (left_angle_deg < right_angle_deg) ? left_angle_deg : right_angle_deg;
    snprintf(b, sizeof(b), "%.0f°", angle);
    lv_label_set_text(g_train_angle_value, b);
    snprintf(b, sizeof(b), "%d", left_count);
    if (g_train_left_value) lv_label_set_text(g_train_left_value, b);
    snprintf(b, sizeof(b), "%d", right_count);
    if (g_train_right_value) lv_label_set_text(g_train_right_value, b);
    snprintf(b, sizeof(b), "%d / %d", group_index, target_groups);
    if (g_train_group_value) lv_label_set_text(g_train_group_value, b);
    if (g_train_progress_bar) lv_bar_set_value(g_train_progress_bar, progress_percent, LV_ANIM_ON);
    snprintf(b, sizeof(b), "%d%%", progress_percent);
    if (g_train_progress_value) lv_label_set_text(g_train_progress_value, b);
    lvgl_port_unlock();
}


extern "C" void rehab_ui_set_training_paused(bool paused)
{
    g_training_paused = paused;
    lvgl_port_lock(-1);
    if (paused) show_paused_ui();
    else show_training_ui();
    lvgl_port_unlock();
}

extern "C" void rehab_ui_group_finished(int group_index, int rest_seconds)
{
    g_current_group = group_index;
    Serial.printf("GROUP_%d_FINISHED -> REST %ds\n", group_index, rest_seconds);
    lvgl_port_lock(-1);
    show_rest_ui(rest_seconds);
    lvgl_port_unlock();
}

extern "C" void rehab_ui_training_finished()
{
    Serial.println("ALL_TRAINING_FINISHED -> RESULT");
    lvgl_port_lock(-1);
    show_result_ui();
    lvgl_port_unlock();
}


static void create_home_ui()
{
    lv_obj_t *screen = lv_scr_act();
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(screen, 0, 0);
    lv_obj_set_style_bg_color(screen, C_BG, 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

    create_header(screen);
    create_main_card(screen);
    create_right_cards(screen);
    create_flow(screen);
    create_nav(screen);
}

static void show_home_ui()
{
    // HOME can be forced remotely from calibration/training/result. Always stop
    // countdown/rest/animation timers first so no stale LVGL timer survives.
    flow_prepare_screen();
    create_home_ui();
}

static void show_home_async(void *user_data)
{
    (void)user_data;
    show_home_ui();
}

static void show_body_positioning_async(void *user_data)
{
    (void)user_data;
    show_body_positioning_ui();
}

static void show_body_position_front_async(void *user_data)
{
    (void)user_data;
    show_body_position_front_ui();
}

static void show_body_positioning_ui()
{
    lv_obj_t *screen = lv_scr_act();
    lv_obj_clean(screen);

    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(screen, 0, 0);
    lv_obj_set_style_bg_color(screen, C_BG, 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

    // V7 approved 800x480 positioning visual: the duplicate five-IMU
    // connection list and the extra explanatory rows are removed.  The right
    // card now contains only the original positioning text and recognition
    // status, with spacing rebalanced.  TF card is not required.
    lv_obj_t *page = lv_img_create(screen);
    lv_img_set_src(page, &body_position_page);
    lv_obj_set_pos(page, 0, 0);

    // Dynamic real recognition progress. The original picture contains decorative
    // dots; these LVGL dots sit directly on top and are driven by the controller's
    // actual stable-hold progress (0-100%), so "姿势识别中" no longer looks frozen.
    const int dotX[4] = {504, 526, 548, 570};
    for (int i = 0; i < 4; ++i) {
        g_body_pose_dots[i] = make_circle(screen, dotX[i], 360, 11, lv_color_hex(0xCDE7D8));
        lv_obj_set_style_border_width(g_body_pose_dots[i], 0, 0);
    }

    // Transparent hit area over the visible "返回" button.
    lv_obj_t *back_btn = lv_btn_create(screen);
    lv_obj_set_pos(back_btn, 24, 419);
    lv_obj_set_size(back_btn, 171, 45);
    lv_obj_clear_flag(back_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(back_btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(back_btn, 0, 0);
    lv_obj_set_style_shadow_width(back_btn, 0, 0);
    lv_obj_set_style_pad_all(back_btn, 0, 0);
    lv_obj_add_event_cb(back_btn, body_back_clicked, LV_EVENT_CLICKED, nullptr);

    /*
      IMPORTANT:
      No manual "Next" button and no timer-based auto jump.

      After we finalize the second positioning screen, the existing body-frame
      algorithm should call that UI only when the real vertical/down-still
      positioning condition succeeds.
    */
}

static void show_body_position_front_ui()
{
    lv_obj_t *screen = lv_scr_act();
    lv_obj_clean(screen);

    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(screen, 0, 0);
    lv_obj_set_style_bg_color(screen, C_BG, 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

    lv_obj_t *page = lv_img_create(screen);
    lv_img_set_src(page, &body_position_front_page);
    lv_obj_set_pos(page, 0, 0);

    // Replace the baked 40%-looking bar with a real detector hold-progress bar.
    g_stage_progress_bar = lv_bar_create(screen);
    lv_obj_set_pos(g_stage_progress_bar, 552, 286);
    lv_obj_set_size(g_stage_progress_bar, 190, 7);
    lv_bar_set_range(g_stage_progress_bar, 0, 100);
    lv_bar_set_value(g_stage_progress_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_radius(g_stage_progress_bar, 4, LV_PART_MAIN);
    lv_obj_set_style_radius(g_stage_progress_bar, 4, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(g_stage_progress_bar, lv_color_hex(0xE2E7E3), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g_stage_progress_bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(g_stage_progress_bar, C_GREEN, LV_PART_INDICATOR);

    // 身体定位 2/2：不提供返回按钮。
    // 当前图片资源中还带着旧的“返回”视觉，因此用页面背景盖住该区域。
    lv_obj_t *cover = lv_obj_create(screen);
    lv_obj_set_pos(cover, 0, 402);
    lv_obj_set_size(cover, 420, 78);
    lv_obj_clear_flag(cover, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(cover, 0, 0);
    lv_obj_set_style_radius(cover, 0, 0);
    lv_obj_set_style_bg_color(cover, C_BG, 0);
    lv_obj_set_style_bg_opa(cover, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(cover, 0, 0);
    lv_obj_set_style_shadow_width(cover, 0, 0);
}

/*
 * Real state-machine hook:
 * call this only when BF_DOWN_STILL has actually passed.
 * It automatically switches from body positioning 1/2 to 2/2.
 */
extern "C" void rehab_ui_body_down_positioning_success()
{
    Serial.println("BODY_POSITION_DOWN_OK");
    Serial.println("UI -> BODY_POSITION_FRONT");

    lvgl_port_lock(-1);
    show_body_position_front_ui();
    lvgl_port_unlock();
}

/*
 * Reserved for the next screen.
 * Call this when BF_FRONT_RAISES reaches BF_READY.
 * The next "动作校准" screen is not connected yet because its visual design
 * has not been finalized.
 */
extern "C" void rehab_ui_body_front_positioning_success()
{
    Serial.println("BODY_POSITION_FRONT_OK");
    Serial.println("BODY_FRAME_READY");
    Serial.println("UI -> CALIBRATION_1_3");

    lvgl_port_lock(-1);
    show_calibration_1_ui();
    lvgl_port_unlock();
}

void setup()
{
    rehab_uart_link_begin();
    delay(120);

    Board *board = new Board();
    board->init();

#if LVGL_PORT_AVOID_TEARING_MODE
    auto lcd = board->getLCD();
    lcd->configFrameBufferNumber(LVGL_PORT_DISP_BUFFER_NUM);
#if ESP_PANEL_DRIVERS_BUS_ENABLE_RGB && CONFIG_IDF_TARGET_ESP32S3
    auto lcd_bus = lcd->getBus();
    if (lcd_bus->getBasicAttributes().type == ESP_PANEL_BUS_TYPE_RGB) {
        static_cast<BusRGB *>(lcd_bus)->configRGB_BounceBufferSize(lcd->getFrameWidth() * 10);
    }
#endif
#endif

    assert(board->begin());
    lvgl_port_init(board->getLCD(), board->getTouch());

    lvgl_port_lock(-1);
    show_home_ui();
    rehab_ui_router_init(lv_scr_act());
    lvgl_port_unlock();

    // Load the animation once while the home page is already visible. Later CAL2/CAL3/
    // TRAIN transitions use PSRAM only, so TF-card latency cannot stall LVGL/touch.
    rehab_motion_animation_preload();

    // UI is now controller-driven; the old serial keyboard page switcher is removed.
}

void loop()
{
    // V13:
    // With ARDUINO_USB_CDC_ON_BOOT=0, Serial is UART0.
    // UART0 is now reserved exclusively for communication with the original
    // RehabMotion controller. Do NOT read Serial anywhere else here, otherwise
    // those bytes would be consumed before rehab_uart_link_update() can parse them.
    rehab_uart_link_update();

    delay(5);
}
