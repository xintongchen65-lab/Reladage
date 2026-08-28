import type { MotionDataSource } from '../../game-platform/motion/data-source'
import type { VitalityMotionFrame } from '../types/motion'
import { VitalityFakeDataSource, type VitalityFakeOptions } from './fake-data-source'
import { VitalityRealDataSource, type VitalityRealTransport } from './real-data-source'

export interface VitalitySourceFactoryOptions extends VitalityFakeOptions { sourceKind?: 'fake' | 'real'; transport?: VitalityRealTransport; onRejected?: (reason: string) => void }
export function createVitalityDataSource(options: VitalitySourceFactoryOptions = {}): MotionDataSource<VitalityMotionFrame> {
  if (options.sourceKind === 'real' && options.transport) return new VitalityRealDataSource(options.transport, options.onRejected as any)
  return new VitalityFakeDataSource(options)
}
