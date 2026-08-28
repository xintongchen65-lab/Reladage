import { randomUUID } from 'node:crypto'
import { ForbiddenError, NotFoundError } from '../errors.js'
import type { Account, AppRepository, BootstrapPayload, DeviceSnapshot, FamilyMember, TrainingPlan, TrainingSessionInput } from '../types.js'

interface MemberState {
  device?: DeviceSnapshot | undefined
  trainingPlan?: TrainingPlan | undefined
  trainingRecords: TrainingSessionInput[]
  readMessageIds: string[]
}

export class MemoryRepository implements AppRepository {
  private accounts = new Map<string, Account>()
  private members = new Map<string, FamilyMember>()
  private access = new Map<string, Set<string>>()
  private data = new Map<string, MemberState>()

  private assertAccess(userId: string, memberId: string): void {
    if (!this.access.get(userId)?.has(memberId)) throw new ForbiddenError()
  }

  async upsertWechatUser(openId: string, _unionId = '', displayName = '微信用户'): Promise<Account> {
    const existing = [...this.accounts.values()].find(item => item.openId === openId)
    if (existing) return existing
    const account = { id: randomUUID(), openId, displayName }
    this.accounts.set(account.id, account)
    this.access.set(account.id, new Set())
    return account
  }

  async getBootstrap(userId: string): Promise<BootstrapPayload> {
    const account = this.accounts.get(userId)
    if (!account) throw new NotFoundError('用户不存在')
    const ids = this.access.get(userId) || new Set<string>()
    const members = [...ids].map(id => this.members.get(id)).filter((item): item is FamilyMember => Boolean(item))
    const memberData: BootstrapPayload['memberData'] = {}
    members.forEach(member => {
      const state = this.data.get(member.id) || { trainingRecords: [], readMessageIds: [] }
      memberData[member.id] = {
        ...(state.device ? { device: state.device } : {}),
        ...(state.trainingPlan ? { trainingPlan: state.trainingPlan } : {}),
        trainingRecords: state.trainingRecords,
        readMessageIds: state.readMessageIds
      }
    })
    return {
      account: { userId: account.id, displayName: account.displayName, status: 'authenticated' },
      members,
      currentMemberId: members[0]?.id || '',
      memberData
    }
  }

  async createMember(userId: string, input: Omit<FamilyMember, 'id' | 'createdAt'>): Promise<FamilyMember> {
    if (!this.accounts.has(userId)) throw new NotFoundError('用户不存在')
    const member = { ...input, id: randomUUID(), createdAt: new Date().toISOString().slice(0, 10) }
    this.members.set(member.id, member)
    this.access.get(userId)?.add(member.id)
    this.data.set(member.id, { trainingRecords: [], readMessageIds: [] })
    return member
  }

  async bindDevice(userId: string, input: { memberId: string; serialNumber: string; deviceName?: string | undefined }): Promise<DeviceSnapshot> {
    this.assertAccess(userId, input.memberId)
    const device = { id: randomUUID(), serialNumber: input.serialNumber, deviceName: input.deviceName || 'RehabMotion', connected: false }
    const state = this.data.get(input.memberId) || { trainingRecords: [], readMessageIds: [] }
    state.device = device
    this.data.set(input.memberId, state)
    return device
  }

  async saveTrainingPlan(userId: string, memberId: string, plan: TrainingPlan): Promise<TrainingPlan> {
    this.assertAccess(userId, memberId)
    const state = this.data.get(memberId) || { trainingRecords: [], readMessageIds: [] }
    state.trainingPlan = { ...plan, updated_at: new Date().toISOString() }
    this.data.set(memberId, state)
    return state.trainingPlan
  }

  async saveTrainingSession(userId: string, memberId: string, record: TrainingSessionInput): Promise<TrainingSessionInput> {
    this.assertAccess(userId, memberId)
    const state = this.data.get(memberId) || { trainingRecords: [], readMessageIds: [] }
    const saved = { ...record, id: record.id || randomUUID() }
    state.trainingRecords = [saved, ...state.trainingRecords.filter(item => item.externalSessionId !== record.externalSessionId)]
    this.data.set(memberId, state)
    return saved
  }

  async saveMessageReadState(userId: string, memberId: string, ids: string[]): Promise<string[]> {
    this.assertAccess(userId, memberId)
    const state = this.data.get(memberId) || { trainingRecords: [], readMessageIds: [] }
    state.readMessageIds = [...new Set(ids)]
    this.data.set(memberId, state)
    return state.readMessageIds
  }
}
