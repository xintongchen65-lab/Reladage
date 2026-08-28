import type { PenaltySessionConfig } from '../types/result'
import { configurePenaltySession, DEFAULT_PENALTY_CONFIG } from './session-runtime'
import { PENALTY_SOURCE_KIND } from '../data-sources/source-factory'
import { resolveDebugEnabled } from '../../game-platform/runtime/debug-gate'

function parse(value: string | undefined): number | undefined { const result = Number(value); return value && Number.isFinite(result) ? result : undefined }
export function resolvePenaltyDebug(requested: string | undefined): boolean {
  return resolveDebugEnabled(requested)
}
export function configurePenaltyFromQuery(query: Record<string, string | undefined>): PenaltySessionConfig {
  return configurePenaltySession({
    targetCount: parse(query.targetCount), targetSets: parse(query.targetSets), frameRateHz: parse(query.frameRateHz),
    targetAngleDeg: parse(query.targetAngleDeg), validAngleDeg: parse(query.validAngleDeg), returnAngleDeg: parse(query.returnAngleDeg),
    restDurationSec: parse(query.restDurationSec), dataTimeoutMs: parse(query.dataTimeoutMs), debugEnabled: resolvePenaltyDebug(query.debug),
    sourceKind: PENALTY_SOURCE_KIND
  })
}
export function penaltyLaunchUrl(debug = false): string {
  const c = DEFAULT_PENALTY_CONFIG
  return `/pages-penalty-game/prepare/index?targetCount=${c.targetCount}&targetSets=${c.targetSets}&frameRateHz=${c.frameRateHz}&targetAngleDeg=${c.targetAngleDeg}&validAngleDeg=${c.validAngleDeg}&returnAngleDeg=${c.returnAngleDeg}&restDurationSec=${c.restDurationSec}&dataTimeoutMs=${c.dataTimeoutMs}${debug ? '&debug=1' : ''}`
}
