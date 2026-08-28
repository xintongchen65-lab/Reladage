# WT9011DCL wearable enclosure & rehabilitation mount

This directory records the mechanical iterations of the wearable mount used with the WT9011DCL-BT50 IMU node.

## Version history

| Version | Status | Included files | Notes |
|---|---|---|---|
| `V1/` | Early prototype | Bottom, lid and holder STL files + editable OpenSCAD source + design notes | First complete wearable enclosure/strap-holder iteration. |
| `V3/` | Revised complete set | Bottom, lid and holder STL files + editable OpenSCAD source + assembly/holder previews + print-feedback notes | Revised after physical trial-print feedback; includes lid-fit, indicator opening and external holder-claw changes. |
| `V5/` | Later holder iteration | `WT9011DCL_holder_V5.stl` | Later standalone holder geometry supplied as STL. Editable CAD source was not included with this iteration. |

The version directories are intentionally retained to document the team's physical-design iteration instead of presenting unrelated duplicate files.

## V5 geometry check

The supplied V5 holder is a valid binary STL containing 2,564 triangles. Its bounding dimensions are approximately **58.0 × 57.4 × 21.9 mm** (STL coordinate units, designed as millimetres in this project).

## Printing / source notes

- The V1 and V3 packages contain `.scad` files, so their dimensions and clearances remain parameter-editable.
- V3's own design notes recommend trial fitting before batch printing because FDM shrinkage and printer tolerances vary.
- STL files are printable exports; the `.scad` files are the editable source for the corresponding complete versions.
- Generated slicer caches and G-code are intentionally excluded from the repository.
