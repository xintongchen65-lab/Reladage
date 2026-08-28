import { exerciseCatalog } from '../data/catalog.js'
import { getDeviceSnapshot, getPrescription, writePrescription } from './device.js'
import { getCurrentMemberId, getCurrentMemberLabel } from './members.js'

const PLAN_KEY = 'rm_training_task_plan_v3'
const SNAPSHOT_KEY = 'rm_training_plan_snapshots_v1'
const DEFAULT_MEMBER_ID = 'father-wang'
const memberKey = key => getCurrentMemberId() === DEFAULT_MEMBER_ID ? key : `${key}_${getCurrentMemberId()}`

export const conservativeAdvancedDefaults = {
  enabled: false,
  tempo_sec_per_rep: 6,
  rest_sec: 60,
  angle_tolerance_deg: 10,
  pain_stop_score: 4,
  max_session_min: 20,
  auto_stop_on_warning: true
}

const taskPresets = {
  1: { sets: 3, reps: 10, frequency_per_week: 5, target_angle_deg: 80, hold_sec: 2 },
  2: { sets: 2, reps: 10, frequency_per_week: 4, target_angle_deg: 45, hold_sec: 2 },
  3: { sets: 2, reps: 8, frequency_per_week: 4, target_angle_deg: 60, hold_sec: 2 },
  4: { sets: 3, reps: 8, frequency_per_week: 5, target_angle_deg: 60, hold_sec: 3 },
  5: { sets: 3, reps: 10, frequency_per_week: 5, target_angle_deg: 70, hold_sec: 2 },
  6: { sets: 3, reps: 8, frequency_per_week: 4, target_angle_deg: 90, hold_sec: 1 },
  7: { sets: 3, reps: 8, frequency_per_week: 3, target_angle_deg: 60, hold_sec: 1 },
  8: { sets: 2, reps: 10, frequency_per_week: 3, target_angle_deg: 60, hold_sec: 1 }
}

export function createTrainingTask(exercise, overrides = {}) {
  const preset = taskPresets[exercise.id] || { sets: 2, reps: 8, frequency_per_week: 3, target_angle_deg: 60, hold_sec: 2 }
  return {
    task_id: overrides.task_id || `task-${exercise.id}-${Date.now()}`,
    exercise_id: exercise.id,
    name: exercise.shortName || exercise.name,
    full_name: exercise.name,
    region: exercise.region || '自定义',
    joint: exercise.joint || '待设置',
    image: exercise.image || '',
    device_supported: Boolean(exercise.deviceSupported),
    source: exercise.source || 'catalog',
    ...preset,
    ...overrides
  }
}

function defaultPlan() {
  const first = createTrainingTask(exerciseCatalog[0], { task_id: 'task-default-upper' })
  const second = createTrainingTask(exerciseCatalog[4], { task_id: 'task-default-lower' })
  return {
    version: 3,
    patient_name: getCurrentMemberLabel(),
    tasks: [first, second],
    advanced: { ...conservativeAdvancedDefaults },
    game: { enabled: false, provider: '', scene_id: '', payload: {} },
    updated_at: ''
  }
}

export function getTrainingTaskPlan() {
  const saved = uni.getStorageSync(memberKey(PLAN_KEY))
  return saved && Array.isArray(saved.tasks) ? saved : defaultPlan()
}

export function getTrainingPlanSnapshots() {
  const saved = uni.getStorageSync(memberKey(SNAPSHOT_KEY))
  return Array.isArray(saved) ? saved : []
}

function saveTrainingPlanSnapshot(plan) {
  const snapshots = getTrainingPlanSnapshots()
  const effectiveAt = plan.updated_at || new Date().toISOString()
  const next = snapshots.filter(item => item.effective_at !== effectiveAt)
  next.push({ effective_at: effectiveAt, plan: JSON.parse(JSON.stringify(plan)) })
  next.sort((a, b) => new Date(a.effective_at) - new Date(b.effective_at))
  uni.setStorageSync(memberKey(SNAPSHOT_KEY), next.slice(-52))
}

function validateIntegerRange(value, min, max, message) {
  const numeric = Number(value)
  if (!Number.isInteger(numeric) || numeric < min || numeric > max) throw new Error(message)
  return numeric
}

function validateTask(task, index) {
  const prefix = `第 ${index + 1} 个动作`
  if (!String(task.name || '').trim()) throw new Error(`${prefix}缺少名称`)
  validateIntegerRange(task.sets, 1, 5, `${prefix}组数范围为 1—5`)
  validateIntegerRange(task.reps, 1, 30, `${prefix}次数范围为 1—30`)
  validateIntegerRange(task.frequency_per_week, 1, 7, `${prefix}频率范围为每周 1—7 天`)
  validateIntegerRange(task.target_angle_deg, 30, 120, `${prefix}目标角度范围为 30°—120°`)
  validateIntegerRange(task.hold_sec, 0, 15, `${prefix}保持时间范围为 0—15 秒`)
}

export function writeTrainingTaskPlan(input) {
  const tasks = Array.isArray(input.tasks) ? input.tasks : []
  if (!tasks.length) return Promise.reject(new Error('请至少选择 1 个训练动作'))
  if (tasks.length > 12) return Promise.reject(new Error('单个计划最多包含 12 个动作'))

  try {
    tasks.forEach(validateTask)
  } catch (error) {
    return Promise.reject(error)
  }

  const advanced = input.advanced && input.advanced.enabled
    ? { ...conservativeAdvancedDefaults, ...input.advanced, enabled: true }
    : { ...conservativeAdvancedDefaults }

  if (advanced.enabled) {
    advanced.tempo_sec_per_rep = validateIntegerRange(advanced.tempo_sec_per_rep, 3, 12, '动作节奏范围为 3—12 秒')
    advanced.rest_sec = validateIntegerRange(advanced.rest_sec, 20, 180, '组间休息范围为 20—180 秒')
    advanced.angle_tolerance_deg = validateIntegerRange(advanced.angle_tolerance_deg, 5, 30, '角度容差范围为 5°—30°')
    advanced.pain_stop_score = validateIntegerRange(advanced.pain_stop_score, 1, 10, '疼痛停止阈值范围为 1—10')
    advanced.max_session_min = validateIntegerRange(advanced.max_session_min, 10, 60, '单次最长时间范围为 10—60 分钟')
  }

  const plan = {
    version: 3,
    patient_name: input.patient_name || getCurrentMemberLabel(),
    tasks: tasks.map((task, index) => ({
      ...task,
      sets: Number(task.sets),
      reps: Number(task.reps),
      frequency_per_week: Number(task.frequency_per_week),
      target_angle_deg: Number(task.target_angle_deg),
      hold_sec: Number(task.hold_sec),
      order: index + 1
    })),
    advanced,
    game: { enabled: false, provider: '', scene_id: '', payload: {}, ...(input.game || {}) },
    updated_at: new Date().toISOString(),
    storage_target: 'eMMC/TNAND'
  }

  uni.setStorageSync(memberKey(PLAN_KEY), plan)
  saveTrainingPlanSnapshot(plan)

  const primary = plan.tasks.find(task => task.device_supported)
  const demoAcknowledged = getDeviceSnapshot().transport === 'DEMO' && getDeviceSnapshot().connected
  if (!primary) return Promise.resolve({ ok: true, command: 'SET_TASK_LIST', plan, deviceAcknowledged: demoAcknowledged })

  const isKnee = primary.exercise_id === 5
  const legacy = getPrescription()
  return writePrescription({
    ...legacy,
    body_mode: isKnee ? 'lower' : 'upper',
    joint_id: isKnee ? 'knee' : 'elbow',
    exercise: isKnee ? 'knee_flexion' : 'elbow_flexion',
    train_mode: plan.game.enabled ? 'game' : 'standard',
    sets: primary.sets,
    reps: Math.max(5, primary.reps),
    target_angle_deg: primary.target_angle_deg,
    valid_angle_deg: Math.max(20, primary.target_angle_deg - advanced.angle_tolerance_deg),
    return_angle_deg: Math.max(5, Math.min(primary.target_angle_deg - advanced.angle_tolerance_deg - 5, 20)),
    hold_sec: primary.hold_sec,
    rest_sec: advanced.rest_sec
  }).then(() => ({ ok: true, command: 'SET_TASK_LIST', plan, deviceAcknowledged: demoAcknowledged }))
}
