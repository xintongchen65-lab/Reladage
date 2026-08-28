import { MoleMotionFrameAdapter, type MoleRejectReason } from '../core/motion-adapter'
import type { SquatMotionFrame } from '../types/motion'
import type { MoleMotionDataSource } from './contracts'

export interface MoleRealTransport {
  start?(): void
  stop?(): void
  subscribe(listener: (payload: unknown) => void): () => void
}

export class MoleRealDataSource implements MoleMotionDataSource {
  private listeners = new Set<(frame: SquatMotionFrame) => void>()
  private adapter = new MoleMotionFrameAdapter()
  private unsubscribe: (() => void) | null = null
  constructor(private readonly transport: MoleRealTransport, private readonly onRejected?: (reason: MoleRejectReason) => void) {}
  start(): void {
    if (this.unsubscribe) return
    this.unsubscribe = this.transport.subscribe((payload) => {
      const result = this.adapter.ingest(payload)
      if (!result.accepted) { this.onRejected?.(result.reason); return }
      this.listeners.forEach((listener) => listener({ ...result.frame }))
    })
    this.transport.start?.()
  }
  stop(): void { this.unsubscribe?.(); this.unsubscribe = null; this.transport.stop?.() }
  reset(): void { this.adapter.reset() }
  subscribe(listener: (frame: SquatMotionFrame) => void): () => void { this.listeners.add(listener); return () => this.listeners.delete(listener) }
}
