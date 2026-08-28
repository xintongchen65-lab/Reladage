import type { MotionDataSource, MotionFrameListener } from './contracts'

export class RealDataSourceStub implements MotionDataSource {
  private readonly listeners = new Set<MotionFrameListener>()

  start(): void {
    throw new Error(
      'RealDataSource 尚未接入。请在 data-sources/source-factory.ts 中装配主项目动作数据服务。'
    )
  }

  stop(): void {}

  reset(): void {}

  subscribe(listener: MotionFrameListener): () => void {
    this.listeners.add(listener)
    return () => this.listeners.delete(listener)
  }
}
