import WebSocket from 'ws'

const endpoint = process.env.RFG_MOCK_WS || 'ws://127.0.0.1:8787'

function openClient() {
  return new Promise((resolve, reject) => {
    const socket = new WebSocket(endpoint)
    socket.once('open', () => resolve(socket))
    socket.once('error', reject)
  })
}

function waitFor(socket, predicate, timeoutMs = 5000) {
  return new Promise((resolve, reject) => {
    const timer = setTimeout(() => {
      socket.off('message', onMessage)
      reject(new Error('等待房间快照超时'))
    }, timeoutMs)
    function onMessage(raw) {
      const message = JSON.parse(raw.toString())
      if (!predicate(message)) return
      clearTimeout(timer)
      socket.off('message', onMessage)
      resolve(message)
    }
    socket.on('message', onMessage)
  })
}

function send(socket, message) { socket.send(JSON.stringify(message)) }

const host = await openClient()
const guest = await openClient()
const config = { targetSets: 3, targetCount: 10 }
send(host, { type: 'create_room', requestId: 'create', identity: { playerId: 'smoke-host', displayName: '房主' }, mode: 'PK', config })
const created = await waitFor(host, (message) => message.type === 'room_snapshot')
const roomCode = created.snapshot.roomCode
send(guest, { type: 'join_room', requestId: 'join', roomCode, identity: { playerId: 'smoke-guest', displayName: '队友' }, config })
await waitFor(guest, (message) => message.type === 'room_snapshot' && message.snapshot.players.length === 2)
send(host, { type: 'set_ready', requestId: 'ready-host', roomCode, playerId: 'smoke-host', ready: true })
await waitFor(host, (message) => message.type === 'room_snapshot' && message.snapshot.players.find((item) => item.playerId === 'smoke-host')?.ready)
send(guest, { type: 'set_ready', requestId: 'ready-guest', roomCode, playerId: 'smoke-guest', ready: true })
await waitFor(guest, (message) => message.type === 'room_snapshot' && message.snapshot.players.every((item) => item.ready))
send(host, { type: 'start_room', requestId: 'start', roomCode, playerId: 'smoke-host' })
await waitFor(host, (message) => message.type === 'room_snapshot' && message.snapshot.status === 'COUNTDOWN')
const running = await waitFor(host, (message) => message.type === 'room_snapshot' && message.snapshot.status === 'RUNNING', 5000)
if (running.snapshot.players.length !== 2 || !/^\d{6}$/.test(roomCode)) throw new Error('房间快照不符合预期')
console.log(`Mock server smoke passed: room ${roomCode}, ${running.snapshot.players.length} players, status ${running.snapshot.status}.`)
host.close()
guest.close()
