import type { PenaltySessionConfig, PenaltyTrainingResult } from '../types/result'

export const DEFAULT_PENALTY_CONFIG: PenaltySessionConfig = {
  targetCount: 10, targetSets: 3, frameRateHz: 25, targetAngleDeg: 80, validAngleDeg: 60,
  returnAngleDeg: 20, restDurationSec: 30, dataTimeoutMs: 1000, debugEnabled: false, sourceKind: 'fake'
}

let config = { ...DEFAULT_PENALTY_CONFIG }
let result: PenaltyTrainingResult | null = null
let published = false
let emitter: ((value: PenaltyTrainingResult) => void) | null = null

function bounded(value: number | undefined, fallback: number, min: number, max: number): number {
  return Number.isFinite(value) && Number(value) >= min && Number(value) <= max ? Number(value) : fallback
}
export function configurePenaltySession(value: Partial<PenaltySessionConfig>): PenaltySessionConfig {
  const targetAngleDeg = bounded(value.targetAngleDeg, 80, 30, 120)
  let validAngleDeg = bounded(value.validAngleDeg, 60, 10, 110)
  let returnAngleDeg = bounded(value.returnAngleDeg, 20, 0, 60)
  if (!(returnAngleDeg < validAngleDeg && validAngleDeg <= targetAngleDeg)) { validAngleDeg = 60; returnAngleDeg = 20 }
  config = {
    targetCount: Math.round(bounded(value.targetCount, 10, 1, 99)), targetSets: Math.round(bounded(value.targetSets, 3, 1, 5)),
    frameRateHz: bounded(value.frameRateHz, 25, 5, 50), targetAngleDeg, validAngleDeg, returnAngleDeg,
    restDurationSec: Math.round(bounded(value.restDurationSec, 30, 0, 120)), dataTimeoutMs: Math.round(bounded(value.dataTimeoutMs, 1000, 500, 5000)),
    debugEnabled: value.debugEnabled === true, sourceKind: value.sourceKind === 'real' ? 'real' : 'fake'
  }
  result = null; published = false
  return { ...config }
}
export function getPenaltyConfig(): PenaltySessionConfig { return { ...config } }
export function preparePenaltyReplay(): PenaltySessionConfig { result = null; published = false; return { ...config } }
export function registerPenaltyResultEmitter(value: ((result: PenaltyTrainingResult) => void) | null): void { emitter = value }
export function publishPenaltyResult(value: PenaltyTrainingResult): boolean { if (published) return false; published = true; result = value; emitter?.(value); return true }
export function consumePenaltyResult(): PenaltyTrainingResult | null { const value = result; result = null; return value }
