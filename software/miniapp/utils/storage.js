const KEYS = {
  profile: 'rehabmotion_profile',
  settings: 'rehabmotion_settings',
  records: 'rehabmotion_records',
  assessment: 'rehabmotion_assessment'
}

const seedProfile = {
  name: '林小满',
  rehabStage: '膝关节术后 · 第 4 周',
  therapist: '陈医生',
  institution: '明川运动康复中心',
  trainingDays: 18,
  streak: 6,
  target: '恢复屈膝活动度与下肢力量'
}

const seedSettings = {
  reminder: true,
  voice: true,
  vibration: false,
  reminderTime: '19:30'
}

const seedRecords = [
  { id: 1, date: '08-11', minutes: 22, completion: 100, pain: 2, score: 92 },
  { id: 2, date: '08-10', minutes: 18, completion: 86, pain: 3, score: 84 },
  { id: 3, date: '08-08', minutes: 20, completion: 100, pain: 3, score: 88 },
  { id: 4, date: '08-07', minutes: 15, completion: 72, pain: 4, score: 76 }
]

export function ensureSeedData() {
  if (!uni.getStorageSync(KEYS.profile)) uni.setStorageSync(KEYS.profile, seedProfile)
  if (!uni.getStorageSync(KEYS.settings)) uni.setStorageSync(KEYS.settings, seedSettings)
  if (!uni.getStorageSync(KEYS.records)) uni.setStorageSync(KEYS.records, seedRecords)
}

export function getProfile() {
  return uni.getStorageSync(KEYS.profile) || seedProfile
}

export function getSettings() {
  return uni.getStorageSync(KEYS.settings) || seedSettings
}

export function saveSettings(settings) {
  uni.setStorageSync(KEYS.settings, settings)
}

export function getRecords() {
  return uni.getStorageSync(KEYS.records) || seedRecords
}

export function addTrainingRecord(record) {
  const records = getRecords()
  records.unshift({ id: Date.now(), ...record })
  uni.setStorageSync(KEYS.records, records.slice(0, 30))
}

export function saveAssessment(assessment) {
  uni.setStorageSync(KEYS.assessment, assessment)
}

export function getAssessment() {
  return uni.getStorageSync(KEYS.assessment) || {
    pain: 3,
    area: '膝关节前侧',
    updatedAt: '今天 08:30'
  }
}
