import { buildFamilyReportInsight, askFamilyReportAgent } from './report-agent.js'

const AGENT_ENDPOINT = ''

export const XIAOSI_SHOWCASE_RECORD = {
  frequencyPerWeek: 5,
  exercises: [
    { id: 'dumbbell-curl', name: '哑铃弯举', count: 30, sets: 3, reps: 10, targetAngle: 80, holdSeconds: 2 },
    { id: 'triceps-extension', name: '肱三头肌伸展', count: 20, sets: 2, reps: 10, targetAngle: 45, holdSeconds: 2 },
    { id: 'knee-extension', name: '膝关节屈伸', count: 20, sets: 2, reps: 10, targetAngle: 70, holdSeconds: 2 }
  ]
}

export const XIAOSI_SHOWCASE_PROMPTS = [
  {
    id: 'report-analysis',
    label: '分析训练报告',
    description: '结合动作数量与训练频率，解读本次训练安排'
  },
  {
    id: 'parameter-recommendation',
    label: '推荐下次训练参数',
    description: '根据当前训练记录，生成下一次参数建议'
  }
]

function requestRemoteAgent(message, reportContext) {
  return new Promise((resolve, reject) => {
    uni.request({
      url: AGENT_ENDPOINT,
      method: 'POST',
      data: { agent: 'xiaosi', message, reportContext },
      success: response => resolve({ answer: response.data.answer, mode: 'agent' }),
      fail: reject
    })
  })
}

function getShowcaseSummary() {
  const exercises = XIAOSI_SHOWCASE_RECORD.exercises
  const totalCount = exercises.reduce((sum, item) => sum + item.count, 0)
  const upperCount = exercises
    .filter(item => item.id !== 'knee-extension')
    .reduce((sum, item) => sum + item.count, 0)
  const lowerCount = totalCount - upperCount
  return {
    totalCount,
    upperCount,
    lowerCount,
    upperShare: Math.round((upperCount / totalCount) * 100),
    frequencyPerWeek: XIAOSI_SHOWCASE_RECORD.frequencyPerWeek,
    exercises
  }
}

function detectShowcaseIntent(message) {
  const text = String(message || '')
  if (/参数|下次|处方|怎么练|推荐/.test(text)) return 'parameter-recommendation'
  if (/报告|分析|情况|训练得怎么样|解读/.test(text)) return 'report-analysis'
  return ''
}

function buildReportAnalysis() {
  const summary = getShowcaseSummary()
  const details = summary.exercises.map(item => `${item.name} ${item.count} 次`).join('、')
  return [
    `我已汇总这组训练记录：${details}，共 ${summary.totalCount} 次，训练频率均为每周 ${summary.frequencyPerWeek} 天。`,
    '',
    `从动作分布看，上肢训练 ${summary.upperCount} 次，下肢训练 ${summary.lowerCount} 次，上肢约占 ${summary.upperShare}%。整体安排覆盖了肘关节屈伸与膝关节屈伸，但上肢训练量相对更集中。`,
    '',
    '建议重点观察三件事：动作是否按目标角度完成、后半程是否出现代偿、连续训练后是否出现疼痛或疲劳增加。如果动作达标率稳定且没有不适，可以继续保持当前训练量；如果出现明显疼痛或动作质量下降，应先降低强度并咨询康复师。'
  ].join('\n')
}

function buildParameterRecommendation() {
  const summary = getShowcaseSummary()
  const lines = summary.exercises.map(item => (
    `${item.name}：${item.sets} 组 × ${item.reps} 次，目标角度 ${item.targetAngle}°，保持 ${item.holdSeconds} 秒`
  ))
  return [
    '根据当前记录，下一次训练建议先保持总量，不急于增加次数：',
    '',
    ...lines,
    '',
    `训练频率继续保持每周 ${summary.frequencyPerWeek} 天，每组之间充分休息。完成动作时以稳定、无明显疼痛为优先；如果连续两次都轻松达标，再由康复师评估是否提高角度或次数。`
  ].join('\n')
}

export function getXiaosiShowcaseContext() {
  return getShowcaseSummary()
}

export function getXiaosiThinkingSteps(message) {
  const intent = detectShowcaseIntent(message)
  if (intent === 'report-analysis') {
    return ['正在读取三项训练记录', '正在汇总动作数量与训练频率', '正在生成训练报告解读']
  }
  if (intent === 'parameter-recommendation') {
    return ['正在读取本次训练表现', '正在核对动作参数与安全范围', '正在生成下一次训练建议']
  }
  return ['正在理解你的问题', '正在查找相关训练数据', '正在整理回答']
}

export function openXiaosiContext(reportContext) {
  return buildFamilyReportInsight(reportContext)
}

export function sendToXiaosi(message, reportContext) {
  const intent = detectShowcaseIntent(message)
  if (intent === 'report-analysis') {
    return Promise.resolve({ answer: buildReportAnalysis(), mode: 'showcase', evidence: true })
  }
  if (intent === 'parameter-recommendation') {
    return Promise.resolve({ answer: buildParameterRecommendation(), mode: 'showcase', evidence: true })
  }
  if (AGENT_ENDPOINT) return requestRemoteAgent(message, reportContext)
  return askFamilyReportAgent(message, reportContext).then(result => ({ ...result, mode: 'safe-fallback' }))
}

export function hasRemoteXiaosiAgent() {
  return Boolean(AGENT_ENDPOINT)
}
