import { MotionFrameAdapter } from '../core/motion-adapter'
import type { RejectReason } from '../core/motion-adapter'
import type { MotionDataSource, MotionFrameListener } from './contracts'

/** 主项目 BLE/Wi-Fi/后端层只需提供这一条原始消息订阅接口。 */
export interface RealMotionTransport {
  start?(): void
  stop?(): void
  subscribe(listener: (payload: unknown) => void): () => void
}

export interface RealDataSourceOptions {
  onRejectedFrame?: (reason: RejectReason) => void
}

/**
 * v3 真实数据源的协议适配层。它只校验并转发主控已经处理好的 JSON，
 * 不解算 IMU、不判定动作，也不对 5 Hz 输入补造事件。
 */
export class RealDataSource implements MotionDataSource {
  private readonly listeners = new Set<MotionFrameListener>()
  private readonly adapter = new MotionFrameAdapter()
  private unsubscribeTransport: (() => void) | null = null
  private started = false

  constructor(
    private readonly transport: RealMotionTransport,
    private readonly options: RealDataSourceOptions = {}
  ) {}

  start(): void {
    if (this.started) return
    this.started = true
    this.unsubscribeTransport = this.transport.subscribe((payload) => {
      const result = this.adapter.ingest(payload)
      if (!result.accepted) {
        this.options.onRejectedFrame?.(result.reason)
        return
      }
      this.listeners.forEach((listener) => listener({ ...result.frame }))
    })
    this.transport.start?.()
  }

  stop(): void {
    if (!this.started) return
    this.started = false
    this.unsubscribeTransport?.()
    this.unsubscribeTransport = null
    this.transport.stop?.()
  }

  reset(): void {
    this.adapter.reset()
  }

  subscribe(listener: MotionFrameListener): () => void {
    this.listeners.add(listener)
    return () => this.listeners.delete(listener)
  }
}
