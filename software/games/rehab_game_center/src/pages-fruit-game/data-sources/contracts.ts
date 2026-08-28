import type { MotionFrame } from '../types/motion'

export type MotionFrameListener = (frame: MotionFrame) => void

export interface MotionDataSource {
  start(): void
  stop(): void
  reset(): void
  subscribe(listener: MotionFrameListener): () => void
}

export type HeldControl = 'leftFlex' | 'leftExtend' | 'rightFlex' | 'rightExtend'

export interface AngleControllableMotionDataSource extends MotionDataSource {
  setHeldControl(control: HeldControl, pressed: boolean): void
}

export interface SessionAwareMotionDataSource extends MotionDataSource {
  setPaused(paused: boolean): void
  resetRepCycleDetectors(): void
}

export interface ControllableMotionDataSource
  extends AngleControllableMotionDataSource,
    SessionAwareMotionDataSource {}

export function isAngleControllableSource(
  source: MotionDataSource
): source is AngleControllableMotionDataSource {
  return 'setHeldControl' in source
}

export function isSessionAwareSource(
  source: MotionDataSource
): source is SessionAwareMotionDataSource {
  return 'setPaused' in source && 'resetRepCycleDetectors' in source
}
