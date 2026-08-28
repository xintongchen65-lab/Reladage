from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

controller_path = ROOT / 'firmware/controller/RehabMotion_v5/RehabMotion_v5.ino'
action_catalog_path = ROOT / 'firmware/controller/RehabMotion_v5/rehab_v5_action_catalog.h'
display_main_path = ROOT / 'firmware/display/src/main.cpp'
uart_path = ROOT / 'firmware/display/src/rehab_uart_link.cpp'
pages_h_path = ROOT / 'firmware/display/src/product_ui/rehab_product_pages.h'
pages_cpp_path = ROOT / 'firmware/display/src/product_ui/rehab_product_pages.cpp'
orchestrator_h_path = ROOT / 'firmware/controller/RehabMotion_v5/src/rehab/application/rehab_system_orchestrator.h'
orchestrator_cpp_path = ROOT / 'firmware/controller/RehabMotion_v5/src/rehab/application/rehab_system_orchestrator.cpp'
exercise_dir = ROOT / 'firmware/controller/RehabMotion_v5/src/rehab/motion_engine/exercises'

controller = controller_path.read_text(encoding='utf-8', errors='ignore')
action_catalog = action_catalog_path.read_text(encoding='utf-8', errors='ignore')
display_main = display_main_path.read_text(encoding='utf-8', errors='ignore')
uart = uart_path.read_text(encoding='utf-8', errors='ignore')
pages_h = pages_h_path.read_text(encoding='utf-8', errors='ignore')
pages = pages_cpp_path.read_text(encoding='utf-8', errors='ignore')
orchestrator_h = orchestrator_h_path.read_text(encoding='utf-8', errors='ignore')
orchestrator_cpp = orchestrator_cpp_path.read_text(encoding='utf-8', errors='ignore')

exercise_sources = [
    'dumbbell_curl.cpp',
    'triceps_extension.cpp',
    'scaption_raise.cpp',
    'wall_crawl.cpp',
    'knee_flex_extend.cpp',
    'sit_to_stand.cpp',
    'box_squat.cpp',
    'step_up.cpp',
]

checks = {
    'eight-action catalog':
        action_catalog.count('{"') >= 8
        and 'RM_ACTION_COUNT = 8' in action_catalog,

    'all eight detector sources':
        all(
            (exercise_dir / name).exists()
            and (exercise_dir / name).stat().st_size > 1800
            for name in exercise_sources
        ),

    'controller selection command':
        'SELECT_EXERCISE:' in controller,

    'controller action range':
        'exerciseIndex >= RM_ACTION_COUNT' in controller,

    'controller eight-action selector':
        '选择动作 · 8项训练库' in controller
        and '8 台阶踩踏' in controller,

    'controller eight-action start-pose switch':
        'START_POSE_CHECK_8ACTION' in controller
        and 'case RM_ACTION_STEP_UP' in controller,

    'controller per-action quality vocabulary':
        'STEP_UP_ROM_TORSO_ASYMMETRY_CADENCE' in controller,

    'controller product orchestrator wired':
        'updateProductApplicationBridge();' in controller
        and 'productOrchestrator.update(snap)' in controller,

    'controller product UI commands':
        all(
            cmd in controller
            for cmd in [
                'CALIBRATION_CONTINUE',
                'RETRY',
                'SYNC_NOW',
            ]
        ),

    'controller live action field':
        'durationSec,\n    exerciseIndex,\n    modeIndex' in controller,

    'display parser action field':
        'g_snap.exerciseIndex' in uart
        and 'long vals[19]' in uart,

    'product UI compiled from src':
        'product_ui/rehab_ui_router.h' in display_main,

    'product router initialized':
        'rehab_ui_router_init(lv_scr_act())' in display_main,

    'exercise selector sends controller command':
        'product_select_exercise' in pages
        and 'SELECT_EXERCISE:%d' in pages,

    'game mode uses controller vocabulary':
        'SELECT_MODE:1' in pages
        and 'SET_MODE:1' not in pages,

    'orchestrator owns product fanout':
        all(
            token in (orchestrator_h + orchestrator_cpp)
            for token in [
                'makeTwinPacket',
                'mapFeedbackToGame',
                'SessionAccumulator',
                'offlineQueue_',
            ]
        ),

    'orchestrator public integration API':
        all(
            token in orchestrator_h
            for token in [
                'loadPrescription',
                'selectExercise',
                'setConnectivity',
                'pendingSyncCount',
            ]
        ),
}

page_names = [
    'RP_HOME',
    'RP_EXERCISE_LIBRARY',
    'RP_PLAN_DETAIL',
    'RP_PRECHECK',
    'RP_WEAR_GUIDE',
    'RP_BODY_POSITION',
    'RP_MOTION_CALIBRATION',
    'RP_LIVE_TRAINING',
    'RP_REST',
    'RP_SESSION_RESULT',
    'RP_DIGITAL_TWIN',
    'RP_GAME_HUB',
    'RP_HISTORY',
    'RP_REPORT',
    'RP_AI_COACH',
    'RP_DEVICE_CENTER',
    'RP_SETTINGS',
    'RP_PRESCRIPTION_SYNC',
    'RP_OFFLINE_SYNC',
    'RP_ABOUT',
]

checks['20 product pages'] = (
    all(name in pages_h for name in page_names)
    and 'RP_PAGE_COUNT' in pages_h
)


# ------------------------------------------------------------
# Repository hygiene checks
#
# GitHub Actions / normal Git repositories necessarily contain
# ROOT/.git. That root metadata must be allowed.
#
# What we actually want to reject is:
#   1. bundled node_modules directories
#   2. nested .git repositories inside project subdirectories
# ------------------------------------------------------------

bundled_node_modules = list(ROOT.rglob('node_modules'))

nested_git_metadata = [
    path
    for path in ROOT.rglob('.git')
    if path != ROOT / '.git'
]


software_checks = {
    'team miniapp present':
        (ROOT / 'software/miniapp/App.vue').exists()
        and (ROOT / 'software/miniapp/pages.json').exists(),

    'Fastify TypeScript backend present':
        (ROOT / 'software/backend/package.json').exists()
        and (ROOT / 'software/backend/src/server.ts').exists(),

    'integrated rehab game center present':
        (ROOT / 'software/games/rehab_game_center/src/pages-fruit-game').exists()
        and (ROOT / 'software/games/rehab_game_center/src/pages-penalty-game').exists()
        and (ROOT / 'software/games/rehab_game_center/src/pages-vitality-park').exists()
        and (ROOT / 'software/games/rehab_game_center/src/pages-mole-game').exists(),

    'digital twin web present':
        (ROOT / 'software/digital_twin/web/src/main.js').exists(),

    'no bundled dependencies':
        not bundled_node_modules
        and not nested_git_metadata,
}

checks.update(software_checks)


failed = [name for name, ok in checks.items() if not ok]

for name, ok in checks.items():
    print(f"[{'OK' if ok else 'FAIL'}] {name}")

if failed:
    raise SystemExit(
        'integration checks failed: ' + ', '.join(failed)
    )

print('RehabMotion repository integration checks passed.')
