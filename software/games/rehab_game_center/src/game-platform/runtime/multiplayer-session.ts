import type { MultiplayerRoomSession } from '../multiplayer/client'
import type { MultiplayerBootstrap } from '../multiplayer/types'

export type MultiplayerGameId = 'fruit' | 'penalty' | 'mole'

export interface PersistedMultiplayerSession {
  version: 1
  gameId: MultiplayerGameId
  expiresAtMs: number
  activeTraining: boolean
  bootstrap: MultiplayerBootstrap
  clientSession: MultiplayerRoomSession
}

const PREFIX = 'rehabmotion:multiplayer-session:v1:'
const SESSION_TTL_MS = 15 * 60 * 1000

export function shouldClearMultiplayerOnUnload(navigating: boolean, leaving: boolean): boolean {
  return !navigating && !leaving
}

function key(gameId: MultiplayerGameId): string { return `${PREFIX}${gameId}` }

export function saveMultiplayerSession(gameId: MultiplayerGameId, bootstrap: MultiplayerBootstrap, clientSession: MultiplayerRoomSession, activeTraining: boolean): void {
  const safeBootstrap: MultiplayerBootstrap = {
    identity: bootstrap.identity,
    wsEndpoint: bootstrap.wsEndpoint,
    trainingConfig: bootstrap.trainingConfig,
    gameId
  }
  const value: PersistedMultiplayerSession = { version: 1, gameId, expiresAtMs: Date.now() + SESSION_TTL_MS, activeTraining, bootstrap: safeBootstrap, clientSession }
  try { uni.setStorageSync(key(gameId), value) } catch { /* Runtime remains authoritative when storage is unavailable. */ }
}

export function loadMultiplayerSession(gameId: MultiplayerGameId): PersistedMultiplayerSession | null {
  try {
    const value = uni.getStorageSync(key(gameId)) as PersistedMultiplayerSession | undefined
    if (!value || value.version !== 1 || value.gameId !== gameId || value.expiresAtMs <= Date.now()) { clearMultiplayerSession(gameId); return null }
    const snapshotGameId = value.clientSession?.snapshot?.gameId ?? 'fruit'
    if (!value.activeTraining || !value.clientSession?.snapshot || snapshotGameId !== gameId) return null
    return value
  } catch { return null }
}

export function clearMultiplayerSession(gameId: MultiplayerGameId): void {
  try { uni.removeStorageSync(key(gameId)) } catch { /* no-op */ }
}
