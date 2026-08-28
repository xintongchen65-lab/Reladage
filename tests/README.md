# Repository checks

Run everything with:

```bash
bash scripts/validate_repo.sh
```

The check suite covers:

- all eight action profiles and detector state machines;
- MotionFrame telemetry JSON;
- prescription/runtime progression;
- session report accumulation;
- game-event mapping;
- digital-twin packet mapping;
- RehabMotion_v5 → MotionFrame adapter;
- team Fastify/TypeScript backend source structure;
- controller/display integration wiring, 8-action command vocabulary and the 20-page UI router.

Board-specific Arduino and PlatformIO builds remain separate from these host-side checks.
