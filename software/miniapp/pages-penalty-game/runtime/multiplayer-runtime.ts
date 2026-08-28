import { MultiplayerClient } from '../../game-platform/multiplayer/client'
import { clearMultiplayerSession, loadMultiplayerSession, saveMultiplayerSession } from '../../game-platform/runtime/multiplayer-session'
import type { MultiplayerBootstrap, RoomSnapshot } from '../../game-platform/multiplayer/types'
import type { PenaltyTrainingResult } from '../types/result'

export interface PenaltyMultiplayerResult {
  protocolVersion: 1
  gameId: 'penalty'
  roomCode: string
  mode: 'PK' | 'COOP'
  localPlayerId: string
  localTraining: PenaltyTrainingResult
  room: RoomSnapshot
  completedAtMs: number
}

interface Runtime {
  client: MultiplayerClient
  active: boolean
  local: PenaltyTrainingResult | null
  emitter: ((result: PenaltyMultiplayerResult) => void) | null
  published: boolean
  persistUnsubscribe: (() => void) | null
}

let runtime: Runtime | null = null

function persistRuntime(): void {
  if (!runtime) return
  const session = runtime.client.exportRoomSession()
  if (session) saveMultiplayerSession('penalty', { ...runtime.client.bootstrap, gameId: 'penalty' }, session, runtime.active)
}

function attachPersistence(state: Runtime): void { state.persistUnsubscribe = state.client.store.subscribe(() => persistRuntime()) }

export function initializePenaltyMultiplayer(bootstrap: MultiplayerBootstrap): MultiplayerClient {
  clearPenaltyMultiplayer()
  runtime = { client: new MultiplayerClient({ ...bootstrap, gameId: 'penalty', trainingConfig: { ...(bootstrap.trainingConfig ?? { targetSets: 3, targetCount: 10 }), gameId: 'penalty' } }), active: false, local: null, emitter: null, published: false, persistUnsubscribe: null }
  attachPersistence(runtime)
  runtime.client.connect()
  return runtime.client
}

export function restorePenaltyMultiplayer(): MultiplayerClient | null {
  if (runtime) return runtime.client
  const saved = loadMultiplayerSession('penalty')
  if (!saved) return null
  const client = new MultiplayerClient(saved.bootstrap)
  client.restoreRoomSession(saved.clientSession)
  runtime = { client, active: saved.activeTraining, local: null, emitter: null, published: false, persistUnsubscribe: null }
  attachPersistence(runtime)
  client.connect()
  return client
}

export function getPenaltyMultiplayerClient(): MultiplayerClient | null { return runtime?.client ?? null }
export function beginPenaltyMultiplayer(): boolean { if (!runtime?.client.store.getSnapshot()) return false; runtime.active = true; persistRuntime(); return true }
export function isPenaltyMultiplayer(): boolean {
  if (!runtime?.active) return false
  const snapshot = runtime.client.store.getSnapshot()
  const state = runtime.client.getConnectionState()
  return !!snapshot && snapshot.gameId === 'penalty' && !['CLOSED', 'FAILED'].includes(state)
}
export function leavePenaltyRoom(): boolean { if (!runtime) return false; runtime.active = false; runtime.local = null; runtime.published = false; clearMultiplayerSession('penalty'); return runtime.client.leaveRoom() }
export function clearPenaltyMultiplayer(): void { runtime?.persistUnsubscribe?.(); runtime?.client.close(); runtime = null; clearMultiplayerSession('penalty') }
export function setPenaltyLocalResult(result: PenaltyTrainingResult): void { if (runtime) runtime.local = result }
export function registerPenaltyMultiplayerEmitter(emitter: ((result: PenaltyMultiplayerResult) => void) | null): void { if (runtime) runtime.emitter = emitter }
export function publishPenaltyMultiplayer(snapshot?: RoomSnapshot): PenaltyMultiplayerResult | null {
  if (!runtime || runtime.published || !runtime.local) return null
  const room = snapshot ?? runtime.client.store.getSnapshot()
  if (!room || room.status !== 'FINISHED') return null
  const result: PenaltyMultiplayerResult = { protocolVersion: 1, gameId: 'penalty', roomCode: room.roomCode, mode: room.mode, localPlayerId: runtime.client.bootstrap.identity.playerId, localTraining: runtime.local, room, completedAtMs: Date.now() }
  runtime.published = true
  runtime.emitter?.(result)
  return result
}
