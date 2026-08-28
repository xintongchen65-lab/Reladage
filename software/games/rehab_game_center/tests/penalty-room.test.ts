import { describe,expect,it } from 'vitest'
import { RoomRegistry } from '../src/pages-fruit-game/core/room-model'
const person=(id:string)=>({playerId:id,displayName:id})
const cfg={targetSets:1,targetCount:2,gameId:'penalty' as const}
describe('penalty multiplayer rooms',()=>{
  it('rejects a different game and fixes the 70-percent cooperation target',()=>{
    const rooms=new RoomRegistry({countdownMs:0,random:()=>.123})
    const code=rooms.createRoom(person('a'),'COOP',cfg).snapshot.roomCode
    expect(()=>rooms.joinRoom(code,person('fruit'),{targetSets:1,targetCount:2,gameId:'fruit'})).toThrowError(/其他康复游戏/)
    rooms.joinRoom(code,person('b'),cfg);rooms.setReady(code,'a',true);rooms.setReady(code,'b',true);rooms.startRoom(code,'a');rooms.tick()
    expect(rooms.getSnapshot(code).teamTarget).toBe(6)
  })
  it('marks a reached goal target without ending rehabilitation early',()=>{
    const rooms=new RoomRegistry({countdownMs:0,random:()=>.321})
    const code=rooms.createRoom(person('a'),'COOP',cfg).snapshot.roomCode;rooms.joinRoom(code,person('b'),cfg)
    rooms.setReady(code,'a',true);rooms.setReady(code,'b',true);rooms.startRoom(code,'a');rooms.tick()
    const event=(id:string,seq:number,successes:number)=>({eventId:`${id}-${seq}`,clientSeq:seq,motionSeq:seq,repEvent:'left_rep_done' as const,trainingState:'RUNNING' as const,setIndex:1,leftCount:1,rightCount:0,leftTotalCount:1,rightTotalCount:0,overallCompletionPercent:25,activeElapsedMs:100,score:successes*100,harvestedCount:successes,attempts:successes,successes})
    rooms.applyProgress(code,'a',event('a',1,3));rooms.applyProgress(code,'b',event('b',1,3))
    expect(rooms.getSnapshot(code)).toMatchObject({teamContribution:6,teamCompleted:true,status:'RUNNING'})
  })
})
