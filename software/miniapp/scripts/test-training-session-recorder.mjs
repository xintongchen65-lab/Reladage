import assert from 'node:assert/strict'
import {
  beginTrainingSession,
  finishTrainingSession,
  ingestTrainingPacket,
  mergeTrainingDay,
  pauseTrainingSession,
  resumeTrainingSession
} from '../services/training-session-recorder.js'

const startedAt = Date.now()
beginTrainingSession({
  id: 'test-session',
  date: '2026-08-27',
  exerciseId: 5,
  exerciseName: '膝关节屈伸',
  source: 'test',
  startedAt,
  config: { sets: 1, reps: 2, side_mode: 'bilateral' }
})

ingestTrainingPacket({
  seq: 1,
  training_state: 'RUNNING',
  set_index: 1,
  target_count: 2,
  left_count: 1,
  right_count: 1,
  left_angle_deg: 72,
  right_angle_deg: 70,
  lr_rom_diff_deg: 2,
  rep_event: 'both_rep_done',
  quality: 'GOOD'
})

pauseTrainingSession(startedAt + 30000)
resumeTrainingSession(startedAt + 45000)

ingestTrainingPacket({
  seq: 2,
  training_state: 'RUNNING',
  set_index: 1,
  target_count: 2,
  left_count: 2,
  right_count: 2,
  left_angle_deg: 78,
  right_angle_deg: 80,
  lr_rom_diff_deg: 2,
  rep_event: 'both_rep_done',
  quality: 'ROM_LOW'
})

const record = finishTrainingSession({ endedAt: startedAt + 75000 })
assert.ok(record)
assert.equal(record.status, 'completed')
assert.equal(record.completion, 100)
assert.equal(record.completedReps, 2)
assert.equal(record.qualifiedReps, 1)
assert.equal(record.qualified, 50)
assert.equal(record.maxAngle, 80)
assert.equal(record.minutes, 1)
assert.equal(record.interrupted, 1)
assert.equal(record.reasons.romLow, 1)
assert.equal(record.sessions.length, 1)

const merged = mergeTrainingDay({
  date: '2026-08-27',
  status: 'completed',
  sessions: [{
    id: 'existing',
    exerciseId: 6,
    name: '坐到站训练',
    status: 'completed',
    plannedReps: 3,
    completedReps: 3,
    qualifiedReps: 3,
    minutes: 2,
    maxAngle: 75
  }],
  reasons: {}
}, record)

assert.equal(merged.sessions.length, 2)
assert.equal(merged.plannedReps, 5)
assert.equal(merged.completedReps, 5)
assert.equal(merged.qualifiedReps, 4)
assert.equal(merged.qualified, 80)
assert.equal(merged.minutes, 3)
assert.equal(merged.maxAngle, 80)

console.log('训练记录器测试通过：实时次数、达标、角度、暂停和同日合并均正确。')
