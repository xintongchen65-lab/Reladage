import Fastify, { type FastifyReply, type FastifyRequest } from 'fastify'
import { z, ZodError } from 'zod'
import { AppError, UnauthorizedError } from './errors.js'
import type { AppRepository, TokenService, WechatAuthClient } from './types.js'

export interface BuildAppOptions {
  repository: AppRepository
  tokenService: TokenService
  wechatAuth: WechatAuthClient
  logger?: boolean
}

const loginSchema = z.object({ code: z.string().min(1).max(128) })
const memberSchema = z.object({
  name: z.string().trim().min(1).max(40),
  relationship: z.string().trim().min(1).max(20),
  stage: z.string().trim().min(1).max(120)
})
const bindDeviceSchema = z.object({
  memberId: z.string().uuid(),
  serialNumber: z.string().trim().min(3).max(80),
  deviceName: z.string().trim().min(1).max(80).optional()
})
const planSchema = z.object({
  version: z.number().int().min(1),
  patient_name: z.string().optional(),
  tasks: z.array(z.record(z.string(), z.unknown())).min(1).max(12),
  advanced: z.record(z.string(), z.unknown()).optional(),
  game: z.record(z.string(), z.unknown()).optional(),
  updated_at: z.string().optional(),
  storage_target: z.string().optional()
})
const sessionSchema = z.object({
  memberId: z.string().uuid(),
  record: z.object({
    id: z.string().optional(),
    externalSessionId: z.string().trim().min(1).max(120),
    date: z.iso.date(),
    startedAt: z.string().optional(),
    endedAt: z.string().optional(),
    status: z.enum(['completed', 'partial', 'not_started', 'cancelled']),
    planned: z.boolean().optional(),
    completion: z.number().min(0).max(100).optional(),
    qualified: z.number().min(0).max(100).optional(),
    minutes: z.number().min(0).max(1440).optional(),
    maxAngle: z.number().min(0).max(360).optional(),
    completedReps: z.number().int().min(0).optional(),
    qualifiedReps: z.number().int().min(0).optional(),
    plannedReps: z.number().int().min(0).optional(),
    reasons: z.record(z.string(), z.number().int().min(0)).optional(),
    interrupted: z.number().int().min(0).optional(),
    source: z.string().max(40).optional(),
    sessions: z.array(z.record(z.string(), z.unknown())).optional()
  })
})
const readStateSchema = z.object({ ids: z.array(z.string().min(1).max(160)).max(500) })
const memberParamsSchema = z.object({ memberId: z.string().uuid() })

function parse<T>(schema: z.ZodType<T>, value: unknown): T {
  const result = schema.safeParse(value)
  if (!result.success) throw new AppError(400, 'VALIDATION_ERROR', result.error.issues[0]?.message || '请求参数无效')
  return result.data
}

export function buildApp(options: BuildAppOptions) {
  const app = Fastify({ logger: options.logger === true, trustProxy: true })
  app.decorateRequest('auth', null)

  const authenticatedUserId = (request: FastifyRequest) => {
    if (!request.auth) throw new UnauthorizedError()
    return request.auth.userId
  }

  const authenticate = async (request: FastifyRequest, _reply: FastifyReply) => {
    const header = request.headers.authorization || ''
    const [scheme, token] = header.split(' ')
    if (scheme !== 'Bearer' || !token) throw new UnauthorizedError()
    request.auth = await options.tokenService.verify(token)
  }

  app.setErrorHandler((error, request, reply) => {
    if (error instanceof AppError) {
      reply.status(error.statusCode).send({ error: { code: error.code, message: error.message } })
      return
    }
    if (error instanceof ZodError) {
      reply.status(400).send({ error: { code: 'VALIDATION_ERROR', message: error.issues[0]?.message || '请求参数无效' } })
      return
    }
    request.log.error(error)
    reply.status(500).send({ error: { code: 'INTERNAL_ERROR', message: '服务器暂时无法处理请求' } })
  })

  app.get('/health', async () => ({
    ok: true,
    service: 'rehabmotion-server',
    time: new Date().toISOString()
  }))

  app.post('/auth/wechat/login', async request => {
    const { code } = parse(loginSchema, request.body)
    const session = await options.wechatAuth.exchangeCode(code)
    const account = await options.repository.upsertWechatUser(session.openId, session.unionId)
    const accessToken = await options.tokenService.sign({ userId: account.id, openId: account.openId })
    return {
      accessToken,
      account: { userId: account.id, displayName: account.displayName, status: 'authenticated' }
    }
  })

  app.get('/app/bootstrap', { preHandler: authenticate }, async request => {
    return options.repository.getBootstrap(authenticatedUserId(request))
  })

  app.post('/family/members', { preHandler: authenticate }, async (request, reply) => {
    const input = parse(memberSchema, request.body)
    const member = await options.repository.createMember(authenticatedUserId(request), input)
    reply.status(201)
    return member
  })

  app.post('/devices/bind', { preHandler: authenticate }, async request => {
    const input = parse(bindDeviceSchema, request.body)
    return options.repository.bindDevice(authenticatedUserId(request), input)
  })

  app.put('/training-plans/:memberId', { preHandler: authenticate }, async request => {
    const { memberId } = parse(memberParamsSchema, request.params)
    const plan = parse(planSchema, request.body)
    return options.repository.saveTrainingPlan(authenticatedUserId(request), memberId, plan)
  })

  app.post('/training-sessions', { preHandler: authenticate }, async (request, reply) => {
    const { memberId, record } = parse(sessionSchema, request.body)
    const saved = await options.repository.saveTrainingSession(authenticatedUserId(request), memberId, record)
    reply.status(201)
    return saved
  })

  app.put('/messages/read-state/:memberId', { preHandler: authenticate }, async request => {
    const { memberId } = parse(memberParamsSchema, request.params)
    const { ids } = parse(readStateSchema, request.body)
    return { ids: await options.repository.saveMessageReadState(authenticatedUserId(request), memberId, ids) }
  })

  return app
}
