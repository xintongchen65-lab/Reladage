import type { BaseMotionFrame } from './types'

export type MotionFrameListener<TFrame extends BaseMotionFrame> = (frame: TFrame) => void

export interface MotionDataSource<TFrame extends BaseMotionFrame = BaseMotionFrame> {
  start(): void
  stop(): void
  reset(): void
  subscribe(listener: MotionFrameListener<TFrame>): () => void
}

export type HeldControl = 'leftFlex' | 'leftExtend' | 'rightFlex' | 'rightExtend'

export interface AngleControllableMotionDataSource<TFrame extends BaseMotionFrame = BaseMotionFrame>
  extends MotionDataSource<TFrame> {
  setHeldControl(control: HeldControl, pressed: boolean): void
}

export interface SessionAwareMotionDataSource<TFrame extends BaseMotionFrame = BaseMotionFrame>
  extends MotionDataSource<TFrame> {
  setPaused(paused: boolean): void
  resetRepCycleDetectors(): void
}

export interface ControllableMotionDataSource<TFrame extends BaseMotionFrame = BaseMotionFrame>
  extends AngleControllableMotionDataSource<TFrame>, SessionAwareMotionDataSource<TFrame> {}

export function isAngleControllableSource<TFrame extends BaseMotionFrame>(
  source: MotionDataSource<TFrame>
): source is AngleControllableMotionDataSource<TFrame> {
  return 'setHeldControl' in source
}

export function isSessionAwareSource<TFrame extends BaseMotionFrame>(
  source: MotionDataSource<TFrame>
): source is SessionAwareMotionDataSource<TFrame> {
  return 'setPaused' in source && 'resetRepCycleDetectors' in source
}
