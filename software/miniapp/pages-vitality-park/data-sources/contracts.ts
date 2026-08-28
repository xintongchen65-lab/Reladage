import type { VitalityMotionFrame } from '../types/motion'
export type {
  AngleControllableMotionDataSource,
  HeldControl,
  SessionAwareMotionDataSource
} from '../../game-platform/motion/data-source'
export { isAngleControllableSource, isSessionAwareSource } from '../../game-platform/motion/data-source'
export type VitalityMotionDataSource = import('../../game-platform/motion/data-source').MotionDataSource<VitalityMotionFrame>
export type VitalityControllableDataSource = import('../../game-platform/motion/data-source').ControllableMotionDataSource<VitalityMotionFrame>

export interface CycleSimulatableSource { simulateCompleteCycle(): boolean }
export function isCycleSimulatableSource(source: unknown): source is CycleSimulatableSource {
  return !!source && typeof (source as CycleSimulatableSource).simulateCompleteCycle === 'function'
}
