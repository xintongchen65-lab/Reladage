import assert from 'node:assert/strict'
import { after, before, test } from 'node:test'
import { buildApp } from '../src/app.js'
import { createTokenService } from '../src/auth/token.js'
import { MemoryRepository } from '../src/repositories/memory.js'
import type { WechatAuthClient } from '../src/types.js'

const repository = new MemoryRepository()
const wechatAuth: WechatAuthClient = {
  async exchangeCode(code) {
    return { openId: `openid-${code}` }
  }
}
const app = buildApp({
  repository,
  wechatAuth,
  tokenService: createTokenService('test-secret-at-least-thirty-two-characters'),
  logger: false
})

before(async () => { await app.ready() })
after(async () => { await app.close() })

async function login(code: string) {
  const response = await app.inject({ method: 'POST', url: '/auth/wechat/login', payload: { code } })
  assert.equal(response.statusCode, 200)
  return response.json<{ accessToken: string; account: { userId: string } }>()
}

test('健康检查无需登录', async () => {
  const response = await app.inject({ method: 'GET', url: '/health' })
  assert.equal(response.statusCode, 200)
  assert.equal(response.json().ok, true)
})

test('未携带令牌不能读取启动数据', async () => {
  const response = await app.inject({ method: 'GET', url: '/app/bootstrap' })
  assert.equal(response.statusCode, 401)
  assert.equal(response.json().error.code, 'UNAUTHORIZED')
})

test('微信登录后可完成成员、设备、方案、训练记录与已读状态闭环', async () => {
  const session = await login('family-a')
  const headers = { authorization: `Bearer ${session.accessToken}` }

  const memberResponse = await app.inject({
    method: 'POST',
    url: '/family/members',
    headers,
    payload: { name: '王建国', relationship: '父亲', stage: '膝关节术后 · 第4周' }
  })
  assert.equal(memberResponse.statusCode, 201)
  const member = memberResponse.json<{ id: string }>()

  const deviceResponse = await app.inject({
    method: 'POST',
    url: '/devices/bind',
    headers,
    payload: { memberId: member.id, serialNumber: 'RM-CORE-0001', deviceName: 'RM-Core 01' }
  })
  assert.equal(deviceResponse.statusCode, 200)

  const planResponse = await app.inject({
    method: 'PUT',
    url: `/training-plans/${member.id}`,
    headers,
    payload: {
      version: 3,
      tasks: [{ exercise_id: 5, name: '膝关节屈伸', sets: 3, reps: 10 }],
      advanced: { enabled: false }
    }
  })
  assert.equal(planResponse.statusCode, 200)

  const trainingResponse = await app.inject({
    method: 'POST',
    url: '/training-sessions',
    headers,
    payload: {
      memberId: member.id,
      record: {
        externalSessionId: 'rm-session-001',
        date: '2026-08-20',
        status: 'completed',
        completion: 100,
        qualified: 87,
        minutes: 21,
        completedReps: 30,
        qualifiedReps: 26,
        plannedReps: 30,
        sessions: [{ exerciseId: 5, name: '膝关节屈伸', completedReps: 30 }]
      }
    }
  })
  assert.equal(trainingResponse.statusCode, 201)

  const readResponse = await app.inject({
    method: 'PUT',
    url: `/messages/read-state/${member.id}`,
    headers,
    payload: { ids: ['training-2026-08-20'] }
  })
  assert.equal(readResponse.statusCode, 200)

  const bootstrapResponse = await app.inject({ method: 'GET', url: '/app/bootstrap', headers })
  assert.equal(bootstrapResponse.statusCode, 200)
  const bootstrap = bootstrapResponse.json<{
    members: Array<{ id: string }>
    memberData: Record<string, { trainingRecords: unknown[]; readMessageIds: string[] }>
  }>()
  assert.equal(bootstrap.members.length, 1)
  assert.equal(bootstrap.memberData[member.id]?.trainingRecords.length, 1)
  assert.deepEqual(bootstrap.memberData[member.id]?.readMessageIds, ['training-2026-08-20'])
})

test('另一个账号不能写入未授权成员', async () => {
  const owner = await login('owner')
  const ownerHeaders = { authorization: `Bearer ${owner.accessToken}` }
  const memberResponse = await app.inject({
    method: 'POST',
    url: '/family/members',
    headers: ownerHeaders,
    payload: { name: '权限测试', relationship: '父亲', stage: '居家训练' }
  })
  const member = memberResponse.json<{ id: string }>()
  const outsider = await login('outsider')
  const response = await app.inject({
    method: 'PUT',
    url: `/training-plans/${member.id}`,
    headers: { authorization: `Bearer ${outsider.accessToken}` },
    payload: { version: 3, tasks: [{ name: '膝关节屈伸' }] }
  })
  assert.equal(response.statusCode, 403)
  assert.equal(response.json().error.code, 'FORBIDDEN')
})
