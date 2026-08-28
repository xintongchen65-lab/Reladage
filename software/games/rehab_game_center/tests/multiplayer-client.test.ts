import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest'
import { MultiplayerClient } from '../src/pages-fruit-game/runtime/multiplayer-client'
import type { RoomSnapshot } from '../src/game-platform/multiplayer/types'

class FakeSocket {
  sent: string[] = []
  private openListener: (() => void) | null = null
  private messageListener: ((event: { data: string | ArrayBuffer }) => void) | null = null
  private closeListener: (() => void) | null = null
  private errorListener: ((error: unknown) => void) | null = null

  send(options: { data: string }): void { this.sent.push(options.data) }
  close(): void { this.closeListener?.() }
  onOpen(listener: () => void): void { this.openListener = listener }
  onMessage(listener: (event: { data: string | ArrayBuffer }) => void): void { this.messageListener = listener }
  onClose(listener: () => void): void { this.closeListener = listener }
  onError(listener: (error: unknown) => void): void { this.errorListener = listener }
  open(): void { this.openListener?.() }
  fail(): void { this.errorListener?.(new Error('offline')) }
  message(value: unknown): void { this.messageListener?.({ data: JSON.stringify(value) }) }
}

const bootstrap = {
  identity: { playerId: 'guest-1', displayName: '访客1' },
  wsEndpoint: 'ws://127.0.0.1:8787',
  trainingConfig: { targetSets: 1, targetCount: 2 }
}

const roomSnapshot = (roomCode = '123456'): RoomSnapshot => ({
  protocolVersion: 1,
  roomSeq: 1,
  roomCode,
  mode: 'PK',
  status: 'WAITING',
  hostPlayerId: 'guest-1',
  config: { targetSets: 1, targetCount: 2 },
  randomSeed: 12,
  startsAtMs: null,
  serverTimeMs: 0,
  teamContribution: 0,
  teamTarget: 0,
  teamCompleted: false,
  players: []
})

describe('MultiplayerClient connection safety', () => {
  const sockets: FakeSocket[] = []

  beforeEach(() => {
    vi.useFakeTimers()
    vi.stubGlobal('uni', {
      connectSocket: vi.fn(() => {
        const socket = new FakeSocket()
        sockets.push(socket)
        return socket
      })
    })
  })

  afterEach(() => {
    sockets.length = 0
    vi.useRealTimers()
    vi.unstubAllGlobals()
  })

  it('retries three times, exposes failure, and supports a manual retry', () => {
    const client = new MultiplayerClient(bootstrap)
    const states: string[] = []
    client.subscribeConnection((state) => states.push(state))
    client.connect()
    expect(client.getConnectionState()).toBe('CONNECTING')

    sockets[0].fail()
    expect(client.getConnectionState()).toBe('RECONNECTING')
    vi.advanceTimersByTime(500)
    sockets[1].fail()
    vi.advanceTimersByTime(1000)
    sockets[2].fail()
    vi.advanceTimersByTime(2000)
    sockets[3].fail()
    expect(client.getConnectionState()).toBe('FAILED')
    expect(sockets).toHaveLength(4)

    client.retryConnection()
    expect(client.getConnectionState()).toBe('CONNECTING')
    sockets[4].open()
    expect(client.isConnected()).toBe(true)
    expect(states).toContain('FAILED')
    expect(states.at(-1)).toBe('CONNECTED')
    client.close()
  })

  it('does not queue room commands while offline', () => {
    const client = new MultiplayerClient(bootstrap)
    const errors: string[] = []
    client.onError((message) => errors.push(message))
    client.connect()
    expect(client.createRoom('PK', { targetSets: 1, targetCount: 2 })).toBe(false)
    expect(client.joinRoom('123456', { targetSets: 1, targetCount: 2 })).toBe(false)
    expect(sockets[0].sent).toHaveLength(0)
    expect(errors).toEqual(['多人服务尚未连接', '多人服务尚未连接'])
    sockets[0].open()
    expect(sockets[0].sent).toHaveLength(0)
    client.close()
  })

  it('leaves the current room locally while keeping the lobby socket reusable', () => {
    const client = new MultiplayerClient(bootstrap)
    client.connect()
    sockets[0].open()
    sockets[0].message({ type: 'room_snapshot', snapshot: roomSnapshot(), resumeToken: 'resume-1' })
    expect(client.store.getSnapshot()?.roomCode).toBe('123456')

    expect(client.leaveRoom()).toBe(true)
    expect(client.store.getSnapshot()).toBeNull()
    expect(client.isConnected()).toBe(true)
    expect(JSON.parse(sockets[0].sent.at(-1) ?? '{}').type).toBe('leave_room')

    expect(client.createRoom('COOP', { targetSets: 1, targetCount: 2 })).toBe(true)
    expect(JSON.parse(sockets[0].sent.at(-1) ?? '{}').type).toBe('create_room')
    client.close()
  })
})
