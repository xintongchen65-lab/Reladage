import { describe, expect, it } from 'vitest'
import { RoomRegistry, RoomRuleError } from '../src/pages-fruit-game/core/room-model'
import type { MultiplayerProgressEvent, PlayerIdentity } from '../src/game-platform/multiplayer/types'

const identity = (id: string): PlayerIdentity => ({ playerId: id, displayName: `玩家${id}` })
const config = { targetSets: 1, targetCount: 2 }
function progress(id: string, seq: number, patch: Partial<MultiplayerProgressEvent> = {}): MultiplayerProgressEvent {
  return {
    eventId: `${id}-${seq}`, clientSeq: seq, motionSeq: seq, repEvent: 'none', trainingState: 'RUNNING',
    setIndex: 1, leftCount: 0, rightCount: 0, leftTotalCount: 0, rightTotalCount: 0,
    overallCompletionPercent: 0, activeElapsedMs: seq * 100, score: 0, harvestedCount: 0, ...patch
  }
}

describe('RoomRegistry', () => {
  it('enforces 2-5 players, readiness, countdown and late-join rejection', () => {
    let now = 1000
    const registry = new RoomRegistry({ now: () => now, random: () => 0.123456, countdownMs: 3000 })
    const created = registry.createRoom(identity('1'), 'PK', config)
    const code = created.snapshot.roomCode
    expect(() => registry.startRoom(code, '1')).toThrowError(RoomRuleError)
    registry.joinRoom(code, identity('2'), config)
    expect(registry.getSnapshot(code).players).toHaveLength(2)
    registry.joinRoom(code, identity('3'), config)
    expect(registry.getSnapshot(code).players).toHaveLength(3)
    for (const id of ['4', '5']) registry.joinRoom(code, identity(id), config)
    expect(registry.getSnapshot(code).players).toHaveLength(5)
    expect(() => registry.joinRoom(code, identity('6'), config)).toThrowError(/人数已满/)
    for (const id of ['1', '2', '3', '4', '5']) registry.setReady(code, id, true)
    const countdown = registry.startRoom(code, '1')
    expect(countdown.status).toBe('COUNTDOWN'); expect(countdown.startsAtMs).toBe(4000)
    expect(() => registry.joinRoom(code, identity('6'), config)).toThrowError(/不能中途加入/)
    now = 4000; expect(registry.tick()[0].status).toBe('RUNNING')
  })

  it('deduplicates events and ranks by completion, score then active time', () => {
    let now = 0
    const registry = new RoomRegistry({ now: () => now, random: () => 0.2, countdownMs: 0 })
    const code = registry.createRoom(identity('a'), 'PK', config).snapshot.roomCode
    registry.joinRoom(code, identity('b'), config); registry.setReady(code, 'a', true); registry.setReady(code, 'b', true); registry.startRoom(code, 'a'); registry.tick()
    const first = progress('a', 1, { repEvent: 'left_rep_done', leftCount: 1, leftTotalCount: 1, overallCompletionPercent: 50, score: 100, harvestedCount: 1 })
    expect(registry.applyProgress(code, 'a', first).accepted).toBe(true)
    expect(registry.applyProgress(code, 'a', first).accepted).toBe(false)
    expect(registry.applyProgress(code, 'a', { ...first, eventId: 'a-new-id', clientSeq: 2 }).accepted).toBe(false)
    registry.applyProgress(code, 'b', progress('b', 1, { leftCount: 1, leftTotalCount: 1, overallCompletionPercent: 50, score: 200, harvestedCount: 1 }))
    expect(registry.getSnapshot(code).players[0].playerId).toBe('b')
    registry.applyProgress(code, 'a', progress('a', 2, { leftCount: 2, leftTotalCount: 2, overallCompletionPercent: 50, score: 200, harvestedCount: 2 }))
    registry.applyProgress(code, 'a', progress('a', 3, { trainingState: 'FINISHED', leftCount: 2, rightCount: 2, leftTotalCount: 2, rightTotalCount: 2, overallCompletionPercent: 100, score: 200, harvestedCount: 2 }))
    expect(registry.getSnapshot(code).players[0].playerId).toBe('a')
  })

  it('keeps a fixed cooperation target and transfers host after reconnect grace', () => {
    let now = 0
    const registry = new RoomRegistry({ now: () => now, random: () => 0.3, countdownMs: 0, reconnectGraceMs: 15000 })
    const created = registry.createRoom(identity('host'), 'COOP', config); const code = created.snapshot.roomCode
    registry.joinRoom(code, identity('next'), config); registry.setReady(code, 'host', true); registry.setReady(code, 'next', true); registry.startRoom(code, 'host'); registry.tick()
    expect(registry.getSnapshot(code).teamTarget).toBe(8)
    registry.disconnect(code, 'host'); now = 14999; registry.tick(); expect(registry.getSnapshot(code).hostPlayerId).toBe('host')
    now = 15000; registry.tick(); const snapshot = registry.getSnapshot(code)
    expect(snapshot.hostPlayerId).toBe('next'); expect(snapshot.teamTarget).toBe(8)
  })

  it('accepts resume during the grace period and rejects it after expiry', () => {
    let now = 0
    const registry = new RoomRegistry({ now: () => now, random: () => 0.4, reconnectGraceMs: 15000 })
    const created = registry.createRoom(identity('a'), 'PK', config); const code = created.snapshot.roomCode
    registry.disconnect(code, 'a'); now = 10000
    expect(registry.resume(code, 'a', created.resumeToken).players[0].connected).toBe(true)
    registry.disconnect(code, 'a'); now = 25000; registry.tick()
    expect(() => registry.resume(code, 'a', created.resumeToken)).toThrowError(/超时/)
  })

  it('settles cooperation only against the fixed team target', () => {
    const registry = new RoomRegistry({ random: () => 0.5, countdownMs: 0 })
    const code = registry.createRoom(identity('a'), 'COOP', config).snapshot.roomCode
    registry.joinRoom(code, identity('b'), config)
    registry.setReady(code, 'a', true); registry.setReady(code, 'b', true)
    registry.startRoom(code, 'a'); registry.tick()
    registry.applyProgress(code, 'a', progress('a', 1, { repEvent: 'both_rep_done', leftCount: 1, rightCount: 1, leftTotalCount: 1, rightTotalCount: 1, overallCompletionPercent: 50 }))
    registry.applyProgress(code, 'b', progress('b', 1, { repEvent: 'both_rep_done', leftCount: 1, rightCount: 1, leftTotalCount: 1, rightTotalCount: 1, overallCompletionPercent: 50 }))
    expect(registry.getSnapshot(code)).toMatchObject({ status: 'RUNNING', teamContribution: 4, teamTarget: 8 })
    registry.applyProgress(code, 'a', progress('a', 2, { repEvent: 'both_rep_done', leftCount: 2, rightCount: 2, leftTotalCount: 2, rightTotalCount: 2, overallCompletionPercent: 100 }))
    registry.applyProgress(code, 'b', progress('b', 2, { repEvent: 'both_rep_done', trainingState: 'FINISHED', leftCount: 2, rightCount: 2, leftTotalCount: 2, rightTotalCount: 2, overallCompletionPercent: 100 }))
    expect(registry.getSnapshot(code)).toMatchObject({ status: 'FINISHED', teamContribution: 8, teamCompleted: true })
  })

  it('closes a waiting room after its final player leaves', () => {
    const registry = new RoomRegistry({ random: () => 0.61 })
    const code = registry.createRoom(identity('solo'), 'PK', config).snapshot.roomCode
    expect(registry.leaveRoom(code, 'solo').status).toBe('CLOSED')
  })
})
