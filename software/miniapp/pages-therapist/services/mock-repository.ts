import type {
  AiSuggestion,
  AlertItem,
  DispatchResult,
  PatientDetail,
  PatientSummary,
  TherapistDashboard,
  TherapistProfile,
  TrainingRecord,
  TrendPoint
} from '../types'
import type { TherapistRepository } from './repository'

const MANAGED_PATIENT_COUNT = 32
const FAMILY_PLAN_KEY = 'rm_training_task_plan_v3'
const DEVICE_CONFIG_KEY = 'rm_device_config_v2'
const DISPATCH_RECEIPT_KEY = 'rehabmotion_demo_dispatch_receipt_v1'

const patients: PatientSummary[] = [
  { id: 'RM-1024', name: '王建国', initial: '王', stage: '膝关节术后第6周', exercise: '膝关节屈伸训练', completed: 5, planned: 6, rom: 87, romDelta: 15, quality: 87, qualityLabel: '整体稳定', lastTraining: '今天 10:12', status: 'stable', statusLabel: '稳定', avatarTone: 'green', trainedToday: true },
  { id: 'RM-1027', name: '李淑兰', initial: '李', stage: '肩关节功能恢复期', exercise: '肩胛平面抬手', completed: 4, planned: 5, rom: 83, romDelta: 1, quality: 72, qualityLabel: '存在代偿', lastTraining: '今天 08:16', status: 'watch', statusLabel: '需关注', avatarTone: 'purple', trainedToday: true },
  { id: 'RM-1031', name: '陈志强', initial: '陈', stage: '膝关节术后第3周', exercise: '膝关节屈伸训练', completed: 2, planned: 5, rom: 61, romDelta: -4, quality: 65, qualityLabel: '近期下降', lastTraining: '3天前', status: 'critical', statusLabel: '重点关注', avatarTone: 'orange', trainedToday: false },
  { id: 'RM-1038', name: '刘建华', initial: '刘', stage: '肘关节恢复期', exercise: '哑铃弯举', completed: 5, planned: 5, rom: 96, romDelta: 4, quality: 91, qualityLabel: '动作良好', lastTraining: '今天 07:54', status: 'stable', statusLabel: '稳定', avatarTone: 'blue', trainedToday: true },
  { id: 'RM-1042', name: '张桂芳', initial: '张', stage: '下肢力量恢复期', exercise: '坐到站训练', completed: 3, planned: 5, rom: 74, romDelta: 2, quality: 78, qualityLabel: '节奏偏快', lastTraining: '昨天 18:20', status: 'watch', statusLabel: '需关注', avatarTone: 'rose', trainedToday: false },
  { id: 'RM-1048', name: '赵国庆', initial: '赵', stage: '腕关节功能恢复期', exercise: '腕关节屈伸训练', completed: 4, planned: 4, rom: 72, romDelta: 3, quality: 88, qualityLabel: '动作良好', lastTraining: '今天 10:06', status: 'stable', statusLabel: '稳定', avatarTone: 'sand', trainedToday: true }
]

const alerts: AlertItem[] = [
  {
    id: 'alert-rom-1031', patientId: 'RM-1031', patient: '陈志强',
    title: '膝关节活动角度连续下降', detail: '最近3次训练由68°下降至61°',
    category: 'trend', level: 'critical', tag: '角度趋势', time: '12分钟前',
    metricLabel: '最近3次最大活动角度',
    trend: [{label:'8月18日',value:68},{label:'8月20日',value:65},{label:'今天',value:61}],
    recommendation: '建议先复核佩戴位置与动作完成情况，再决定是否调整目标角度。'
  },
  {
    id: 'alert-motion-1024', patientId: 'RM-1024', patient: '王建国',
    title: '动作过快提示仍需关注', detail: '最近3次共出现3次动作过快提示',
    category: 'motion', level: 'watch', tag: '动作质量', time: '28分钟前',
    metricLabel: '最近3次动作过快提示次数',
    trend: [{label:'8月26日',value:3},{label:'8月28日',value:2},{label:'今天',value:1}],
    recommendation: '建议小幅调整目标角度后继续观察动作速度与稳定性，避免因追求幅度而加快动作。'
  },
  {
    id: 'alert-motion-1027', patientId: 'RM-1027', patient: '李淑兰',
    title: '肩部代偿出现频率增加', detail: '最近5次训练中出现4次代偿提示',
    category: 'motion', level: 'watch', tag: '动作质量', time: '1小时前',
    metricLabel: '最近5次代偿提示次数',
    trend: [{label:'第1次',value:1},{label:'第2次',value:1},{label:'第3次',value:2},{label:'第4次',value:3},{label:'第5次',value:4}],
    recommendation: '建议查看动作质量记录，确认抬手高度和肩胛控制是否需要重新指导。'
  },
  {
    id: 'alert-adherence-1042', patientId: 'RM-1042', patient: '张桂芳',
    title: '本周计划完成不足', detail: '本周计划5次，目前完成3次',
    category: 'adherence', level: 'watch', tag: '训练执行', time: '2小时前',
    metricLabel: '本周累计完成次数',
    trend: [{label:'周一',value:1},{label:'周二',value:1},{label:'周三',value:2},{label:'周四',value:2},{label:'今天',value:3}],
    recommendation: '建议先了解未完成原因，并根据实际生活节奏调整训练安排。'
  }
]

const suggestions: AiSuggestion[] = [
  { id: 'ai-1024', patientId: 'RM-1024', patient: '王建国', exercise: '膝关节屈伸训练', fromAngle: 60, toAngle: 65, minAngle: 55, maxAngle: 80, completion: 83, qualifiedRate: 87, stability: '连续4周提升', sets: 3, reps: 10, holdSec: 2, frequencyPerWeek: 5, reason: '近4周最大活动角度由72°提升至87°，动作达标率由77%提升至87%，稳定性提高且左右差异缩小，建议小幅调整目标并继续观察。', status: 'pending' },
  { id: 'ai-1038', patientId: 'RM-1038', patient: '刘建华', exercise: '哑铃弯举', fromAngle: 90, toAngle: 95, minAngle: 70, maxAngle: 110, completion: 100, qualifiedRate: 91, stability: '连续3次稳定', sets: 3, reps: 10, holdSec: 2, frequencyPerWeek: 5, reason: '近期训练完成稳定，最大活动角度持续提升，可由康复师评估是否小幅提高目标。', status: 'pending' },
  { id: 'ai-1027', patientId: 'RM-1027', patient: '李淑兰', exercise: '肩胛平面抬手', fromAngle: 85, toAngle: 85, minAngle: 60, maxAngle: 100, completion: 80, qualifiedRate: 72, stability: '代偿提示增加', sets: 3, reps: 8, holdSec: 2, frequencyPerWeek: 4, reason: '动作代偿频率有所上升，建议暂时保持当前目标并复核动作。', status: 'pending' }
]

const trendSets: Record<string, TrendPoint[]> = {
  'RM-1024': [{label:'第1周',value:72},{label:'第2周',value:77},{label:'第3周',value:82},{label:'本周',value:87}],
  'RM-1027': [{label:'第1周',value:79},{label:'第2周',value:82},{label:'第3周',value:84},{label:'本周',value:83}],
  'RM-1031': [{label:'第1周',value:70},{label:'第2周',value:68},{label:'第3周',value:65},{label:'本周',value:61}],
  'RM-1038': [{label:'第1周',value:82},{label:'第2周',value:87},{label:'第3周',value:92},{label:'本周',value:96}],
  'RM-1042': [{label:'第1周',value:67},{label:'第2周',value:70},{label:'第3周',value:72},{label:'本周',value:74}],
  'RM-1048': [{label:'第1周',value:63},{label:'第2周',value:66},{label:'第3周',value:69},{label:'本周',value:72}]
}

function buildRecords(patient: PatientSummary): TrainingRecord[] {
  const values = trendSets[patient.id] || []
  return values.slice().reverse().map((point, index) => ({
    id: patient.id + '-record-' + index,
    date: index === 0 ? '今天' : index === 1 ? '8月21日' : index === 2 ? '8月19日' : '8月17日',
    exercise: patient.exercise,
    durationMin: 12 + index,
    reps: Math.max(18, 30 - index * 2),
    rom: point.value,
    qualifiedRate: Math.max(60, patient.quality - index * 2),
    quality: patient.quality >= 85 ? '动作良好' : patient.quality >= 75 ? '基本达标' : '需要复核'
  }))
}

function getPatient(patientId: string): PatientSummary {
  return patients.find(item => item.id === patientId) || patients[0]
}

function getDispatchReceipt(): { suggestionId?: string; targetAngle?: number } {
  const saved = uni.getStorageSync(DISPATCH_RECEIPT_KEY)
  return saved && typeof saved === 'object' ? saved : {}
}

function suggestionSnapshot(item: AiSuggestion): AiSuggestion {
  const receipt = getDispatchReceipt()
  if (receipt.suggestionId !== item.id) return { ...item }
  return { ...item, status: 'sent', toAngle: Number(receipt.targetAngle || item.toAngle) }
}

function persistFamilyPlan(item: AiSuggestion) {
  const currentPlan = uni.getStorageSync(FAMILY_PLAN_KEY)
  if (currentPlan && Array.isArray(currentPlan.tasks)) {
    const nextPlan = {
      ...currentPlan,
      tasks: currentPlan.tasks.map((task: any) => Number(task.exercise_id) === 5 ? {
        ...task,
        sets: item.sets,
        reps: item.reps,
        hold_sec: item.holdSec,
        frequency_per_week: item.frequencyPerWeek,
        target_angle_deg: item.toAngle
      } : task),
      updated_at: new Date().toISOString()
    }
    uni.setStorageSync(FAMILY_PLAN_KEY, nextPlan)
  }
  const config = uni.getStorageSync(DEVICE_CONFIG_KEY) || {}
  uni.setStorageSync(DEVICE_CONFIG_KEY, {
    ...config,
    sets: item.sets,
    reps: item.reps,
    hold_sec: item.holdSec,
    target_angle_deg: item.toAngle,
    valid_angle_deg: Math.max(30, item.toAngle - 10)
  })
  uni.setStorageSync(DISPATCH_RECEIPT_KEY, { suggestionId: item.id, patientId: item.patientId, targetAngle: item.toAngle, sentAt: Date.now() })
}


export const mockRepository: TherapistRepository = {
  async getDashboard(): Promise<TherapistDashboard> {
    const attentionIds = new Set(alerts.map(item => item.patientId))
    const visibleSuggestions = suggestions.map(suggestionSnapshot)
    return {
      managedCount: MANAGED_PATIENT_COUNT,
      todayDone: 18,
      todayTotal: 24,
      attentionPatientCount: attentionIds.size,
      alertCount: alerts.length,
      pendingAiCount: visibleSuggestions.filter(item => item.status === 'pending').length,
      priorityAlerts: alerts.slice(0, 3)
    }
  },
  async listPatients() {
    return patients.map(item => ({ ...item }))
  },
  async getPatientDetail(patientId: string): Promise<PatientDetail> {
    const patient = getPatient(patientId)
    return {
      ...patient,
      gender: patient.id === 'RM-1027' || patient.id === 'RM-1042' ? '女' : '男',
      age: patient.id === 'RM-1024' ? 68 : patient.id === 'RM-1027' ? 65 : 63,
      joined: '2026年7月',
      records: buildRecords(patient),
      romTrend: (trendSets[patient.id] || []).map(item => ({ ...item }))
    }
  },
  async listAlerts() {
    return alerts.map(item => ({ ...item, trend: item.trend.map(point => ({ ...point })) }))
  },
  async getAlert(alertId: string) {
    const item = alerts.find(alert => alert.id === alertId)
    return item ? { ...item, trend: item.trend.map(point => ({ ...point })) } : null
  },
  async listAiSuggestions() {
    return suggestions.map(suggestionSnapshot)
  },
  async getAiSuggestion(id: string) {
    const item = suggestions.find(suggestion => suggestion.id === id)
    return item ? suggestionSnapshot(item) : null
  },
  async approveAiSuggestion(id: string, patch = {}): Promise<DispatchResult> {
    const item = suggestions.find(suggestion => suggestion.id === id)
    if (item) {
      if (patch.targetAngle !== undefined) {
        item.toAngle = Math.min(item.maxAngle, Math.max(item.minAngle, Math.round(patch.targetAngle)))
      }
      item.status = 'sent'
      persistFamilyPlan(item)
    }
    return { status: 'sent', sentAt: Date.now() }
  },
  async getProfile(): Promise<TherapistProfile> {
    return { name: '李敏', role: '康复治疗师', organization: 'RehabMotion 康复中心', managedCount: MANAGED_PATIENT_COUNT, workingYears: 8 }
  }
}
