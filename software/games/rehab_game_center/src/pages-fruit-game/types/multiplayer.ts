export * from '../../game-platform/multiplayer/types'
import type { RoomMode, RoomSnapshot } from '../../game-platform/multiplayer/types'
import type { TrainingResult } from './result'

export interface MultiplayerTrainingResult {
  protocolVersion: 1
  roomCode: string
  mode: RoomMode
  localPlayerId: string
  localTraining: TrainingResult
  room: RoomSnapshot
  completedAtMs: number
}
