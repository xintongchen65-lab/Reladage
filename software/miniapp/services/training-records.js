import { getDeviceSnapshot, getHistory } from './device.js'

const DAY_MS = 24 * 60 * 60 * 1000

const pad = value => String(value).padStart(2, '0')

export function toISODate(value = new Date()) {
  const date = value instanceof Date ? new Date(value) : new Date(String(value).length === 10 ? String(value) + 'T12:00:00' : value)
  return `${date.getFullYear()}-${pad(date.getMonth() + 1)}-${pad(date.getDate())}`
}

export function todayISO() {
  const reference = uni.getStorageSync('rehabmotion_demo_reference_date_v1')
  if (getDeviceSnapshot().transport === 'DEMO' && /^\d{4}-\d{2}-\d{2}$/.test(String(reference || ''))) return String(reference)
  return toISODate(new Date())
}

function shiftISO(value, days) {
  const date = new Date(value + 'T12:00:00')
  date.setDate(date.getDate() + days)
  return toISODate(date)
}

function statusFrom(record) {
  const explicit = String(record.status || '').toLowerCase()
  if (explicit) return explicit
  if (record.completed) return 'completed'
  if (Number(record.completion || 0) > 0 || Number(record.minutes || 0) > 0) return 'partial'
  return record.planned ? 'not_started' : 'unplanned'
}

function normalizeSession(session, record, index) {
  const completedReps = Number(session.completedReps || 0)
  const plannedReps = Number(session.plannedReps || record.plannedReps || 0)
  const qualifiedReps = Number(session.qualifiedReps || 0)
  let status = String(session.status || '')
  if (!status) status = completedReps >= plannedReps && plannedReps > 0 ? 'completed' : completedReps > 0 ? 'partial' : 'not_started'
  return {
    id: session.id || `${record.date}-session-${index}`,
    exerciseId: session.exerciseId || null,
    name: session.name || session.exercise || record.exercise || record.exercise_name || '训练动作',
    status,
    plannedReps,
    completedReps,
    qualifiedReps,
    minutes: Number(session.minutes || 0),
    maxAngle: Number(session.maxAngle || 0)
  }
}

function fallbackSessions(record) {
  const status = statusFrom(record)
  if (status === 'unplanned' && !record.exercise && !record.exercise_name) return []
  return [normalizeSession({
    id: `${record.date}-primary`,
    name: record.exercise || record.exercise_name || '当天训练任务',
    status,
    plannedReps: Number(record.plannedReps || 0),
    completedReps: Number(record.completedReps || 0),
    qualifiedReps: Number(record.qualifiedReps || 0),
    minutes: Number(record.minutes || 0),
    maxAngle: Number(record.maxAngle || 0)
  }, record, 0)]
}

function demoDateShift(records) {
  if (getDeviceSnapshot().transport !== 'DEMO' || !records.length) return 0
  const latest = [...records].map(item => String(item.date || '')).filter(value => /^\d{4}-\d{2}-\d{2}$/.test(value)).sort().pop()
  if (!latest) return 0
  const latestTime = new Date(latest + 'T12:00:00').getTime()
  const todayTime = new Date(todayISO() + 'T12:00:00').getTime()
  return Math.round((todayTime - latestTime) / DAY_MS)
}

function normalizeRecord(record, shiftDays) {
  const shiftedDate = shiftDays ? shiftISO(record.date, shiftDays) : record.date
  const base = { ...record, date: shiftedDate }
  const sessions = Array.isArray(record.sessions) && record.sessions.length
    ? record.sessions.map((session, index) => normalizeSession(session, base, index))
    : fallbackSessions(base)
  const completedReps = sessions.reduce((sum, item) => sum + item.completedReps, 0)
  const qualifiedReps = sessions.reduce((sum, item) => sum + item.qualifiedReps, 0)
  const plannedReps = sessions.reduce((sum, item) => sum + item.plannedReps, 0)
  const minutes = sessions.reduce((sum, item) => sum + item.minutes, 0)
  const status = statusFrom(record)
  return {
    ...base,
    status,
    completed: status === 'completed',
    sessions,
    completedReps: sessions.length ? completedReps : Number(record.completedReps || 0),
    qualifiedReps: sessions.length ? qualifiedReps : Number(record.qualifiedReps || 0),
    plannedReps: sessions.length ? plannedReps : Number(record.plannedReps || 0),
    minutes: sessions.length && minutes ? minutes : Number(record.minutes || 0),
    qualified: completedReps ? Math.round(qualifiedReps / completedReps * 100) : Number(record.qualified || 0)
  }
}

export function getTrainingRecords() {
  const raw = [...getHistory()].filter(item => /^\d{4}-\d{2}-\d{2}$/.test(String(item.date || '')))
  const shiftDays = demoDateShift(raw)
  return raw.map(item => normalizeRecord(item, shiftDays)).sort((a, b) => b.date.localeCompare(a.date))
}

export function getTrainingRecord(date) {
  return getTrainingRecords().find(item => item.date === date) || null
}

export function getTodayTrainingRecord() {
  return getTrainingRecord(todayISO())
}

export function getTrainingDateBounds() {
  const records = getTrainingRecords()
  return {
    min: records[records.length - 1]?.date || todayISO(),
    max: todayISO()
  }
}

export function formatTrainingDate(value) {
  const date = new Date(value + 'T12:00:00')
  const weekdays = ['星期日', '星期一', '星期二', '星期三', '星期四', '星期五', '星期六']
  return `${date.getMonth() + 1}月${date.getDate()}日 · ${weekdays[date.getDay()]}`
}
