# Source integration note

This repository combines the team's hardware firmware and software applications into one submission tree.

- `firmware/controller/` and `firmware/display/`: ESP32-S3 controller and 7-inch display firmware.
- `software/miniapp/` and `software/backend/`: the team's public MiniApp/backend submission source.
- `software/games/rehab_game_center/`: the newest integrated rehabilitation game-center source. It already contains the fruit game, so the older standalone fruit-game archive is intentionally not duplicated.
- `software/digital_twin/web/`: digital-twin visualization source.
- `hardware/`: reserved for team-created PCB/CAD/STL files.

Generated dependencies and build outputs (`node_modules`, `dist`, `.pio`, nested `.git`) are intentionally excluded because they are reproducible and should not be committed as project source.
