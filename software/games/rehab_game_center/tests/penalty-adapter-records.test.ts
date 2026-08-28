import { describe,expect,it } from 'vitest'
import { KneeMotionFrameAdapter } from '../src/pages-penalty-game/core/motion-adapter'
import { createInitialKneeFrame } from '../src/pages-penalty-game/types/motion'
import { loadPenaltyRecords,savePenaltyResult } from '../src/pages-penalty-game/home/records'
import { createInitialPenaltyState,createPenaltyResult } from '../src/pages-penalty-game/core/game-engine'

describe('penalty protocol and records',()=>{
  it('derives alternating active_side when real frames omit it',()=>{
    const adapter=new KneeMotionFrameAdapter();const one=createInitialKneeFrame(2,1)
    const first=adapter.ingest({...one,seq:1,active_side:undefined})
    expect(first.accepted&&first.frame.active_side).toBe('left')
    const second=adapter.ingest({...one,seq:2,left_count:1,rep_event:'left_rep_done',active_side:undefined})
    expect(second.accepted&&second.frame.active_side).toBe('left')
    const third=adapter.ingest({...one,seq:3,left_count:1,rep_event:'none',active_side:undefined})
    expect(third.accepted&&third.frame.active_side).toBe('right')
  })
  it('allows REST group reset but rejects REST events and overall regression',()=>{
    const adapter=new KneeMotionFrameAdapter();const base=createInitialKneeFrame(1,2)
    expect(adapter.ingest({...base,seq:1}).accepted).toBe(true)
    expect(adapter.ingest({...base,seq:2,left_count:1,right_count:1,completion_percent:100,overall_completion_percent:50,training_state:'REST',quality:'REST',warning:'resting'}).accepted).toBe(true)
    expect(adapter.ingest({...base,seq:3,set_index:2,overall_completion_percent:50}).accepted).toBe(true)
    expect(adapter.ingest({...base,seq:4,set_index:2,overall_completion_percent:40}).accepted).toBe(false)
    const fresh=new KneeMotionFrameAdapter()
    expect(fresh.ingest({...base,seq:1,training_state:'REST',rep_event:'left_rep_done',quality:'REST',warning:'resting'})).toMatchObject({accepted:false,reason:'invalid_rest_event'})
  })
  it('uses an isolated storage key, ignores zero-action results and deduplicates',()=>{
    let stored:unknown=[];const storage={getStorageSync:()=>stored,setStorageSync:(_k:string,v:unknown)=>{stored=v}}
    const frame={...createInitialKneeFrame(10,1),training_state:'STOPPED' as const}
    const zero=createPenaltyResult('STOPPED',frame,createInitialPenaltyState(),1000,800,{leftTotalCount:0,rightTotalCount:0,leftMaxRomDeg:0,rightMaxRomDeg:0,overallCompletionPercent:0},100)
    expect(savePenaltyResult(zero,storage).reason).toBe('no_activity')
    const partial={...zero,completedAtMs:101,training:{...zero.training,left_total_count:1}}
    expect(savePenaltyResult(partial,storage).saved).toBe(true)
    expect(savePenaltyResult(partial,storage).reason).toBe('duplicate')
    expect(loadPenaltyRecords(storage)).toHaveLength(1)
  })
})
