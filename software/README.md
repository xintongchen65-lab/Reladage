# Software applications

| Module | Path | Stack / role |
|---|---|---|
| Patient/family MiniApp | `miniapp/` | uni-app / Vue, training plan, device, report, AI and game entry |
| Backend service | `backend/` | Fastify + TypeScript, authentication, device/plan/session services |
| Rehabilitation game center | `games/rehab_game_center/` | uni-app Vue 3 + TypeScript; fruit, penalty, vitality-park and mole games |
| Digital twin | `digital_twin/web/` | Web visualization of motion / error states |

The game center consumes normalized rehabilitation motion events instead of replacing the controller-side rehabilitation algorithm.
