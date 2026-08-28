import type { SquatMotionFrame } from '../types/motion'
export type MoleMotionDataSource=import('../../game-platform/motion/data-source').MotionDataSource<SquatMotionFrame>
export interface MoleControllableDataSource extends MoleMotionDataSource{setSquatDirection(direction:'down'|'up'|'none'):void;setPaused(paused:boolean):void;simulateCompleteCycle():boolean;finishTraining(reason:'FINISHED'|'STOPPED'):void}
export function isMoleControllableSource(v:unknown):v is MoleControllableDataSource{return!!v&&typeof(v as MoleControllableDataSource).setSquatDirection==='function'}
