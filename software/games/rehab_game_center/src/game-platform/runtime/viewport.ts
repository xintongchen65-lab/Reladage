export interface ViewportLayout { safeTopPx: number }

export function getViewportLayout(): ViewportLayout {
  let safeTopPx = 8
  // #ifdef MP-WEIXIN
  try {
    const capsule = uni.getMenuButtonBoundingClientRect()
    if (capsule && Number.isFinite(capsule.bottom)) safeTopPx = Math.max(safeTopPx, capsule.bottom + 6)
  } catch { safeTopPx = 52 }
  // #endif
  return { safeTopPx }
}

export function viewportStyle(layout: ViewportLayout, cssVariable = '--rfg-safe-top'): Record<string, string> {
  return { [cssVariable]: `${Math.max(0, layout.safeTopPx)}px` }
}
