import assert from 'node:assert/strict'
import fs from 'node:fs'
import { fileURLToPath } from 'node:url'

const sourcePath = fileURLToPath(new URL('../services/device-protocol.js', import.meta.url))
const source = fs.readFileSync(sourcePath, 'utf8')
const protocol = await import(`data:text/javascript;base64,${Buffer.from(source).toString('base64')}`)

const first = protocol.encodeFrame({
  type: protocol.MESSAGE_TYPES.COMMAND,
  seq: 7,
  payload: protocol.createCommandPayload(protocol.COMMANDS.SET_MODE, { joint_id: 'knee', name: '膝关节' }, 'request-7')
})
const decoded = protocol.decodeFrame(first)
assert.equal(decoded.seq, 7)
assert.equal(decoded.payload.request_id, 'request-7')
assert.equal(decoded.payload.data.name, '膝关节')

const parsed = []
const errors = []
const parser = protocol.createFrameParser({ onFrame: frame => parsed.push(frame), onError: error => errors.push(error) })
const chunks = protocol.splitFrame(first, 11)
chunks.forEach(chunk => parser.push(chunk))
assert.equal(parsed.length, 1)
assert.equal(parser.bufferedBytes(), 0)

const second = protocol.encodeFrame({ type: protocol.MESSAGE_TYPES.REALTIME, seq: 8, payload: { left_angle_deg: 84, right_angle_deg: 82 } })
const joined = new Uint8Array(first.byteLength + second.byteLength)
joined.set(new Uint8Array(first), 0)
joined.set(new Uint8Array(second), first.byteLength)
const stickyFrames = protocol.createFrameParser().push(joined)
assert.equal(stickyFrames.length, 2)

const damaged = new Uint8Array(first.slice(0))
damaged[10] ^= 0xff
const recoveryInput = new Uint8Array(damaged.length + second.byteLength)
recoveryInput.set(damaged, 0)
recoveryInput.set(new Uint8Array(second), damaged.length)
const recovered = []
const recoveryErrors = []
const recoveryParser = protocol.createFrameParser({ onFrame: frame => recovered.push(frame), onError: error => recoveryErrors.push(error) })
recoveryParser.push(recoveryInput)
assert.ok(recoveryErrors.some(error => error.code === protocol.PROTOCOL_ERRORS.CRC_MISMATCH))
assert.equal(recovered.length, 1)
assert.equal(recovered[0].seq, 8)

console.log('设备协议测试通过：完整包、半包、粘包、校验失败与后续恢复均正常。')
