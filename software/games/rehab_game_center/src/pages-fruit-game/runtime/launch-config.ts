import type { SessionConfig } from '../types/result'
import { configureSession, DEFAULT_SESSION_CONFIG } from './session-runtime'
export { isDebugBuild, resolveDebugEnabled } from '../../game-platform/runtime/debug-gate'
import { resolveDebugEnabled } from '../../game-platform/runtime/debug-gate'

export type LaunchQuery = Record<string, string | undefined>
function parseNumber(value: string | undefined): number | undefined {
  if (value === undefined || value.trim() === '') return undefined
  const parsed = Number(value)
  return Number.isFinite(parsed) ? parsed : undefined
}

export function configureSessionFromQuery(query: LaunchQuery): SessionConfig {
  return configureSession({
    targetCount: parseNumber(query.targetCount),
    targetSets: parseNumber(query.targetSets),
    frameRateHz: parseNumber(query.frameRateHz),
    targetAngleDeg: parseNumber(query.targetAngleDeg),
    validAngleDeg: parseNumber(query.validAngleDeg),
    returnAngleDeg: parseNumber(query.returnAngleDeg),
    restDurationSec: parseNumber(query.restDurationSec),
    dataTimeoutMs: parseNumber(query.dataTimeoutMs),
    debugEnabled: resolveDebugEnabled(query.debug)
  })
}

export function defaultLaunchUrl(debug = false): string {
  const config = DEFAULT_SESSION_CONFIG
  const params = [
    `targetCount=${config.targetCount}`,
    `targetSets=${config.targetSets}`,
    `frameRateHz=${config.frameRateHz}`,
    `targetAngleDeg=${config.targetAngleDeg}`,
    `validAngleDeg=${config.validAngleDeg}`,
    `returnAngleDeg=${config.returnAngleDeg}`,
    `restDurationSec=${config.restDurationSec}`,
    `dataTimeoutMs=${config.dataTimeoutMs}`
  ]
  if (debug) params.push('debug=1')
  return `/pages-fruit-game/prepare/index?${params.join('&')}`
}
