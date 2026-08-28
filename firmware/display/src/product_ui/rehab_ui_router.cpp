#include "rehab_ui_router.h"
#include <string.h>
#include <stdlib.h>

static RehabUiModel g_model = rehab_ui_default_model();
static bool g_active = false;

void rehab_ui_router_init(lv_obj_t* root){ rehab_product_pages_bind_root(root); g_active=false; }
void rehab_ui_router_set_model(const RehabUiModel& model){ g_model=model; }
const RehabUiModel& rehab_ui_router_model(){ return g_model; }
void rehab_ui_router_open(RehabProductPage page){ g_active=true; rehab_product_show_page(page); }
bool rehab_ui_router_is_active(){ return g_active; }
void rehab_ui_router_leave(){ g_active=false; }

static int parse_int_field(const char* line,const char* key,int fallback){
  const char* p=strstr(line,key); return p?atoi(p+strlen(key)):fallback;
}
static float parse_float_field(const char* line,const char* key,float fallback){
  const char* p=strstr(line,key); return p?(float)atof(p+strlen(key)):fallback;
}

void rehab_ui_router_handle_event(const char* line){
  if(!line) return;

  // Product-level asynchronous events. These are intentionally separate from
  // the high-frequency LIVE snapshot so a new prescription, sync warning or
  // completed session can navigate without coupling the screen to the backend.
  if(strncmp(line,"EVT|PRESCRIPTION|",17)==0){ rehab_ui_router_open(RP_PRESCRIPTION_SYNC); return; }
  if(strncmp(line,"EVT|SYNC|",9)==0){
    g_model.syncQueueCount=(uint32_t)parse_int_field(line,"queue=",(int)g_model.syncQueueCount);
    rehab_ui_router_open(RP_OFFLINE_SYNC); return;
  }
  if(strncmp(line,"EVT|PRECHECK|",13)==0){ rehab_ui_router_open(RP_PRECHECK); return; }
  if(strncmp(line,"EVT|WEAR|",9)==0){ rehab_ui_router_open(RP_WEAR_GUIDE); return; }
  if(strncmp(line,"EVT|BODY_REFERENCE|",19)==0){
    g_model.bodyReferenceReady=parse_int_field(line,"ready=",1)!=0; rehab_ui_router_open(RP_BODY_POSITION); return;
  }
  if(strncmp(line,"EVT|CALIBRATION|",16)==0){
    g_model.calibrationReady=parse_int_field(line,"ready=",0)!=0; rehab_ui_router_open(RP_MOTION_CALIBRATION); return;
  }
  if(strncmp(line,"EVT|TRAIN|",10)==0){ rehab_ui_router_open(RP_LIVE_TRAINING); return; }
  if(strncmp(line,"EVT|REST|",9)==0){
    g_model.restSeconds=(uint16_t)parse_int_field(line,"seconds=",30); rehab_ui_router_open(RP_REST); return;
  }
  if(strncmp(line,"EVT|SESSION_DONE|",17)==0){
    g_model.completedTotal=(uint16_t)parse_int_field(line,"completed=",g_model.completedTotal);
    g_model.targetTotal=(uint16_t)parse_int_field(line,"target=",g_model.targetTotal);
    g_model.weeklyPassRate=parse_float_field(line,"pass=",g_model.weeklyPassRate);
    g_model.weeklyMaxRomDeg=parse_float_field(line,"rom=",g_model.weeklyMaxRomDeg);
    g_model.elapsedSeconds=(uint32_t)parse_int_field(line,"duration=",(int)g_model.elapsedSeconds);
    rehab_ui_router_open(RP_SESSION_RESULT); return;
  }
  if(strncmp(line,"EVT|DEVICE|",11)==0){
    g_model.imuMask=(uint8_t)parse_int_field(line,"imu=",g_model.imuMask);
    g_model.batteryPct=parse_int_field(line,"battery=",g_model.batteryPct);
    g_model.wifiConnected=parse_int_field(line,"wifi=",g_model.wifiConnected?1:0)!=0;
    g_model.sdReady=parse_int_field(line,"sd=",g_model.sdReady?1:0)!=0;
    g_model.syncQueueCount=(uint32_t)parse_int_field(line,"queue=",(int)g_model.syncQueueCount);
    return;
  }
  if(strncmp(line,"EVT|QUALITY|",12)==0){
    g_model.qualityScore=parse_float_field(line,"score=",g_model.qualityScore);
    g_model.planeDeviationDeg=parse_float_field(line,"plane=",g_model.planeDeviationDeg);
    g_model.compensationDeg=parse_float_field(line,"comp=",g_model.compensationDeg);
    g_model.torsoTiltDeg=parse_float_field(line,"torso=",g_model.torsoTiltDeg);
    g_model.asymmetryDeg=parse_float_field(line,"asym=",g_model.asymmetryDeg);
    return;
  }
}
