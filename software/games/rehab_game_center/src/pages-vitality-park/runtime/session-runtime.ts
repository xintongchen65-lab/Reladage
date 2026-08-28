import type { VitalitySessionConfig, VitalityTrainingResult } from '../types/result'

const DEFAULT_CONFIG: VitalitySessionConfig = { targetCount: 10, targetSets: 1, frameRateHz: 25, targetAngleDeg: 80, validAngleDeg: 60, returnAngleDeg: 20, restDurationSec: 30, dataTimeoutMs: 1000, debugEnabled: false, sourceKind: 'fake' }
let config: VitalitySessionConfig = { ...DEFAULT_CONFIG }
let result: VitalityTrainingResult | null = null
let published = false
let callerReturnUrl = '/pages-vitality-park/home/index'
let emitter: ((value: VitalityTrainingResult) => void) | null = null

export function configureVitalitySession(input: Partial<VitalitySessionConfig> = {}): VitalitySessionConfig { config = { ...DEFAULT_CONFIG, ...input, targetCount: Math.max(1, Math.round(Number(input.targetCount ?? DEFAULT_CONFIG.targetCount))), targetSets: Math.max(1, Math.round(Number(input.targetSets ?? DEFAULT_CONFIG.targetSets))) }; return { ...config } }
export function getVitalityConfig(): VitalitySessionConfig { return { ...config } }
export function setVitalityCallerReturnUrl(url: string): void { callerReturnUrl = url }
export function getVitalityCallerReturnUrl(): string { return callerReturnUrl }
export function publishVitalityResult(value: VitalityTrainingResult): VitalityTrainingResult { if (!result) result = value; if (!published) { published = true; emitter?.(result) }; return result }
export function consumeVitalityResult(): VitalityTrainingResult | null { const value = result; result = null; return value }
export function registerVitalityResultEmitter(value: ((result: VitalityTrainingResult) => void) | null): void { emitter = value }
export function prepareVitalityReplay(): VitalitySessionConfig { result = null; published = false; return { ...config } }
export function clearVitalitySession(): void { result = null; published = false; emitter = null; config = { ...DEFAULT_CONFIG }; callerReturnUrl = '/pages-vitality-park/home/index' }
