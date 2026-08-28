export const jointModes = [
  {
    id: 'elbow',
    name: '手肘',
    subtitle: '肘关节屈伸',
    code: 'upper',
    exercise: 'elbow_flexion',
    enabled: true,
    color: '#DCEEE7',
    accent: '#287865',
    icon: 'EL',
    wear: [
      'A / B 佩戴在上臂，传感器方向一致',
      'C / D 佩戴在前臂，避开肘窝',
      '屈肘至舒适角度后完成零位标定'
    ]
  },
  {
    id: 'knee',
    name: '膝关节',
    subtitle: '膝关节屈伸',
    code: 'lower',
    exercise: 'knee_flexion',
    enabled: true,
    color: '#EAF0D7',
    accent: '#748A36',
    icon: 'KN',
    wear: [
      'A / B 固定在大腿前侧中线',
      'C / D 固定在小腿胫骨前侧',
      '坐姿伸直膝关节后完成零位标定'
    ]
  },
  {
    id: 'wrist',
    name: '手腕',
    subtitle: '后续扩展模式',
    code: 'wrist',
    exercise: 'wrist_flexion',
    enabled: false,
    color: '#F4E7DD',
    accent: '#AA6E53',
    icon: 'WR',
    wear: [
      '佩戴方案仍在验证中',
      '当前固件暂未开放腕关节算法',
      '可先查看动作库，不向设备下发配置'
    ]
  }
]

export const exerciseCatalog = [
  {
    id: 1,
    name: '哑铃弯举 / 屈肘训练',
    shortName: '哑铃弯举',
    region: '上肢',
    joint: '肘关节',
    deviceSupported: true,
    dose: '3 组 × 10 次',
    goal: '恢复屈肘活动度与肱二头肌力量',
    image: '/static/exercises/dumbbell-curl.png'
  },
  {
    id: 2,
    name: '肱三头肌伸展',
    shortName: '肱三头肌伸展',
    region: '上肢',
    joint: '肘关节',
    deviceSupported: false,
    dose: '2 组 × 10 次',
    goal: '改善肘伸展控制',
    image: '/static/exercises/triceps-extension.png'
  },
  {
    id: 3,
    name: '肩胛平面抬手',
    shortName: '肩胛平面抬手',
    region: '上肢',
    joint: '肩关节',
    deviceSupported: false,
    dose: '2 组 × 8 次',
    goal: '改善肩胛控制与上举能力',
    image: '/static/exercises/scapular-plane-raise.png'
  },
  {
    id: 4,
    name: '墙面爬手',
    shortName: '墙面爬手',
    region: '上肢',
    joint: '肩关节',
    deviceSupported: false,
    dose: '3 组 × 8 次',
    goal: '循序恢复肩关节活动度',
    image: '/static/exercises/wall-crawl.png'
  },
  {
    id: 5,
    name: '膝关节屈伸',
    shortName: '膝关节屈伸',
    region: '下肢',
    joint: '膝关节',
    deviceSupported: true,
    dose: '3 组 × 10 次',
    goal: '改善膝关节活动度与控制',
    image: '/static/exercises/knee-flexion-extension.png'
  },
  {
    id: 6,
    name: '坐到站训练',
    shortName: '坐到站',
    region: '下肢',
    joint: '髋膝协同',
    deviceSupported: false,
    dose: '3 组 × 8 次',
    goal: '提升日常起立能力',
    image: '/static/exercises/sit-to-stand.png'
  },
  {
    id: 7,
    name: '箱式深蹲',
    shortName: '箱式深蹲',
    region: '下肢',
    joint: '髋膝协同',
    deviceSupported: false,
    dose: '3 组 × 8 次',
    goal: '增强下肢力量与动作控制',
    image: '/static/exercises/box-squat.png'
  },
  {
    id: 8,
    name: '台阶踩踏 Step Up',
    shortName: '台阶踩踏',
    region: '下肢',
    joint: '膝关节',
    deviceSupported: false,
    dose: '2 组 × 10 次',
    goal: '提高单腿稳定与上下台阶能力',
    image: '/static/exercises/step-up.png'
  }
]

export const qualityMap = {
  GOOD: { label: '动作正常', tone: 'good' },
  ROM_LOW: { label: '幅度不足', tone: 'warn' },
  TOO_FAST: { label: '动作过快', tone: 'warn' },
  TOO_SLOW: { label: '动作过慢', tone: 'warn' },
  ASYMMETRY: { label: '双侧不对称', tone: 'warn' },
  UNSTABLE: { label: '动作不稳定', tone: 'warn' },
  NOT_RETURNED: { label: '未回到起始位', tone: 'warn' }
}

export const warningMap = {
  none: '',
  SIGNAL_LOST: 'IMU 数据丢失',
  imu_signal_lost: 'IMU 数据丢失',
  IMU_DISCONNECTED: 'IMU 未连接',
  NOT_CALIBRATED: '设备未校准',
  SD_ERROR: '训练可继续，但记录写入失败',
  LOW_BATTERY: '设备电量不足'
}
