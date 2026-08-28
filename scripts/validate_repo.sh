#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TMP="${TMPDIR:-/tmp}/rehabmotion_repo_test"
rm -rf "$TMP" && mkdir -p "$TMP"
CORE="$ROOT/firmware/controller/RehabMotion_v5/src/rehab/motion_engine/core"
EX="$ROOT/firmware/controller/RehabMotion_v5/src/rehab/motion_engine/exercises"
COMMON=(
  "$CORE/exercise_catalog.cpp"
  "$CORE/exercise_detector.cpp"
  "$CORE/exercise_engine.cpp"
  "$CORE/telemetry_schema.cpp"
  "$CORE/exercise_calibration.cpp"
  "$CORE/exercise_semantics.cpp"
  "$CORE/motion_feature_pipeline.cpp"
  "$EX/dumbbell_curl.cpp"
  "$EX/triceps_extension.cpp"
  "$EX/scaption_raise.cpp"
  "$EX/wall_crawl.cpp"
  "$EX/knee_flex_extend.cpp"
  "$EX/sit_to_stand.cpp"
  "$EX/box_squat.cpp"
  "$EX/step_up.cpp"
)

g++ -std=c++17 -Wall -Wextra -pedantic -I"$CORE" -I"$EX" \
  "$ROOT/tests/test_action_library.cpp" "${COMMON[@]}" -o "$TMP/action_library"
"$TMP/action_library"

g++ -std=c++17 -Wall -Wextra -pedantic -I"$CORE" -I"$EX" \
  "$ROOT/tests/test_system_modules.cpp" "${COMMON[@]}" \
  "$ROOT/firmware/controller/RehabMotion_v5/src/rehab/application/rehab_runtime.cpp" \
  "$ROOT/firmware/controller/RehabMotion_v5/src/rehab/protocol/rehab_protocol.cpp" \
  "$ROOT/firmware/controller/RehabMotion_v5/src/rehab/storage/session_report.cpp" \
  "$ROOT/firmware/controller/RehabMotion_v5/src/rehab/games/game_mapping.cpp" \
  "$ROOT/firmware/controller/RehabMotion_v5/src/rehab/digital_twin/motionframe_packet.cpp" \
  "$ROOT/firmware/controller/RehabMotion_v5/src/rehab/integration/v5_motion_adapter.cpp" \
  "$ROOT/firmware/controller/RehabMotion_v5/src/rehab/application/rehab_system_orchestrator.cpp" \
  -o "$TMP/system_modules"
"$TMP/system_modules"

echo "RehabMotion host checks passed."

python3 "$ROOT/scripts/validate_integration.py"
