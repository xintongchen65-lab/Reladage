# Integrated application flow

## Controller

The Arduino sketch keeps the established RehabMotion_v5 calibration/training path and
adds a common eight-action catalog shared with the repository motion engine.

Screen commands:

```text
SELECT_EXERCISE:0 .. SELECT_EXERCISE:7
SELECT_MODE:0 .. SELECT_MODE:2
START_FLOW
BEGIN_POSITION
COUNTDOWN_DONE
PAUSE
RESUME
STOP
SKIP_REST
CALIBRATION_CONTINUE
RETRY
SYNC_NOW
HOME
```

Action mapping:

| ID | Code | Chinese name | Primary sensor pair |
|---:|---|---|---|
| 0 | dumbbell_curl | 哑铃弯举 | A/B + E |
| 1 | triceps_extension | 肱三头肌伸展 | A/B + E |
| 2 | scaption_raise | 肩胛平面抬手 | A/B + E |
| 3 | wall_crawl | 墙面爬手 | A/B + E |
| 4 | knee_flex_extend | 膝关节屈伸 | C/D + E |
| 5 | sit_to_stand | 坐到站训练 | C/D + E |
| 6 | box_squat | 箱式深蹲 | C/D + E |
| 7 | step_up | 台阶踩踏 Step Up | C/D + E |

The shared detector implementation is compiled from `RehabMotion_v5/src/rehab/`.
The V5 telemetry-to-MotionFrame adapter is under `src/rehab/integration/`. `updateProductApplicationBridge()` feeds those live measurements into `RehabSystemOrchestrator`, which owns the report/game/digital-twin/offline-sync fanout after the hardware training path has released formal training.

## 7-inch display

The current approved home/calibration/training UI remains in `firmware/display/src/main.cpp`.
The complete 20-page product shell is compiled from `firmware/display/src/product_ui/`.

The home dashboard now exposes navigation into history, report, settings, prescription
and action library pages. Selecting an action on the action-library page sends
`SELECT_EXERCISE:<id>` to the controller. Starting from the plan page sends `START_FLOW`
and hands control back to the established calibration/training state machine.

The controller LIVE packet includes the selected exercise and train mode so both
processors use the same action vocabulary.

## Software clients

`software/miniapp/` contains the patient/family Mini Program UI shell for plan,
training, reports, device status and AI-assisted recommendation review.

`software/miniapp/pages-therapist/` contains the clinician dashboard shell for patient overview,
ROM trend review and plan confirmation/downlink.

`software/backend/` contains prescription, telemetry, report, recommendation and device
state services plus a FastAPI transport layer.
