import { randomUUID } from 'node:crypto'
import { AppError, ForbiddenError, NotFoundError } from '../errors.js'
import type {
  Account,
  AppRepository,
  BootstrapPayload,
  DeviceSnapshot,
  FamilyMember,
  TrainingPlan,
  TrainingSessionInput
} from '../types.js'

type Row = Record<string, unknown>
type QueryValue = string | number | boolean | undefined

const asNumber = (value: unknown) => Number(value || 0)

function dateOnly(value: unknown): string {
  return new Date(String(value)).toISOString().slice(0, 10)
}

function mapMember(row: Row): FamilyMember {
  return {
    id: String(row.id),
    name: String(row.name),
    relationship: String(row.relationship),
    stage: String(row.stage),
    createdAt: dateOnly(row.created_at)
  }
}

function mapDevice(row: Row): DeviceSnapshot {
  const status = row.last_status && typeof row.last_status === 'object'
    ? row.last_status as Record<string, unknown>
    : {}
  return {
    id: String(row.id),
    serialNumber: String(row.serial_number),
    deviceName: String(row.device_name),
    connected: false,
    battery: asNumber(status.battery),
    firmware: String(row.firmware_version || status.firmware || ''),
    jointId: String(status.joint_id || ''),
    bodyMode: String(status.body_mode || ''),
    storage: status.storage && typeof status.storage === 'object'
      ? status.storage as Record<string, unknown>
      : {},
    imu: Array.isArray(status.imu) ? status.imu as Array<Record<string, unknown>> : []
  }
}

export class CloudBaseRestRepository implements AppRepository {
  private readonly baseUrl: string

  constructor(
    envId: string,
    private readonly apiKey: string,
    private readonly request: typeof fetch = fetch
  ) {
    this.baseUrl = `https://${envId}.api.tcloudbasegateway.com/v1/rdb/rest`
  }

  private async call<T>(
    method: 'GET' | 'POST' | 'PATCH' | 'DELETE',
    resource: string,
    query: Record<string, QueryValue> = {},
    body?: unknown,
    prefer?: string
  ): Promise<T> {
    const url = new URL(`${this.baseUrl}/${resource}`)
    for (const [key, value] of Object.entries(query)) {
      if (value !== undefined) url.searchParams.set(key, String(value))
    }
    const headers: Record<string, string> = {
      Authorization: `Bearer ${this.apiKey}`,
      Accept: 'application/json'
    }
    if (body !== undefined) headers['Content-Type'] = 'application/json'
    if (prefer) headers.Prefer = prefer
    const response = await this.request(url, {
      method,
      headers,
      ...(body !== undefined ? { body: JSON.stringify(body) } : {})
    })
    if (!response.ok) {
      const code = response.status === 401 || response.status === 403
        ? 'DATABASE_AUTH_FAILED'
        : 'DATABASE_REQUEST_FAILED'
      const message = response.status === 401 || response.status === 403
        ? '服务器数据库凭证无效'
        : '数据库服务暂时不可用'
      throw new AppError(502, code, message)
    }
    const text = await response.text()
    return (text ? JSON.parse(text) : null) as T
  }

  private async rows(resource: string, query: Record<string, QueryValue>): Promise<Row[]> {
    return this.call<Row[]>('GET', resource, query)
  }

  private async insert(resource: string, body: Row | Row[]): Promise<Row[]> {
    return this.call<Row[]>(
      'POST',
      resource,
      { select: '*' },
      body,
      'return=representation'
    )
  }

  private async patch(resource: string, query: Record<string, QueryValue>, body: Row): Promise<Row[]> {
    return this.call<Row[]>(
      'PATCH',
      resource,
      { ...query, select: '*' },
      body,
      'return=representation'
    )
  }

  private async remove(resource: string, query: Record<string, QueryValue>): Promise<void> {
    await this.call<null>('DELETE', resource, query)
  }

  private async assertAccess(userId: string, memberId: string): Promise<void> {
    const rows = await this.rows('member_permissions', {
      select: 'member_id',
      user_id: `eq.${userId}`,
      member_id: `eq.${memberId}`,
      limit: 1
    })
    if (!rows.length) throw new ForbiddenError()
  }

  async upsertWechatUser(openId: string, unionId = '', displayName = '微信用户'): Promise<Account> {
    const existing = await this.rows('app_users', {
      select: 'id,wechat_openid,display_name',
      wechat_openid: `eq.${openId}`,
      limit: 1
    })
    let row = existing[0]
    if (row) {
      const updated = await this.patch('app_users', { wechat_openid: `eq.${openId}` }, {
        ...(unionId ? { wechat_unionid: unionId } : {}),
        updated_at: new Date().toISOString()
      })
      row = updated[0] || row
    } else {
      const inserted = await this.insert('app_users', {
        id: randomUUID(),
        wechat_openid: openId,
        ...(unionId ? { wechat_unionid: unionId } : {}),
        display_name: displayName
      })
      row = inserted[0]
    }
    if (!row) throw new AppError(502, 'DATABASE_WRITE_FAILED', '用户写入失败')
    return {
      id: String(row.id),
      openId: String(row.wechat_openid),
      displayName: String(row.display_name)
    }
  }

  async getBootstrap(userId: string): Promise<BootstrapPayload> {
    const accounts = await this.rows('app_users', {
      select: 'id,display_name',
      id: `eq.${userId}`,
      limit: 1
    })
    const account = accounts[0]
    if (!account) throw new NotFoundError('用户不存在')

    const permissions = await this.rows('member_permissions', {
      select: 'member_id',
      user_id: `eq.${userId}`
    })
    const members: FamilyMember[] = []
    for (const permission of permissions) {
      const memberRows = await this.rows('family_members', {
        select: 'id,name,relationship,stage,created_at',
        id: `eq.${String(permission.member_id)}`,
        archived_at: 'is.null',
        limit: 1
      })
      if (memberRows[0]) members.push(mapMember(memberRows[0]))
    }
    members.sort((left, right) => left.createdAt.localeCompare(right.createdAt))

    const memberData: BootstrapPayload['memberData'] = {}
    for (const member of members) {
      const [bindings, plans, sessions, readRows] = await Promise.all([
        this.rows('member_devices', {
          select: 'device_id',
          member_id: `eq.${member.id}`,
          active: 'eq.true',
          order: 'bound_at.desc',
          limit: 1
        }),
        this.rows('training_plans', {
          select: 'payload',
          member_id: `eq.${member.id}`,
          order: 'revision.desc',
          limit: 1
        }),
        this.rows('training_sessions', {
          select: '*',
          member_id: `eq.${member.id}`,
          order: 'training_date.desc,created_at.desc',
          limit: 180
        }),
        this.rows('message_reads', {
          select: 'message_id',
          user_id: `eq.${userId}`,
          member_id: `eq.${member.id}`
        })
      ])
      const binding = bindings[0]
      const deviceRows = binding
        ? await this.rows('devices', {
            select: 'id,serial_number,device_name,firmware_version,last_status',
            id: `eq.${String(binding.device_id)}`,
            limit: 1
          })
        : []
      memberData[member.id] = {
        ...(deviceRows[0] ? { device: mapDevice(deviceRows[0]) } : {}),
        ...(plans[0]?.payload ? { trainingPlan: plans[0].payload } : {}),
        trainingRecords: sessions.map(row => ({
          id: String(row.external_session_id),
          externalSessionId: String(row.external_session_id),
          date: dateOnly(row.training_date),
          startedAt: row.started_at,
          endedAt: row.ended_at,
          status: row.status,
          planned: row.planned,
          completion: asNumber(row.completion),
          qualified: asNumber(row.qualified),
          minutes: asNumber(row.minutes),
          maxAngle: asNumber(row.max_angle),
          completedReps: asNumber(row.completed_reps),
          qualifiedReps: asNumber(row.qualified_reps),
          plannedReps: asNumber(row.planned_reps),
          reasons: row.reasons || {},
          interrupted: asNumber(row.interrupted),
          source: row.source,
          sessions: row.items || []
        })),
        readMessageIds: readRows.map(row => String(row.message_id))
      }
    }
    return {
      account: {
        userId: String(account.id),
        displayName: String(account.display_name),
        status: 'authenticated'
      },
      members,
      currentMemberId: members[0]?.id || '',
      memberData
    }
  }

  async createMember(userId: string, input: Omit<FamilyMember, 'id' | 'createdAt'>): Promise<FamilyMember> {
    const memberId = randomUUID()
    const inserted = await this.insert('family_members', {
      id: memberId,
      name: input.name,
      relationship: input.relationship,
      stage: input.stage,
      created_by: userId
    })
    try {
      await this.insert('member_permissions', {
        user_id: userId,
        member_id: memberId,
        role: 'family'
      })
    } catch (error) {
      await this.remove('family_members', { id: `eq.${memberId}` })
      throw error
    }
    const row = inserted[0]
    if (!row) throw new AppError(502, 'DATABASE_WRITE_FAILED', '成员写入失败')
    return mapMember(row)
  }

  async bindDevice(
    userId: string,
    input: { memberId: string; serialNumber: string; deviceName?: string | undefined }
  ): Promise<DeviceSnapshot> {
    await this.assertAccess(userId, input.memberId)
    const existing = await this.rows('devices', {
      select: '*',
      serial_number: `eq.${input.serialNumber}`,
      limit: 1
    })
    let device = existing[0]
    if (device) {
      const updated = await this.patch('devices', { id: `eq.${String(device.id)}` }, {
        device_name: input.deviceName || 'RehabMotion',
        updated_at: new Date().toISOString()
      })
      device = updated[0] || device
    } else {
      const inserted = await this.insert('devices', {
        id: randomUUID(),
        serial_number: input.serialNumber,
        device_name: input.deviceName || 'RehabMotion'
      })
      device = inserted[0]
    }
    if (!device) throw new AppError(502, 'DATABASE_WRITE_FAILED', '设备写入失败')

    await this.patch('member_devices', {
      member_id: `eq.${input.memberId}`,
      active: 'eq.true'
    }, {
      active: false,
      unbound_at: new Date().toISOString()
    })
    const bindings = await this.rows('member_devices', {
      select: 'member_id,device_id',
      member_id: `eq.${input.memberId}`,
      device_id: `eq.${String(device.id)}`,
      limit: 1
    })
    if (bindings[0]) {
      await this.patch('member_devices', {
        member_id: `eq.${input.memberId}`,
        device_id: `eq.${String(device.id)}`
      }, {
        active: true,
        bound_at: new Date().toISOString(),
        unbound_at: null
      })
    } else {
      await this.insert('member_devices', {
        member_id: input.memberId,
        device_id: device.id,
        active: true
      })
    }
    return mapDevice(device)
  }

  async saveTrainingPlan(userId: string, memberId: string, plan: TrainingPlan): Promise<TrainingPlan> {
    await this.assertAccess(userId, memberId)
    const rows = await this.rows('training_plans', {
      select: 'revision',
      member_id: `eq.${memberId}`,
      order: 'revision.desc',
      limit: 1
    })
    const revision = asNumber(rows[0]?.revision) + 1
    const payload = {
      ...plan,
      updated_at: new Date().toISOString(),
      server_revision: revision
    }
    await this.insert('training_plans', {
      id: randomUUID(),
      member_id: memberId,
      revision,
      schema_version: Number(plan.version || 1),
      payload,
      created_by: userId
    })
    return payload
  }

  async saveTrainingSession(
    userId: string,
    memberId: string,
    record: TrainingSessionInput
  ): Promise<TrainingSessionInput> {
    await this.assertAccess(userId, memberId)
    const values: Row = {
      member_id: memberId,
      external_session_id: record.externalSessionId,
      training_date: record.date,
      started_at: record.startedAt || null,
      ended_at: record.endedAt || null,
      status: record.status,
      planned: record.planned !== false,
      completion: record.completion || 0,
      qualified: record.qualified || 0,
      minutes: record.minutes || 0,
      max_angle: record.maxAngle || 0,
      completed_reps: record.completedReps || 0,
      qualified_reps: record.qualifiedReps || 0,
      planned_reps: record.plannedReps || 0,
      reasons: record.reasons || {},
      interrupted: record.interrupted || 0,
      source: record.source || 'device',
      items: record.sessions || [],
      updated_at: new Date().toISOString()
    }
    const existing = await this.rows('training_sessions', {
      select: 'id',
      member_id: `eq.${memberId}`,
      external_session_id: `eq.${record.externalSessionId}`,
      limit: 1
    })
    let sessionId = existing[0]?.id ? String(existing[0].id) : ''
    if (sessionId) {
      await this.patch('training_sessions', { id: `eq.${sessionId}` }, values)
    } else {
      sessionId = randomUUID()
      await this.insert('training_sessions', { id: sessionId, ...values })
    }
    await this.remove('training_session_items', { session_id: `eq.${sessionId}` })
    const items = (record.sessions || []).map((item, index) => ({
      id: randomUUID(),
      session_id: sessionId,
      item_order: index + 1,
      payload: item
    }))
    if (items.length) await this.insert('training_session_items', items)
    return { ...record, id: sessionId }
  }

  async saveMessageReadState(userId: string, memberId: string, ids: string[]): Promise<string[]> {
    await this.assertAccess(userId, memberId)
    const uniqueIds = [...new Set(ids)]
    await this.remove('message_reads', {
      user_id: `eq.${userId}`,
      member_id: `eq.${memberId}`
    })
    if (uniqueIds.length) {
      await this.insert('message_reads', uniqueIds.map(messageId => ({
        user_id: userId,
        member_id: memberId,
        message_id: messageId
      })))
    }
    return uniqueIds
  }
}
