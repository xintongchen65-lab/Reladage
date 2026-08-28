import fs from 'node:fs'
import zlib from 'node:zlib'

function paeth(left, above, upperLeft) {
  const prediction = left + above - upperLeft
  const leftDistance = Math.abs(prediction - left)
  const aboveDistance = Math.abs(prediction - above)
  const upperLeftDistance = Math.abs(prediction - upperLeft)
  if (leftDistance <= aboveDistance && leftDistance <= upperLeftDistance) return left
  if (aboveDistance <= upperLeftDistance) return above
  return upperLeft
}

export function readRgbaPng(file) {
  const buffer = fs.readFileSync(file)
  const signature = '89504e470d0a1a0a'
  if (buffer.subarray(0, 8).toString('hex') !== signature) throw new Error(`${file}: invalid PNG signature`)

  let offset = 8
  let width = 0
  let height = 0
  const idat = []
  while (offset < buffer.length) {
    const length = buffer.readUInt32BE(offset)
    const type = buffer.subarray(offset + 4, offset + 8).toString('ascii')
    const data = buffer.subarray(offset + 8, offset + 8 + length)
    if (type === 'IHDR') {
      width = data.readUInt32BE(0)
      height = data.readUInt32BE(4)
      if (data[8] !== 8 || data[9] !== 6 || data[12] !== 0) {
        throw new Error(`${file}: expected non-interlaced 8-bit RGBA PNG`)
      }
    }
    if (type === 'IDAT') idat.push(data)
    offset += length + 12
    if (type === 'IEND') break
  }

  const bytesPerPixel = 4
  const stride = width * bytesPerPixel
  const compressed = Buffer.concat(idat)
  const raw = zlib.inflateSync(compressed)
  const rgba = Buffer.alloc(width * height * bytesPerPixel)
  let rawOffset = 0

  for (let y = 0; y < height; y += 1) {
    const filter = raw[rawOffset]
    rawOffset += 1
    for (let x = 0; x < stride; x += 1) {
      const source = raw[rawOffset + x]
      const outputIndex = y * stride + x
      const left = x >= bytesPerPixel ? rgba[outputIndex - bytesPerPixel] : 0
      const above = y > 0 ? rgba[outputIndex - stride] : 0
      const upperLeft = y > 0 && x >= bytesPerPixel ? rgba[outputIndex - stride - bytesPerPixel] : 0
      if (filter === 0) rgba[outputIndex] = source
      else if (filter === 1) rgba[outputIndex] = (source + left) & 0xff
      else if (filter === 2) rgba[outputIndex] = (source + above) & 0xff
      else if (filter === 3) rgba[outputIndex] = (source + Math.floor((left + above) / 2)) & 0xff
      else if (filter === 4) rgba[outputIndex] = (source + paeth(left, above, upperLeft)) & 0xff
      else throw new Error(`${file}: unsupported PNG filter ${filter}`)
    }
    rawOffset += stride
  }

  return { width, height, rgba }
}

export function opaqueBounds(image, alphaThreshold = 32) {
  let left = image.width
  let top = image.height
  let right = -1
  let bottom = -1
  for (let y = 0; y < image.height; y += 1) {
    for (let x = 0; x < image.width; x += 1) {
      const alpha = image.rgba[(y * image.width + x) * 4 + 3]
      if (alpha <= alphaThreshold) continue
      left = Math.min(left, x)
      top = Math.min(top, y)
      right = Math.max(right, x)
      bottom = Math.max(bottom, y)
    }
  }
  return right < 0 ? null : { left, top, right, bottom }
}

export function countOpaqueComponents(image, alphaThreshold = 32, minimumSize = 10) {
  const visited = new Uint8Array(image.width * image.height)
  let components = 0
  for (let start = 0; start < visited.length; start += 1) {
    if (visited[start] || image.rgba[start * 4 + 3] <= alphaThreshold) continue
    let size = 0
    const queue = [start]
    visited[start] = 1
    while (queue.length) {
      const index = queue.pop()
      size += 1
      const x = index % image.width
      const y = Math.floor(index / image.width)
      const neighbours = []
      if (x > 0) neighbours.push(index - 1)
      if (x + 1 < image.width) neighbours.push(index + 1)
      if (y > 0) neighbours.push(index - image.width)
      if (y + 1 < image.height) neighbours.push(index + image.width)
      neighbours.forEach((next) => {
        if (visited[next] || image.rgba[next * 4 + 3] <= alphaThreshold) return
        visited[next] = 1
        queue.push(next)
      })
    }
    if (size >= minimumSize) components += 1
  }
  return components
}
