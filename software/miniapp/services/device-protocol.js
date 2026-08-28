export const PROTOCOL_VERSION = 1
export const MAX_PAYLOAD_BYTES = 4096

export const MESSAGE_TYPES = Object.freeze({
  HELLO: 0x01,
  DEVICE_STATUS: 0x02,
  COMMAND: 0x10,
  COMMAND_ACK: 0x11,
  REALTIME: 0x20,
  SESSION_SUMMARY: 0x21,
  ERROR: 0x7f
})

export const COMMANDS = Object.freeze({
  SET_MODE: 'SET_MODE',
  SET_PRESCRIPTION: 'SET_PRESCRIPTION',
  SET_TASK_LIST: 'SET_TASK_LIST',
  START: 'START',
  PAUSE: 'PAUSE',
  RESUME: 'RESUME',
  STOP: 'STOP',
  CALIBRATE: 'CALIBRATE',
  GET_STATUS: 'GET_STATUS',
  CHECK_FIRMWARE: 'CHECK_FIRMWARE',
  PING: 'PING'
})

export const PROTOCOL_ERRORS = Object.freeze({
  BAD_MAGIC: 'BAD_MAGIC',
  BAD_VERSION: 'BAD_VERSION',
  BAD_LENGTH: 'BAD_LENGTH',
  PAYLOAD_TOO_LARGE: 'PAYLOAD_TOO_LARGE',
  CRC_MISMATCH: 'CRC_MISMATCH',
  INVALID_JSON: 'INVALID_JSON',
  ACK_TIMEOUT: 'ACK_TIMEOUT',
  TRANSPORT_UNAVAILABLE: 'TRANSPORT_UNAVAILABLE'
})

const MAGIC_FIRST = 0x52
const MAGIC_SECOND = 0x4d
const HEADER_BYTES = 9
const CRC_BYTES = 2

export class DeviceProtocolError extends Error {
  constructor(code, message, details = {}) {
    super(message)
    this.name = 'DeviceProtocolError'
    this.code = code
    this.details = details
  }
}

function asBytes(input) {
  if (input instanceof Uint8Array) return input
  if (input instanceof ArrayBuffer) return new Uint8Array(input)
  if (ArrayBuffer.isView(input)) return new Uint8Array(input.buffer, input.byteOffset, input.byteLength)
  if (Array.isArray(input)) return Uint8Array.from(input)
  throw new TypeError('设备数据必须为 ArrayBuffer 或 Uint8Array')
}

function encodeUtf8(value) {
  const text = String(value)
  if (typeof TextEncoder !== 'undefined') return new TextEncoder().encode(text)
  const encoded = unescape(encodeURIComponent(text))
  const bytes = new Uint8Array(encoded.length)
  for (let index = 0; index < encoded.length; index += 1) bytes[index] = encoded.charCodeAt(index)
  return bytes
}

function decodeUtf8(bytes) {
  if (typeof TextDecoder !== 'undefined') return new TextDecoder('utf-8', { fatal: true }).decode(bytes)
  let encoded = ''
  for (let index = 0; index < bytes.length; index += 1) encoded += String.fromCharCode(bytes[index])
  return decodeURIComponent(escape(encoded))
}

function joinBytes(first, second) {
  const joined = new Uint8Array(first.length + second.length)
  joined.set(first, 0)
  joined.set(second, first.length)
  return joined
}

function findMagic(bytes) {
  for (let index = 0; index < bytes.length - 1; index += 1) {
    if (bytes[index] === MAGIC_FIRST && bytes[index + 1] === MAGIC_SECOND) return index
  }
  return -1
}

export function crc16Ccitt(input) {
  const bytes = asBytes(input)
  let crc = 0xffff
  for (let index = 0; index < bytes.length; index += 1) {
    crc ^= bytes[index] << 8
    for (let bit = 0; bit < 8; bit += 1) crc = crc & 0x8000 ? ((crc << 1) ^ 0x1021) & 0xffff : (crc << 1) & 0xffff
  }
  return crc
}

export function encodeFrame({ type, seq = 0, flags = 0, payload = {}, version = PROTOCOL_VERSION }) {
  if (!Number.isInteger(type) || type < 0 || type > 0xff) throw new TypeError('消息类型必须是 0—255 的整数')
  const payloadBytes = payload === undefined || payload === null ? new Uint8Array(0) : encodeUtf8(JSON.stringify(payload))
  if (payloadBytes.length > MAX_PAYLOAD_BYTES) {
    throw new DeviceProtocolError(PROTOCOL_ERRORS.PAYLOAD_TOO_LARGE, '设备消息负载超过协议限制', { length: payloadBytes.length })
  }

  const frame = new Uint8Array(HEADER_BYTES + payloadBytes.length + CRC_BYTES)
  frame[0] = MAGIC_FIRST
  frame[1] = MAGIC_SECOND
  frame[2] = version & 0xff
  frame[3] = type & 0xff
  frame[4] = flags & 0xff
  frame[5] = seq & 0xff
  frame[6] = seq >> 8 & 0xff
  frame[7] = payloadBytes.length & 0xff
  frame[8] = payloadBytes.length >> 8 & 0xff
  frame.set(payloadBytes, HEADER_BYTES)

  const crc = crc16Ccitt(frame.subarray(2, HEADER_BYTES + payloadBytes.length))
  frame[HEADER_BYTES + payloadBytes.length] = crc & 0xff
  frame[HEADER_BYTES + payloadBytes.length + 1] = crc >> 8 & 0xff
  return frame.buffer
}

export function decodeFrame(input) {
  const bytes = asBytes(input)
  if (bytes.length < HEADER_BYTES + CRC_BYTES) {
    throw new DeviceProtocolError(PROTOCOL_ERRORS.BAD_LENGTH, '设备消息长度不足', { length: bytes.length })
  }
  if (bytes[0] !== MAGIC_FIRST || bytes[1] !== MAGIC_SECOND) {
    throw new DeviceProtocolError(PROTOCOL_ERRORS.BAD_MAGIC, '设备消息头无效')
  }
  if (bytes[2] !== PROTOCOL_VERSION) {
    throw new DeviceProtocolError(PROTOCOL_ERRORS.BAD_VERSION, '设备协议版本不兼容', { received: bytes[2], supported: PROTOCOL_VERSION })
  }

  const payloadLength = bytes[7] | bytes[8] << 8
  if (payloadLength > MAX_PAYLOAD_BYTES) {
    throw new DeviceProtocolError(PROTOCOL_ERRORS.PAYLOAD_TOO_LARGE, '设备消息负载超过协议限制', { length: payloadLength })
  }
  const expectedLength = HEADER_BYTES + payloadLength + CRC_BYTES
  if (bytes.length !== expectedLength) {
    throw new DeviceProtocolError(PROTOCOL_ERRORS.BAD_LENGTH, '设备消息长度与消息头不一致', { expected: expectedLength, received: bytes.length })
  }

  const expectedCrc = bytes[expectedLength - 2] | bytes[expectedLength - 1] << 8
  const actualCrc = crc16Ccitt(bytes.subarray(2, expectedLength - CRC_BYTES))
  if (actualCrc !== expectedCrc) {
    throw new DeviceProtocolError(PROTOCOL_ERRORS.CRC_MISMATCH, '设备消息校验失败', { expected: expectedCrc, received: actualCrc })
  }

  let payload = {}
  if (payloadLength) {
    try {
      payload = JSON.parse(decodeUtf8(bytes.subarray(HEADER_BYTES, HEADER_BYTES + payloadLength)))
    } catch (error) {
      throw new DeviceProtocolError(PROTOCOL_ERRORS.INVALID_JSON, '设备消息内容无法解析', { cause: error.message })
    }
  }

  return {
    version: bytes[2],
    type: bytes[3],
    flags: bytes[4],
    seq: bytes[5] | bytes[6] << 8,
    payload
  }
}

export function splitFrame(input, maxChunkBytes = 20) {
  const bytes = asBytes(input)
  const size = Number(maxChunkBytes)
  if (!Number.isInteger(size) || size < 1) throw new TypeError('分包大小必须是正整数')
  const chunks = []
  for (let offset = 0; offset < bytes.length; offset += size) {
    const chunk = bytes.slice(offset, Math.min(bytes.length, offset + size))
    chunks.push(chunk.buffer)
  }
  return chunks
}

export function createFrameParser({ onFrame, onError, maxPayloadBytes = MAX_PAYLOAD_BYTES } = {}) {
  let pending = new Uint8Array(0)

  const reportError = error => {
    if (typeof onError === 'function') onError(error)
  }

  return {
    push(input) {
      pending = joinBytes(pending, asBytes(input))
      const frames = []

      while (pending.length >= 2) {
        const magicIndex = findMagic(pending)
        if (magicIndex < 0) {
          pending = pending[pending.length - 1] === MAGIC_FIRST ? pending.slice(-1) : new Uint8Array(0)
          break
        }
        if (magicIndex > 0) pending = pending.slice(magicIndex)
        if (pending.length < HEADER_BYTES) break

        const payloadLength = pending[7] | pending[8] << 8
        if (payloadLength > maxPayloadBytes) {
          reportError(new DeviceProtocolError(PROTOCOL_ERRORS.PAYLOAD_TOO_LARGE, '设备消息负载超过接收限制', { length: payloadLength }))
          pending = pending.slice(2)
          continue
        }

        const frameLength = HEADER_BYTES + payloadLength + CRC_BYTES
        if (pending.length < frameLength) break
        const candidate = pending.slice(0, frameLength)
        try {
          const frame = decodeFrame(candidate)
          frames.push(frame)
          if (typeof onFrame === 'function') onFrame(frame)
          pending = pending.slice(frameLength)
        } catch (error) {
          reportError(error)
          pending = pending.slice(2)
        }
      }
      return frames
    },
    reset() {
      pending = new Uint8Array(0)
    },
    bufferedBytes() {
      return pending.length
    }
  }
}

export function createCommandPayload(command, data = {}, requestId = '') {
  if (!Object.values(COMMANDS).includes(command)) throw new TypeError('设备命令不在协议清单中')
  return {
    request_id: requestId || `cmd-${Date.now()}`,
    command,
    data,
    sent_at_ms: Date.now()
  }
}
