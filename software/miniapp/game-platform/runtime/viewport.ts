export interface ViewportLayout {
  safeTopPx: number
  safeRightPx: number
  safeBottomPx: number
  safeLeftPx: number
  widthPx: number
  heightPx: number
  stageWidthPx: number
  stageHeightPx: number
  stageScale: number
}

export const GAME_DESIGN_WIDTH = 1280
export const GAME_DESIGN_HEIGHT = 720

const finite = (value: unknown, fallback: number): number => {
  const numeric = Number(value)
  return Number.isFinite(numeric) && numeric >= 0 ? numeric : fallback
}

export function getViewportLayout(): ViewportLayout {
  let widthPx = 844
  let heightPx = 390
  let safeTopPx = 8
  let safeRightPx = 12
  let safeBottomPx = 8
  let safeLeftPx = 12
  let stageWidthPx = Math.min(widthPx, heightPx * GAME_DESIGN_WIDTH / GAME_DESIGN_HEIGHT)
  let stageHeightPx = stageWidthPx * GAME_DESIGN_HEIGHT / GAME_DESIGN_WIDTH
  try {
    const info = (uni as any).getWindowInfo?.() ?? (uni as any).getSystemInfoSync?.() ?? {}
    widthPx = Math.max(1, finite(info.windowWidth, widthPx))
    heightPx = Math.max(1, finite(info.windowHeight, heightPx))
    stageWidthPx = Math.min(widthPx, heightPx * GAME_DESIGN_WIDTH / GAME_DESIGN_HEIGHT)
    stageHeightPx = stageWidthPx * GAME_DESIGN_HEIGHT / GAME_DESIGN_WIDTH
    const stageLeft = (widthPx - stageWidthPx) / 2
    const stageTop = (heightPx - stageHeightPx) / 2
    const stageRight = stageLeft + stageWidthPx
    const stageBottom = stageTop + stageHeightPx
    const safe = info.safeArea ?? {}
    safeTopPx = Math.max(8, finite(safe.top, stageTop) - stageTop + 6)
    safeLeftPx = Math.max(12, finite(safe.left, stageLeft) - stageLeft + 6)
    safeRightPx = Math.max(12, stageRight - finite(safe.right, stageRight) + 6)
    safeBottomPx = Math.max(8, stageBottom - finite(safe.bottom, stageBottom) + 6)
    // #ifdef MP-WEIXIN
    const capsule = (uni as any).getMenuButtonBoundingClientRect?.()
    if (capsule && Number.isFinite(capsule.left) && Number.isFinite(capsule.right)) {
      const overlapsStage = capsule.left < stageRight && capsule.right > stageLeft
      if (overlapsStage && widthPx > heightPx) safeRightPx = Math.max(safeRightPx, stageRight - capsule.left + 8)
      else if (overlapsStage && Number.isFinite(capsule.bottom)) safeTopPx = Math.max(safeTopPx, capsule.bottom - stageTop + 6)
    }
    // #endif
  } catch {}
  const stageScale = Math.max(0.001, Math.min(stageWidthPx / GAME_DESIGN_WIDTH, stageHeightPx / GAME_DESIGN_HEIGHT))
  return { safeTopPx, safeRightPx, safeBottomPx, safeLeftPx, widthPx, heightPx, stageWidthPx, stageHeightPx, stageScale }
}

export function viewportStyle(layout: ViewportLayout, cssVariable = '--rfg-safe-top'): Record<string, string> {
  return {
    [cssVariable]: `${Math.max(0, layout.safeTopPx)}px`,
    '--game-safe-right': `${Math.max(0, layout.safeRightPx)}px`,
    '--game-safe-bottom': `${Math.max(0, layout.safeBottomPx)}px`,
    '--game-safe-left': `${Math.max(0, layout.safeLeftPx)}px`
  }
}
export function gameStageStyle(layout: ViewportLayout, cssVariable = '--rfg-safe-top'): Record<string, string> {
  const scale = Math.max(0.001, layout.stageScale)
  return {
    width: `${GAME_DESIGN_WIDTH}px`,
    height: `${GAME_DESIGN_HEIGHT}px`,
    transform: `scale(${scale})`,
    'transform-origin': 'center center',
    [cssVariable]: `${Math.max(0, layout.safeTopPx / scale)}px`,
    '--game-safe-right': `${Math.max(0, layout.safeRightPx / scale)}px`,
    '--game-safe-bottom': `${Math.max(0, layout.safeBottomPx / scale)}px`,
    '--game-safe-left': `${Math.max(0, layout.safeLeftPx / scale)}px`,
    '--game-stage-scale': `${scale}`
  }
}