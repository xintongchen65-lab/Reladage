export {
  appendReturnUrl,
  configureCallerReturnUrl,
  getCallerReturnUrl,
  normalizeReturnUrl,
  resetNavigationRuntime,
  returnToGameCenter,
  returnToCaller
} from '../../game-platform/runtime/navigation'
export type { ReturnToCallerOptions } from '../../game-platform/runtime/navigation'
export const DEFAULT_RETURN_URL = '/pages/game-center/index'
