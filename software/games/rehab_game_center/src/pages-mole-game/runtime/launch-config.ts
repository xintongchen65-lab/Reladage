import { resolveDebugEnabled } from '../../game-platform/runtime/debug-gate'
import type { MoleSessionConfig } from '../types/result'
import { configureMoleSession, getMoleConfig } from './session-runtime'

function numberParam(value: string | undefined): number | undefined {
  if (value === undefined || value.trim() === '') return undefined
  const parsed = Number(value)
  return Number.isFinite(parsed) ? parsed : undefined
}

export function configureMoleFromQuery(query: Record<string, string | undefined>): MoleSessionConfig {
  const base = getMoleConfig()
  return configureMoleSession({
    ...base,
    targetCount: numberParam(query.targetCount),
    targetSets: numberParam(query.targetSets),
    frameRateHz: numberParam(query.frameRateHz),
    targetAngleDeg: numberParam(query.targetAngleDeg),
    validAngleDeg: numberParam(query.validAngleDeg),
    returnAngleDeg: numberParam(query.returnAngleDeg),
    warningWindowMs: numberParam(query.warningWindowMs),
    debugEnabled: resolveDebugEnabled(query.debug),
    sourceKind: query.source === 'real' ? 'real' : 'fake'
  })
}
