import type { TherapistSafeArea } from '../types'

type WindowInfo = {
  statusBarHeight?: number
  windowWidth?: number
  windowHeight?: number
  safeAreaInsets?: { top?: number; bottom?: number; left?: number; right?: number }
  safeArea?: { top?: number; bottom?: number; left?: number; right?: number }
}

type CapsuleRect = {
  top: number
  bottom: number
  left: number
  right: number
  width?: number
  height?: number
}

const DEFAULT_TAB_BAR = 62
const DEFAULT_INNER_HEIGHT = 34

function readWindowInfo(): WindowInfo {
  try {
    const api = uni as unknown as {
      getWindowInfo?: () => WindowInfo
      getSystemInfoSync?: () => WindowInfo
    }
    return api.getWindowInfo?.() || api.getSystemInfoSync?.() || {}
  } catch {
    return {}
  }
}

function readCapsule(): CapsuleRect | null {
  try {
    const api = uni as unknown as {
      getMenuButtonBoundingClientRect?: () => CapsuleRect
    }
    return api.getMenuButtonBoundingClientRect?.() || null
  } catch {
    return null
  }
}

export function getTherapistSafeArea(): TherapistSafeArea {
  const info = readWindowInfo()
  const capsule = readCapsule()
  const width = Math.max(1, Number(info.windowWidth || 375))
  const height = Math.max(1, Number(info.windowHeight || 667))
  const insets = info.safeAreaInsets || {}
  const safe = info.safeArea || {}
  const topInset = Number(insets.top ?? safe.top ?? info.statusBarHeight ?? 20)
  const bottomInset = Number(insets.bottom ?? (safe.bottom !== undefined ? Math.max(0, height - Number(safe.bottom)) : 0))
  const leftInset = Number(insets.left ?? safe.left ?? 0)
  const rightInset = Number(insets.right ?? (safe.right !== undefined ? Math.max(0, width - Number(safe.right)) : 0))
  const top = Math.max(20, capsule?.top || topInset + 6)
  const headerInnerHeight = Math.max(DEFAULT_INNER_HEIGHT, Number(capsule?.height || 0))
  const capsuleRight = capsule?.left ? Math.max(96, width - capsule.left + 10) : 18

  return {
    top,
    bottom: Math.max(0, bottomInset),
    left: Math.max(0, leftInset),
    right: Math.max(0, rightInset),
    headerHeight: top + headerInnerHeight + 10,
    headerInnerHeight,
    tabBarHeight: DEFAULT_TAB_BAR,
    capsuleRight
  }
}

export function getTherapistViewportStyle(area = getTherapistSafeArea()): Record<string, string> {
  return {
    '--therapist-safe-top': area.top + 'px',
    '--therapist-safe-bottom': area.bottom + 'px',
    '--therapist-safe-left': area.left + 'px',
    '--therapist-safe-right': area.right + 'px',
    '--therapist-header-height': area.headerHeight + 'px',
    '--therapist-header-inner-height': area.headerInnerHeight + 'px',
    '--therapist-tabbar-height': area.tabBarHeight + 'px',
    '--therapist-capsule-right': area.capsuleRight + 'px'
  }
}
