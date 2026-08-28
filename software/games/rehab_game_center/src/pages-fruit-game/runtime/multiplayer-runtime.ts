import { clearMultiplayerSession, loadMultiplayerSession, saveMultiplayerSession } from '../../game-platform/runtime/multiplayer-session'
import type { MultiplayerBootstrap, MultiplayerTrainingResult, RoomSnapshot } from '../types/multiplayer'
import type { TrainingResult } from '../types/result'
import { MultiplayerClient } from './multiplayer-client'

interface MultiplayerRuntimeState {
  client: MultiplayerClient
  localResult: TrainingResult | null
  resultEmitter: ((result: MultiplayerTrainingResult) => void) | null
  published: boolean
  activeTraining: boolean
  persistUnsubscribe: (() => void) | null
}

let runtime: MultiplayerRuntimeState | null = null

function persistRuntime(): void {
  if (!runtime) return
  const session = runtime.client.exportRoomSession()
  if (session) saveMultiplayerSession('fruit', { ...runtime.client.bootstrap, gameId: 'fruit' }, session, runtime.activeTraining)
}

function attachPersistence(state: MultiplayerRuntimeState): void {
  state.persistUnsubscribe = state.client.store.subscribe(() => persistRuntime())
}

export function initializeMultiplayerRuntime(bootstrap: MultiplayerBootstrap): MultiplayerClient {
  clearMultiplayerRuntime()
  runtime = { client: new MultiplayerClient({ ...bootstrap, gameId: 'fruit' }), localResult: null, resultEmitter: null, published: false, activeTraining: false, persistUnsubscribe: null }
  attachPersistence(runtime)
  runtime.client.connect()
  return runtime.client
}

export function restoreMultiplayerRuntime(): MultiplayerClient | null {
  if (runtime) return runtime.client
  const saved = loadMultiplayerSession('fruit')
  if (!saved) return null
  const client = new MultiplayerClient(saved.bootstrap)
  client.restoreRoomSession(saved.clientSession)
  runtime = { client, localResult: null, resultEmitter: null, published: false, activeTraining: saved.activeTraining, persistUnsubscribe: null }
  attachPersistence(runtime)
  client.connect()
  return client
}

export function getMultiplayerClient(): MultiplayerClient | null { return runtime?.client ?? null }
export function beginMultiplayerTraining(): boolean { if (!runtime?.client.store.getSnapshot()) return false; runtime.activeTraining = true; persistRuntime(); return true }
export function leaveCurrentMultiplayerRoom(): boolean { if (!runtime) return false; runtime.activeTraining = false; runtime.localResult = null; runtime.published = false; clearMultiplayerSession('fruit'); return runtime.client.leaveRoom() }
export function isMultiplayerSession(): boolean {
  if (!runtime?.activeTraining) return false
  const snapshot = runtime.client.store.getSnapshot()
  const state = runtime.client.getConnectionState()
  return !!snapshot && (snapshot.gameId ?? 'fruit') === 'fruit' && !['CLOSED', 'FAILED'].includes(state)
}
export function registerMultiplayerResultEmitter(emitter: ((result: MultiplayerTrainingResult) => void) | null): void { if (runtime) runtime.resultEmitter = emitter }
export function setLocalMultiplayerResult(result: TrainingResult): void { if (runtime) runtime.localResult = result }
export function publishMultiplayerResult(snapshot?: RoomSnapshot): MultiplayerTrainingResult | null {
  if (!runtime || runtime.published || !runtime.localResult) return null
  const room = snapshot ?? runtime.client.store.getSnapshot()
  if (!room || room.status !== 'FINISHED') return null
  const result: MultiplayerTrainingResult = { protocolVersion: 1, roomCode: room.roomCode, mode: room.mode, localPlayerId: runtime.client.bootstrap.identity.playerId, localTraining: runtime.localResult, room, completedAtMs: Date.now() }
  runtime.published = true
  runtime.resultEmitter?.(result)
  return result
}
export function clearMultiplayerRuntime(): void {
  runtime?.persistUnsubscribe?.()
  runtime?.client.close()
  runtime = null
  clearMultiplayerSession('fruit')
}
