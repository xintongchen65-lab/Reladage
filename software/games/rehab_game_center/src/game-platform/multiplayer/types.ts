import type { RepEvent, TrainingState } from '../motion/types'

export const MULTIPLAYER_PROTOCOL_VERSION = 1
export type GameId = 'fruit' | 'penalty' | 'mole'
export type RoomMode = 'PK' | 'COOP'
export type RoomStatus = 'WAITING' | 'COUNTDOWN' | 'RUNNING' | 'FINISHED' | 'CLOSED'
export type MultiplayerPlayerState =
  | 'WAITING' | 'READY' | 'RUNNING' | 'REST' | 'PAUSED'
  | 'DISCONNECTED' | 'FINISHED' | 'LEFT'

export interface PlayerIdentity {
  playerId: string
  displayName: string
  avatarUrl?: string
}

export interface MultiplayerBootstrap {
  identity: PlayerIdentity
  authToken?: string
  wsEndpoint: string
  trainingConfig?: RoomTrainingConfig
  gameId?: GameId
}

export interface RoomTrainingConfig {
  targetSets: number
  targetCount: number
  gameId?: GameId
}

export interface PublicRoomPlayer extends PlayerIdentity {
  joinedAtMs: number
  state: MultiplayerPlayerState
  ready: boolean
  connected: boolean
  overallCompletionPercent: number
  setIndex: number
  leftCount: number
  rightCount: number
  leftTotalCount: number
  rightTotalCount: number
  contribution: number
  score: number
  harvestedCount: number
  attempts?: number
  successes?: number
  activeElapsedMs: number
  rank: number
}

export interface RoomSnapshot {
  protocolVersion: 1
  roomSeq: number
  roomCode: string
  gameId?: GameId
  mode: RoomMode
  status: RoomStatus
  hostPlayerId: string
  config: RoomTrainingConfig
  randomSeed: number
  startsAtMs: number | null
  serverTimeMs: number
  teamContribution: number
  teamTarget: number
  teamCompleted: boolean
  players: PublicRoomPlayer[]
}

export interface MultiplayerProgressEvent {
  eventId: string
  clientSeq: number
  motionSeq: number
  repEvent: RepEvent
  trainingState: TrainingState
  setIndex: number
  leftCount: number
  rightCount: number
  leftTotalCount: number
  rightTotalCount: number
  overallCompletionPercent: number
  activeElapsedMs: number
  score: number
  harvestedCount: number
  attempts?: number
  successes?: number
}

export type ClientRoomMessage =
  | { type: 'create_room'; requestId: string; identity: PlayerIdentity; authToken?: string; mode: RoomMode; config: RoomTrainingConfig }
  | { type: 'join_room'; requestId: string; identity: PlayerIdentity; authToken?: string; roomCode: string; config: RoomTrainingConfig }
  | { type: 'set_ready'; requestId: string; roomCode: string; playerId: string; ready: boolean }
  | { type: 'start_room'; requestId: string; roomCode: string; playerId: string }
  | { type: 'end_room'; requestId: string; roomCode: string; playerId: string }
  | { type: 'leave_room'; requestId: string; roomCode: string; playerId: string }
  | { type: 'resume_room'; requestId: string; roomCode: string; playerId: string; resumeToken: string; lastRoomSeq: number }
  | { type: 'progress'; requestId: string; roomCode: string; playerId: string; progress: MultiplayerProgressEvent }
  | { type: 'heartbeat'; requestId: string; roomCode?: string; playerId?: string; clientTimeMs: number }

export type ServerRoomMessage =
  | { type: 'connected'; serverTimeMs: number }
  | { type: 'room_snapshot'; snapshot: RoomSnapshot; resumeToken?: string }
  | { type: 'ack'; requestId: string; eventId?: string; serverTimeMs: number }
  | { type: 'pong'; requestId: string; clientTimeMs: number; serverTimeMs: number }
  | { type: 'error'; requestId?: string; code: string; message: string }
