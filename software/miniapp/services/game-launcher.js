export const REHAB_GAME_SCENES = Object.freeze([
  { id: 'fruit-game', title: '摘水果', exerciseIds: [1], path: '/pages-fruit-game/prepare/index', resultEvent: 'fruitGameResult' },
  { id: 'penalty-game', title: '点球大战', exerciseIds: [5], path: '/pages-penalty-game/prepare/index', resultEvent: 'penaltyGameResult' },
  { id: 'vitality-park', title: '活力公园', exerciseIds: [6], path: '/pages-vitality-park/prepare/index', resultEvent: 'vitalityParkResult' },
  { id: 'mole-game', title: '地鼠大作战', exerciseIds: [7], path: '/pages-mole-game/prepare/index', resultEvent: 'moleGameResult' }
])

const clampInteger = (value, min, max, fallback) => {
  const numeric = Number(value)
  if (!Number.isFinite(numeric)) return fallback
  return Math.min(max, Math.max(min, Math.round(numeric)))
}

const appendQuery = (path, params) => {
  const query = Object.keys(params)
    .filter(key => params[key] !== undefined && params[key] !== null && params[key] !== '')
    .map(key => `${encodeURIComponent(key)}=${encodeURIComponent(String(params[key]))}`)
    .join('&')
  return query ? `${path}?${query}` : path
}

export function getGameSceneForTask(task) {
  if (!task) return null
  const exerciseId = Number(task.exercise_id)
  return REHAB_GAME_SCENES.find(scene => scene.exerciseIds.includes(exerciseId)) || null
}

export function getAvailableGameEntries(tasks = []) {
  return tasks.map(task => {
    const scene = getGameSceneForTask(task)
    return scene ? { ...scene, task } : null
  }).filter(Boolean)
}

export function buildRehabGameLaunch(task, advanced = {}, returnUrl = '/pages/plan/index', options = {}) {
  const scene = getGameSceneForTask(task)
  if (!scene) throw new Error('当前动作暂未配置趣味训练')
  const targetAngle = clampInteger(task.target_angle_deg, 30, 120, scene.id === 'mole-game' ? 90 : 80)
  const tolerance = clampInteger(advanced.angle_tolerance_deg, 5, 30, 10)
  const validAngle = Math.max(30, Math.min(targetAngle, targetAngle - tolerance))
  const returnAngle = Math.max(5, Math.min(validAngle - 5, 20))
  return {
    scene,
    url: appendQuery(scene.path, {
      returnUrl,
      targetCount: clampInteger(task.reps, 1, 30, 8),
      targetSets: clampInteger(task.sets, 1, 5, 2),
      targetAngleDeg: targetAngle,
      validAngleDeg: validAngle,
      returnAngleDeg: returnAngle,
      restDurationSec: clampInteger(advanced.rest_sec, 0, 120, 30),
      debug: options.debug ? 1 : undefined
    })
  }
}

export function launchRehabGame(options = {}) {
  let launch
  try {
    launch = buildRehabGameLaunch(options.task, options.advanced, options.returnUrl, options)
  } catch (error) {
    return Promise.reject(error)
  }
  return new Promise((resolve, reject) => {
    const events = {}
    events[launch.scene.resultEvent] = result => {
      uni.$emit('rehabmotion:game-result', {
        sceneId: launch.scene.id,
        taskId: options.task && options.task.task_id,
        result
      })
      if (typeof options.onResult === 'function') options.onResult(result)
    }
    uni.navigateTo({ url: launch.url, events, success: resolve, fail: reject })
  })
}
