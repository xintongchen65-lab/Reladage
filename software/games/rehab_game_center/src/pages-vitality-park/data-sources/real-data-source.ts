import type { MotionDataSource } from '../../game-platform/motion/data-source'
import type { VitalityMotionFrame } from '../types/motion'
import { VitalityMotionFrameAdapter, type VitalityRejectReason } from '../core/motion-adapter'

export interface VitalityRealTransport { start?(): void; stop?(): void; subscribe(listener: (payload: unknown) => void): () => void }

export class VitalityRealDataSource implements MotionDataSource<VitalityMotionFrame> {
  private listeners = new Set<(frame: VitalityMotionFrame) => void>()
  private adapter = new VitalityMotionFrameAdapter()
  private unsubscribe: (() => void) | null = null
  constructor(private readonly transport: VitalityRealTransport, private readonly onRejected?: (reason: VitalityRejectReason) => void) {}
  start(): void { if (this.unsubscribe) return; this.unsubscribe = this.transport.subscribe((payload) => { const result = this.adapter.ingest(payload); if (!result.accepted) { this.onRejected?.(result.reason); return }; this.listeners.forEach((listener) => listener({ ...result.frame })) }); this.transport.start?.() }
  stop(): void { this.unsubscribe?.(); this.unsubscribe = null; this.transport.stop?.() }
  reset(): void { this.adapter.reset() }
  subscribe(listener: (frame: VitalityMotionFrame) => void): () => void { this.listeners.add(listener); return () => this.listeners.delete(listener) }
}
