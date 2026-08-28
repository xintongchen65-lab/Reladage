export type ArmPose = 'low' | 'mid' | 'high'

export interface MotionMapOptions {
  minAngleDeg: number
  maxAngleDeg: number
}

export const DEFAULT_MOTION_MAP: MotionMapOptions = {
  minAngleDeg: 0,
  maxAngleDeg: 80
}

export function clamp(value: number, min: number, max: number): number {
  return Math.min(max, Math.max(min, value))
}

export function mapAngleToProgress(
  angleDeg: number,
  options: MotionMapOptions = DEFAULT_MOTION_MAP
): number {
  if (options.maxAngleDeg <= options.minAngleDeg) return 0
  return clamp(
    (angleDeg - options.minAngleDeg) / (options.maxAngleDeg - options.minAngleDeg),
    0,
    1
  )
}

export function progressToPose(progress: number, previous: ArmPose = 'low'): ArmPose {
  const value = clamp(progress, 0, 1)
  if (previous === 'low') {
    if (value >= 0.75) return 'high'
    return value >= 0.4 ? 'mid' : 'low'
  }
  if (previous === 'high') {
    if (value <= 0.25) return 'low'
    return value <= 0.6 ? 'mid' : 'high'
  }
  if (value <= 0.25) return 'low'
  if (value >= 0.75) return 'high'
  return 'mid'
}
