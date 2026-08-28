export const exercises = [
  {
    id: 'heel-slide',
    order: '01',
    name: '仰卧足跟滑动',
    target: '改善膝关节屈曲活动度',
    dosage: '12 次 × 2 组',
    duration: 45,
    rest: 20,
    level: '轻度',
    color: '#DDF0E8',
    accent: '#2B7968',
    cue: '足跟贴住床面，缓慢滑向臀部，保持膝盖朝向正上方。',
    tips: ['动作全程保持匀速', '疼痛超过 4 分立即停止', '不要憋气，保持自然呼吸']
  },
  {
    id: 'quad-set',
    order: '02',
    name: '股四头肌等长收缩',
    target: '激活大腿前侧肌群',
    dosage: '保持 8 秒 × 10 次',
    duration: 50,
    rest: 20,
    level: '轻度',
    color: '#EEF2D5',
    accent: '#7F963C',
    cue: '膝下垫毛巾卷，向下压紧，同时绷紧大腿前侧。',
    tips: ['脚尖朝向正上方', '感受膝盖后侧下压', '腰背保持自然放松']
  },
  {
    id: 'bridge',
    order: '03',
    name: '臀桥',
    target: '增强臀部与核心稳定',
    dosage: '10 次 × 2 组',
    duration: 40,
    rest: 30,
    level: '中度',
    color: '#F6E5D2',
    accent: '#B6753A',
    cue: '双脚与髋同宽，收紧臀部抬起骨盆，肩髋膝保持一条直线。',
    tips: ['不要过度挺腰', '膝盖不要向内夹', '顶端停留 2 秒']
  }
]

export const weekPlan = [
  { day: '周一', date: '10', type: '下肢力量', minutes: 20, state: 'done' },
  { day: '周二', date: '11', type: '关节活动', minutes: 22, state: 'done' },
  { day: '今天', date: '12', type: '综合训练', minutes: 22, state: 'today' },
  { day: '周四', date: '13', type: '主动恢复', minutes: 15, state: 'next' },
  { day: '周五', date: '14', type: '下肢力量', minutes: 25, state: 'next' },
  { day: '周六', date: '15', type: '关节活动', minutes: 18, state: 'next' },
  { day: '周日', date: '16', type: '休息与放松', minutes: 10, state: 'rest' }
]

export const weeklyScores = [68, 76, 72, 84, 80, 92, 88]
