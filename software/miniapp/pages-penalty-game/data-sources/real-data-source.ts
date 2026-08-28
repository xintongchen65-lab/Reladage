import type { PenaltyMotionDataSource } from './contracts'
import type { KneeMotionFrame } from '../types/motion'
import { KneeMotionFrameAdapter, type PenaltyRejectReason } from '../core/motion-adapter'

export interface PenaltyRealTransport {
  start?(): void
  stop?(): void
  subscribe(listener: (payload: unknown) => void): () => void
}

export class PenaltyRealDataSource implements PenaltyMotionDataSource {
  private listeners = new Set<(frame: KneeMotionFrame) => void>()
  private adapter = new KneeMotionFrameAdapter()
  private unsubscribe: (() => void) | null = null
  constructor(private readonly transport: PenaltyRealTransport, private readonly onRejected?: (reason: PenaltyRejectReason) => void) {}
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
  subscribe(listener: (frame: KneeMotionFrame) => void): () => void { this.listeners.add(listener); return () => this.listeners.delete(listener) }
}
