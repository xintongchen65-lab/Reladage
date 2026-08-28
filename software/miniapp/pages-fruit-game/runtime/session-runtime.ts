import type { SessionConfig, TrainingResult } from '../types/result'

export const DEFAULT_SESSION_CONFIG: SessionConfig = {
  targetCount: 10,
  targetSets: 3,
  frameRateHz: 25,
  targetAngleDeg: 80,
  validAngleDeg: 60,
  returnAngleDeg: 20,
  restDurationSec: 30,
  dataTimeoutMs: 1000,
  debugEnabled: false
}

let sessionConfig: SessionConfig = { ...DEFAULT_SESSION_CONFIG }
let pendingResult: TrainingResult | null = null
let resultPublished = false
let resultEmitter: ((result: TrainingResult) => void) | null = null

function boundedNumber(value: number | undefined, fallback: number, min: number, max: number): number {
  if (!Number.isFinite(value)) return fallback
  const numericValue = Number(value)
  return numericValue >= min && numericValue <= max ? numericValue : fallback
}

export function configureSession(config: Partial<SessionConfig>): SessionConfig {
  let targetAngleDeg = boundedNumber(config.targetAngleDeg, DEFAULT_SESSION_CONFIG.targetAngleDeg, 30, 120)
  let validAngleDeg = boundedNumber(config.validAngleDeg, DEFAULT_SESSION_CONFIG.validAngleDeg, 10, 110)
  let returnAngleDeg = boundedNumber(config.returnAngleDeg, DEFAULT_SESSION_CONFIG.returnAngleDeg, 0, 60)
  if (!(returnAngleDeg < validAngleDeg && validAngleDeg <= targetAngleDeg)) {
    targetAngleDeg = DEFAULT_SESSION_CONFIG.targetAngleDeg
    validAngleDeg = DEFAULT_SESSION_CONFIG.validAngleDeg
    returnAngleDeg = DEFAULT_SESSION_CONFIG.returnAngleDeg
  }
  sessionConfig = {
    targetCount: Math.round(boundedNumber(config.targetCount, DEFAULT_SESSION_CONFIG.targetCount, 1, 99)),
    targetSets: Math.round(boundedNumber(config.targetSets, DEFAULT_SESSION_CONFIG.targetSets, 1, 5)),
    frameRateHz: boundedNumber(config.frameRateHz, DEFAULT_SESSION_CONFIG.frameRateHz, 5, 50),
    targetAngleDeg,
    validAngleDeg,
    returnAngleDeg,
    restDurationSec: Math.round(boundedNumber(config.restDurationSec, DEFAULT_SESSION_CONFIG.restDurationSec, 0, 120)),
    dataTimeoutMs: Math.round(boundedNumber(config.dataTimeoutMs, DEFAULT_SESSION_CONFIG.dataTimeoutMs, 500, 5000)),
    debugEnabled: config.debugEnabled === true
  }
  pendingResult = null
  resultPublished = false
  return { ...sessionConfig }
}

export function getSessionConfig(): SessionConfig { return { ...sessionConfig } }
export function prepareReplaySession(): SessionConfig { pendingResult = null; resultPublished = false; return { ...sessionConfig } }
export function registerResultEmitter(emitter: ((result: TrainingResult) => void) | null): void { resultEmitter = emitter }
export function publishResult(result: TrainingResult): boolean {
  if (resultPublished) return false
  resultPublished = true
  pendingResult = result
  resultEmitter?.(result)
  return true
}
export function consumeResult(): TrainingResult | null { const result = pendingResult; pendingResult = null; return result }
export function clearSessionRuntime(): void {
  sessionConfig = { ...DEFAULT_SESSION_CONFIG }
  pendingResult = null
  resultPublished = false
  resultEmitter = null
}
