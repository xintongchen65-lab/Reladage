import { setCurrentMember } from './members.js'

const KEYS = {
  history: 'rm_history_v2',
  device: 'rm_device_snapshot_v2',
  config: 'rm_device_config_v2',
  plan: 'rm_training_task_plan_v3',
  planSnapshots: 'rm_training_plan_snapshots_v1',
  readMessages: 'rehabmotion_read_messages_v2',
  scene: 'rehabmotion_demo_scene_v1',
  referenceDate: 'rehabmotion_demo_reference_date_v1',
  dispatchReceipt: 'rehabmotion_demo_dispatch_receipt_v1',
  videoVersion: 'rehabmotion_video_dataset_version_v1'
}

export const DEMO_SCENES = Object.freeze({
  BEFORE: 'before',
  COMPLETED: 'completed',
  ATTENTION: 'attention',
  VIDEO: 'video'
})

const clone = value => JSON.parse(JSON.stringify(value))
const pad = value => String(value).padStart(2, '0')
let demoReferenceDate = new Date()

function setDemoReference(scene) {
  const date = new Date()
  date.setHours(12, 0, 0, 0)
  if (scene === DEMO_SCENES.VIDEO) {
    const daysToSunday = (7 - date.getDay()) % 7
    date.setDate(date.getDate() + daysToSunday)
  }
  demoReferenceDate = date
}

function restoreDemoReference() {
  const saved = String(uni.getStorageSync(KEYS.referenceDate) || '')
  if (!/^\d{4}-\d{2}-\d{2}$/.test(saved)) return
  const parsed = new Date(`${saved}T12:00:00`)
  if (!Number.isNaN(parsed.getTime())) demoReferenceDate = parsed
}

function dateFromOffset(offset) {
  const date = new Date(demoReferenceDate)
  date.setHours(12, 0, 0, 0)
  date.setDate(date.getDate() - offset)
  return `${date.getFullYear()}-${pad(date.getMonth() + 1)}-${pad(date.getDate())}`
}

function isoAtOffset(offset) {
  return `${dateFromOffset(offset)}T08:00:00.000Z`
}

function makeSession({ date, id, exerciseId, name, status, plannedReps, completedReps, qualifiedReps, minutes, maxAngle }) {
  return { id: `${date}-${id}`, exerciseId, name, status, plannedReps, completedReps, qualifiedReps, minutes, maxAngle }
}

function buildTodayRecord(completed = false, attention = false) {
  const date = dateFromOffset(0)
  const thirdCompleted = completed
  const sessions = [
    makeSession({ date, id: 'knee', exerciseId: 5, name: '膝关节屈伸', status: 'completed', plannedReps: 20, completedReps: 20, qualifiedReps: attention ? 15 : 17, minutes: 13, maxAngle: 86 }),
    makeSession({ date, id: 'sit', exerciseId: 6, name: '坐到站训练', status: 'completed', plannedReps: 8, completedReps: 8, qualifiedReps: attention ? 6 : 7, minutes: 8, maxAngle: 87 }),
    makeSession({ date, id: 'step', exerciseId: 8, name: '台阶踩踏', status: thirdCompleted ? 'completed' : 'not_started', plannedReps: 10, completedReps: thirdCompleted ? 10 : 0, qualifiedReps: thirdCompleted ? 9 : 0, minutes: thirdCompleted ? 8 : 0, maxAngle: thirdCompleted ? 82 : 0 })
  ]
  const completedReps = sessions.reduce((sum, item) => sum + item.completedReps, 0)
  const qualifiedReps = sessions.reduce((sum, item) => sum + item.qualifiedReps, 0)
  const plannedReps = sessions.reduce((sum, item) => sum + item.plannedReps, 0)
  const minutes = sessions.reduce((sum, item) => sum + item.minutes, 0)
  return {
    date,
    planned: true,
    status: completed ? 'completed' : 'partial',
    completed,
    completion: completed ? 100 : 67,
    qualified: completedReps ? Math.round(qualifiedReps / completedReps * 100) : 0,
    maxAngle: Math.max(...sessions.map(item => item.maxAngle)),
    minutes,
    exercise: '膝关节屈伸',
    completedReps,
    qualifiedReps,
    plannedReps,
    painAfter: attention ? 3 : 1,
    painBefore: 1,
    fatigue: 2,
    reasons: attention ? { paceFast: 5, holdShort: 2, romLow: 2 } : { paceFast: 1 },
    avgSpeed: 12.4,
    stability: 92,
    lrRomDiff: 3.2,
    compensationCount: 0,
    interrupted: completed ? 0 : 1,
    syncedAfterOffline: true,
    syncedAt: '10:12',
    sessions
  }
}

function buildPastRecord(offset, scene) {
  const date = dateFromOffset(offset)
  const weekday = new Date(`${date}T12:00:00`).getDay()
  const videoRestDay = scene === DEMO_SCENES.VIDEO && [3, 10, 17, 29].includes(offset)
  const planned = weekday !== 0 && !videoRestDay
  if (!planned) return { date, planned: false, status: 'unplanned', completed: false, completion: 0, qualified: 0, maxAngle: 0, minutes: 0, sessions: [] }

  const missed = scene === DEMO_SCENES.VIDEO ? [13, 22].includes(offset) : [5, 13, 22].includes(offset)
  const partial = scene === DEMO_SCENES.VIDEO ? false : [9, 18, 26].includes(offset)
  const band = Math.min(3, Math.floor((offset - 1) / 7))
  const baseQualified = [87, 84, 81, 77][band]
  const exerciseId = offset % 3 === 0 ? 6 : offset % 4 === 0 ? 8 : 5
  const name = exerciseId === 6 ? '坐到站训练' : exerciseId === 8 ? '台阶踩踏' : '膝关节屈伸'
  const plannedReps = exerciseId === 6 ? (scene === DEMO_SCENES.VIDEO && offset === 12 ? 18 : scene === DEMO_SCENES.VIDEO && offset === 18 ? 17 : 16) : 20
  const completedReps = missed ? 0 : partial ? Math.round(plannedReps * 0.6) : plannedReps
  const qualified = missed ? 0 : scene === DEMO_SCENES.VIDEO ? baseQualified : Math.max(68, Math.min(94, baseQualified + offset % 3 - 1))
  const qualifiedReps = Math.round(completedReps * qualified / 100) + (scene === DEMO_SCENES.VIDEO && [1, 2, 24, 25].includes(offset) && !missed ? 1 : 0)
  const status = missed ? 'not_started' : partial ? 'partial' : 'completed'
  const minutes = missed ? 0 : partial ? 10 : scene === DEMO_SCENES.VIDEO ? 13 + offset % 3 - (offset === 1 ? 1 : 0) : 18 + offset % 5
  const maxAngle = missed ? 0 : scene === DEMO_SCENES.VIDEO ? [87, 82, 77, 72][band] - Math.max(0, offset % 7 - 1) + (offset === 23 ? 1 : 0) : 72 + (3 - band) * 4 + offset % 3
  return {
    date,
    planned: true,
    status,
    completed: status === 'completed',
    completion: missed ? 0 : partial ? 60 : 100,
    qualified,
    maxAngle,
    minutes,
    exercise: name,
    completedReps,
    qualifiedReps,
    plannedReps,
    painBefore: 1,
    painAfter: partial ? 2 : 1,
    fatigue: partial ? 3 : 2,
    reasons: missed ? {} : { paceFast: Math.max(1, band + 1), holdShort: band > 1 ? 1 : 0, asymmetry: band > 1 ? 1 : 0 },
    avgSpeed: [12.4, 13.1, 14, 15.2][band],
    stability: [92, 89, 85, 80][band],
    lrRomDiff: [3.2, 4.1, 5.3, 6.8][band],
    compensationCount: band > 1 ? 1 : 0,
    interrupted: 0,
    sessions: [
      makeSession({ date, id: `exercise-${exerciseId}`, exerciseId, name, status, plannedReps, completedReps, qualifiedReps, minutes, maxAngle })
    ]
  }
}

function buildHistory(scene) {
  const records = [buildTodayRecord(scene === DEMO_SCENES.COMPLETED, scene === DEMO_SCENES.ATTENTION)]
  for (let offset = 1; offset <= 29; offset += 1) records.push(buildPastRecord(offset, scene))
  if (scene === DEMO_SCENES.ATTENTION) {
    const yesterday = records.find(item => item.date === dateFromOffset(1))
    if (yesterday && yesterday.status !== 'unplanned') {
      yesterday.qualified = 74
      yesterday.reasons = { paceFast: 6, holdShort: 2, romLow: 2 }
      yesterday.qualifiedReps = Math.round(yesterday.completedReps * .74)
      if (yesterday.sessions[0]) yesterday.sessions[0].qualifiedReps = yesterday.qualifiedReps
    }
  }
  return records
}

function buildPlan() {
  return {
    version: 3,
    patient_name: '父亲 · 王建国',
    tasks: [
      { task_id: 'demo-knee', exercise_id: 5, name: '膝关节屈伸', full_name: '膝关节屈伸', region: '下肢', joint: '膝关节', image: '/static/exercises/knee-flexion-extension.png', device_supported: true, source: 'catalog', sets: 3, reps: 10, frequency_per_week: 5, target_angle_deg: 60, hold_sec: 2, order: 1 },
      { task_id: 'demo-sit', exercise_id: 6, name: '坐到站训练', full_name: '坐到站训练', region: '下肢', joint: '膝关节', image: '/static/exercises/sit-to-stand.png', device_supported: true, source: 'catalog', sets: 3, reps: 8, frequency_per_week: 4, target_angle_deg: 90, hold_sec: 1, order: 2 },
      { task_id: 'demo-step', exercise_id: 8, name: '台阶踩踏', full_name: '台阶踩踏 Step Up', region: '下肢', joint: '膝关节', image: '/static/exercises/step-up.png', device_supported: true, source: 'catalog', sets: 2, reps: 10, frequency_per_week: 3, target_angle_deg: 60, hold_sec: 1, order: 3 }
    ],
    advanced: { enabled: false, tempo_sec_per_rep: 6, rest_sec: 60, angle_tolerance_deg: 10, pain_stop_score: 4, max_session_min: 20, auto_stop_on_warning: true },
    game: { enabled: false, provider: '', scene_id: '', payload: {} },
    updated_at: isoAtOffset(1),
    storage_target: 'eMMC/TNAND'
  }
}

function buildDevice(overrides = {}) {
  return {
    connected: true,
    transport: 'DEMO',
    deviceName: 'RM-Core 01',
    battery: 82,
    firmware: 'v3-J · 20260814',
    joint_id: 'knee',
    body_mode: 'lower',
    storage: { medium: 'SD / eMMC', total_gb: 8, free_gb: 5.6, status: 'NORMAL', last_file: 'session_018.csv' },
    sync: { status: 'SYNCED', lastSyncedAt: '10:12', pendingRecords: 0 },
    imu: [
      { id: 'A', role: '左侧近端', online: true, calibrated: true, signal: 92 },
      { id: 'B', role: '左侧远端', online: true, calibrated: true, signal: 89 },
      { id: 'C', role: '右侧近端', online: true, calibrated: true, signal: 94 },
      { id: 'D', role: '右侧远端', online: true, calibrated: true, signal: 90 }
    ],
    ...overrides
  }
}

export function applyDemoScenario(scene = DEMO_SCENES.BEFORE) {
  const selected = Object.values(DEMO_SCENES).includes(scene) ? scene : DEMO_SCENES.BEFORE
  setDemoReference(selected)
  setCurrentMember('father-wang')
  const plan = buildPlan()
  uni.setStorageSync(KEYS.history, buildHistory(selected))
  uni.setStorageSync(KEYS.device, buildDevice())
  uni.setStorageSync(KEYS.config, {
    body_mode: 'lower', joint_id: 'knee', exercise: 'knee_flexion', train_mode: 'standard',
    side_mode: 'bilateral', sets: 3, reps: 10, target_angle_deg: 60,
    valid_angle_deg: 50, return_angle_deg: 20, hold_sec: 2, rest_sec: 60,
    training_days: 28, quality_enabled: true
  })
  uni.setStorageSync(KEYS.plan, plan)
  uni.setStorageSync(KEYS.planSnapshots, [{ effective_at: isoAtOffset(28), plan: clone(plan) }])
  uni.removeStorageSync(KEYS.readMessages)
  uni.removeStorageSync(KEYS.dispatchReceipt)
  uni.setStorageSync(KEYS.scene, selected)
  uni.setStorageSync(KEYS.referenceDate, dateFromOffset(0))
  if (selected === DEMO_SCENES.VIDEO) uni.setStorageSync(KEYS.videoVersion, 'ican-video-v2')
  return { scene: selected, device: buildDevice(), records: buildHistory(selected), plan }
}

export function completeDemoTraining() {
  restoreDemoReference()
  const record = buildTodayRecord(true, false)
  const stored = uni.getStorageSync(KEYS.history)
  const history = Array.isArray(stored) ? stored : []
  const records = [
    record,
    ...history.filter(item => item && item.date !== record.date)
  ].sort((a, b) => String(b.date).localeCompare(String(a.date)))
  uni.setStorageSync(KEYS.history, records)
  return {
    scene: uni.getStorageSync(KEYS.scene) || DEMO_SCENES.VIDEO,
    record,
    records
  }
}

export function setDemoDeviceState(state) {
  const current = uni.getStorageSync(KEYS.device) || buildDevice()
  let next = buildDevice(current)
  if (state === 'offline') next = { ...next, connected: false }
  if (state === 'low-battery') next = { ...next, connected: true, battery: 14 }
  if (state === 'storage-warning') next = { ...next, connected: true, storage: { ...next.storage, status: 'WARNING', free_gb: .3 } }
  if (state === 'normal') next = buildDevice()
  uni.setStorageSync(KEYS.device, next)
  return next
}

export function getDemoScene() {
  return uni.getStorageSync(KEYS.scene) || DEMO_SCENES.BEFORE
}
