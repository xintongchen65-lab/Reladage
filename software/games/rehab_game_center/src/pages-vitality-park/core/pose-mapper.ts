import type { VitalityMotionFrame, VitalityMotionStage } from '../types/motion'

export type VitalityPose = 'sitting' | 'lean-forward' | 'lift-off' | 'half-standing' | 'standing' | 'sit-back'
export type VitalityMotionDirection = 'rising' | 'descending' | 'idle'

const stageProgress: Record<VitalityMotionStage, number> = {
  SITTING: 0,
  LEAN_FORWARD: 0.22,
  LIFT_OFF: 0.42,
  HALF_STANDING: 0.66,
  STANDING: 1,
  SIT_BACK: 0.08
}

const stagePose: Record<VitalityMotionStage, VitalityPose> = {
  SITTING: 'sitting',
  LEAN_FORWARD: 'lean-forward',
  LIFT_OFF: 'lift-off',
  HALF_STANDING: 'half-standing',
  STANDING: 'standing',
  SIT_BACK: 'sit-back'
}

export interface VitalityPoseDecision {
  progress: number
  pose: VitalityPose
  direction: VitalityMotionDirection
}

export function progressFromFrame(frame: VitalityMotionFrame): number {
  if (typeof frame.motion_progress === 'number' && Number.isFinite(frame.motion_progress)) return clamp(frame.motion_progress)
  if (frame.motion_stage) return stageProgress[frame.motion_stage]
  const span = Math.max(1, frame.target_angle_deg - frame.return_angle_deg)
  return clamp((Math.max(frame.left_angle_deg, frame.right_angle_deg) - frame.return_angle_deg) / span)
}

export function inferMotionDirection(
  currentProgress: number,
  previousProgress: number,
  previousDirection: VitalityMotionDirection = 'idle'
): VitalityMotionDirection {
  const delta = currentProgress - previousProgress
  if (delta > 0.008) return 'rising'
  if (delta < -0.008) return 'descending'
  return previousDirection
}

export function poseForProgress(progress: number, direction: VitalityMotionDirection = 'rising'): VitalityPose {
  const value = clamp(progress)

  // Returning to the chair must replay the same physical sequence in reverse.
  // A dedicated sit-back frame is only used during the final settling phase.
  if (direction === 'descending') {
    if (value >= 0.82) return 'standing'
    if (value >= 0.56) return 'half-standing'
    if (value >= 0.36) return 'lift-off'
    if (value >= 0.16) return 'lean-forward'
    if (value >= 0.035) return 'sit-back'
    return 'sitting'
  }

  if (value <= 0.1) return 'sitting'
  if (value <= 0.3) return 'lean-forward'
  if (value <= 0.5) return 'lift-off'
  if (value <= 0.8) return 'half-standing'
  return 'standing'
}

export function decidePoseFromFrame(
  frame: VitalityMotionFrame,
  previousProgress: number,
  previousDirection: VitalityMotionDirection = 'idle'
): VitalityPoseDecision {
  if (frame.training_state === 'REST') return { progress: 0, pose: 'sitting', direction: 'idle' }

  // Real controller preference order:
  // 1) motion_progress, 2) motion_stage, 3) controller angles.
  if (typeof frame.motion_progress === 'number' && Number.isFinite(frame.motion_progress)) {
    const progress = clamp(frame.motion_progress)
    const direction = inferMotionDirection(progress, previousProgress, previousDirection)
    return { progress, direction, pose: poseForProgress(progress, direction) }
  }

  if (frame.motion_stage) {
    const pose = stagePose[frame.motion_stage]
    const direction: VitalityMotionDirection = frame.motion_stage === 'SIT_BACK' ? 'descending' : frame.motion_stage === 'SITTING' ? 'idle' : previousDirection === 'descending' ? 'descending' : 'rising'
    return { progress: stageProgress[frame.motion_stage], pose, direction }
  }

  const progress = progressFromFrame(frame)
  const direction = inferMotionDirection(progress, previousProgress, previousDirection)
  return { progress, direction, pose: poseForProgress(progress, direction) }
}

export function poseIndex(pose: VitalityPose): number {
  return ['sitting', 'lean-forward', 'lift-off', 'half-standing', 'standing', 'sit-back'].indexOf(pose)
}

function clamp(value: number): number {
  return Math.max(0, Math.min(1, value))
}
