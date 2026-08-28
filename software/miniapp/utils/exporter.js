function csvCell(value) {
  return `"${String(value ?? '').replace(/"/g, '""')}"`
}

function reportCsv(report, records, notes) {
  const rows = [
    ['RehabMotion 居家康复专业报告'],
    ['统计周期', report.period],
    ['患者', report.patient ? report.patient.name : ''],
    ['训练依从率', `${report.adherence ?? report.completion}%`],
    ['处方剂量完成率', `${report.execution ?? report.completion}%`],
    ['动作达标率', `${report.qualified}%`],
    ['最大活动角度', `${report.maxAngle}°`],
    ['训练后平均疼痛', `${report.avgPainAfter ?? ''}/10`],
    ['漏练次数', report.missed],
    ['训练中断次数', report.interruptions ?? ''],
    ['医生备注', notes],
    [],
    ['日期', '动作', '是否完成', '计划动作', '完成动作', '合格动作', '完成率', '达标率', '平均角度', '最大角度', '训练分钟', '中断次数', '训练前疼痛', '训练后疼痛', '设备警告']
  ]
  records.forEach(item => rows.push([
    item.date, item.exercise || '', item.completed ? '是' : '否', item.plannedReps ?? '', item.completedReps ?? '', item.qualifiedReps ?? '',
    item.completion, item.qualified, item.avgAngle ?? '', item.maxAngle, item.minutes, item.interrupted, item.painBefore ?? '', item.painAfter ?? '', item.warning || ''
  ]))
  return '\ufeff' + rows.map(row => row.map(csvCell).join(',')).join('\n')
}

function simplePdf(report) {
  const lines = [
    'RehabMotion Home Rehabilitation Report',
    `Period: ${report.period}`,
    `Session adherence: ${report.adherence ?? report.completion}%`,
    `Prescription dose completed: ${report.execution ?? report.completion}%`,
    `Qualified movement rate: ${report.qualified}%`,
    `Maximum range of motion: ${report.maxAngle} deg`,
    `Average post-exercise pain: ${report.avgPainAfter ?? '-'} / 10`,
    `Missed sessions: ${report.missed}`,
    `Training interruptions: ${report.interruptions ?? '-'}`,
    'Home execution data only. Not a medical diagnosis.'
  ]
  const stream = ['BT', '/F1 14 Tf', '56 770 Td']
  lines.forEach((line, index) => { if (index) stream.push('0 -28 Td'); stream.push(`(${line.replace(/[()\\]/g, '\\$&')}) Tj`) })
  stream.push('ET')
  const content = stream.join('\n')
  const objects = [
    '<< /Type /Catalog /Pages 2 0 R >>',
    '<< /Type /Pages /Kids [3 0 R] /Count 1 >>',
    '<< /Type /Page /Parent 2 0 R /MediaBox [0 0 595 842] /Resources << /Font << /F1 5 0 R >> >> /Contents 4 0 R >>',
    `<< /Length ${content.length} >>\nstream\n${content}\nendstream`,
    '<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>'
  ]
  let pdf = '%PDF-1.4\n'
  const offsets = [0]
  objects.forEach((object, index) => { offsets.push(pdf.length); pdf += `${index + 1} 0 obj\n${object}\nendobj\n` })
  const xref = pdf.length
  pdf += `xref\n0 ${objects.length + 1}\n0000000000 65535 f \n`
  offsets.slice(1).forEach(offset => { pdf += `${String(offset).padStart(10, '0')} 00000 n \n` })
  pdf += `trailer\n<< /Size ${objects.length + 1} /Root 1 0 R >>\nstartxref\n${xref}\n%%EOF`
  return pdf
}

export function exportReport(type, report, records, notes) {
  return new Promise((resolve, reject) => {
    const manager = uni.getFileSystemManager && uni.getFileSystemManager()
    if (!manager) { reject(new Error('当前环境不支持文件导出')); return }
    const basePath = typeof wx !== 'undefined' && wx.env ? wx.env.USER_DATA_PATH : '_doc'
    const isPdf = type === 'pdf'
    const filePath = `${basePath}/RehabMotion_Report.${isPdf ? 'pdf' : 'csv'}`
    manager.writeFile({
      filePath,
      data: isPdf ? simplePdf(report) : reportCsv(report, records, notes),
      encoding: 'utf8',
      success: () => uni.openDocument({ filePath, showMenu: true, success: () => resolve(filePath), fail: reject }),
      fail: reject
    })
  })
}
