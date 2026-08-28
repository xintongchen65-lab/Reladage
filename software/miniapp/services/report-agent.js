function buildObservations(report) {
  const observations = [
    `本周按计划完成 ${report.completedSessions}/${report.plannedSessions} 次训练，训练依从率 ${report.adherence}%。`,
    `已完成动作中有 ${report.qualified}% 达标，最高关节角度较周期初增加 ${report.angleGain}°。`
  ]
  if (report.avgPainAfter > 0) observations.push(`训练后平均疼痛记录为 ${report.avgPainAfter}/10。`)
  return observations
}

function buildSuggestions(report) {
  const suggestions = []
  if (report.notStartedCount > 0) suggestions.push('和老人约定固定训练时间，训练前由家属做一次轻量提醒。')
  if (report.partialCount > 0) suggestions.push('本周有训练中途结束，建议先了解是疲劳、不适还是外部打断，并如实记录原因。')
  if (report.qualified < 85) suggestions.push('陪练时优先提醒动作放慢、做到目标幅度，不建议自行增加次数或角度。')
  if (report.avgPainAfter <= 3) suggestions.push('目前记录以轻微疼痛为主，训练后继续询问疼痛是否持续或加重，并如实记录。')
  if (report.avgPainAfter >= 4 || report.highPainSessions > 0) suggestions.push('疼痛记录已达到需要关注的范围，建议暂停自行加量，并联系医生或康复师确认。')
  return suggestions.slice(0, 3)
}

export function buildFamilyReportInsight(report) {
  return {
    title: report.status === 'steady' ? '坚持情况不错，继续关注训练后的感受' : '本周存在需要家属跟进的情况',
    observations: buildObservations(report),
    suggestions: buildSuggestions(report),
    disclaimer: '总结仅用于解读居家训练记录，不提供诊断，也不会自动调整康复处方。'
  }
}

export function askFamilyReportAgent(question, report) {
  const content = String(question || '').trim()
  let answer = ''
  if (/医生|康复师|就医|复诊/.test(content)) {
    answer = report.highPainSessions > 0 || report.avgPainAfter >= 4
      ? '报告中出现了需要专业人员确认的疼痛信号。建议先暂停自行加量，并把本周疼痛、角度和中断记录发给医生或康复师。'
      : '目前报告没有记录到高疼痛训练，但报告不能排除风险。如果出现持续加重的疼痛、刺痛、麻木、明显肿胀或活动突然下降，应停止训练并联系医生或康复师。'
  } else if (/下周|陪练|怎么做|建议/.test(content)) {
    answer = `下周家属可以先帮助保持固定训练时间，并在训练后确认疼痛感受。当前处方参数不建议由家属自行增加；若要调整角度、次数或频率，请交给医生或康复师判断。`
  } else if (/未完成|未开始|中途结束|坚持|提醒/.test(content)) {
    answer = report.uncompleted
      ? `本周有 ${report.uncompleted} 次未完成训练，其中${report.incompleteBreakdown}。先了解具体原因；如果因疼痛或不适停止，不要强行补练。`
      : '本周没有未完成训练。保持当前提醒节奏即可，不需要为了追求连续打卡而额外加练。'
  } else if (/疼|痛|不舒服/.test(content)) {
    answer = `本周训练后平均疼痛为 ${report.avgPainAfter}/10。请继续记录疼痛是否持续、是否逐次升高以及是否伴随刺痛、麻木或肿胀；出现明显异常时停止训练并联系专业人员。`
  } else {
    answer = `从本周记录看，老人完成了 ${report.completedSessions}/${report.plannedSessions} 次训练，动作达标率 ${report.qualified}%。家属最有价值的帮助是稳定提醒、确认训练后感受，并把异常变化准确反馈给康复师。`
  }
  return Promise.resolve({ answer, disclaimer: '仅解读训练记录，不替代医生诊断或处方调整。' })
}
