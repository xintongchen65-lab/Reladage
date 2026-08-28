import assert from 'node:assert/strict'
import test from 'node:test'
import { AppError } from '../src/errors.js'
import { CloudBaseRestRepository } from '../src/repositories/cloudbase-rest.js'

test('CloudBase 数据访问使用服务端 API Key，并可创建微信用户', async () => {
  const calls: Array<{ url: string; init?: RequestInit | undefined }> = []
  const responses = [
    new Response('[]', { status: 200, headers: { 'content-type': 'application/json' } }),
    new Response(JSON.stringify([{
      id: 'd52cb86b-ff38-455c-a7cf-e4ed73bcab68',
      wechat_openid: 'openid-1',
      display_name: '微信用户'
    }]), { status: 201, headers: { 'content-type': 'application/json' } })
  ]
  const request = (async (input: string | URL | Request, init?: RequestInit) => {
    calls.push({ url: String(input), init })
    const response = responses.shift()
    if (!response) throw new Error('缺少模拟响应')
    return response
  }) as typeof fetch
  const repository = new CloudBaseRestRepository('rehabmotion-test', 'service-role-key', request)

  const account = await repository.upsertWechatUser('openid-1')

  assert.equal(account.openId, 'openid-1')
  assert.equal(calls.length, 2)
  assert.match(calls[0]?.url || '', /rehabmotion-test\.api\.tcloudbasegateway\.com/)
  assert.equal(new Headers(calls[0]?.init?.headers).get('Authorization'), 'Bearer service-role-key')
  assert.equal(calls[1]?.init?.method, 'POST')
  assert.equal(new Headers(calls[1]?.init?.headers).get('Prefer'), 'return=representation')
})

test('CloudBase 拒绝 API Key 时不向客户端泄漏上游响应', async () => {
  const request = (async () => new Response('upstream secret details', { status: 403 })) as typeof fetch
  const repository = new CloudBaseRestRepository('rehabmotion-test', 'bad-key', request)

  await assert.rejects(
    () => repository.upsertWechatUser('openid-2'),
    (error: unknown) => {
      assert.ok(error instanceof AppError)
      assert.equal(error.code, 'DATABASE_AUTH_FAILED')
      assert.equal(error.message, '服务器数据库凭证无效')
      return true
    }
  )
})
