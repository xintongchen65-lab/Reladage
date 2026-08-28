import { buildMonthlyReport, buildWeeklyReport } from './report-data.js'

export const trendRangeOptions = ['近4周', '近3个月', '近6个月', '近1年', '全部']
export const trendMetricOptions = [
  { id: 'qualified', name: '动作达标率', unit: '%' },
  { id: 'adherence', name: '训练计划完成率', unit: '%' },
  { id: 'problems', name: '动作问题发生次数', unit: '次' },
  { id: 'pain', name: '训练后疼痛反馈', unit: '/10' }
]

function rangeWeeks(range) {
  return { '近4周': 4, '近3个月': 12, '近6个月': 24, '近1年': 52, '全部': 52 }[range] || 4
}

function metricValue(report, metricId) {
  if (metricId === 'adherence') return report.adherence
  if (metricId === 'problems') return report.problems.reduce((total, item) => total + item.count, 0)
  if (metricId === 'pain') return report.avgPainAfter
  return report.qualified
}

function valueText(value) {
  return Number.isInteger(value) ? String(value) : Number(value).toFixed(1)
}

export function getTrendMetric(metricId = 'qualified', range = '近4周') {
  const descriptor = trendMetricOptions.find(item => item.id === metricId) || trendMetricOptions[0]
  const count = rangeWeeks(range)
  const reports = Array.from({ length: count }, (_, index) => buildWeeklyReport(index - count + 1))
  const meaningful = reports.filter(report => report.plannedSessions > 0 || report.sessions.some(item => item.completed || item.minutes > 0))
  const visible = meaningful.length ? meaningful : reports.slice(-1)
  const values = visible.map(report => metricValue(report, descriptor.id))
  const labels = visible.map(report => report.period.replace(/月/g, '.').replace(/日/g, '').split('—')[1])
  const current = values[values.length - 1] || 0
  const first = values[0] || 0
  const change = Number((current - first).toFixed(1))
  const direction = change > 0 ? '上升' : change < 0 ? '下降' : '持平'
  let interpretation = `${descriptor.name}在所选周期内保持不变。`
  if (values.length > 1) interpretation = `${descriptor.name}由 ${valueText(first)}${descriptor.unit} 变为 ${valueText(current)}${descriptor.unit}，${direction} ${valueText(Math.abs(change))}${descriptor.unit === '%' ? ' 个百分点' : descriptor.unit}。`
  return { ...descriptor, values, labels, current, change, interpretation, recordedWeeks: visible.length }
}

export function getReportHomeData() {
  const week = buildWeeklyReport(0)
  const month = buildMonthlyReport(0)
  const trend = getTrendMetric('qualified', '近4周')
  return { week, month: { ...month, dominantIntensity: month.feedback.dominantIntensity }, trend }
}

export function getWeeklyDetailData(weekOffset = 0) { return buildWeeklyReport(weekOffset) }

export function getMonthlyDetailData(monthOffset = 0) {
  const report = buildMonthlyReport(monthOffset)
  const trend = getTrendMetric('qualified', '近4周')
  return { ...report, weeklyTrend: { values: trend.values, labels: trend.labels } }
}
