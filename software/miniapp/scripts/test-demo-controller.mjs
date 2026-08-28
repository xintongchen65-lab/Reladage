import assert from 'node:assert/strict'
import fs from 'node:fs'
import { fileURLToPath } from 'node:url'

const servicesDir = fileURLToPath(new URL('../services/', import.meta.url))
const store = new Map()
globalThis.uni = {
  getStorageSync: key => store.get(key),
  setStorageSync: (key, value) => store.set(key, value),
  removeStorageSync: key => store.delete(key)
}

const membersSource = fs.readFileSync(`${servicesDir}members.js`, 'utf8')
const membersUrl = `data:text/javascript;base64,${Buffer.from(membersSource).toString('base64')}`
const demoSource = fs
  .readFileSync(`${servicesDir}demo-controller.js`, 'utf8')
  .replace("from './members.js'", `from '${membersUrl}'`)
const demo = await import(`data:text/javascript;base64,${Buffer.from(demoSource).toString('base64')}`)

let result = demo.applyDemoScenario(demo.DEMO_SCENES.BEFORE)
assert.equal(result.records.length, 30)
assert.equal(result.records[0].status, 'partial')
assert.equal(result.records[0].sessions.filter(item => item.status === 'completed').length, 2)

result = demo.completeDemoTraining()
assert.equal(result.records[0].status, 'completed')
assert.equal(result.records[0].sessions.filter(item => item.status === 'completed').length, 3)

result = demo.applyDemoScenario(demo.DEMO_SCENES.ATTENTION)
assert.ok(result.records[0].qualified < 90)
assert.ok(result.records[0].reasons.paceFast > 0)

const device = demo.setDemoDeviceState('low-battery')
assert.equal(device.connected, true)
assert.equal(device.battery, 14)

console.log('拍摄场景测试通过：训练前、训练完成、关注事项和设备状态均可切换。')
