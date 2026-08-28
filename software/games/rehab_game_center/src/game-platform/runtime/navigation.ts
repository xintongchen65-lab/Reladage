export const DEFAULT_GAME_CENTER_URL = '/pages/game-center/index'

let callerReturnUrl = DEFAULT_GAME_CENTER_URL

export function normalizeReturnUrl(value: string | undefined, fallback = DEFAULT_GAME_CENTER_URL): string {
  if (!value) return fallback
  let candidate = value.trim()
  try { candidate = decodeURIComponent(candidate) } catch { return fallback }
  if (!/^\/(?!\/)[A-Za-z0-9_/-]+$/.test(candidate)) return fallback
  if (candidate.includes('..') || candidate.includes('://')) return fallback
  return candidate
}

export function configureCallerReturnUrl(value: string | undefined, fallback = DEFAULT_GAME_CENTER_URL): string {
  callerReturnUrl = normalizeReturnUrl(value, fallback)
  return callerReturnUrl
}

export function getCallerReturnUrl(): string { return callerReturnUrl }

export function appendReturnUrl(url: string, returnUrl = callerReturnUrl): string {
  const separator = url.includes('?') ? '&' : '?'
  return `${url}${separator}returnUrl=${encodeURIComponent(normalizeReturnUrl(returnUrl))}`
}

export interface ReturnToCallerOptions { delta?: number; returnUrl?: string }

export function returnToGameCenter(): void {
  uni.reLaunch({ url: DEFAULT_GAME_CENTER_URL })
}

export function returnToCaller(options: ReturnToCallerOptions = {}): void {
  const delta = Math.max(1, Math.round(options.delta ?? 1))
  const fallbackUrl = normalizeReturnUrl(options.returnUrl ?? callerReturnUrl)
  const fallback = () => uni.reLaunch({ url: fallbackUrl })
  let count = 0
  try { count = getCurrentPages().length } catch { fallback(); return }
  if (count <= delta) { fallback(); return }
  uni.navigateBack({ delta, fail: fallback })
}

export function resetNavigationRuntime(): void { callerReturnUrl = DEFAULT_GAME_CENTER_URL }
