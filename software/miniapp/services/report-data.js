import { getTrainingRecords, todayISO } from './training-records.js'
import { getTrainingTaskPlan, getTrainingPlanSnapshots } from './training-plan.js'
import { getAssessment, getRecords } from '../utils/storage.js'
import { getCurrentMember } from './members.js'

const DAY_MS = 24 * 60 * 60 * 1000

const supplementalByDate = {
  '08-17': { exercise: '膝关节屈伸', startedAt: '19:32', avgAngle: 78, painBefore: 1, painAfter: 2, fatigue: 2, reasons: { romLow: 2, paceFast: 1, asymmetry: 0 } },
  '08-16': { exercise: '坐到站训练', startedAt: '19:26', avgAngle: 76, painBefore: 1, painAfter: 1, fatigue: 2, reasons: { romLow: 1, paceFast: 1, asymmetry: 0 } },
  '08-15': { exercise: '膝关节屈伸', startedAt: '19:40', avgAngle: 72, painBefore: 2, painAfter: 2, fatigue: 3, reasons: { romLow: 2, paceFast: 2, asymmetry: 0 } },
  '08-14': { exercise: '膝关节屈伸', startedAt: '19:32', avgAngle: 78, painBefore: 1, painAfter: 2, fatigue: 2, reasons: { romLow: 2, paceFast: 1, asymmetry: 1 } },
  '08-13': { exercise: '坐到站训练', startedAt: '19:26', avgAngle: 76, painBefore: 1, painAfter: 1, fatigue: 2, reasons: { romLow: 1, paceFast: 1, asymmetry: 0 } },
  '08-12': { exercise: '膝关节屈伸', startedAt: '19:40', avgAngle: 74, painBefore: 1, painAfter: 2, fatigue: 2, reasons: { romLow: 2, paceFast: 1, asymmetry: 0 } },
  '08-11': { exercise: '膝关节屈伸', startedAt: '', avgAngle: 0, painBefore: 0, painAfter: 0, fatigue: 0, reasons: { romLow: 0, paceFast: 0, asymmetry: 0 } }
}

function number(value, fallback = 0) {
  const parsed = Number(value)
  return Number.isFinite(parsed) ? parsed : fallback
}

function startOfDay(value) {
  const date = value instanceof Date ? new Date(value) : new Date(value)
  date.setHours(0, 0, 0, 0)
  return date
}

function addDays(value, count) {
  const date = startOfDay(value)
  date.setDate(date.getDate() + count)
  return date
}

function toISODate(value) {
  const date = startOfDay(value)
  const month = String(date.getMonth() + 1).padStart(2, '0')
  const day = String(date.getDate()).padStart(2, '0')
  return `${date.getFullYear()}-${month}-${day}`
}

function parseRecordDate(value, reference = new Date()) {
  const text = String(value || '')
  if (/^\d{4}-\d{2}-\d{2}$/.test(text)) return startOfDay(`${text}T12:00:00`)
  if (/^\d{2}-\d{2}$/.test(text)) {
    const [month, day] = text.split('-').map(Number)
    let date = new Date(reference.getFullYear(), month - 1, day)
    if (date.getTime() - reference.getTime() > 45 * DAY_MS) date = new Date(reference.getFullYear() - 1, month - 1, day)
    return startOfDay(date)
  }
  const parsed = new Date(value)
  return Number.isNaN(parsed.getTime()) ? null : startOfDay(parsed)
}

function formatShortDate(value) {
  const date = startOfDay(value)
  return `${date.getMonth() + 1}月${date.getDate()}日`
}

function formatPeriod(start, end) {
  return `${formatShortDate(start)}—${formatShortDate(end)}`
}

function sum(records, key) {
  return records.reduce((total, item) => total + number(item[key]), 0)
}

function roundAverage(records, key, digits = 0) {
  if (!records.length) return 0
  const value = sum(records, key) / records.length
  return digits ? Number(value.toFixed(digits)) : Math.round(value)
}

function getReferenceDate() {
  return startOfDay(todayISO() + 'T12:00:00')
}

function getPlanAt(date) {
  const snapshots = typeof getTrainingPlanSnapshots === 'function' ? getTrainingPlanSnapshots() : []
  const target = startOfDay(date).getTime() + DAY_MS - 1
  const available = snapshots
    .filter(item => new Date(item.effective_at || item.updated_at || 0).getTime() <= target)
    .sort((a, b) => new Date(a.effective_at || a.updated_at || 0) - new Date(b.effective_at || b.updated_at || 0))
  const snapshot = available[available.length - 1]
  return snapshot && snapshot.plan ? snapshot.plan : getTrainingTaskPlan()
}

function plannedOffsets(plan) {
  const tasks = Array.isArray(plan && plan.tasks) ? plan.tasks : []
  const offsets = new Set()
  tasks.forEach((task, taskIndex) => {
    const frequency = Math.max(1, Math.min(7, number(task.frequency_per_week, 3)))
    for (let index = 0; index < frequency; index += 1) offsets.add((taskIndex + index) % 7)
  })
  return offsets.size ? offsets : new Set([0, 2, 4])
}

function inferStatus(record, date, planned) {
  const explicit = String(record && record.status || '').toLowerCase()
  if (explicit === 'cancelled') return 'cancelled'
  if (explicit === 'completed' || (record && record.completed)) return 'completed'
  if (explicit === 'partial' || number(record && record.minutes) > 0 || number(record && record.completion) > 0) return 'partial'
  if (explicit === 'not_started' || record && record.warning === 'MISSED') return 'not_started'
  if (date > startOfDay(new Date())) return planned ? 'scheduled' : 'unplanned'
  return planned ? 'not_started' : 'unplanned'
}

function normalizeRecord(record, date, planned, selfReports) {
  const mmdd = toISODate(date).slice(5)
  const extra = supplementalByDate[mmdd] || {}
  const selfReport = selfReports.find(item => String(item.date || '').endsWith(mmdd))
  const plannedReps = number(record && record.plannedReps, 30)
  const completedReps = number(record && record.completedReps, Math.round(plannedReps * number(record && record.completion) / 100))
  const qualifiedReps = number(record && record.qualifiedReps, Math.round(completedReps * number(record && record.qualified) / 100))
  const status = inferStatus(record, date, planned)
  return {
    ...(record || {}),
    isoDate: toISODate(date),
    date: mmdd,
    dateLabel: String(date.getDate()),
    dayLabel: ['日', '一', '二', '三', '四', '五', '六'][date.getDay()],
    planned,
    status,
    statusLabel: { completed: '已完成', partial: '中途结束', not_started: '未开始', cancelled: '已取消', scheduled: '待训练', unplanned: '未安排' }[status],
    completed: status === 'completed',
    exercise: record && (record.exercise || record.exercise_name) || extra.exercise || '膝关节屈伸',
    startedAt: record && record.startedAt || extra.startedAt || '',
    plannedReps,
    completedReps,
    qualifiedReps,
    completion: status === 'completed' ? Math.max(number(record && record.completion), 100) : number(record && record.completion),
    qualified: number(record && record.qualified),
    minutes: number(record && record.minutes),
    maxAngle: number(record && record.maxAngle),
    avgAngle: number(record && record.avgAngle, extra.avgAngle || Math.max(0, number(record && record.maxAngle) - 6)),
    painBefore: number(record && record.painBefore, selfReport ? selfReport.pain : extra.painBefore),
    painAfter: number(record && record.painAfter, selfReport ? selfReport.pain : extra.painAfter),
    fatigue: number(record && record.fatigue, extra.fatigue),
    avgSpeed: number(record && record.avgSpeed),
    stability: number(record && record.stability),
    lrRomDiff: number(record && record.lrRomDiff),
    compensationCount: number(record && record.compensationCount),
    interrupted: number(record && record.interrupted),
    syncedAfterOffline: Boolean(record && record.syncedAfterOffline),
    reasons: { romLow: 0, paceFast: 0, asymmetry: 0, ...(extra.reasons || {}), ...(record && record.reasons || {}) }
  }
}

function recordsByDate(reference) {
  const map = new Map()
  getTrainingRecords().forEach(record => {
    const date = parseRecordDate(record.recorded_on || record.isoDate || record.date, reference)
    if (date) map.set(toISODate(date), record)
  })
  return map
}

function buildOccurrences(start, end, plan) {
  const map = recordsByDate(end)
  const selfReports = getRecords()
  const offsets = plannedOffsets(plan)
  const memberActiveSince = startOfDay(getCurrentMember().createdAt || start)
  const days = []
  for (let date = startOfDay(start), index = 0; date <= end; date = addDays(date, 1), index += 1) {
    const record = map.get(toISODate(date))
    const planned = date < memberActiveSince ? false : (record && typeof record.planned === 'boolean' ? record.planned : offsets.has(index % 7))
    days.push(normalizeRecord(record, date, planned, selfReports))
  }
  return days
}

function aggregateProblems(completedOrPartial) {
  const items = [
    { id: 'paceFast', label: '动作过快' },
    { id: 'holdShort', label: '保持时间不足' },
    { id: 'romLow', label: '动作幅度不足' },
    { id: 'asymmetry', label: '双侧动作不对称' }
  ].map(item => ({ ...item, count: completedOrPartial.reduce((total, session) => total + number(session.reasons[item.id]), 0) }))
  const maximum = Math.max(1, ...items.map(item => item.count))
  return items.sort((a, b) => b.count - a.count).slice(0, 3).map(item => ({ ...item, percent: Math.round(item.count / maximum * 100) }))
}

function aggregateFeedback(completedOrPartial, previous = []) {
  const intensity = [
    { label: '合适', count: completedOrPartial.filter(item => item.fatigue >= 1 && item.fatigue <= 3).length },
    { label: '偏轻', count: completedOrPartial.filter(item => item.fatigue === 0).length },
    { label: '偏重', count: completedOrPartial.filter(item => item.fatigue >= 4).length }
  ]
  const pain = [
    { label: '无明显疼痛', count: completedOrPartial.filter(item => item.painAfter <= 1).length },
    { label: '轻微', count: completedOrPartial.filter(item => item.painAfter >= 2 && item.painAfter <= 3).length },
    { label: '明显', count: completedOrPartial.filter(item => item.painAfter >= 4).length }
  ]
  const dominantIntensity = [...intensity].sort((a, b) => b.count - a.count)[0].label
  const mild = pain[1].count
  const previousMild = previous.filter(item => item.painAfter >= 2 && item.painAfter <= 3).length
  const difference = mild - previousMild
  const comparison = difference === 0 ? '轻微疼痛反馈次数与上一周期相同。' : `轻微疼痛反馈较上一周期${difference > 0 ? '增加' : '减少'} ${Math.abs(difference)} 次。`
  return { dominantIntensity, intensity, pain, comparison }
}

function aggregatePeriod(start, end, plan, previousOccurrences = []) {
  const sessions = buildOccurrences(start, end, plan)
  const planned = sessions.filter(item => item.planned && item.status !== 'cancelled' && item.status !== 'scheduled')
  const completed = planned.filter(item => item.status === 'completed')
  const partial = planned.filter(item => item.status === 'partial')
  const notStarted = planned.filter(item => item.status === 'not_started')
  const valid = sessions.filter(item => item.status === 'completed' || item.status === 'partial')
  const plannedValid = [...completed, ...partial]
  const plannedCompletedReps = sum(plannedValid, 'completedReps')
  const completedReps = sum(valid, 'completedReps')
  const qualifiedReps = sum(valid, 'qualifiedReps')
  const qualified = completedReps ? Math.round(qualifiedReps / completedReps * 100) : 0
  const plannedSessions = planned.length
  const completedSessions = completed.length
  const adherence = plannedSessions ? Math.round(completedSessions / plannedSessions * 100) : 0
  const maxAngleValues = valid.map(item => item.maxAngle).filter(Boolean)
  const chronological = [...sessions]
  const angleTimeline = valid.filter(item => item.maxAngle > 0)
  const baselineAngle = angleTimeline.length ? angleTimeline[0].maxAngle : 0
  const maxAngle = maxAngleValues.length ? Math.max(...maxAngleValues) : 0
  const problems = aggregateProblems(valid)
  const deviceInterruptions = sum(valid, 'interrupted')
  const highPainSessions = valid.filter(item => item.painAfter >= 4).length
  const warningSessions = sessions.filter(item => item.warning && item.warning !== 'none' && item.warning !== 'MISSED').length
  const completeDataSessions = valid.filter(item => item.maxAngle > 0 && item.completedReps >= 0 && item.qualified >= 0).length
  const previousValid = previousOccurrences.filter(item => item.status === 'completed' || item.status === 'partial')
  return {
    sessions,
    plannedSessions,
    completedSessions,
    adherence,
    execution: sum(planned, 'plannedReps') ? Math.min(100, Math.round(plannedCompletedReps / sum(planned, 'plannedReps') * 100)) : 0,
    qualified,
    plannedReps: sum(planned, 'plannedReps'),
    completedReps,
    qualifiedReps,
    totalMinutes: sum(valid, 'minutes'),
    totalMoves: completedReps,
    qualifiedMoves: qualifiedReps,
    uncompleted: partial.length + notStarted.length,
    partialCount: partial.length,
    notStartedCount: notStarted.length,
    cancelledCount: sessions.filter(item => item.status === 'cancelled').length,
    missed: notStarted.length,
    deviceInterruptions,
    interruptions: deviceInterruptions,
    chronological,
    baselineAngle,
    maxAngle,
    angleGain: maxAngle - baselineAngle,
    highPainSessions,
    warningSessions,
    dataCompleteness: valid.length ? Math.round(completeDataSessions / valid.length * 100) : 0,
    lastSession: [...valid].reverse()[0] || null,
    avgPainBefore: roundAverage(valid.filter(item => item.painBefore > 0), 'painBefore', 1),
    avgPainAfter: roundAverage(valid.filter(item => item.painAfter > 0), 'painAfter', 1),
    avgSpeed: roundAverage(valid.filter(item => item.avgSpeed > 0), 'avgSpeed', 1),
    stability: roundAverage(valid.filter(item => item.stability > 0), 'stability'),
    lrRomDiff: roundAverage(valid.filter(item => item.lrRomDiff > 0), 'lrRomDiff', 1),
    compensationCount: sum(valid, 'compensationCount'),
    offlineSyncedSessions: valid.filter(item => item.syncedAfterOffline).length,
    problems,
    reasons: problems,
    feedback: aggregateFeedback(valid, previousValid),
    completion: adherence
  }
}

export function buildWeeklyReport(weekOffset = 0) {
  const reference = getReferenceDate()
  const weekdayIndex = reference.getDay() === 0 ? 6 : reference.getDay() - 1
  const currentMonday = addDays(reference, -weekdayIndex)
  const start = addDays(currentMonday, number(weekOffset) * 7)
  const end = addDays(start, 6)
  const previousStart = addDays(start, -7)
  const previousEnd = addDays(end, -7)
  const plan = getPlanAt(end)
  const previous = buildOccurrences(previousStart, previousEnd, getPlanAt(previousEnd))
  const aggregate = aggregatePeriod(start, end, plan, previous)
  const earliest = getTrainingRecords().map(item => parseRecordDate(item.recorded_on || item.isoDate || item.date, reference)).filter(Boolean).sort((a, b) => a - b)[0]
  const mainProblem = aggregate.problems.find(item => item.count > 0)
  const status = aggregate.notStartedCount >= 2 || aggregate.feedback.pain[2].count > 0 || aggregate.qualified && aggregate.qualified < 75 ? 'attention' : 'steady'
  const incompleteBreakdown = [
    aggregate.notStartedCount ? `未开始 ${aggregate.notStartedCount} 次` : '',
    aggregate.partialCount ? `中途结束 ${aggregate.partialCount} 次` : ''
  ].filter(Boolean).join(' · ') || '本周期无未完成训练'
  return {
    ...aggregate,
    weekOffset: number(weekOffset),
    startDate: toISODate(start),
    endDate: toISODate(end),
    period: formatPeriod(start, end),
    periodLong: `${start.getFullYear()}年${formatPeriod(start, end)}`,
    isCurrent: number(weekOffset) === 0,
    canNext: number(weekOffset) < 0,
    canPrevious: earliest ? start > earliest : false,
    patient: { name: getCurrentMember().name, relation: getCurrentMember().relationship },
    status,
    statusLabel: status === 'steady' ? '本周训练执行整体稳定' : '本周训练有事项需要关注',
    summary: status === 'steady' ? '本周训练执行整体稳定' : '本周训练执行存在波动',
    mainProblem: mainProblem ? mainProblem.label : '暂无明显集中问题',
    incompleteBreakdown,
    interpretation: `计划训练 ${aggregate.plannedSessions} 次，已完成 ${aggregate.completedSessions} 次；${incompleteBreakdown}。`,
    currentAssessment: getAssessment(),
    prescriptionTasks: plan.tasks || [],
    prescriptionUpdatedAt: plan.updated_at || ''
  }
}

export function buildMonthlyReport(monthOffset = 0) {
  const reference = getReferenceDate()
  const monthStart = new Date(reference.getFullYear(), reference.getMonth() + number(monthOffset), 1)
  const monthEnd = new Date(monthStart.getFullYear(), monthStart.getMonth() + 1, 0)
  const visibleEnd = monthOffset === 0 ? reference : monthEnd
  const previousMonthEnd = addDays(monthStart, -1)
  const previousMonthStart = new Date(previousMonthEnd.getFullYear(), previousMonthEnd.getMonth(), 1)
  const plan = getPlanAt(visibleEnd)
  const previous = buildOccurrences(previousMonthStart, previousMonthEnd, getPlanAt(previousMonthEnd))
  const aggregate = aggregatePeriod(monthStart, visibleEnd, plan, previous)
  const leading = (monthStart.getDay() + 6) % 7
  const calendar = Array.from({ length: leading }, () => ({ empty: true }))
  const allDays = buildOccurrences(monthStart, monthEnd, plan)
  allDays.forEach(item => calendar.push({ ...item, empty: false, future: item.isoDate > toISODate(reference) }))
  while (calendar.length % 7) calendar.push({ empty: true })
  return {
    ...aggregate,
    monthOffset: number(monthOffset),
    period: `${monthStart.getFullYear()}年${monthStart.getMonth() + 1}月`,
    label: `${monthStart.getMonth() + 1}月训练概览`,
    cutoff: monthOffset === 0 ? `截至 ${formatShortDate(reference)}` : '完整月份',
    canNext: number(monthOffset) < 0,
    calendar,
    interpretation: `本月计划训练 ${aggregate.plannedSessions} 次，已完成 ${aggregate.completedSessions} 次；未完成 ${aggregate.uncompleted} 次。`
  }
}

export function getReportReferenceDate() { return getReferenceDate() }
