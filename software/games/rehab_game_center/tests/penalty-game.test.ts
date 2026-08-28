import { describe,expect,it } from 'vitest'
import { KneeRepCycleDetector } from '../src/pages-penalty-game/core/rep-cycle-detector'
import { advancePenaltyPhase,createInitialPenaltyState,pickShotDirection,pickShotOutcome,ShotEventReconciler,triggerShot } from '../src/pages-penalty-game/core/game-engine'
import { createInitialKneeFrame } from '../src/pages-penalty-game/types/motion'
import { indexedRandom } from '../src/game-platform/random/seeded'
import { KneeAngleInterpolator } from '../src/pages-penalty-game/core/angle-interpolator'

describe('penalty action and game rules',()=>{
  it('only completes after zero position, valid flexion and return',()=>{
    const detector=new KneeRepCycleDetector(60,20)
    expect(detector.update(45).completed).toBe(false)
    detector.update(10);detector.update(30);detector.update(59)
    expect(detector.update(19).completed).toBe(false)
    detector.update(25);detector.update(60)
    expect(detector.update(40).completed).toBe(false)
    expect(detector.update(20).completed).toBe(true)
    expect(detector.update(10).completed).toBe(false)
  })
  it('implements 70/20/10 outcomes and equal direction thirds',()=>{
    expect(pickShotOutcome(()=>0)).toBe('GOAL');expect(pickShotOutcome(()=>.699999)).toBe('GOAL')
    expect(pickShotOutcome(()=>.7)).toBe('SAVE');expect(pickShotOutcome(()=>.899999)).toBe('SAVE');expect(pickShotOutcome(()=>.9)).toBe('MISS')
    expect(pickShotDirection(()=>0)).toBe('left');expect(pickShotDirection(()=>1/3)).toBe('center');expect(pickShotDirection(()=>2/3)).toBe('right')
  })
  it('scores consecutive goals and breaks combo on saves, misses and wrong side',()=>{
    const first=triggerShot(createInitialPenaltyState(),'left',()=>0,()=>0).state
    expect(first).toMatchObject({shots:1,goals:1,score:100,combo:1,bestCombo:1})
    let ready=advancePenaltyPhase(first,1600)
    const second=triggerShot(ready,'right',()=>0,()=>.5).state
    expect(second).toMatchObject({score:220,combo:2,bestCombo:2})
    ready=advancePenaltyPhase(second,1600)
    const saved=triggerShot(ready,'left',()=>.8,()=>.5).state
    expect(saved).toMatchObject({saves:1,score:220,combo:0,bestCombo:2})
    ready=advancePenaltyPhase(saved,1600)
    const wrong=triggerShot({...ready,combo:3},'left').state
    expect(wrong.combo).toBe(0);expect(wrong.shots).toBe(3)
  })
  it('reproduces multiplayer random choices from seed and shot index',()=>{
    const valuesA=[indexedRandom(13579,0,0),indexedRandom(13579,0,1),indexedRandom(13579,4,0)]
    const valuesB=[indexedRandom(13579,0,0),indexedRandom(13579,0,1),indexedRandom(13579,4,0)]
    expect(valuesA).toEqual(valuesB)
  })
  it('interpolates low-frequency visual angles without creating motion events',()=>{
    const visual=new KneeAngleInterpolator();visual.update({left_angle_deg:0,right_angle_deg:0},0);visual.update({left_angle_deg:80,right_angle_deg:40},200)
    expect(visual.sample(300)).toEqual({left:40,right:20});expect(visual.sample(400)).toEqual({left:80,right:40})
  })
  it('deduplicates rep event and later count while filling one missing event',()=>{
    const reconciler=new ShotEventReconciler();const base=createInitialKneeFrame(10,1)
    expect(reconciler.accept({...base,seq:1,rep_event:'left_rep_done'})).toMatchObject({side:'left',source:'event'})
    expect(reconciler.accept({...base,seq:2,active_side:'right',left_count:1,rep_event:'none'}).side).toBeNull()
    expect(reconciler.accept({...base,seq:3,active_side:'right',left_count:1,right_count:1,rep_event:'none'})).toMatchObject({side:'right',source:'count'})
    expect(reconciler.accept({...base,seq:4,active_side:'left',left_count:3,right_count:1,rep_event:'none'})).toMatchObject({side:null,countJump:true})
  })
})
