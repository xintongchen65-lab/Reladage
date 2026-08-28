import type { MotionDataSource } from './contracts'
import { FakeDataSource } from './fake-data-source'

export interface DataSourceFactoryOptions {
  targetCount?: number
  targetSets?: number
  frameRateHz?: number
  targetAngleDeg?: number
  validAngleDeg?: number
  returnAngleDeg?: number
  restDurationSec?: number
}

export function createMotionDataSource(options: DataSourceFactoryOptions = {}): MotionDataSource {
  // 正式迁移只在这里装配 RealDataSource；正式参数由v3帧提供，不由页面覆盖。
  return new FakeDataSource(options)
}
