import { WebSocket, WebSocketServer } from 'ws'
import { RoomRegistry, RoomRuleError } from '../src/pages-fruit-game/core/room-model'
import type { ClientRoomMessage, RoomSnapshot, ServerRoomMessage } from '../src/game-platform/multiplayer/types'

const port = Number(process.env.RFG_MOCK_PORT || 8787)
const registry = new RoomRegistry()
const server = new WebSocketServer({ port })
const membership = new Map<WebSocket, { roomCode: string; playerId: string }>()

function send(socket: WebSocket, message: ServerRoomMessage): void {
  if (socket.readyState === WebSocket.OPEN) socket.send(JSON.stringify(message))
}

function broadcast(snapshot: RoomSnapshot, resume?: { socket: WebSocket; token: string }): void {
  membership.forEach((member, socket) => {
    if (member.roomCode !== snapshot.roomCode) return
    send(socket, {
      type: 'room_snapshot',
      snapshot,
      resumeToken: resume?.socket === socket ? resume.token : undefined
    })
  })
}

function bind(socket: WebSocket, roomCode: string, playerId: string): void {
  membership.set(socket, { roomCode, playerId })
}

function handle(socket: WebSocket, message: ClientRoomMessage): void {
  if (message.type === 'heartbeat') {
    send(socket, { type: 'pong', requestId: message.requestId, clientTimeMs: message.clientTimeMs, serverTimeMs: Date.now() })
    return
  }
  if (message.type === 'create_room') {
    const created = registry.createRoom(message.identity, message.mode, message.config)
    bind(socket, created.snapshot.roomCode, message.identity.playerId)
    broadcast(created.snapshot, { socket, token: created.resumeToken })
    return
  }
  if (message.type === 'join_room') {
    const joined = registry.joinRoom(message.roomCode, message.identity, message.config)
    bind(socket, joined.snapshot.roomCode, message.identity.playerId)
    broadcast(joined.snapshot, { socket, token: joined.resumeToken })
    return
  }
  if (message.type === 'resume_room') {
    const snapshot = registry.resume(message.roomCode, message.playerId, message.resumeToken)
    bind(socket, message.roomCode, message.playerId)
    broadcast(snapshot, { socket, token: message.resumeToken })
    return
  }
  if (message.type === 'set_ready') broadcast(registry.setReady(message.roomCode, message.playerId, message.ready))
  else if (message.type === 'start_room') broadcast(registry.startRoom(message.roomCode, message.playerId))
  else if (message.type === 'end_room') broadcast(registry.endRoom(message.roomCode, message.playerId))
  else if (message.type === 'leave_room') {
    const snapshot = registry.leaveRoom(message.roomCode, message.playerId)
    membership.delete(socket)
    broadcast(snapshot)
  }
  else if (message.type === 'progress') {
    const result = registry.applyProgress(message.roomCode, message.playerId, message.progress)
    send(socket, { type: 'ack', requestId: message.requestId, eventId: message.progress.eventId, serverTimeMs: Date.now() })
    if (result.accepted) broadcast(result.snapshot)
  }
}

server.on('connection', (socket) => {
  send(socket, { type: 'connected', serverTimeMs: Date.now() })
  socket.on('message', (data) => {
    try {
      handle(socket, JSON.parse(data.toString()) as ClientRoomMessage)
    } catch (error) {
      const ruleError = error instanceof RoomRuleError ? error : null
      send(socket, {
        type: 'error',
        code: ruleError?.code ?? 'BAD_MESSAGE',
        message: ruleError?.message ?? '无法处理房间消息'
      })
    }
  })
  socket.on('close', () => {
    const member = membership.get(socket)
    membership.delete(socket)
    if (!member) return
    try { broadcast(registry.disconnect(member.roomCode, member.playerId)) } catch { /* room already closed */ }
  })
})

setInterval(() => registry.tick().forEach((snapshot) => broadcast(snapshot)), 250)
console.log(`RehabMotion mock room server listening on ws://127.0.0.1:${port}`)
