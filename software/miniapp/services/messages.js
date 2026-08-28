import { getDeviceSnapshot } from './device.js'
import { formatTrainingDate, getTodayTrainingRecord, getTrainingRecords, todayISO } from './training-records.js'
import { getCurrentMemberLabel } from './members.js'

const READ_KEY = 'rehabmotion_read_messages_v2'

function messageTime(date) {
  if (date === todayISO()) return '今天'
  return formatTrainingDate(date).split(' · ')[0]
}

export function getMessages() {
  const device = getDeviceSnapshot()
  const records = getTrainingRecords()
  const today = getTodayTrainingRecord()
  const latest = today || records.find(item => item.status !== 'unplanned') || null
  const result = []

  if (latest) {
    if (latest.syncedAfterOffline) {
      result.push({
        id: 'sync-' + latest.date,
        type: 'sync',
        typeLabel: '同步',
        title: '离线训练记录已同步',
        time: latest.syncedAt || messageTime(latest.date),
        desc: `设备恢复联网后，已同步本次 ${latest.minutes || 0} 分钟训练记录，报告数据已更新。`,
        source: device.deviceName || 'RM-Core 01',
        action: '查看记录',
        route: `/pages/daily/index?date=${latest.date}`
      })
    }
    const completedTasks = latest.sessions.filter(item => item.status === 'completed').length
    result.push({
      id: 'training-' + latest.date,
      type: 'training',
      typeLabel: '训练',
      title: latest.status === 'completed' ? '训练记录已更新' : '训练计划尚未全部完成',
      time: messageTime(latest.date),
      desc: `已完成 ${completedTasks}/${latest.sessions.length} 项，累计 ${latest.minutes || 0} 分钟，动作达标率 ${latest.qualified || 0}%。`,
      source: getCurrentMemberLabel(),
      action: '查看训练',
      route: `/pages/daily/index?date=${latest.date}`
    })

    const paceFast = Number(latest.reasons?.paceFast || 0)
    if (paceFast > 0) {
      result.push({
        id: 'attention-pace-' + latest.date,
        type: 'attention',
        typeLabel: '关注',
        title: '动作节奏需要关注',
        time: messageTime(latest.date),
        desc: `训练记录到动作过快 ${paceFast} 次，下次训练时可以适当放慢。`,
        source: '训练质量提示',
        action: '查看详情',
        route: `/pages/daily/index?date=${latest.date}`
      })
    }
  }

  const storageAbnormal = device.storage && device.storage.status && device.storage.status !== 'NORMAL'
  const lowBattery = Number(device.battery || 0) <= 20
  if (!device.connected || storageAbnormal || lowBattery) {
    const state = !device.connected
      ? { id: 'device-disconnected', title: 'RehabMotion 设备已断开', desc: '请检查设备电源和连接状态，恢复连接后再开始训练。' }
      : storageAbnormal
        ? { id: 'device-storage-warning', title: '设备存储状态需要关注', desc: '训练记录存储出现异常，请进入设备维护查看详细状态。' }
        : { id: 'device-low-battery', title: '设备电量较低', desc: `${device.deviceName || '设备'} 当前电量 ${device.battery || 0}%，建议训练前充电。` }
    result.push({
      ...state,
      type: 'device',
      typeLabel: '设备',
      time: '设备状态',
      source: device.deviceName || 'RM-Core 01',
      action: '设备维护',
      route: '/pages/maintenance/index'
    })
  }

  return result
}

export function getReadMessageIds() {
  const saved = uni.getStorageSync(READ_KEY)
  if (Array.isArray(saved)) return saved
  return []
}

export function isMessageUnread(id) {
  return !getReadMessageIds().includes(id)
}

export function getUnreadMessageCount() {
  const readIds = getReadMessageIds()
  return getMessages().filter(item => !readIds.includes(item.id)).length
}

export function markMessageRead(id) {
  const readIds = getReadMessageIds()
  if (!readIds.includes(id)) uni.setStorageSync(READ_KEY, [...readIds, id])
}

export function markAllMessagesRead() {
  uni.setStorageSync(READ_KEY, getMessages().map(item => item.id))
}
