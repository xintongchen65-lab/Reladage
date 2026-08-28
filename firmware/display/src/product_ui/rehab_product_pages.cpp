#include "rehab_product_pages.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "../rehab_uart_link.h"
#include "rehab_ui_router.h"

namespace {
lv_obj_t *g_root = nullptr;
RehabProductPage g_page = RP_HOME;

extern "C" void rehab_ui_show_home();

static lv_color_t C_BG(){ return lv_color_hex(0xF4F8F6); }
static lv_color_t C_CARD(){ return lv_color_hex(0xFFFFFF); }
static lv_color_t C_TEXT(){ return lv_color_hex(0x243029); }
static lv_color_t C_MUTED(){ return lv_color_hex(0x77847D); }
static lv_color_t C_ACCENT(){ return lv_color_hex(0x20B486); }
static lv_color_t C_BLUE(){ return lv_color_hex(0x4B7BEC); }
static lv_color_t C_WARN(){ return lv_color_hex(0xE89B3C); }
static lv_color_t C_DANGER(){ return lv_color_hex(0xD76B6B); }
static lv_color_t C_SOFT(){ return lv_color_hex(0xE9F6F1); }

static lv_obj_t* card(lv_obj_t* p,int x,int y,int w,int h,int r=16){
  lv_obj_t* o=lv_obj_create(p);
  lv_obj_set_pos(o,x,y); lv_obj_set_size(o,w,h);
  lv_obj_set_style_radius(o,r,0); lv_obj_set_style_bg_color(o,C_CARD(),0); lv_obj_set_style_bg_opa(o,LV_OPA_COVER,0);
  lv_obj_set_style_border_width(o,1,0); lv_obj_set_style_border_color(o,lv_color_hex(0xE4ECE8),0);
  lv_obj_set_style_pad_all(o,0,0); lv_obj_clear_flag(o,LV_OBJ_FLAG_SCROLLABLE); return o;
}
static lv_obj_t* txt(lv_obj_t* p,const char* s,int x,int y,int fs=18,lv_color_t c=C_TEXT()){
  lv_obj_t* l=lv_label_create(p); lv_label_set_text(l,s?s:""); lv_obj_set_pos(l,x,y); lv_obj_set_style_text_color(l,c,0); (void)fs; return l;
}
static void chip(lv_obj_t* p,const char* s,int x,int y,int w,lv_color_t c){
  lv_obj_t* o=card(p,x,y,w,30,15); lv_obj_set_style_bg_color(o,c,0); lv_obj_set_style_border_width(o,0,0); txt(o,s,12,7,13,lv_color_white());
}
static void clear(){ if(!g_root) g_root=lv_scr_act(); lv_obj_clean(g_root); lv_obj_set_style_bg_color(g_root,C_BG(),0); lv_obj_set_style_bg_opa(g_root,LV_OPA_COVER,0); }

static lv_obj_t* hit(lv_obj_t* parent,int x,int y,int w,int h,lv_event_cb_t cb,intptr_t user=0){
  lv_obj_t* b=lv_btn_create(parent); lv_obj_set_pos(b,x,y); lv_obj_set_size(b,w,h); lv_obj_clear_flag(b,LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_opa(b,LV_OPA_TRANSP,0); lv_obj_set_style_border_width(b,0,0); lv_obj_set_style_shadow_width(b,0,0); lv_obj_set_style_pad_all(b,0,0);
  lv_obj_add_event_cb(b,cb,LV_EVENT_CLICKED,(void*)user); return b;
}
static void product_back_home(lv_event_t *e){ if(lv_event_get_code(e)!=LV_EVENT_CLICKED)return; rehab_ui_router_leave(); rehab_ui_show_home(); }
static void product_open_page(lv_event_t *e){ if(lv_event_get_code(e)!=LV_EVENT_CLICKED)return; rehab_product_show_page((RehabProductPage)(intptr_t)lv_event_get_user_data(e)); }
static void send_cmd(const char* c){ if(c&&*c) rehab_uart_link_send_command(c); }
static void product_select_exercise(lv_event_t *e){
  if(lv_event_get_code(e)!=LV_EVENT_CLICKED)return;
  const int idx=(int)(intptr_t)lv_event_get_user_data(e); char cmd[48]; snprintf(cmd,sizeof(cmd),"SELECT_EXERCISE:%d",idx); send_cmd(cmd);
  RehabUiModel m=rehab_ui_router_model(); m.exerciseIndex=(uint8_t)idx; m.currentExercise=rehab_ui_exercise_name((uint8_t)idx); m.targetAngleDeg=m.tasks[idx].targetDeg; m.targetSets=m.tasks[idx].sets; m.targetReps=m.tasks[idx].reps; rehab_ui_router_set_model(m);
  rehab_product_show_page(RP_PLAN_DETAIL);
}
static void product_start_flow(lv_event_t *e){ if(lv_event_get_code(e)==LV_EVENT_CLICKED){ send_cmd("START_FLOW"); rehab_product_show_page(RP_PRECHECK); } }
static void product_pause(lv_event_t *e){ if(lv_event_get_code(e)==LV_EVENT_CLICKED) send_cmd("PAUSE"); }
static void product_resume(lv_event_t *e){ if(lv_event_get_code(e)==LV_EVENT_CLICKED) send_cmd("RESUME"); }
static void product_stop(lv_event_t *e){ if(lv_event_get_code(e)==LV_EVENT_CLICKED) send_cmd("STOP"); }
static void product_continue_calibration(lv_event_t *e){ if(lv_event_get_code(e)==LV_EVENT_CLICKED) send_cmd("CALIBRATION_CONTINUE"); }
static void product_retry(lv_event_t *e){ if(lv_event_get_code(e)==LV_EVENT_CLICKED) send_cmd("RETRY"); }
static void product_sync_now(lv_event_t *e){ if(lv_event_get_code(e)==LV_EVENT_CLICKED) send_cmd("SYNC_NOW"); }
static void product_game_start(lv_event_t *e){
  if(lv_event_get_code(e)!=LV_EVENT_CLICKED)return; int idx=(int)(intptr_t)lv_event_get_user_data(e); char cmd[48];
  snprintf(cmd,sizeof(cmd),"SELECT_EXERCISE:%d",idx); send_cmd(cmd); send_cmd("SELECT_MODE:1"); send_cmd("START_FLOW"); rehab_product_show_page(RP_PRECHECK);
}

static void title(const char* zh,const char* en){
  txt(g_root,"Reladage / 轻松绷",24,14,18,C_ACCENT()); txt(g_root,zh,24,46,26,C_TEXT()); txt(g_root,en,24,77,13,C_MUTED());
  if(g_page!=RP_HOME){ txt(g_root,"‹ 首页",700,22,18,C_MUTED()); hit(g_root,680,8,104,54,product_back_home); }
}
static void metric_box(lv_obj_t* p,const char* name,const char* value,int x,int y,int w,lv_color_t c){
  lv_obj_t* o=card(p,x,y,w,82,16); txt(o,name,14,12,13,C_MUTED()); txt(o,value,14,39,26,c);
}
static void status_dot(lv_obj_t* p,bool ok,int x,int y,const char* label){
  lv_obj_t* d=lv_obj_create(p); lv_obj_set_pos(d,x,y+4); lv_obj_set_size(d,10,10); lv_obj_set_style_radius(d,LV_RADIUS_CIRCLE,0); lv_obj_set_style_border_width(d,0,0); lv_obj_set_style_bg_color(d,ok?C_ACCENT():C_WARN(),0);
  txt(p,label,x+18,y,14,ok?C_TEXT():C_WARN());
}
static int imu_count(uint8_t mask){ int n=0; for(int i=0;i<5;i++) if(mask&(1u<<i)) n++; return n; }

static void page_home(){
  const RehabUiModel& m=rehab_ui_router_model(); char b[96];
  title("今日康复训练","TODAY'S REHABILITATION PLAN");
  lv_obj_t* main=card(g_root,24,112,468,220,20); txt(main,"当前训练处方",20,16,15,C_MUTED());
  snprintf(b,sizeof(b),"%u 项任务 · %u 分钟",m.taskCount,m.weeklyMinutes>30?19:15); txt(main,b,20,46,20,C_TEXT());
  int complete=0; for(int i=0;i<m.taskCount;i++) if(m.tasks[i].completed) complete++;
  snprintf(b,sizeof(b),"%d / %u",complete,m.taskCount); txt(main,b,20,82,42,C_ACCENT());
  txt(main,"设备端完成动作判断，断网仍可训练",20,132,14,C_MUTED()); chip(main,"开始训练",20,170,138,C_ACCENT());
  hit(main,12,158,170,55,product_open_page,(intptr_t)RP_PLAN_DETAIL);

  lv_obj_t* st=card(g_root,510,112,266,104,20); txt(st,"设备状态",18,15,14,C_MUTED());
  snprintf(b,sizeof(b),"IMU %d/5 · %d%%",imu_count(m.imuMask),m.batteryPct); txt(st,b,18,44,23,imu_count(m.imuMask)==5?C_ACCENT():C_WARN());
  txt(st,m.wifiConnected?"云端在线":"离线模式",18,76,13,m.wifiConnected?C_BLUE():C_WARN());

  lv_obj_t* sync=card(g_root,510,228,266,104,20); txt(sync,"数据同步",18,15,14,C_MUTED());
  snprintf(b,sizeof(b),"待同步 %lu 条",(unsigned long)m.syncQueueCount); txt(sync,b,18,44,22,m.syncQueueCount?C_WARN():C_ACCENT());
  txt(sync,m.sdReady?"SD 本地记录正常":"SD 存储异常",18,76,13,m.sdReady?C_TEXT():C_DANGER());

  lv_obj_t* flow=card(g_root,24,348,752,104,18); txt(flow,"动作库",22,18,15,C_BLUE()); txt(flow,"训练记录",210,18,15,C_BLUE()); txt(flow,"康复报告",398,18,15,C_BLUE()); txt(flow,"设备中心",586,18,15,C_BLUE());
  txt(flow,"8 个动作",22,54,14,C_MUTED()); txt(flow,"训练历史",210,54,14,C_MUTED()); txt(flow,"ROM / 质量",398,54,14,C_MUTED()); txt(flow,"5 IMU / SD",586,54,14,C_MUTED());
  hit(flow,0,0,188,104,product_open_page,(intptr_t)RP_EXERCISE_LIBRARY); hit(flow,188,0,188,104,product_open_page,(intptr_t)RP_HISTORY);
  hit(flow,376,0,188,104,product_open_page,(intptr_t)RP_REPORT); hit(flow,564,0,188,104,product_open_page,(intptr_t)RP_DEVICE_CENTER);
}

static void page_exercise_library(){
  title("动作库","8-EXERCISE REHABILITATION LIBRARY");
  for(int i=0;i<8;i++){
    int col=i%2,row=i/2; lv_obj_t* c=card(g_root,24+col*384,108+row*83,368,70,14); char n[72]; snprintf(n,sizeof(n),"%02d  %s",i+1,rehab_ui_exercise_name(i)); txt(c,n,16,12,16,C_TEXT());
    txt(c,rehab_ui_exercise_metric(i),16,39,12,C_MUTED()); chip(c,rehab_ui_exercise_region(i),270,11,82,i<5?C_BLUE():C_ACCENT()); hit(c,0,0,368,70,product_select_exercise,(intptr_t)i);
  }
}

static void page_plan_detail(){
  const RehabUiModel& m=rehab_ui_router_model(); const uint8_t idx=m.exerciseIndex<8?m.exerciseIndex:0; const RehabUiTask& t=m.tasks[idx]; char b[96];
  title("训练计划详情","PRESCRIPTION DETAIL");
  lv_obj_t* hero=card(g_root,24,112,752,118,20); txt(hero,t.name,20,16,24,C_TEXT()); txt(hero,rehab_ui_exercise_metric(idx),20,52,14,C_MUTED());
  snprintf(b,sizeof(b),"%u 组 × %u 次",t.sets,t.reps); chip(hero,b,530,18,170,C_ACCENT()); snprintf(b,sizeof(b),"目标角度 %.0f° · 组间休息 %u 秒",t.targetDeg,t.restSec); txt(hero,b,20,84,15,C_BLUE());
  lv_obj_t* tech=card(g_root,24,246,368,164,18); txt(tech,"动作评价项目",18,16,16,C_TEXT()); txt(tech,"• 动作幅度 / ROM",18,50,14,C_MUTED()); txt(tech,"• 动作平面偏差",18,78,14,C_MUTED()); txt(tech,"• 近端代偿与躯干代偿",18,106,14,C_MUTED()); txt(tech,"• 速度、稳定性与返回控制",18,134,14,C_MUTED());
  lv_obj_t* path=card(g_root,408,246,368,164,18); txt(path,"训练流程",18,16,16,C_TEXT()); txt(path,"佩戴检查 → 人体定位",18,50,14,C_MUTED()); txt(path,"动作校准 → 正式训练",18,78,14,C_MUTED()); txt(path,"组间休息 → 训练结果",18,106,14,C_MUTED()); chip(path,"开始训练",210,118,132,C_ACCENT()); hit(path,198,106,155,52,product_start_flow);
  txt(g_root,"处方可由患者/家属端下发，并在设备端离线执行。",24,430,14,C_MUTED());
}

static void page_precheck(){
  const RehabUiModel& m=rehab_ui_router_model(); char b[64];
  title("训练前检查","PRE-TRAINING CHECK");
  lv_obj_t* c=card(g_root,24,112,752,270,20); txt(c,"系统检查",20,18,18,C_TEXT());
  status_dot(c,imu_count(m.imuMask)==5,26,58,"5 个 IMU 连接"); status_dot(c,m.sdReady,26,96,"Micro SD 本地记录"); status_dot(c,m.voiceReady,26,134,"离线语音模块"); status_dot(c,m.bodyReferenceReady,26,172,"腰腹人体参考");
  status_dot(c,m.batteryPct>=20,380,58,"电量满足训练"); status_dot(c,true,380,96,"训练处方已加载"); status_dot(c,m.wifiConnected||m.sdReady,380,134,m.wifiConnected?"网络同步可用":"离线训练可用");
  snprintf(b,sizeof(b),"同步队列 %lu 条",(unsigned long)m.syncQueueCount); status_dot(c,true,380,172,b);
  chip(c,"下一步：佩戴确认",520,218,190,C_ACCENT()); hit(c,500,206,220,54,product_open_page,(intptr_t)RP_WEAR_GUIDE);
  txt(g_root,"检查未通过时仍保留离线训练与本地记录路径，传感器异常会在训练中阻止计数。",24,406,13,C_MUTED());
}

static void page_wear_guide(){
  const RehabUiModel& m=rehab_ui_router_model();
  title("传感器佩戴","5-IMU WEAR GUIDE");
  const char* n[]={"A 左上臂","B 左前臂","C 大腿/右侧训练肢","D 小腿/远端","E 腰腹人体参考"};
  for(int i=0;i<5;i++){ lv_obj_t* c=card(g_root,24,110+i*59,530,48,12); txt(c,n[i],16,13,14,C_TEXT()); bool ok=(m.imuMask&(1u<<i))!=0; chip(c,ok?"已连接":"等待连接",410,9,100,ok?C_ACCENT():C_WARN()); }
  lv_obj_t* side=card(g_root,572,110,204,284,18); txt(side,"佩戴原则",18,18,16,C_TEXT()); txt(side,"固定牢靠",18,54,14,C_MUTED()); txt(side,"方向无需绝对一致",18,82,14,C_MUTED()); txt(side,"校准建立人体语义轴",18,110,14,C_MUTED()); txt(side,"E 随人体转身更新参考",18,138,14,C_MUTED()); chip(side,"佩戴完成",30,220,144,C_ACCENT()); hit(side,18,208,168,58,product_open_page,(intptr_t)RP_BODY_POSITION);
  txt(g_root,"A/B 用于上肢链，C/D 用于下肢链，E 作为动态人体坐标系参考。",24,420,13,C_MUTED());
}

static void page_body_position(){
  title("人体定位","BODY POSITION / DYNAMIC REFERENCE");
  lv_obj_t* a=card(g_root,24,112,360,292,20); txt(a,"01  中立站姿",18,18,17,C_TEXT()); txt(a,"身体自然直立或按动作要求就位",18,54,14,C_MUTED()); txt(a,"保持 2 秒稳定",18,82,14,C_MUTED());
  txt(a,"02  正前方确认",18,128,17,C_TEXT()); txt(a,"建立 FRONT / SIDE / DOWN 语义方向",18,164,14,C_MUTED()); txt(a,"不绑定房间绝对方向",18,192,14,C_MUTED());
  chip(a,"人体参考已建立",18,238,172,C_ACCENT());
  lv_obj_t* b=card(g_root,402,112,374,292,20); txt(b,"动态人体坐标系",18,18,17,C_TEXT()); txt(b,"腰腹 E 节点随人体朝向变化",18,54,14,C_MUTED()); txt(b,"整体转身 → 参考系同步旋转",18,84,14,C_MUTED()); txt(b,"肢体离轴 → 仍判定平面偏移",18,114,14,C_MUTED()); txt(b,"躯干倾斜 → 独立记录代偿",18,144,14,C_MUTED()); chip(b,"进入动作校准",200,238,148,C_BLUE()); hit(b,188,226,170,58,product_open_page,(intptr_t)RP_MOTION_CALIBRATION);
  txt(g_root,"该参考用于减少家庭训练中因站位变化、转身造成的固定空间方向误判。",24,424,13,C_MUTED());
}

static void page_motion_calibration(){
  const RehabUiModel& m=rehab_ui_router_model(); char b[96];
  title("动作校准","MOTION CALIBRATION 1 / 2 / 3");
  lv_obj_t* p=card(g_root,24,112,752,238,20); txt(p,m.currentExercise,20,18,19,C_TEXT());
  const char* step[]={"1  起始姿态：保持静止，记录中立姿态","2  功能动作：完成一次标准动作，学习功能轴","3  返回确认：回到起始位，验证动作范围与方向"};
  for(int i=0;i<3;i++){ lv_obj_t* s=card(p,18,54+i*52,716,42,12); txt(s,step[i],14,12,14,C_TEXT()); chip(s,i<2?"已完成":(m.calibrationReady?"已完成":"进行中"),590,6,104,i<2||m.calibrationReady?C_ACCENT():C_WARN()); }
  snprintf(b,sizeof(b),"目标 %.0f° · 起始阈值按动作独立配置",m.targetAngleDeg); txt(p,b,18,212,13,C_MUTED());
  chip(g_root,"继续 / 确认校准",496,372,186,C_ACCENT()); hit(g_root,482,360,215,58,product_continue_calibration);
  chip(g_root,"重新校准",306,372,150,C_BLUE()); hit(g_root,292,360,178,58,product_retry);
  txt(g_root,"校准成功后进入正式训练；失败时保留失败原因并允许重新采样。",24,430,13,C_MUTED());
}

static void page_live_training(){
  const RehabUiModel& m=rehab_ui_router_model(); char b[96];
  title("实时训练","LIVE TRAINING");
  lv_obj_t* angle=card(g_root,24,112,286,222,20); txt(angle,m.currentExercise,20,16,16,C_TEXT()); snprintf(b,sizeof(b),"%.1f°",m.currentAngleDeg); txt(angle,b,20,48,50,C_ACCENT()); snprintf(b,sizeof(b),"目标 %.0f°",m.targetAngleDeg); txt(angle,b,20,106,15,C_MUTED()); snprintf(b,sizeof(b),"第 %u/%u 组 · %u/%u 次",m.currentSet,m.targetSets,m.currentRep,m.targetReps); txt(angle,b,20,140,16,C_TEXT()); snprintf(b,sizeof(b),"峰值 ROM %.1f°",m.peakRomDeg); txt(angle,b,20,174,14,C_BLUE());
  lv_obj_t* q=card(g_root,326,112,450,222,20); txt(q,"动作质量",20,16,15,C_MUTED()); txt(q,m.qualityText?m.qualityText:"实时评价",20,44,27,m.qualityScore>=80?C_ACCENT():(m.qualityScore>=60?C_WARN():C_DANGER())); snprintf(b,sizeof(b),"质量评分  %.0f",m.qualityScore); txt(q,b,252,50,15,C_BLUE());
  snprintf(b,sizeof(b),"平面偏差  %.1f°",m.planeDeviationDeg); txt(q,b,20,94,14,C_TEXT()); snprintf(b,sizeof(b),"近端代偿  %.1f°",m.compensationDeg); txt(q,b,226,94,14,C_TEXT()); snprintf(b,sizeof(b),"躯干倾斜  %.1f°",m.torsoTiltDeg); txt(q,b,20,128,14,C_TEXT()); snprintf(b,sizeof(b),"左右差异  %.1f°",m.asymmetryDeg); txt(q,b,226,128,14,C_TEXT()); txt(q,"达到标准动作才计入有效完成",20,174,14,C_MUTED());
  lv_obj_t* ctl=card(g_root,24,350,752,92,18); chip(ctl,"暂停",18,28,110,C_WARN()); chip(ctl,"结束训练",144,28,130,C_DANGER()); chip(ctl,"数字孪生",474,28,118,C_BLUE()); chip(ctl,"游戏",608,28,96,C_ACCENT()); hit(ctl,12,18,120,58,product_pause); hit(ctl,138,18,140,58,product_stop); hit(ctl,466,18,130,58,product_open_page,(intptr_t)RP_DIGITAL_TWIN); hit(ctl,600,18,108,58,product_open_page,(intptr_t)RP_GAME_HUB);
}

static void page_rest(){
  const RehabUiModel& m=rehab_ui_router_model(); char b[80];
  title("组间休息","REST BETWEEN SETS");
  lv_obj_t* c=card(g_root,110,112,580,286,24); snprintf(b,sizeof(b),"第 %u / %u 组完成",m.currentSet,m.targetSets); txt(c,b,30,24,20,C_TEXT()); snprintf(b,sizeof(b),"%u",m.restSeconds); txt(c,b,236,66,64,C_ACCENT()); txt(c,"秒后继续",228,140,16,C_MUTED()); txt(c,"可活动肢体，保持传感器佩戴",30,190,14,C_MUTED()); chip(c,"立即继续",370,224,150,C_ACCENT()); hit(c,354,210,178,62,product_resume);
}

static void page_session_result(){
  const RehabUiModel& m=rehab_ui_router_model(); char b[80];
  title("训练完成","SESSION RESULT");
  snprintf(b,sizeof(b),"%u / %u",m.completedTotal,m.targetTotal); metric_box(g_root,"完成次数",b,24,118,174,C_ACCENT()); snprintf(b,sizeof(b),"%.0f%%",m.weeklyPassRate); metric_box(g_root,"动作达标率",b,214,118,174,C_ACCENT()); snprintf(b,sizeof(b),"%.0f°",m.weeklyMaxRomDeg); metric_box(g_root,"最大 ROM",b,404,118,174,C_BLUE()); snprintf(b,sizeof(b),"%lu min",(unsigned long)(m.elapsedSeconds/60)); metric_box(g_root,"训练时长",b,594,118,182,C_TEXT());
  lv_obj_t* q=card(g_root,24,220,752,154,18); txt(q,"训练质量总结",18,16,16,C_TEXT()); txt(q,"动作幅度、平面、代偿、稳定性已写入本地 Session 记录",18,50,14,C_MUTED()); txt(q,m.wifiConnected?"云端同步完成 / 进行中":"当前离线：记录已进入自动补传队列",18,80,14,m.wifiConnected?C_ACCENT():C_WARN()); chip(q,"查看康复报告",548,104,170,C_BLUE()); hit(q,532,94,190,52,product_open_page,(intptr_t)RP_REPORT);
  chip(g_root,"返回首页",620,402,140,C_ACCENT()); hit(g_root,606,390,160,58,product_back_home);
}

static void page_digital_twin(){
  const RehabUiModel& m=rehab_ui_router_model(); char b[80];
  title("数字孪生","DIGITAL TWIN / MOTION MIRROR");
  lv_obj_t* a=card(g_root,24,112,370,320,20); txt(a,"五节点运动输入",20,18,17,C_TEXT()); txt(a,"A/B/C/D/E → MotionFrame",20,48,14,C_MUTED()); snprintf(b,sizeof(b),"主关节 %.1f°",m.currentAngleDeg); txt(a,b,20,92,24,C_ACCENT()); snprintf(b,sizeof(b),"躯干 %.1f°",m.torsoTiltDeg); txt(a,b,20,132,17,C_TEXT()); snprintf(b,sizeof(b),"平面偏差 %.1f°",m.planeDeviationDeg); txt(a,b,20,166,17,m.planeDeviationDeg>25?C_WARN():C_TEXT()); txt(a,"人体朝向随 E 节点动态更新",20,220,14,C_BLUE()); txt(a,"姿态包可发送至屏幕/小程序/游戏",20,254,13,C_MUTED());
  lv_obj_t* c=card(g_root,410,112,366,320,20); txt(c,"虚拟姿态状态",20,18,17,C_TEXT()); txt(c,"关节链：肩 / 肘 / 膝 / 躯干",20,48,14,C_MUTED()); txt(c,"错误区域高亮",20,92,18,C_WARN()); txt(c,"ROM 不足 → 关节标记",20,128,14,C_TEXT()); txt(c,"平面偏移 → 轨迹标记",20,158,14,C_TEXT()); txt(c,"代偿动作 → 躯干/近端标记",20,188,14,C_TEXT()); snprintf(b,sizeof(b),"质量评分 %.0f",m.qualityScore); txt(c,b,20,230,24,C_ACCENT()); txt(c,"实时镜像 · 端侧完成",20,276,14,C_BLUE());
}

static void page_game_hub(){
  title("游戏训练","REHABILITATION GAME HUB");
  const char* games[]={"划船挑战","高处摘果","肩部摘果","打地鼠","点球大战","活力公园","平衡步道","台阶冒险"};
  for(int i=0;i<8;i++){ int col=i%2,row=i/2; lv_obj_t* c=card(g_root,24+col*384,108+row*83,368,70,14); txt(c,games[i],16,12,17,C_TEXT()); txt(c,rehab_ui_exercise_name(i),16,40,13,C_MUTED()); chip(c,"达标触发",272,20,78,C_ACCENT()); hit(c,0,0,368,70,product_game_start,(intptr_t)i); }
}

static void page_history(){
  title("训练记录","TRAINING HISTORY");
  const char* day[]={"08/28  12:40","08/27  20:16","08/26  18:42","08/25  19:03"}; const char* ex[]={"哑铃弯举","膝关节屈伸","肱三头肌伸展","坐到站训练"}; const char* result[]={"30次 · 89%","30次 · 87%","20次 · 91%","24次 · 84%"};
  for(int i=0;i<4;i++){ lv_obj_t* c=card(g_root,24,112+i*78,752,64,14); txt(c,day[i],16,18,13,C_MUTED()); txt(c,ex[i],178,17,17,C_TEXT()); txt(c,result[i],420,18,15,C_ACCENT()); txt(c,"详情 ›",650,18,14,C_BLUE()); }
  txt(g_root,"记录包含每次 ROM、质量位图、动作评分、错误类型和离线同步状态。",24,430,13,C_MUTED());
}

static void page_report(){
  const RehabUiModel& m=rehab_ui_router_model(); char b[64];
  title("康复报告","ROM / QUALITY / ADHERENCE REPORT");
  snprintf(b,sizeof(b),"%u 天",m.weeklyTrainingDays); metric_box(g_root,"本周训练",b,24,112,178,C_ACCENT()); snprintf(b,sizeof(b),"%.0f%%",m.weeklyPassRate); metric_box(g_root,"动作达标率",b,216,112,178,C_ACCENT()); snprintf(b,sizeof(b),"%.0f°",m.weeklyMaxRomDeg); metric_box(g_root,"最大 ROM",b,408,112,178,C_BLUE()); snprintf(b,sizeof(b),"%.0f",m.weeklyQualityScore); metric_box(g_root,"质量评分",b,600,112,176,C_TEXT());
  lv_obj_t* chart=card(g_root,24,212,492,216,18); txt(chart,"7 日 ROM 趋势",18,16,15,C_TEXT());
  for(int i=0;i<7;i++){ int h=(int)(m.romTrend[i]*1.25f); if(h<30)h=30; if(h>138)h=138; lv_obj_t* bar=lv_obj_create(chart); lv_obj_set_pos(bar,34+i*63,176-h); lv_obj_set_size(bar,30,h); lv_obj_set_style_bg_color(bar,C_ACCENT(),0); lv_obj_set_style_border_width(bar,0,0); lv_obj_set_style_radius(bar,7,0); snprintf(b,sizeof(b),"%.0f",m.romTrend[i]); txt(chart,b,31+i*63,184,11,C_MUTED()); }
  lv_obj_t* detail=card(g_root,532,212,244,216,18); txt(detail,"关键错误趋势",16,16,15,C_TEXT()); txt(detail,"幅度不足  8%",16,54,14,C_MUTED()); txt(detail,"平面偏移  5%",16,84,14,C_MUTED()); txt(detail,"代偿动作  4%",16,114,14,C_MUTED()); txt(detail,"不稳定    3%",16,144,14,C_MUTED()); chip(detail,"AI 建议",16,172,98,C_BLUE()); hit(detail,8,162,114,48,product_open_page,(intptr_t)RP_AI_COACH);
}

static void page_ai_coach(){
  const RehabUiModel& m=rehab_ui_router_model(); char b[96];
  title("AI 康复助手","DATA-DRIVEN TRAINING ASSISTANT");
  lv_obj_t* c=card(g_root,24,112,752,218,20); txt(c,"基于近期训练记录生成参数建议",20,18,15,C_MUTED()); snprintf(b,sizeof(b),"%s 目标角度",m.currentExercise); txt(c,b,20,54,20,C_TEXT()); snprintf(b,sizeof(b),"%.0f°  →  %.0f°",m.targetAngleDeg,m.targetAngleDeg+5); txt(c,b,20,88,32,C_ACCENT()); txt(c,"依据：ROM 达标率、动作质量、平面偏差、代偿比例和训练完成率",20,136,14,C_MUTED()); chip(c,"等待康复师 / 家属确认",20,174,198,C_BLUE());
  lv_obj_t* q=card(g_root,24,348,752,92,18); txt(q,"恢复情况摘要",18,14,15,C_TEXT()); txt(q,"本周 ROM 趋势上升，错误动作比例下降；建议维持训练频次并小幅提高目标。",18,48,14,C_ACCENT());
}

static void page_device_center(){
  const RehabUiModel& m=rehab_ui_router_model(); char b[80];
  title("设备中心","DEVICE / SENSOR STATUS");
  const char* sensor[]={"IMU-A 左上臂","IMU-B 左前臂","IMU-C 大腿/训练肢","IMU-D 小腿/远端","IMU-E 腰腹参考"};
  for(int i=0;i<5;i++){ lv_obj_t* c=card(g_root,24,108+i*57,510,47,12); txt(c,sensor[i],16,13,14,C_TEXT()); bool ok=(m.imuMask&(1u<<i))!=0; txt(c,ok?"在线":"离线",440,13,14,ok?C_ACCENT():C_WARN()); }
  lv_obj_t* st=card(g_root,552,108,224,294,18); txt(st,"主控终端",18,18,16,C_TEXT()); snprintf(b,sizeof(b),"电量 %d%%",m.batteryPct); txt(st,b,18,52,15,C_ACCENT()); txt(st,m.wifiConnected?"Wi-Fi 已连接":"Wi-Fi 离线",18,82,15,m.wifiConnected?C_TEXT():C_WARN()); txt(st,m.sdReady?"SD 存储正常":"SD 未就绪",18,112,15,m.sdReady?C_TEXT():C_DANGER()); snprintf(b,sizeof(b),"剩余 %lu MB",(unsigned long)m.storageFreeMb); txt(st,b,18,142,14,C_MUTED()); snprintf(b,sizeof(b),"同步队列 %lu",(unsigned long)m.syncQueueCount); txt(st,b,18,172,14,m.syncQueueCount?C_WARN():C_ACCENT()); txt(st,"固件 RehabMotion_v5",18,204,13,C_MUTED()); txt(st,m.bodyReferenceReady?"人体参考：就绪":"人体参考：需校准",18,234,14,m.bodyReferenceReady?C_BLUE():C_WARN()); chip(st,"设备设置",18,258,120,C_BLUE()); hit(st,8,248,140,44,product_open_page,(intptr_t)RP_SETTINGS);
}

static void page_settings(){
  title("设置","SYSTEM SETTINGS");
  const char* items[]={"训练音量","屏幕亮度","离线语音","Wi-Fi 网络","数据同步","动作校准","存储管理","关于系统"};
  const char* state[]={"70%","80%","已开启","已连接","自动","查看","7.4 GB","v5"};
  for(int i=0;i<8;i++){ int col=i%2,row=i/2; lv_obj_t* c=card(g_root,24+col*384,108+row*82,368,68,14); txt(c,items[i],16,14,16,C_TEXT()); txt(c,state[i],250,14,14,C_ACCENT()); txt(c,"›",334,14,18,C_MUTED()); if(i==7)hit(c,0,0,368,68,product_open_page,(intptr_t)RP_ABOUT); if(i==5)hit(c,0,0,368,68,product_open_page,(intptr_t)RP_MOTION_CALIBRATION); }
}

static void page_prescription_sync(){
  const RehabUiModel& m=rehab_ui_router_model(); char b[80];
  title("新训练计划已同步","PRESCRIPTION SYNC");
  lv_obj_t* c=card(g_root,80,108,640,308,24); snprintf(b,sizeof(b),"已收到 %u 项训练任务",m.taskCount); txt(c,b,28,22,19,C_ACCENT());
  for(int i=0;i<m.taskCount && i<4;i++){ snprintf(b,sizeof(b),"%02d  %s",i+1,m.tasks[i].name); txt(c,b,28,66+i*42,16,C_TEXT()); snprintf(b,sizeof(b),"%u×%u · %.0f°",m.tasks[i].sets,m.tasks[i].reps,m.tasks[i].targetDeg); txt(c,b,440,66+i*42,14,C_MUTED()); }
  txt(c,"来源：患者/家属小程序或康复师管理端",28,242,14,C_MUTED()); chip(c,"确认并保存",454,258,150,C_ACCENT());
}

static void page_offline_sync(){
  const RehabUiModel& m=rehab_ui_router_model(); char b[96];
  title("离线训练与自动同步","OFFLINE-FIRST DATA FLOW");
  const char* s[]={"1  网络断开","2  端侧训练继续","3  SD 记录 + 断电检查点","4  网络恢复","5  自动补传后台"};
  for(int i=0;i<5;i++){ lv_obj_t* c=card(g_root,130,104+i*63,540,48,14); txt(c,s[i],20,14,16,i==0?C_WARN():(i==4?C_ACCENT():C_TEXT())); }
  snprintf(b,sizeof(b),"当前待同步 %lu 条 · SD %s",(unsigned long)m.syncQueueCount,m.sdReady?"正常":"异常"); txt(g_root,b,130,422,14,m.syncQueueCount?C_WARN():C_MUTED()); chip(g_root,"立即同步",556,414,112,C_BLUE()); hit(g_root,544,404,130,48,product_sync_now);
}

static void page_about(){
  title("关于轻松绷","ABOUT RELADAGE / REHABMOTION");
  lv_obj_t* c=card(g_root,70,112,660,300,24); txt(c,"Reladage / 轻松绷",28,26,26,C_ACCENT()); txt(c,"居家智能康复训练系统",28,64,18,C_TEXT()); txt(c,"ESP32-S3 · 5×IMU · Dynamic Body Reference",28,108,15,C_MUTED()); txt(c,"8 动作引擎 · SD Offline · Voice · MiniApp",28,140,15,C_MUTED()); txt(c,"Digital Twin · Game Rehab · Report · AI Assistant",28,172,15,C_MUTED()); txt(c,"端侧动作判定 + 断网记录 + 联网自动同步",28,214,14,C_BLUE()); txt(c,"© 2026 Dothink / Reladage Team",28,258,13,C_MUTED());
}

} // namespace

void rehab_product_pages_bind_root(lv_obj_t *root){ g_root=root; }
void rehab_product_pages_init(lv_obj_t *root){ g_root=root; rehab_product_show_page(RP_HOME); }
void rehab_product_show_page(RehabProductPage page){
  if(!g_root) g_root=lv_scr_act();
  if(page<RP_HOME || page>=RP_PAGE_COUNT) page=RP_HOME;
  g_page=page; clear();
  switch(page){
    case RP_HOME: page_home(); break;
    case RP_EXERCISE_LIBRARY: page_exercise_library(); break;
    case RP_PLAN_DETAIL: page_plan_detail(); break;
    case RP_PRECHECK: page_precheck(); break;
    case RP_WEAR_GUIDE: page_wear_guide(); break;
    case RP_BODY_POSITION: page_body_position(); break;
    case RP_MOTION_CALIBRATION: page_motion_calibration(); break;
    case RP_LIVE_TRAINING: page_live_training(); break;
    case RP_REST: page_rest(); break;
    case RP_SESSION_RESULT: page_session_result(); break;
    case RP_DIGITAL_TWIN: page_digital_twin(); break;
    case RP_GAME_HUB: page_game_hub(); break;
    case RP_HISTORY: page_history(); break;
    case RP_REPORT: page_report(); break;
    case RP_AI_COACH: page_ai_coach(); break;
    case RP_DEVICE_CENTER: page_device_center(); break;
    case RP_SETTINGS: page_settings(); break;
    case RP_PRESCRIPTION_SYNC: page_prescription_sync(); break;
    case RP_OFFLINE_SYNC: page_offline_sync(); break;
    case RP_ABOUT: page_about(); break;
    default: page_home(); break;
  }
}
RehabProductPage rehab_product_current_page(){ return g_page; }
