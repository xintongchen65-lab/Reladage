import { getTherapistSafeArea, getTherapistViewportStyle } from './therapist-safe-area'
import { onMounted, onUnmounted, reactive, ref } from 'vue'

export function useTherapistViewport() {
  const area = ref(getTherapistSafeArea())
  const style = reactive(getTherapistViewportStyle(area.value))
  const refresh = () => { area.value = getTherapistSafeArea(); Object.assign(style, getTherapistViewportStyle(area.value)) }
  const handleResize = () => refresh()
  onMounted(() => { (uni as unknown as { onWindowResize?: (callback: () => void) => void }).onWindowResize?.(handleResize) })
  onUnmounted(() => { (uni as unknown as { offWindowResize?: (callback: () => void) => void }).offWindowResize?.(handleResize) })
  return { area, style, refresh }
}
