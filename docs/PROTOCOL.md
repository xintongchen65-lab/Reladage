# Device / Screen / Cloud Protocol

## 屏幕 → 主控命令

```text
CMD|START
CMD|PAUSE
CMD|RESUME
CMD|STOP
CMD|BEGIN_POSITION
CMD|SELECT_EXERCISE|0..7
CMD|DEVICE_STATE?
CMD|PRESCRIPTION?
CMD|RX_ACK|<plan_id>
```

兼容当前屏幕主流程中的短命令：`START_FLOW`、`BEGIN_POSITION`、`PAUSE`、`RESUME`、`STOP`、`HOME`。

## 主控 → 屏幕事件

```text
EVT|DEVICE|ready=1|imu=1F|battery=82|wifi=1|sd=1
EVT|EXERCISE|dumbbell_curl|哑铃弯举 / 屈肘训练
EVT|PRESCRIPTION|plan=20260827001|items=3
EVT|SYNC|wifi=0|queued=12
EVT|TRAIN|...
EVT|SESSION_DONE|...
```

## 处方 JSON

```json
{
  "plan_id": 20260827001,
  "issued_at": "2026-08-27T09:00:00+08:00",
  "items": [
    {"exercise":"dumbbell_curl","sets":3,"reps":10,"target_deg":80,"rest_sec":30},
    {"exercise":"triceps_extension","sets":2,"reps":10,"target_deg":75,"rest_sec":30},
    {"exercise":"knee_flex_extend","sets":3,"reps":10,"target_deg":80,"rest_sec":30}
  ]
}
```

协议实现位于 `firmware/controller/RehabMotion_v5/src/rehab/protocol/`，页面事件路由位于 `firmware/display/src/product_ui/rehab_ui_router.*`。
