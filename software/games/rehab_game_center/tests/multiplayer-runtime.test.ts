import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import {
  beginMultiplayerTraining,
  clearMultiplayerRuntime,
  getMultiplayerClient,
  initializeMultiplayerRuntime,
  isMultiplayerSession,
  leaveCurrentMultiplayerRoom
} from '../src/pages-fruit-game/runtime/multiplayer-runtime'

describe('multiplayer runtime isolation', () => {
  beforeEach(() => {
    vi.stubGlobal('uni', { connectSocket: vi.fn(() => ({
      send: vi.fn(), close: vi.fn(), onOpen: vi.fn(), onMessage: vi.fn(), onClose: vi.fn(), onError: vi.fn()
    })) })
  })
  afterEach(() => { clearMultiplayerRuntime(); vi.unstubAllGlobals() })

  it('does not treat a waiting room snapshot as an active multiplayer game', () => {
    const client = initializeMultiplayerRuntime({
      identity: { playerId: 'guest-1', displayName: '访客1' },
      wsEndpoint: 'ws://127.0.0.1:8787'
    })
    client.store.setSnapshot({
      protocolVersion: 1, roomSeq: 1, roomCode: '123456', mode: 'COOP', status: 'WAITING',
      hostPlayerId: 'guest-1', config: { targetSets: 1, targetCount: 2 }, randomSeed: 1,
      startsAtMs: null, serverTimeMs: 0, teamContribution: 0, teamTarget: 4, teamCompleted: false, players: []
    })
    expect(isMultiplayerSession()).toBe(false)
    expect(beginMultiplayerTraining()).toBe(true)
    expect(isMultiplayerSession()).toBe(true)
    expect(leaveCurrentMultiplayerRoom()).toBe(false)
    expect(isMultiplayerSession()).toBe(false)
    expect(getMultiplayerClient()?.store.getSnapshot()).toBeNull()
  })
})
