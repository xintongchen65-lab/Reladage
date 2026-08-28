import { jointModes } from '../data/catalog.js'
import { getCurrentMemberId } from './members.js'
import { applyDemoScenario, DEMO_SCENES } from './demo-controller.js'
import {
  beginTrainingSession,
  finishTrainingSession,
  ingestTrainingPacket,
  mergeTrainingDay,
  pauseTrainingSession,
  resumeTrainingSession
} from './training-session-recorder.js'

const KEYS = {
  config: 'rm_device_config_v2', snapshot: 'rm_device_snapshot_v2', history: 'rm_history_v2',
  role: 'rm_view_role_v2', notes: 'rm_doctor_notes_v2'
}

const DEFAULT_MEMBER_ID = 'father-wang'
const VIDEO_DATA_VERSION = 'ican-video-v2'
const VIDEO_DATA_KEY = 'rehabmotion_video_dataset_version_v1'
const DEMO_REFERENCE_KEY = 'rehabmotion_demo_reference_date_v1'
const EXERCISE_NAMES = Object.freeze({
  elbow_flexion: '哑铃弯举',
  knee_flexion: '膝关节屈伸',
  wrist_flexion: '手腕屈伸'
})
const EXERCISE_IDS = Object.freeze({ elbow_flexion: 1, knee_flexion: 5 })
const memberKey = key => getCurrentMemberId() === DEFAULT_MEMBER_ID ? key : `${key}_${getCurrentMemberId()}`

const defaultConfig = {
  body_mode: 'upper', joint_id: 'elbow', exercise: 'elbow_flexion', train_mode: 'standard',
  side_mode: 'bilateral', sets: 3, reps: 10, target_angle_deg: 80,
  valid_angle_deg: 60, return_angle_deg: 20, hold_sec: 2, rest_sec: 30,
  training_days: 28, quality_enabled: true
}

const defaultSnapshot = {
  connected: true, transport: 'DEMO', deviceName: 'RM-Core 01', battery: 82,
  firmware: 'v3-J · 20260814', joint_id: 'elbow', body_mode: 'upper',
  storage: { medium: 'SD / eMMC', total_gb: 8, free_gb: 5.6, status: 'NORMAL', last_file: 'session_018.csv' },
  imu: [
    { id: 'A', role: '左侧近端', online: true, calibrated: true, signal: 92 },
    { id: 'B', role: '左侧远端', online: true, calibrated: true, signal: 89 },
    { id: 'C', role: '右侧近端', online: true, calibrated: true, signal: 94 },
    { id: 'D', role: '右侧远端', online: true, calibrated: true, signal: 90 }
  ]
}

const seedHistory = [
  {
    date: '2026-08-17',
    planned: true,
    status: 'partial',
    completed: false,
    completion: 67,
    qualified: 89,
    maxAngle: 86,
    minutes: 21,
    exercise: '膝关节屈伸',
    completedReps: 27,
    qualifiedReps: 24,
    plannedReps: 37,
    painAfter: 2,
    fatigue: 2,
    reasons: { paceFast: 1, romLow: 1 },
    sessions: [
      { id: '2026-08-17-knee', exerciseId: 5, name: '膝关节屈伸', status: 'completed', plannedReps: 20, completedReps: 20, qualifiedReps: 17, minutes: 13, maxAngle: 86 },
      { id: '2026-08-17-sit', exerciseId: 6, name: '坐到站训练', status: 'completed', plannedReps: 7, completedReps: 7, qualifiedReps: 7, minutes: 8, maxAngle: 0 },
      { id: '2026-08-17-step', exerciseId: 8, name: '台阶踩踏 Step Up', status: 'not_started', plannedReps: 10, completedReps: 0, qualifiedReps: 0, minutes: 0, maxAngle: 0 }
    ]
  },
  { date: '2026-08-16', planned: true, status: 'completed', completed: true, completion: 100, qualified: 89, maxAngle: 85, minutes: 23, exercise: '坐到站训练', completedReps: 30, qualifiedReps: 27, plannedReps: 30, painAfter: 1, fatigue: 2, reasons: { paceFast: 1 } },
  { date: '2026-08-15', planned: true, status: 'partial', completed: false, completion: 43, qualified: 77, maxAngle: 76, minutes: 9, exercise: '膝关节屈伸', completedReps: 13, qualifiedReps: 10, plannedReps: 30, painAfter: 2, fatigue: 3, reasons: { paceFast: 2, holdShort: 1, romLow: 1 } },
  { date: '2026-08-14', planned: true, status: 'completed', completed: true, completion: 100, qualified: 87, maxAngle: 84, minutes: 21, exercise: '膝关节屈伸', completedReps: 30, qualifiedReps: 26, plannedReps: 30, painAfter: 2, fatigue: 2, reasons: { paceFast: 2, holdShort: 1 } },
  { date: '2026-08-13', planned: true, status: 'completed', completed: true, completion: 100, qualified: 91, maxAngle: 82, minutes: 24, exercise: '坐到站训练', completedReps: 30, qualifiedReps: 27, plannedReps: 30, painAfter: 1, fatigue: 2, reasons: { paceFast: 1 } },
  { date: '2026-08-12', planned: true, status: 'completed', completed: true, completion: 100, qualified: 88, maxAngle: 80, minutes: 23, exercise: '膝关节屈伸', completedReps: 30, qualifiedReps: 26, plannedReps: 30, painAfter: 2, fatigue: 2, reasons: { holdShort: 1, romLow: 1 } },
  { date: '2026-08-11', planned: true, status: 'completed', completed: true, completion: 100, qualified: 84, maxAngle: 80, minutes: 20, exercise: '台阶踩踏', completedReps: 30, qualifiedReps: 25, plannedReps: 30, painAfter: 1, fatigue: 2, reasons: { paceFast: 2, holdShort: 1 } },
  { date: '2026-08-10', planned: false, status: 'unplanned', completed: false, completion: 0, qualified: 0, maxAngle: 0, minutes: 0 },
  { date: '2026-08-09', planned: true, status: 'completed', completed: true, completion: 100, qualified: 86, maxAngle: 80, minutes: 22, exercise: '膝关节屈伸', completedReps: 30, qualifiedReps: 26, plannedReps: 30, painAfter: 1, fatigue: 2, reasons: { paceFast: 2, holdShort: 1 } },
  { date: '2026-08-08', planned: true, status: 'not_started', completed: false, completion: 0, qualified: 0, maxAngle: 0, minutes: 0, exercise: '坐到站训练', plannedReps: 30 },
  { date: '2026-08-07', planned: true, status: 'completed', completed: true, completion: 100, qualified: 85, maxAngle: 79, minutes: 21, exercise: '膝关节屈伸', completedReps: 30, qualifiedReps: 26, plannedReps: 30, painAfter: 2, fatigue: 2, reasons: { paceFast: 2, romLow: 1 } },
  { date: '2026-08-06', planned: true, status: 'partial', completed: false, completion: 60, qualified: 78, maxAngle: 72, minutes: 11, exercise: '台阶踩踏', completedReps: 18, qualifiedReps: 14, plannedReps: 30, painAfter: 2, fatigue: 3, reasons: { paceFast: 2, holdShort: 2, romLow: 1 } },
  { date: '2026-08-05', planned: true, status: 'completed', completed: true, completion: 100, qualified: 84, maxAngle: 78, minutes: 20, exercise: '坐到站训练', completedReps: 30, qualifiedReps: 25, plannedReps: 30, painAfter: 1, fatigue: 2, reasons: { paceFast: 2, holdShort: 1 } },
  { date: '2026-08-04', planned: true, status: 'completed', completed: true, completion: 100, qualified: 82, maxAngle: 77, minutes: 20, exercise: '膝关节屈伸', completedReps: 30, qualifiedReps: 25, plannedReps: 30, painAfter: 2, fatigue: 2, reasons: { paceFast: 2, holdShort: 1, romLow: 1 } },
  { date: '2026-08-03', planned: false, status: 'unplanned', completed: false, completion: 0, qualified: 0, maxAngle: 0, minutes: 0 },
  { date: '2026-08-02', planned: true, status: 'completed', completed: true, completion: 100, qualified: 84, maxAngle: 76, minutes: 21, exercise: '膝关节屈伸', completedReps: 30, qualifiedReps: 25, plannedReps: 30, painAfter: 2, fatigue: 2, reasons: { paceFast: 2, holdShort: 1 } },
  { date: '2026-08-01', planned: true, status: 'not_started', completed: false, completion: 0, qualified: 0, maxAngle: 0, minutes: 0, exercise: '坐到站训练', plannedReps: 30 },
  { date: '2026-07-31', planned: true, status: 'completed', completed: true, completion: 100, qualified: 82, maxAngle: 75, minutes: 20, exercise: '膝关节屈伸', completedReps: 30, qualifiedReps: 25, plannedReps: 30, painAfter: 2, fatigue: 2, reasons: { paceFast: 3, holdShort: 1 } },
  { date: '2026-07-30', planned: true, status: 'partial', completed: false, completion: 53, qualified: 75, maxAngle: 69, minutes: 10, exercise: '台阶踩踏', completedReps: 16, qualifiedReps: 12, plannedReps: 30, painAfter: 3, fatigue: 3, reasons: { paceFast: 3, holdShort: 2, romLow: 2 } },
  { date: '2026-07-29', planned: true, status: 'completed', completed: true, completion: 100, qualified: 81, maxAngle: 74, minutes: 19, exercise: '坐到站训练', completedReps: 30, qualifiedReps: 24, plannedReps: 30, painAfter: 2, fatigue: 2, reasons: { paceFast: 3, holdShort: 1, romLow: 1 } },
  { date: '2026-07-28', planned: true, status: 'completed', completed: true, completion: 100, qualified: 79, maxAngle: 73, minutes: 19, exercise: '膝关节屈伸', completedReps: 30, qualifiedReps: 24, plannedReps: 30, painAfter: 2, fatigue: 2, reasons: { paceFast: 3, holdShort: 1, romLow: 1 } },
  { date: '2026-07-27', planned: false, status: 'unplanned', completed: false, completion: 0, qualified: 0, maxAngle: 0, minutes: 0 },
  { date: '2026-07-26', planned: true, status: 'completed', completed: true, completion: 100, qualified: 80, maxAngle: 72, minutes: 20, exercise: '膝关节屈伸', completedReps: 30, qualifiedReps: 24, plannedReps: 30, painAfter: 2, fatigue: 2, reasons: { paceFast: 3, holdShort: 2 } },
  { date: '2026-07-25', planned: true, status: 'not_started', completed: false, completion: 0, qualified: 0, maxAngle: 0, minutes: 0, exercise: '坐到站训练', plannedReps: 30 },
  { date: '2026-07-24', planned: true, status: 'completed', completed: true, completion: 100, qualified: 78, maxAngle: 71, minutes: 19, exercise: '膝关节屈伸', completedReps: 30, qualifiedReps: 23, plannedReps: 30, painAfter: 2, fatigue: 2, reasons: { paceFast: 3, holdShort: 2, romLow: 1 } },
  { date: '2026-07-23', planned: true, status: 'partial', completed: false, completion: 50, qualified: 71, maxAngle: 65, minutes: 9, exercise: '台阶踩踏', completedReps: 15, qualifiedReps: 11, plannedReps: 30, painAfter: 3, fatigue: 3, reasons: { paceFast: 4, holdShort: 2, romLow: 2 } },
  { date: '2026-07-22', planned: true, status: 'completed', completed: true, completion: 100, qualified: 77, maxAngle: 70, minutes: 18, exercise: '坐到站训练', completedReps: 30, qualifiedReps: 23, plannedReps: 30, painAfter: 2, fatigue: 2, reasons: { paceFast: 3, holdShort: 2, romLow: 1 } },
  { date: '2026-07-21', planned: true, status: 'completed', completed: true, completion: 100, qualified: 75, maxAngle: 69, minutes: 18, exercise: '膝关节屈伸', completedReps: 30, qualifiedReps: 23, plannedReps: 30, painAfter: 2, fatigue: 2, reasons: { paceFast: 4, holdShort: 2, romLow: 1 } }
]

let listeners = []
let timer = null
let externalPacketAt = 0
let demo = { tick: 0, leftCount: 0, rightCount: 0, leftRom: 0, rightRom: 0, state: 'IDLE', setIndex: 1 }

export function ensureDeviceSeed() {
  if (getCurrentMemberId() === DEFAULT_MEMBER_ID && uni.getStorageSync(VIDEO_DATA_KEY) !== VIDEO_DATA_VERSION) {
    applyDemoScenario(DEMO_SCENES.VIDEO)
    uni.setStorageSync(VIDEO_DATA_KEY, VIDEO_DATA_VERSION)
    return
  }
  if (!uni.getStorageSync(memberKey(KEYS.config))) uni.setStorageSync(memberKey(KEYS.config), defaultConfig)
  if (!uni.getStorageSync(KEYS.snapshot)) uni.setStorageSync(KEYS.snapshot, defaultSnapshot)
  const savedHistory = uni.getStorageSync(KEYS.history)
  const isLegacyDemo = Array.isArray(savedHistory) && savedHistory.length === 7 && savedHistory.every(item => /^\d{2}-\d{2}$/.test(String(item.date || '')))
  const isAggregateDemo = Array.isArray(savedHistory) && savedHistory[0] && savedHistory[0].date === '2026-08-17' && !Array.isArray(savedHistory[0].sessions)
  if (getCurrentMemberId() === DEFAULT_MEMBER_ID && (!savedHistory || isLegacyDemo || isAggregateDemo)) uni.setStorageSync(KEYS.history, seedHistory)
  if (!uni.getStorageSync(KEYS.role)) uni.setStorageSync(KEYS.role, 'elder')
}

export const getDeviceSnapshot = () => uni.getStorageSync(KEYS.snapshot) || defaultSnapshot
export const getPrescription = () => uni.getStorageSync(memberKey(KEYS.config)) || defaultConfig
export const getHistory = () => {
  const saved = uni.getStorageSync(memberKey(KEYS.history))
  if (Array.isArray(saved)) return saved
  return getCurrentMemberId() === DEFAULT_MEMBER_ID ? seedHistory : []
}
export const getViewerRole = () => uni.getStorageSync(KEYS.role) || 'elder'
export const setViewerRole = role => uni.setStorageSync(KEYS.role, role)
export const getDoctorNotes = () => uni.getStorageSync(memberKey(KEYS.notes)) || '继续保持当前训练强度。若疼痛超过 4 分，请暂停训练并联系康复师。'
export const saveDoctorNotes = value => uni.setStorageSync(memberKey(KEYS.notes), value)

function currentRecordDate() {
  const reference = String(uni.getStorageSync(DEMO_REFERENCE_KEY) || '')
  if (getDeviceSnapshot().transport === 'DEMO' && /^\d{4}-\d{2}-\d{2}$/.test(reference)) return reference
  const date = new Date()
  const pad = value => String(value).padStart(2, '0')
  return `${date.getFullYear()}-${pad(date.getMonth() + 1)}-${pad(date.getDate())}`
}

function currentTrainingContext() {
  const config = getPrescription()
  const snapshot = getDeviceSnapshot()
  return {
    config,
    date: currentRecordDate(),
    exerciseId: EXERCISE_IDS[config.exercise] || null,
    exerciseName: EXERCISE_NAMES[config.exercise] || (config.joint_id === 'knee' ? '膝关节屈伸' : '训练动作'),
    source: snapshot.transport === 'DEMO' ? 'demo' : 'device'
  }
}

function persistTrainingRecord(record) {
  if (!record) return null
  const history = getHistory()
  const existing = history.find(item => item && item.date === record.date)
  const merged = mergeTrainingDay(existing, record)
  const next = [
    merged,
    ...history.filter(item => item && item.date !== record.date)
  ].sort((a, b) => String(b.date || '').localeCompare(String(a.date || '')))
  uni.setStorageSync(memberKey(KEYS.history), next)
  return merged
}

export function switchJointMode(jointId) {
  const mode = jointModes.find(item => item.id === jointId)
  if (!mode || !mode.enabled) return Promise.reject(new Error('该关节模式暂未开放'))
  const snapshot = { ...getDeviceSnapshot(), joint_id: mode.id, body_mode: mode.code }
  const config = {
    ...getPrescription(), joint_id: mode.id, body_mode: mode.code, exercise: mode.exercise,
    target_angle_deg: mode.id === 'elbow' ? 80 : 70,
    valid_angle_deg: mode.id === 'elbow' ? 60 : 50,
    return_angle_deg: mode.id === 'elbow' ? 20 : 15
  }
  uni.setStorageSync(KEYS.snapshot, snapshot)
  uni.setStorageSync(memberKey(KEYS.config), config)
  return Promise.resolve({ ok: true, command: 'SET_MODE', config })
}

export function writePrescription(input) {
  const config = { ...getPrescription(), ...input }
  if (config.sets < 1 || config.sets > 5) return Promise.reject(new Error('组数范围为 1—5'))
  if (config.reps < 5 || config.reps > 30) return Promise.reject(new Error('次数范围为 5—30'))
  if (config.target_angle_deg < 30 || config.target_angle_deg > 120) return Promise.reject(new Error('目标角度范围为 30°—120°'))
  if (config.valid_angle_deg > config.target_angle_deg) return Promise.reject(new Error('有效角度不能高于目标角度'))
  if (config.return_angle_deg >= config.valid_angle_deg) return Promise.reject(new Error('回落角度必须低于有效角度'))
  uni.setStorageSync(memberKey(KEYS.config), config)
  return Promise.resolve({ ok: true, command: 'SET_PRESCRIPTION', config })
}

// 仅用于无硬件时演示。页面消费字段，不参与角度、次数或质量判定。
// 接入设备后由 ingestDeviceJson 接收 v3 串口/BLE 网关的 JSON，演示源会自动让位。
function demoPacket() {
  const config = getPrescription()
  demo.tick += 1
  const wave = (Math.sin(demo.tick / 3.2 - Math.PI / 2) + 1) / 2
  const waveRight = (Math.sin(demo.tick / 3.2 - Math.PI / 2 - .18) + 1) / 2
  const leftAngle = demo.state === 'RUNNING' ? Math.round(wave * (config.target_angle_deg + 5) * 10) / 10 : 0
  const rightAngle = demo.state === 'RUNNING' ? Math.round(waveRight * (config.target_angle_deg + 2) * 10) / 10 : 0
  demo.leftRom = Math.max(demo.leftRom, leftAngle)
  demo.rightRom = Math.max(demo.rightRom, rightAngle)
  let repEvent = 'none'
  if (demo.state === 'RUNNING' && demo.tick % 20 === 0) {
    demo.leftCount = Math.min(config.reps, demo.leftCount + 1)
    demo.rightCount = Math.min(config.reps, demo.rightCount + (demo.tick % 40 ? 1 : 0))
    repEvent = demo.leftCount === demo.rightCount ? 'both_rep_done' : 'left_rep_done'
  }
  const slower = Math.min(demo.leftCount, demo.rightCount)
  const setBase = (demo.setIndex - 1) * config.reps + slower
  const completion = Math.min(100, Math.round(setBase / (config.sets * config.reps) * 100))
  const quality = Math.abs(leftAngle - rightAngle) > 12 ? 'ASYMMETRY' : (Math.max(leftAngle, rightAngle) < config.valid_angle_deg && wave > .9 ? 'ROM_LOW' : 'GOOD')
  return {
    seq: demo.tick, timestamp_ms: Date.now(), body_mode: config.body_mode, mode: config.body_mode,
    exercise: config.exercise, train_mode: config.train_mode, set_index: demo.setIndex, target_sets: config.sets,
    left_angle_deg: leftAngle, right_angle_deg: rightAngle, left_rom_deg: demo.leftRom, right_rom_deg: demo.rightRom,
    lr_rom_diff_deg: Math.round(Math.abs(demo.leftRom - demo.rightRom) * 10) / 10,
    left_count: demo.leftCount, right_count: demo.rightCount, target_count: config.reps,
    completion_percent: Math.round(slower / config.reps * 100), overall_completion_percent: completion,
    training_state: demo.state, rep_event: repEvent, quality, warning: 'none', rest_remaining_sec: 0
  }
}

function emit(packet) {
  ingestTrainingPacket(packet)
  listeners.forEach(callback => callback(packet))
}

export function ingestDeviceJson(raw) {
  try {
    const packet = typeof raw === 'string' ? JSON.parse(raw.replace(/^JSON:/, '')) : raw
    externalPacketAt = Date.now()
    emit(packet)
    return packet
  } catch (error) {
    return null
  }
}

export function subscribeRealtime(callback) {
  listeners.push(callback)
  callback(demoPacket())
  if (!timer) {
    timer = setInterval(() => {
      if (Date.now() - externalPacketAt > 1500) emit(demoPacket())
    }, 400)
  }
  return () => {
    listeners = listeners.filter(item => item !== callback)
    if (!listeners.length && timer) { clearInterval(timer); timer = null }
  }
}

export function sendTrainingCommand(command) {
  const now = Date.now()
  if (command === 'START') {
    demo = { tick: 0, leftCount: 0, rightCount: 0, leftRom: 0, rightRom: 0, state: 'RUNNING', setIndex: 1 }
    beginTrainingSession({ ...currentTrainingContext(), startedAt: now })
  } else if (command === 'PAUSE') {
    demo.state = 'PAUSED'
    pauseTrainingSession(now)
  } else if (command === 'RESUME') {
    demo.state = 'RUNNING'
    resumeTrainingSession(now)
  } else if (command === 'STOP') {
    demo.state = 'FINISHED'
  }

  emit(demoPacket())
  const record = command === 'STOP'
    ? persistTrainingRecord(finishTrainingSession({ endedAt: Date.now() }))
    : null
  return Promise.resolve({ ok: true, command, record })
}

export function calibrateImu(target = 'all') {
  return sendTrainingCommand('CALIBRATE').then(result => ({ ...result, target }))
}

export function checkFirmwareUpdate() {
  const snapshot = getDeviceSnapshot()
  return Promise.resolve({ ok: true, currentVersion: snapshot.firmware, updateAvailable: false })
}
