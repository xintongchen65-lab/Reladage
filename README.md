# Reladage 轻松绷

**基于多 IMU 姿态感知的居家智能康复训练系统**

Reladage is a multi-terminal rehabilitation prototype built around five wireless IMUs and an ESP32-S3 controller. The controller performs motion sensing, relative-joint evaluation and training-state management; a separate 7-inch ESP32-S3 display provides local interaction. The repository also contains the team's MiniApp/backend, rehabilitation games and digital-twin visualization.

## Repository map

```text
Reladage/
├─ firmware/
│  ├─ controller/RehabMotion_v5/     # ESP32-S3 main controller + 8-action engine
│  └─ display/                       # 7-inch LVGL display firmware
├─ software/
│  ├─ miniapp/                       # patient / family MiniApp
│  ├─ backend/                       # Fastify + TypeScript backend
│  ├─ games/rehab_game_center/       # fruit / penalty / vitality park / mole games
│  └─ digital_twin/web/              # digital-twin visualization
├─ hardware/
│  ├─ mechanical/                    # STL / STEP / wearable & enclosure CAD
│  └─ electronics/                   # PCB / schematic / BOM sources when available
├─ data/schemas/                     # normalized data schemas
├─ examples/                         # sample prescription/session/report data
├─ docs/                             # architecture, protocol, action and UI documentation
├─ tests/                            # host-side controller/module tests
├─ scripts/                          # repository validation scripts
└─ .github/workflows/                # GitHub Actions host checks
```

## Main entry points

### ESP32-S3 controller

- Entry: `firmware/controller/RehabMotion_v5/RehabMotion_v5.ino`
- Eight-action engine: `firmware/controller/RehabMotion_v5/src/rehab/motion_engine/exercises/`
- Runtime/orchestration: `firmware/controller/RehabMotion_v5/src/rehab/application/`
- Game-event mapping: `firmware/controller/RehabMotion_v5/src/rehab/games/`
- Digital-twin packet mapping: `firmware/controller/RehabMotion_v5/src/rehab/digital_twin/`

### 7-inch display

- Entry: `firmware/display/src/main.cpp`
- Product pages: `firmware/display/src/product_ui/rehab_product_pages.cpp`
- UI routing: `firmware/display/src/product_ui/rehab_ui_router.cpp`
- Controller UART link: `firmware/display/src/rehab_uart_link.cpp`

### MiniApp and backend

- MiniApp: `software/miniapp/`
- Backend: `software/backend/`
- Backend environment template: `software/backend/.env.example`

### Rehabilitation games

`software/games/rehab_game_center/` is the newest integrated game-center project. It includes:

- fruit-picking rehabilitation game;
- penalty-kick rehabilitation game;
- vitality-park sit-to-stand game;
- mole / box-squat game;
- shared MotionFrame/multiplayer/runtime modules.

The older standalone fruit-game archive is not duplicated here because its current functionality is already contained in the integrated game center.

### Digital twin

- Web entry: `software/digital_twin/web/src/main.js`
- Visual assets: `software/digital_twin/web/public/assets/`

## Eight-action library

The controller repository contains eight action detector modules:

1. Dumbbell curl
2. Triceps extension
3. Scaption raise
4. Wall crawl
5. Knee flexion / extension
6. Sit-to-stand
7. Box squat
8. Step-up

See `docs/08_EXERCISE_ALGORITHM_SPEC.md` and `docs/ACTION_LIBRARY.md`.

## Data flow

```text
5 wireless IMUs
      ↓ BLE
ESP32-S3 controller
      ↓
relative motion / calibration / action quality / state machine
      ├─ UART → 7-inch display
      ├─ local storage / offline recovery
      ├─ normalized motion events → rehabilitation games
      ├─ digital-twin motion packet
      └─ backend → MiniApp / reports / plan synchronization
```

## Host-side repository checks

On a machine with `g++` and Python 3:

```bash
bash scripts/validate_repo.sh
```

These checks cover the eight action modules, runtime/report/game/digital-twin mappings and controller/display/repository integration structure. Board-specific Arduino/PlatformIO builds remain hardware-environment dependent.

## Local dependencies and secrets

Generated dependencies and build output are intentionally not committed. Install dependencies inside each software project according to its own `package.json` / README.

Real Wi-Fi credentials, CloudBase keys, WeChat secrets and tokens must **not** be committed. Use `software/backend/.env.example` as the configuration template and keep real `.env*` files local.

## CAD / STL

The repository includes the team-designed WT9011DCL wearable enclosure/strap-holder iterations under `hardware/mechanical/wt9011dcl_wearable_mount/` (V1, revised V3 with editable OpenSCAD source and previews, plus a later V5 holder STL).

Team-designed STL/STEP files may be committed under `hardware/mechanical/`. GitHub accepts normal STL files and can be used as the single source repository for the mechanical design as well. Keep ordinary Git files under GitHub's per-file size limit; use Git LFS if a binary design file is unusually large.
