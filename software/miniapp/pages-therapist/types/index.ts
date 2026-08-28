export type TherapistTab = 'home' | 'patients' | 'tasks' | 'profile'

export type TherapistSafeArea = {
  top: number
  bottom: number
  left: number
  right: number
  headerHeight: number
  headerInnerHeight: number
  tabBarHeight: number
  capsuleRight: number
}

export type PatientStatus = 'stable' | 'watch' | 'critical' | 'idle'

export type TrendPoint = {
  label: string
  value: number
}

export type PatientSummary = {
  id: string
  name: string
  initial: string
  stage: string
  exercise: string
  completed: number
  planned: number
  rom: number
  romDelta: number
  quality: number
  qualityLabel: string
  lastTraining: string
  status: PatientStatus
  statusLabel: string
  avatarTone: string
  trainedToday: boolean
}

export type AlertItem = {
  id: string
  patientId: string
  patient: string
  title: string
  detail: string
  category: 'motion' | 'trend' | 'adherence' | 'pain'
  level: 'critical' | 'watch'
  tag: string
  time: string
  metricLabel: string
  trend: TrendPoint[]
  recommendation: string
}

export type AiSuggestion = {
  id: string
  patientId: string
  patient: string
  exercise: string
  fromAngle: number
  toAngle: number
  minAngle: number
  maxAngle: number
  completion: number
  qualifiedRate: number
  stability: string
  sets: number
  reps: number
  holdSec: number
  frequencyPerWeek: number
  reason: string
  status: 'pending' | 'sent'
}

export type TrainingRecord = {
  id: string
  date: string
  exercise: string
  durationMin: number
  reps: number
  rom: number
  qualifiedRate: number
  quality: string
}

export type PatientDetail = PatientSummary & {
  gender: string
  age: number
  joined: string
  records: TrainingRecord[]
  romTrend: TrendPoint[]
}

export type TherapistDashboard = {
  managedCount: number
  todayDone: number
  todayTotal: number
  attentionPatientCount: number
  alertCount: number
  pendingAiCount: number
  priorityAlerts: AlertItem[]
}

export type TherapistProfile = {
  name: string
  role: string
  organization: string
  managedCount: number
  workingYears: number
}

export type PlanPatch = {
  targetAngle?: number
}

export type DispatchResult = {
  status: 'sent'
  sentAt: number
}
