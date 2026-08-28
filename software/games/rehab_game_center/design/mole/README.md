# Mole Asset Production

## Locked Decisions

- Asset source: the four 1536x1024 sprite sheets supplied by the user on 2026-08-21.
- The sheets are deterministic edit targets. Runtime art is cropped from their original pixels without API generation or resolution upscaling.
- Example HUD, buttons and unrelated labels from the sheets are not used. The supplied high-resolution `地鼠大作战` wooden title sign is explicitly approved for runtime use.
- The final arena has exactly five holes: two in the back row and three in the front row.
- The environment, five normalized mole sprites, fixed hole, hammer and effects remain separate runtime assets.
- Supplied alpha is preserved and contracted locally to remove atlas-edge residue.

## Reference Roles

- `reference/supplied-primary-sheet.png`: primary five-hole scene, five character skins, title sign, hammer and effects.
- `reference/supplied-composition-sheet.png`: secondary clean hole and composition source.
- `reference/supplied-atlas-sheet.png` and `reference/supplied-animation-sheet.png`: supplemental supplied variants retained as source masters.
- The older `mole-style-*` and `composition-03.png` files remain historical references only.

## Runtime Build

Run `python scripts/process-mole-supplied-assets.py`. It exports the wide farm background, five identically sized 340x260 character canvases, fixed hole, hammer, title sign and effects into `src/pages-mole-game/static/`.

## Validation

- Inspect every exported asset at 100%. Reject soft, smeared, doubled, cropped, or inconsistent subjects.
- Reject white/black/color-key halos and neighboring-object contamination.
- Export all five mole sprites to one canvas size with the same bottom anchor.
- Keep at least 8 transparent pixels around opaque sprite bounds.
- Run `pnpm check:assets`, `pnpm check:boundary`, both WeChat builds, and `pnpm check:weixin-build` after replacement.

## Generation Status

Replacement is complete from the supplied source sheets. No `OPENAI_API_KEY` or generation step is required.
