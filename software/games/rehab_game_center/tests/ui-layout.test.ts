import fs from 'node:fs'
import path from 'node:path'
import { describe, expect, it } from 'vitest'

const read = (file: string) => fs.readFileSync(path.join(process.cwd(), file), 'utf8')

describe('mini-program layout and collection safety', () => {
  it('uses native page scrolling in portrait and a real record scroll-view in landscape', () => {
    const source = read('src/pages/demo/index.vue')
    expect(source).toContain('<scroll-view class="rfg-demo__records"')
    expect(source).toContain(':scroll-y="isLandscape"')
    expect(source).toMatch(/\.rfg-demo \{[^}]*min-height: 100vh[^}]*overflow: visible/)
    expect(source).toMatch(/@media \(orientation: landscape\)[\s\S]*\.rfg-demo \{[^}]*height: 100vh[^}]*overflow: hidden/)
    expect(source).toMatch(/\.rfg-demo__records \{[^}]*height: 100%[^}]*overflow: hidden/)
  })

  it('pauses while the collection book is open and requires rearming on return', () => {
    const game = read('src/pages-fruit-game/game/index.vue')
    expect(game).toContain('<RfgCollectionBook')
    expect(game).toContain('this.guard.pauseByUser()')
    expect(game).toContain('this.guard.resume()')
    expect(game).toContain('this.sessionAwareSource()?.resetRepCycleDetectors()')
    expect(game).toContain("frame.training_state === 'REST'")
    expect(game).toContain('this.guardState === \'RESTING\'')
  })

  it('keeps the festive background exclusive to the collection component', () => {
    const game = read('src/pages-fruit-game/game/index.vue')
    const collection = read('src/pages-fruit-game/components/CollectionBook.vue')
    expect(game).not.toContain('orchard.basketZone')
    expect(collection).toContain('FRUIT_GAME_ASSETS.orchard.basketZone')
  })

  it('uses the same explicit debug gate for single-player and multiplayer entry', () => {
    const demo = read('src/pages/demo/index.vue')
    const lobby = read('src/pages-fruit-game/multiplayer/lobby/index.vue')
    expect(demo.match(/debug=1/g)).toHaveLength(2)
    expect(lobby).toContain('resolveDebugEnabled(query?.debug)')
    expect(lobby).not.toContain("process.env.NODE_ENV !== 'production'")
  })

  it('uses fixed single-line flex buttons and visible back controls on pre-training pages', () => {
    const lobby = read('src/pages-fruit-game/multiplayer/lobby/index.vue')
    const room = read('src/pages-fruit-game/multiplayer/room/index.vue')
    const prepare = read('src/pages-fruit-game/prepare/index.vue')
    expect(lobby).toMatch(/\.rfg-mp-lobby__modes\{display:flex/)
    expect(lobby).toMatch(/\.rfg-mp-lobby__button\{[^}]*height:56px[^}]*font-size:17px[^}]*white-space:nowrap/)
    expect(lobby).toContain(':disabled="!canSubmit"')
    expect(lobby).toContain('重新连接')
    expect(prepare).toContain('class="rfg-prepare__back"')
    expect(lobby).toContain('class="rfg-mp-lobby__back"')
    expect(room).toContain('class="rfg-room__back"')
  })

  it('requires an explicit active room before rendering multiplayer game state', () => {
    const game = read('src/pages-fruit-game/game/index.vue')
    const runtime = read('src/pages-fruit-game/runtime/multiplayer-runtime.ts')
    expect(game).toContain("query?.multiplayer === '1'")
    expect(game).toContain("title: '房间已退出'")
    expect(runtime).toContain('if (!runtime?.activeTraining) return false')
    expect(runtime).toContain("!['CLOSED', 'FAILED'].includes(state)")
  })

  it('keeps WeChat HUD and debug controls in bounded flex regions', () => {
    const hud = read('src/pages-fruit-game/components/GameHud.vue')
    const debug = read('src/pages-fruit-game/components/DebugControls.vue')
    expect(hud).toMatch(/\.rfg-hud \{[\s\S]*?display: flex;/)
    expect(hud).not.toContain('grid-template-columns: minmax(132px')
    expect(debug).toMatch(/\.rfg-debug--collapsed \{[^}]*width: 112px;[^}]*transform: none;/)
    expect(debug).toMatch(/\.rfg-debug__toggle \{[^}]*width: 112px;[^}]*height: 34px;/)
  })

  it('provides a compact portrait fallback for the result page', () => {
    const result = read('src/pages-fruit-game/result/index.vue')
    expect(result).toContain('@media (orientation: portrait), (max-width: 520px)')
    expect(result).toMatch(/\.rfg-result__columns \{[\s\S]*?flex-direction: column;/)
    expect(result).toContain('grid-template-columns: repeat(2, minmax(0, 1fr))')
    expect(result).toMatch(/\.rfg-result__metric \{[^}]*white-space: normal;/)
  })

  it('anchors each fruit, glow and label inside one movable target container', () => {
    const game = read('src/pages-fruit-game/game/index.vue')
    expect(game).toContain('class="rfg-game__target-visual"')
    expect(game).toMatch(/\.rfg-game__target-glow, \.rfg-game__fruit \{[^}]*inset: 0;[^}]*width: 100%;[^}]*height: 100%;/)
    expect(game).toMatch(/\.rfg-game__target--left \{ left: 8%; \}/)
    expect(game).toMatch(/\.rfg-game__target--right \{ right: 8%; \}/)
  })

  it('keeps the goalkeeper offset from the central player and adds result scrolling fallbacks', () => {
    const game = read('src/pages-penalty-game/game/index.vue')
    const result = read('src/pages-penalty-game/result/index.vue')
    expect(game).toMatch(/\.rpg-game__keeper\{left:4%;/)
    expect(game).toMatch(/\.rpg-game__keeper--left\{transform:translateX\(-52%\)/)
    expect(game).toMatch(/\.rpg-game__keeper--right\{transform:translateX\(62%\)/)
    expect(result).toContain('overflow-y:auto')
    expect(result).toContain('@media(orientation:portrait),(max-width:520px)')
  })

  it('does not clear multiplayer runtimes during normal lobby navigation', () => {
    const fruitLobby = read('src/pages-fruit-game/multiplayer/lobby/index.vue')
    const penaltyLobby = read('src/pages-penalty-game/multiplayer/lobby/index.vue')
    const fruitRoom = read('src/pages-fruit-game/multiplayer/room/index.vue')
    const penaltyRoom = read('src/pages-penalty-game/multiplayer/room/index.vue')
    const calibrate = read('src/pages-penalty-game/calibrate/index.vue')
    expect(fruitLobby).toContain('shouldClearMultiplayerOnUnload(this.navigating, this.leaving)')
    expect(penaltyLobby).toContain('shouldClearMultiplayerOnUnload(this.navigating, this.leaving)')
    expect(fruitRoom).toContain('if(!this.navigating&&!this.leaving)leaveCurrentMultiplayerRoom()')
    expect(penaltyRoom).toContain('if(!this.navigating&&!this.leaving)leavePenaltyRoom()')
    expect(calibrate).toContain('onUnload(){this.clear()}')
    expect(calibrate).toContain('if(this.multiplayer)leavePenaltyRoom()')
  })
})
