import type { KneeMotionFrame } from '../types/motion'
export type {
  AngleControllableMotionDataSource,
  HeldControl,
  SessionAwareMotionDataSource
} from '../../game-platform/motion/data-source'
export { isAngleControllableSource, isSessionAwareSource } from '../../game-platform/motion/data-source'
export type PenaltyMotionDataSource = import('../../game-platform/motion/data-source').MotionDataSource<KneeMotionFrame>
export type PenaltyControllableDataSource = import('../../game-platform/motion/data-source').ControllableMotionDataSource<KneeMotionFrame>

export interface CycleSimulatableSource {
  simulateCompleteCycle(side: 'left' | 'right'): boolean
}
export function isCycleSimulatableSource(source: unknown): source is CycleSimulatableSource {
  return !!source && typeof (source as CycleSimulatableSource).simulateCompleteCycle === 'function'
}
