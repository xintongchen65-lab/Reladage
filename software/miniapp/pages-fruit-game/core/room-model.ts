import type {
  MultiplayerProgressEvent,
  MultiplayerPlayerState,
  PlayerIdentity,
  PublicRoomPlayer,
  RoomMode,
  RoomSnapshot,
  RoomStatus,
  RoomTrainingConfig
} from '../types/multiplayer'
import type { GameId } from '../types/multiplayer'

interface InternalPlayer extends PublicRoomPlayer {
  lastClientSeq: number
  lastMotionSeq: number
  seenEventIds: Set<string>
  resumeToken: string
  disconnectedUntilMs: number | null
  stateBeforeDisconnect: MultiplayerPlayerState
}

interface InternalRoom {
  roomSeq: number
  roomCode: string
  gameId: GameId
  mode: RoomMode
  status: RoomStatus
  hostPlayerId: string
  config: RoomTrainingConfig
  randomSeed: number
  startsAtMs: number | null
  lockedPlayerIds: string[]
  teamTarget: number
  teamCompleted: boolean
  players: InternalPlayer[]
}

export class RoomRuleError extends Error {
  constructor(readonly code: string, message: string) { super(message) }
}

export interface RoomModelOptions {
  now?: () => number
  random?: () => number
  reconnectGraceMs?: number
  countdownMs?: number
}

export class RoomRegistry {
  private readonly rooms = new Map<string, InternalRoom>()
  private readonly now: () => number
  private readonly random: () => number
  private readonly reconnectGraceMs: number
  private readonly countdownMs: number

  constructor(options: RoomModelOptions = {}) {
    this.now = options.now ?? Date.now
    this.random = options.random ?? Math.random
    this.reconnectGraceMs = options.reconnectGraceMs ?? 15000
    this.countdownMs = options.countdownMs ?? 3000
  }

  createRoom(identity: PlayerIdentity, mode: RoomMode, config: RoomTrainingConfig): { snapshot: RoomSnapshot; resumeToken: string } {
    this.validateIdentity(identity)
    this.validateConfig(config)
    const roomCode = this.generateRoomCode()
    const player = this.createPlayer(identity)
    const room: InternalRoom = {
      roomSeq: 1,
      roomCode,
      gameId: config.gameId ?? 'fruit',
      mode,
      status: 'WAITING',
      hostPlayerId: identity.playerId,
      config: { ...config },
      randomSeed: Math.floor(this.random() * 0x7fffffff),
      startsAtMs: null,
      lockedPlayerIds: [],
      teamTarget: 0,
      teamCompleted: false,
      players: [player]
    }
    this.rooms.set(roomCode, room)
    return { snapshot: this.snapshot(room), resumeToken: player.resumeToken }
  }

  joinRoom(roomCode: string, identity: PlayerIdentity, config: RoomTrainingConfig): { snapshot: RoomSnapshot; resumeToken: string } {
    const room = this.getRoom(roomCode)
    this.validateIdentity(identity)
    if (room.status !== 'WAITING') throw new RoomRuleError('ROOM_STARTED', '房间已开始，不能中途加入')
    if (room.gameId !== (config.gameId ?? 'fruit')) throw new RoomRuleError('GAME_MISMATCH', '房间属于其他康复游戏')
    if (room.players.filter((item) => item.state !== 'LEFT').length >= 5) throw new RoomRuleError('ROOM_FULL', '房间人数已满')
    if (room.config.targetSets !== config.targetSets || room.config.targetCount !== config.targetCount) {
      throw new RoomRuleError('TRAINING_CONFIG_MISMATCH', '训练组数或每组次数与房间不一致')
    }
    if (room.players.some((item) => item.playerId === identity.playerId && item.state !== 'LEFT')) {
      throw new RoomRuleError('PLAYER_EXISTS', '玩家已在房间中')
    }
    const player = this.createPlayer(identity)
    room.players.push(player)
    this.bump(room)
    return { snapshot: this.snapshot(room), resumeToken: player.resumeToken }
  }

  setReady(roomCode: string, playerId: string, ready: boolean): RoomSnapshot {
    const room = this.getRoom(roomCode)
    if (room.status !== 'WAITING') throw new RoomRuleError('ROOM_NOT_WAITING', '房间不在等待状态')
    const player = this.getPlayer(room, playerId)
    player.ready = ready
    player.state = ready ? 'READY' : 'WAITING'
    this.bump(room)
    return this.snapshot(room)
  }

  startRoom(roomCode: string, playerId: string): RoomSnapshot {
    const room = this.getRoom(roomCode)
    if (room.hostPlayerId !== playerId) throw new RoomRuleError('HOST_ONLY', '只有房主可以开始')
    const active = room.players.filter((item) => item.state !== 'LEFT')
    if (active.length < 2) throw new RoomRuleError('NOT_ENOUGH_PLAYERS', '至少需要2人')
    if (active.some((item) => !item.ready || !item.connected)) throw new RoomRuleError('PLAYERS_NOT_READY', '所有在线玩家准备后才能开始')
    room.status = 'COUNTDOWN'
    room.startsAtMs = this.now() + this.countdownMs
    room.lockedPlayerIds = active.map((item) => item.playerId)
    const plannedActions = active.length * room.config.targetSets * room.config.targetCount * (room.gameId === 'mole' ? 1 : 2)
    room.teamTarget = room.gameId === 'penalty' ? Math.ceil(plannedActions * 0.7) : room.gameId === 'mole' ? Math.ceil(plannedActions * 0.75) : plannedActions
    this.bump(room)
    return this.snapshot(room)
  }

  tick(): RoomSnapshot[] {
    const changed: RoomSnapshot[] = []
    const now = this.now()
    this.rooms.forEach((room) => {
      let dirty = false
      if (room.status === 'COUNTDOWN' && room.startsAtMs !== null && now >= room.startsAtMs) {
        room.status = 'RUNNING'
        room.players.forEach((player) => { if (room.lockedPlayerIds.includes(player.playerId)) player.state = 'RUNNING' })
        dirty = true
      }
      room.players.forEach((player) => {
        if (player.state === 'DISCONNECTED' && player.disconnectedUntilMs !== null && now >= player.disconnectedUntilMs) {
          player.state = 'LEFT'
          player.connected = false
          player.disconnectedUntilMs = null
          dirty = true
        }
      })
      if (dirty) {
        this.transferHostIfNeeded(room)
        this.settleIfNeeded(room)
        this.bump(room)
        changed.push(this.snapshot(room))
      }
    })
    return changed
  }

  applyProgress(roomCode: string, playerId: string, progress: MultiplayerProgressEvent): { accepted: boolean; snapshot: RoomSnapshot } {
    const room = this.getRoom(roomCode)
    if (room.status !== 'RUNNING') throw new RoomRuleError('ROOM_NOT_RUNNING', '房间尚未开始或已经结束')
    const player = this.getPlayer(room, playerId)
    if (player.seenEventIds.has(progress.eventId) || progress.clientSeq <= player.lastClientSeq || progress.motionSeq <= player.lastMotionSeq) {
      return { accepted: false, snapshot: this.snapshot(room) }
    }
    if (progress.leftTotalCount < player.leftTotalCount || progress.rightTotalCount < player.rightTotalCount ||
      progress.overallCompletionPercent < player.overallCompletionPercent || progress.score < player.score ||
      progress.harvestedCount < player.harvestedCount || progress.activeElapsedMs < player.activeElapsedMs) {
      throw new RoomRuleError('PROGRESS_REGRESSION', '玩家进度不能倒退')
    }
    const nextContribution = room.gameId === 'mole' ? Math.max(progress.leftTotalCount, progress.rightTotalCount) : progress.leftTotalCount + progress.rightTotalCount
    const countDelta = nextContribution - player.contribution
    if (countDelta > 2) throw new RoomRuleError('COUNT_JUMP', '单个动作事件的次数增量不能超过2')
    const scoreDelta = progress.score - player.score
    if (scoreDelta > 500) throw new RoomRuleError('SCORE_JUMP', '单个动作事件的得分增量不能超过500')
    player.lastClientSeq = progress.clientSeq
    player.lastMotionSeq = progress.motionSeq
    player.seenEventIds.add(progress.eventId)
    player.setIndex = progress.setIndex
    player.leftCount = progress.leftCount
    player.rightCount = progress.rightCount
    player.leftTotalCount = progress.leftTotalCount
    player.rightTotalCount = progress.rightTotalCount
    player.contribution = nextContribution
    player.overallCompletionPercent = progress.overallCompletionPercent
    player.activeElapsedMs = progress.activeElapsedMs
    player.score = progress.score
    player.harvestedCount = progress.harvestedCount
    player.attempts = progress.attempts ?? progress.harvestedCount
    player.successes = progress.successes ?? progress.harvestedCount
    player.state = this.mapTrainingState(progress.trainingState)
    this.settleIfNeeded(room)
    this.bump(room)
    return { accepted: true, snapshot: this.snapshot(room) }
  }

  disconnect(roomCode: string, playerId: string): RoomSnapshot {
    const room = this.getRoom(roomCode)
    const player = this.getPlayer(room, playerId)
    if (player.state !== 'LEFT') {
      player.stateBeforeDisconnect = player.state
      player.state = 'DISCONNECTED'
      player.connected = false
      player.disconnectedUntilMs = this.now() + this.reconnectGraceMs
      this.bump(room)
    }
    return this.snapshot(room)
  }

  resume(roomCode: string, playerId: string, resumeToken: string): RoomSnapshot {
    const room = this.getRoom(roomCode)
    const player = this.getPlayer(room, playerId)
    if (player.resumeToken !== resumeToken || player.state === 'LEFT') throw new RoomRuleError('RESUME_REJECTED', '重连凭证无效或已超时')
    player.connected = true
    player.state = player.stateBeforeDisconnect
    player.disconnectedUntilMs = null
    this.bump(room)
    return this.snapshot(room)
  }

  leaveRoom(roomCode: string, playerId: string): RoomSnapshot {
    const room = this.getRoom(roomCode)
    const player = this.getPlayer(room, playerId)
    player.state = 'LEFT'; player.connected = false; player.ready = false
    this.transferHostIfNeeded(room)
    this.settleIfNeeded(room)
    if (room.players.every((item) => item.state === 'LEFT')) room.status = 'CLOSED'
    this.bump(room)
    return this.snapshot(room)
  }

  endRoom(roomCode: string, playerId: string): RoomSnapshot {
    const room = this.getRoom(roomCode)
    if (room.hostPlayerId !== playerId) throw new RoomRuleError('HOST_ONLY', '只有房主可以结束比赛')
    room.status = 'FINISHED'
    room.teamCompleted = this.teamContribution(room) >= room.teamTarget && room.teamTarget > 0
    this.bump(room)
    return this.snapshot(room)
  }

  getSnapshot(roomCode: string): RoomSnapshot { return this.snapshot(this.getRoom(roomCode)) }

  private settleIfNeeded(room: InternalRoom): void {
    if (room.status !== 'RUNNING') return
    const locked = room.players.filter((item) => room.lockedPlayerIds.includes(item.playerId))
    const allDone = locked.length > 0 && locked.every((item) => item.state === 'FINISHED' || item.state === 'LEFT')
    const teamDone = room.mode === 'COOP' && room.teamTarget > 0 && this.teamContribution(room) >= room.teamTarget
    room.teamCompleted = teamDone
    if (allDone || ((room.gameId === 'fruit' || room.gameId === 'mole') && teamDone)) {
      room.status = 'FINISHED'
    }
  }

  private snapshot(room: InternalRoom): RoomSnapshot {
    const players = room.players.map((player) => this.publicPlayer(player))
    const ranked = [...players].sort((left, right) => {
      const leftFinished = left.state === 'FINISHED' ? 1 : 0
      const rightFinished = right.state === 'FINISHED' ? 1 : 0
      return rightFinished - leftFinished ||
        right.overallCompletionPercent - left.overallCompletionPercent ||
        right.score - left.score || left.activeElapsedMs - right.activeElapsedMs ||
        left.joinedAtMs - right.joinedAtMs || left.playerId.localeCompare(right.playerId)
    })
    const rankById = new Map(ranked.map((player, index) => [player.playerId, index + 1]))
    players.forEach((player) => { player.rank = rankById.get(player.playerId) ?? 0 })
    players.sort((left, right) => left.rank - right.rank)
    return {
      protocolVersion: 1,
      roomSeq: room.roomSeq,
      roomCode: room.roomCode,
      gameId: room.gameId,
      mode: room.mode,
      status: room.status,
      hostPlayerId: room.hostPlayerId,
      config: { ...room.config },
      randomSeed: room.randomSeed,
      startsAtMs: room.startsAtMs,
      serverTimeMs: this.now(),
      teamContribution: this.teamContribution(room),
      teamTarget: room.teamTarget,
      teamCompleted: room.teamCompleted,
      players
    }
  }

  private publicPlayer(player: InternalPlayer): PublicRoomPlayer {
    const { lastClientSeq: _a, lastMotionSeq: _b, seenEventIds: _c, resumeToken: _d, disconnectedUntilMs: _e, stateBeforeDisconnect: _f, ...publicValue } = player
    return { ...publicValue }
  }
  private teamContribution(room: InternalRoom): number {
    return room.players.reduce((sum, player) => sum + ((room.gameId === 'penalty' || room.gameId === 'mole') ? (player.successes ?? 0) : player.contribution), 0)
  }
  private mapTrainingState(state: MultiplayerProgressEvent['trainingState']): MultiplayerPlayerState {
    if (state === 'REST') return 'REST'
    if (state === 'PAUSED' || state === 'IDLE') return 'PAUSED'
    if (state === 'FINISHED') return 'FINISHED'
    if (state === 'STOPPED') return 'LEFT'
    return 'RUNNING'
  }
  private transferHostIfNeeded(room: InternalRoom): void {
    const host = room.players.find((item) => item.playerId === room.hostPlayerId)
    if (host && host.state !== 'LEFT') return
    const next = room.players.filter((item) => item.connected && item.state !== 'LEFT').sort((a, b) => a.joinedAtMs - b.joinedAtMs)[0]
    if (next) room.hostPlayerId = next.playerId
  }
  private createPlayer(identity: PlayerIdentity): InternalPlayer {
    const now = this.now()
    return {
      ...identity,
      joinedAtMs: now,
      state: 'WAITING', ready: false, connected: true,
      overallCompletionPercent: 0, setIndex: 1, leftCount: 0, rightCount: 0,
      leftTotalCount: 0, rightTotalCount: 0, contribution: 0,
      score: 0, harvestedCount: 0, attempts: 0, successes: 0, activeElapsedMs: 0, rank: 0,
      lastClientSeq: 0, lastMotionSeq: -1, seenEventIds: new Set(), resumeToken: this.token(),
      disconnectedUntilMs: null, stateBeforeDisconnect: 'WAITING'
    }
  }
  private validateIdentity(identity: PlayerIdentity): void {
    if (!identity.playerId.trim() || !identity.displayName.trim()) throw new RoomRuleError('INVALID_IDENTITY', '玩家身份不完整')
  }
  private validateConfig(config: RoomTrainingConfig): void {
    if (!Number.isInteger(config.targetSets) || config.targetSets < 1 || config.targetSets > 5 ||
      !Number.isInteger(config.targetCount) || config.targetCount < 1 || config.targetCount > 99) {
      throw new RoomRuleError('INVALID_CONFIG', '训练组数或次数无效')
    }
  }
  private generateRoomCode(): string {
    for (let attempt = 0; attempt < 100; attempt += 1) {
      const code = Math.floor(this.random() * 1000000).toString().padStart(6, '0')
      if (!this.rooms.has(code)) return code
    }
    throw new RoomRuleError('ROOM_CODE_EXHAUSTED', '暂时无法创建房间')
  }
  private token(): string { return `${this.now().toString(36)}-${Math.floor(this.random() * Number.MAX_SAFE_INTEGER).toString(36)}` }
  private getRoom(code: string): InternalRoom { const room = this.rooms.get(code); if (!room) throw new RoomRuleError('ROOM_NOT_FOUND', '房间不存在'); return room }
  private getPlayer(room: InternalRoom, id: string): InternalPlayer { const player = room.players.find((item) => item.playerId === id); if (!player) throw new RoomRuleError('PLAYER_NOT_FOUND', '玩家不在房间中'); return player }
  private bump(room: InternalRoom): void { room.roomSeq += 1 }
}
