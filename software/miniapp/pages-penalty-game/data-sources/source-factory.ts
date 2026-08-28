import type { PenaltyMotionDataSource } from './contracts'
import { PenaltyFakeDataSource, type PenaltyFakeOptions } from './fake-data-source'

export type PenaltyDataSourceFactoryOptions = PenaltyFakeOptions
export const PENALTY_SOURCE_KIND: 'fake' | 'real' = 'fake'

export function createPenaltyMotionDataSource(options: PenaltyDataSourceFactoryOptions = {}): PenaltyMotionDataSource {
  return new PenaltyFakeDataSource(options)
}
