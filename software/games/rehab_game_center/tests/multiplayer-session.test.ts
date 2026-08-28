import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import { clearMultiplayerSession, loadMultiplayerSession, saveMultiplayerSession, shouldClearMultiplayerOnUnload } from '../src/game-platform/runtime/multiplayer-session'
import type { RoomSnapshot } from '../src/game-platform/multiplayer/types'

const snapshot: RoomSnapshot = {
  protocolVersion: 1, roomSeq: 5, roomCode: '123456', gameId: 'penalty', mode: 'PK', status: 'RUNNING', hostPlayerId: 'p1',
  config: { targetSets: 3, targetCount: 10, gameId: 'penalty' }, randomSeed: 7, startsAtMs: 1000, serverTimeMs: 1000,
  teamContribution: 0, teamTarget: 0, teamCompleted: false, players: []
}

describe('multiplayer navigation session', () => {
  const storage = new Map<string, unknown>()
  beforeEach(() => {
    storage.clear()
    vi.stubGlobal('uni', {
      setStorageSync: (key: string, value: unknown) => storage.set(key, value),
      getStorageSync: (key: string) => storage.get(key),
      removeStorageSync: (key: string) => storage.delete(key)
    })
  })
  afterEach(() => { vi.unstubAllGlobals() })

  it('preserves runtime on lobby-to-room navigation and clears only a real exit', () => {
    expect(shouldClearMultiplayerOnUnload(true, false)).toBe(false)
    expect(shouldClearMultiplayerOnUnload(false, true)).toBe(false)
    expect(shouldClearMultiplayerOnUnload(false, false)).toBe(true)
  })

  it('restores a short-lived active room marker without persisting the auth token', () => {
    saveMultiplayerSession('penalty', { identity: { playerId: 'p1', displayName: '球员1' }, wsEndpoint: 'ws://test', authToken: 'secret', gameId: 'penalty' }, { roomCode: '123456', resumeToken: 'resume', clientSeq: 3, snapshot }, true)
    const restored = loadMultiplayerSession('penalty')
    expect(restored?.activeTraining).toBe(true)
    expect(restored?.clientSession.snapshot.status).toBe('RUNNING')
    expect(restored?.bootstrap.authToken).toBeUndefined()
    clearMultiplayerSession('penalty')
    expect(loadMultiplayerSession('penalty')).toBeNull()
  })
})
