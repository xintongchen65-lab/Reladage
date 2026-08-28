import { loadRecords,persistRecord,type GameTrainingRecord,type SyncStorage } from '../../game-platform/records/record-store'
import type { TrainingResult } from '../types/result'

export const FRUIT_RECORD_KEY='rehabmotion-fruit-game:records:v1'
export type FruitRecord=GameTrainingRecord<TrainingResult>
const finite=(v:unknown):v is number=>typeof v==='number'&&Number.isFinite(v)
export function normalizeFruitResult(value:unknown):TrainingResult|null{
  if(!value||typeof value!=='object')return null
  const r=value as TrainingResult
  if(!['FINISHED','STOPPED'].includes(r.endReason)||!finite(r.completedAtMs)||!finite(r.elapsedMs)||!finite(r.activeElapsedMs)||!r.training||!r.game)return null
  const t=r.training as TrainingResult['training'];const g=r.game as TrainingResult['game']
  if(!finite(t.left_count)||!finite(t.right_count)||!finite(t.target_count)||!finite(t.completion_percent)||!finite(g.harvestedCount)||!finite(g.score)||!finite(g.maxCombo)||!finite(g.wrongSideCount))return null
  return{...r,training:{...t,set_index:finite(t.set_index)?t.set_index:1,target_sets:finite(t.target_sets)?t.target_sets:1,overall_completion_percent:finite(t.overall_completion_percent)?t.overall_completion_percent:t.completion_percent,left_total_count:finite(t.left_total_count)?t.left_total_count:t.left_count,right_total_count:finite(t.right_total_count)?t.right_total_count:t.right_count,session_left_rom_deg:finite(t.session_left_rom_deg)?t.session_left_rom_deg:t.left_rom_deg,session_right_rom_deg:finite(t.session_right_rom_deg)?t.session_right_rom_deg:t.right_rom_deg,session_lr_rom_diff_deg:finite(t.session_lr_rom_diff_deg)?t.session_lr_rom_diff_deg:t.lr_rom_diff_deg},game:{...g,normalFruitCount:finite(g.normalFruitCount)?g.normalFruitCount:g.harvestedCount,goldenAppleCount:finite(g.goldenAppleCount)?g.goldenAppleCount:0,rainbowFruitCount:finite(g.rainbowFruitCount)?g.rainbowFruitCount:0,bothWatermelonCount:finite(g.bothWatermelonCount)?g.bothWatermelonCount:0}}
}
export function loadFruitRecords(storage?:SyncStorage):FruitRecord[]{return loadRecords({storageKey:FRUIT_RECORD_KEY,normalizeResult:normalizeFruitResult,storage})}
export function saveFruitResult(result:TrainingResult,storage:SyncStorage=uni){return persistRecord({storageKey:FRUIT_RECORD_KEY,idPrefix:'fruit',result,normalizeResult:normalizeFruitResult,storage})}
