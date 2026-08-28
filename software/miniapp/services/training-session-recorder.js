const QUALITY_REASON_MAP = Object.freeze({
  ROM_LOW: 'romLow',
  PACE_FAST: 'paceFast',
  TOO_FAST: 'paceFast',
  HOLD_SHORT: 'holdShort',
  ASYMMETRY: 'asymmetry'
})

let activeSession = null

const numeric = (value, fallback = 0) => {
  const result = Number(value)
  return Number.isFinite(result) ? result : fallback
}

const firstNumeric = (...values) => {
  for (const value of values) {
    const result = Number(value)
    if (Number.isFinite(result)) return result
  }
  return null
}

const round = (value, digits = 0) => {
  const scale = 10 ** digits
  return Math.round(numeric(value) * scale) / scale
}

const normalizedState = packet => String(packet && packet.training_state || '').toUpperCase()
const normalizedQuality = packet => String(packet && packet.quality || '').toUpperCase()
const isNone = value => !value || ['NONE', 'NORMAL', 'GOOD'].includes(String(value).toUpperCase())

function completedCount(packet, config) {
  const explicit = firstNumeric(
    packet && packet.overall_completed_count,
    packet && packet.completed_reps,
    packet && packet.completed_count,
    packet && packet.total_count
  )
  const targetPerSet = Math.max(1, numeric(packet && packet.target_count, config.reps))
  const setIndex = Math.max(1, numeric(packet && packet.set_index, 1))
  const left = Math.max(0, numeric(packet && packet.left_count))
  const right = Math.max(0, numeric(packet && packet.right_count))
  const perSet = String(config.side_mode || '').toLowerCase() === 'bilateral'
    ? Math.min(left, right)
    : Math.max(left, right)
  const calculated = (setIndex - 1) * targetPerSet + perSet
  return Math.max(0, explicit === null ? calculated : Math.max(explicit, calculated))
}

function qualifiedCount(packet) {
  return firstNumeric(
    packet && packet.qualified_reps,
    packet && packet.qualified_count,
    packet && packet.overall_qualified_count
  )
}

function incrementReason(session, packet, repDelta) {
  const repEvent = String(packet && packet.rep_event || '').toUpperCase()
  if (repDelta <= 0 && isNone(repEvent)) return
  const reason = QUALITY_REASON_MAP[normalizedQuality(packet)]
  if (reason) session.reasons[reason] = numeric(session.reasons[reason]) + 1
}

function addActiveDuration(session, now) {
  if (!session.runningSince) return
  session.activeMs += Math.max(0, numeric(now, Date.now()) - session.runningSince)
  session.runningSince = 0
}

export function beginTrainingSession(options = {}) {
  const config = { ...(options.config || {}) }
  const startedAt = numeric(options.startedAt, Date.now())
  activeSession = {
    id: options.id || `local-${startedAt}`,
    date: options.date || '',
    exerciseId: options.exerciseId ?? null,
    exerciseName: options.exerciseName || '训练动作',
    source: options.source || 'device',
    config,
    startedAt,
    runningSince: startedAt,
    activeMs: 0,
    pauseCount: 0,
    completedReps: 0,
    qualifiedReps: 0,
    maxAngle: 0,
    angleTotal: 0,
    angleSamples: 0,
    speedTotal: 0,
    speedSamples: 0,
    stabilityTotal: 0,
    stabilitySamples: 0,
    lrDiffTotal: 0,
    lrDiffSamples: 0,
    reasons: { paceFast: 0, holdShort: 0, romLow: 0, asymmetry: 0 },
    warnings: [],
    lastSequence: null,
    lastPacketAt: startedAt
  }
  return getActiveTrainingSession()
}

export function pauseTrainingSession(now = Date.now()) {
  if (!activeSession) return null
  addActiveDuration(activeSession, now)
  activeSession.pauseCount += 1
  return getActiveTrainingSession()
}

export function resumeTrainingSession(now = Date.now()) {
  if (!activeSession) return null
  if (!activeSession.runningSince) activeSession.runningSince = numeric(now, Date.now())
  return getActiveTrainingSession()
}

export function ingestTrainingPacket(packet) {
  if (!activeSession || !packet || typeof packet !== 'object') return null
  const sequence = packet.seq ?? packet.sequence
  if (sequence !== undefined && sequence !== null && sequence === activeSession.lastSequence) return getActiveTrainingSession()
  activeSession.lastSequence = sequence ?? activeSession.lastSequence
  activeSession.lastPacketAt = Date.now()

  const state = normalizedState(packet)
  if (state === 'PAUSED') addActiveDuration(activeSession, Date.now())
  if (state === 'RUNNING' && !activeSession.runningSince) activeSession.runningSince = Date.now()

  const before = activeSession.completedReps
  const after = completedCount(packet, activeSession.config)
  const repDelta = Math.max(0, after - before)
  activeSession.completedReps = Math.max(before, after)

  const explicitQualified = qualifiedCount(packet)
  if (explicitQualified !== null) {
    activeSession.qualifiedReps = Math.max(activeSession.qualifiedReps, explicitQualified)
  } else if (repDelta > 0 && normalizedQuality(packet) === 'GOOD') {
    activeSession.qualifiedReps += Math.min(repDelta, 1)
  }
  incrementReason(activeSession, packet, repDelta)

  const angle = Math.max(
    numeric(packet.left_angle_deg),
    numeric(packet.right_angle_deg),
    numeric(packet.left_rom_deg),
    numeric(packet.right_rom_deg),
    numeric(packet.max_angle_deg)
  )
  if (angle > 0) {
    activeSession.maxAngle = Math.max(activeSession.maxAngle, angle)
    activeSession.angleTotal += angle
    activeSession.angleSamples += 1
  }

  const speed = firstNumeric(packet.avg_speed_deg_s, packet.avg_speed, packet.speed_deg_s)
  if (speed !== null && speed > 0) {
    activeSession.speedTotal += speed
    activeSession.speedSamples += 1
  }
  const stability = firstNumeric(packet.stability_percent, packet.stability)
  if (stability !== null && stability > 0) {
    activeSession.stabilityTotal += stability
    activeSession.stabilitySamples += 1
  }
  const lrDiff = firstNumeric(packet.lr_rom_diff_deg, packet.lr_angle_diff_deg)
  if (lrDiff !== null && lrDiff >= 0) {
    activeSession.lrDiffTotal += lrDiff
    activeSession.lrDiffSamples += 1
  }

  const warning = packet.warning
  if (!isNone(warning) && !activeSession.warnings.includes(String(warning))) {
    activeSession.warnings.push(String(warning))
  }
  return getActiveTrainingSession()
}

export function finishTrainingSession(options = {}) {
  if (!activeSession) return null
  const endedAt = numeric(options.endedAt, Date.now())
  addActiveDuration(activeSession, endedAt)

  const session = activeSession
  activeSession = null
  const plannedReps = Math.max(1, numeric(session.config.sets, 1) * numeric(session.config.reps, 1))
  const completedReps = Math.min(plannedReps, Math.max(0, Math.round(session.completedReps)))
  const qualifiedReps = Math.min(completedReps, Math.max(0, Math.round(session.qualifiedReps)))
  const completion = Math.min(100, Math.round(completedReps / plannedReps * 100))
  const status = completion >= 100 ? 'completed' : 'partial'
  const minutes = Math.max(completedReps > 0 ? 1 : 0, Math.round(session.activeMs / 60000))
  const maxAngle = round(session.maxAngle, 1)
  const recordSession = {
    id: session.id,
    exerciseId: session.exerciseId,
    name: session.exerciseName,
    status,
    plannedReps,
    completedReps,
    qualifiedReps,
    minutes,
    maxAngle
  }

  return {
    id: session.id,
    externalSessionId: session.id,
    date: session.date,
    recorded_on: session.date,
    source: session.source,
    planned: true,
    status,
    completed: status === 'completed',
    completion,
    qualified: completedReps ? Math.round(qualifiedReps / completedReps * 100) : 0,
    exercise: session.exerciseName,
    plannedReps,
    completedReps,
    qualifiedReps,
    minutes,
    maxAngle,
    avgAngle: session.angleSamples ? round(session.angleTotal / session.angleSamples, 1) : 0,
    avgSpeed: session.speedSamples ? round(session.speedTotal / session.speedSamples, 1) : 0,
    stability: session.stabilitySamples ? Math.round(session.stabilityTotal / session.stabilitySamples) : 0,
    lrRomDiff: session.lrDiffSamples ? round(session.lrDiffTotal / session.lrDiffSamples, 1) : 0,
    compensationCount: numeric(session.reasons.asymmetry),
    interrupted: session.pauseCount,
    warning: session.warnings[0] || 'none',
    warnings: [...session.warnings],
    reasons: { ...session.reasons },
    startedAt: new Date(session.startedAt).toISOString(),
    endedAt: new Date(endedAt).toISOString(),
    sessions: [recordSession]
  }
}

export function getActiveTrainingSession() {
  if (!activeSession) return null
  return {
    ...activeSession,
    config: { ...activeSession.config },
    reasons: { ...activeSession.reasons },
    warnings: [...activeSession.warnings]
  }
}

export function resetTrainingSession() {
  activeSession = null
}

const sum = (items, key) => items.reduce((total, item) => total + numeric(item && item[key]), 0)

export function mergeTrainingDay(existing, incoming) {
  if (!existing || existing.date !== incoming.date) return incoming
  const sessionsById = new Map()
  ;[...(existing.sessions || []), ...(incoming.sessions || [])].forEach((item, index) => {
    sessionsById.set(item.id || `${incoming.date}-session-${index}`, item)
  })
  const sessions = [...sessionsById.values()]
  const plannedReps = sum(sessions, 'plannedReps')
  const completedReps = sum(sessions, 'completedReps')
  const qualifiedReps = sum(sessions, 'qualifiedReps')
  const minutes = sum(sessions, 'minutes')
  const completion = plannedReps ? Math.min(100, Math.round(completedReps / plannedReps * 100)) : 0
  const reasons = {}
  const reasonKeys = new Set([
    ...Object.keys(existing.reasons || {}),
    ...Object.keys(incoming.reasons || {})
  ])
  reasonKeys.forEach(key => {
    reasons[key] = numeric(existing.reasons && existing.reasons[key]) + numeric(incoming.reasons && incoming.reasons[key])
  })

  return {
    ...existing,
    ...incoming,
    status: completion >= 100 ? 'completed' : 'partial',
    completed: completion >= 100,
    completion,
    qualified: completedReps ? Math.round(qualifiedReps / completedReps * 100) : 0,
    plannedReps,
    completedReps,
    qualifiedReps,
    minutes,
    maxAngle: Math.max(numeric(existing.maxAngle), numeric(incoming.maxAngle)),
    avgAngle: incoming.avgAngle || existing.avgAngle || 0,
    avgSpeed: incoming.avgSpeed || existing.avgSpeed || 0,
    stability: incoming.stability || existing.stability || 0,
    lrRomDiff: incoming.lrRomDiff || existing.lrRomDiff || 0,
    compensationCount: numeric(existing.compensationCount) + numeric(incoming.compensationCount),
    interrupted: numeric(existing.interrupted) + numeric(incoming.interrupted),
    reasons,
    sessions
  }
}
