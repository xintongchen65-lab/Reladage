export type MemberRole = 'elder' | 'family' | 'doctor' | 'therapist'

export interface Account {
  id: string
  openId: string
  displayName: string
}

export interface FamilyMember {
  id: string
  name: string
  relationship: string
  stage: string
  createdAt: string
}

export interface DeviceSnapshot {
  id?: string | undefined
  serialNumber: string
  deviceName: string
  connected?: boolean | undefined
  battery?: number | undefined
  firmware?: string | undefined
  jointId?: string | undefined
  bodyMode?: string | undefined
  storage?: Record<string, unknown> | undefined
  imu?: Array<Record<string, unknown>> | undefined
}

export interface TrainingPlan {
  version: number
  patient_name?: string | undefined
  tasks: Array<Record<string, unknown>>
  advanced?: Record<string, unknown> | undefined
  game?: Record<string, unknown> | undefined
  updated_at?: string | undefined
  storage_target?: string | undefined
}

export interface TrainingSessionInput {
  id?: string | undefined
  externalSessionId: string
  date: string
  startedAt?: string | undefined
  endedAt?: string | undefined
  status: string
  planned?: boolean | undefined
  completion?: number | undefined
  qualified?: number | undefined
  minutes?: number | undefined
  maxAngle?: number | undefined
  completedReps?: number | undefined
  qualifiedReps?: number | undefined
  plannedReps?: number | undefined
  reasons?: Record<string, number> | undefined
  interrupted?: number | undefined
  source?: string | undefined
  sessions?: Array<Record<string, unknown>> | undefined
}

export interface BootstrapPayload {
  account: { userId: string; displayName: string; status: 'authenticated' }
  members: FamilyMember[]
  currentMemberId: string
  memberData: Record<string, Record<string, unknown>>
}

export interface AppRepository {
  upsertWechatUser(openId: string, unionId?: string, displayName?: string): Promise<Account>
  getBootstrap(userId: string): Promise<BootstrapPayload>
  createMember(userId: string, input: Omit<FamilyMember, 'id' | 'createdAt'>): Promise<FamilyMember>
  bindDevice(userId: string, input: { memberId: string; serialNumber: string; deviceName?: string | undefined }): Promise<DeviceSnapshot>
  saveTrainingPlan(userId: string, memberId: string, plan: TrainingPlan): Promise<TrainingPlan>
  saveTrainingSession(userId: string, memberId: string, record: TrainingSessionInput): Promise<TrainingSessionInput>
  saveMessageReadState(userId: string, memberId: string, ids: string[]): Promise<string[]>
  close?(): Promise<void>
}

export interface WechatSession {
  openId: string
  unionId?: string | undefined
  sessionKey?: string | undefined
}

export interface WechatAuthClient {
  exchangeCode(code: string): Promise<WechatSession>
}

export interface TokenClaims {
  userId: string
  openId: string
}

export interface TokenService {
  sign(claims: TokenClaims): Promise<string>
  verify(token: string): Promise<TokenClaims>
}
