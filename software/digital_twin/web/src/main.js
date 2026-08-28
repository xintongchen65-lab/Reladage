import './styles.css'

const ASSET = '/assets/'

const states = [
  {
    id: 'mapping',
    kicker: '01 / 实时映射',
    label: '实时姿态映射',
    title: '肘关节屈伸',
    status: '正常状态',
    statusTone: 'ok',
    rom: '72°',
    angle: '72°',
    offset: '0°',
    stability: '—',
    detail: '右肘运动轨迹已锁定',
    stateImage: '01_normal_72deg_fullbody.png',
    trajectory: '12_normal_trajectory.png',
    marker: '06_joint_marker_green.png',
    duration: 3200,
  },
  {
    id: 'error',
    kicker: '02 / 异常识别',
    label: '把动作错误“画出来”',
    title: '前臂偏离运动平面',
    status: '姿态异常',
    statusTone: 'warn',
    rom: '67°',
    angle: '67°',
    offset: '12°',
    stability: '64%',
    detail: '检测到肘部运动平面偏移',
    stateImage: '02_error_plane_offset_fullbody.png',
    trajectory: '13_error_trajectory.png',
    marker: '07_joint_marker_red.png',
    duration: 3600,
  },
  {
    id: 'corrected',
    kicker: '03 / 动作纠正',
    label: '运动平面正常',
    title: '动作调整完成',
    status: '动作达标',
    statusTone: 'ok',
    rom: '82°',
    angle: '82°',
    offset: '0°',
    stability: '91%',
    detail: '动作质量达到本组训练标准',
    stateImage: '03_corrected_82deg_fullbody.png',
    trajectory: '12_normal_trajectory.png',
    marker: '06_joint_marker_green.png',
    duration: 3200,
  },
  {
    id: 'insight',
    kicker: '04 / 数据沉淀',
    label: '数据进入下一次训练',
    title: '个人运动数字孪生',
    status: '本次训练完成',
    statusTone: 'insight',
    rom: '82°',
    angle: '82°',
    offset: '0°',
    stability: '91%',
    detail: '历史趋势已生成参数建议',
    stateImage: '03_corrected_82deg_fullbody.png',
    trajectory: '12_normal_trajectory.png',
    marker: '08_joint_marker_cyan.png',
    duration: 3600,
  },
]

const app = document.querySelector('#app')

app.innerHTML = `
  <main class="shell" aria-label="个人运动数字孪生展示页">
    <header class="topbar">
      <div class="brand">
        <img src="${ASSET}logo.png" alt="Reladage" class="brand__logo" />
        <div>
          <p class="eyebrow">RELADAGE / REHABMOTION</p>
          <h1>个人运动数字孪生</h1>
        </div>
      </div>
      <div class="session-chip"><span class="live-dot"></span>演示模式 · 右肘屈伸</div>
    </header>

    <section class="content">
      <div class="stage-panel">
        <div class="stage-heading">
          <div>
            <p class="section-label">DIGITAL TWIN / LIVE VIEW</p>
            <h2 id="state-title">肘关节屈伸</h2>
          </div>
          <div class="stage-index"><span id="state-step">01</span><i>/</i>04</div>
        </div>

        <div class="stage" id="stage" data-tone="ok">
          <div class="stage-grid"></div>
          <div class="axis-label axis-label--x">X</div>
          <div class="axis-label axis-label--y">Y</div>
          <img class="plane" id="plane" src="${ASSET}10_motion_plane.png" alt="" />
          <img class="trajectory" id="trajectory" src="${ASSET}12_normal_trajectory.png" alt="" />
          <div class="figure-wrap" id="figure-wrap">
            <img class="state-figure state-figure--back" id="state-figure-back" src="${ASSET}01_normal_72deg_fullbody.png" alt="" />
            <img class="state-figure" id="state-figure" src="${ASSET}01_normal_72deg_fullbody.png" alt="数字孪生人体" />
            <img class="arm-layer arm-layer--upper" src="${ASSET}02_right_upper_arm.png" alt="" />
            <img class="arm-layer arm-layer--forearm" src="${ASSET}03_right_forearm_hand.png" alt="" />
            <img class="tracking-ring" src="${ASSET}09_tracking_ring.png" alt="" />
            <img class="joint-marker" id="joint-marker" src="${ASSET}06_joint_marker_green.png" alt="" />
            <img class="rom-arc" src="${ASSET}11_rom_arc.png" alt="" />
          </div>
          <div class="figure-callout" id="figure-callout"><span class="callout-dot"></span><span>右肘关节</span></div>
          <div class="rom-bubble"><span class="rom-bubble__label">当前 ROM</span><strong id="rom-value">72°</strong><span class="rom-bubble__unit">右肘屈伸</span></div>
          <div class="plane-note" id="plane-note"><span class="plane-note__icon">↗</span><span>运动平面</span></div>
          <div class="error-callout" id="error-callout"><img src="${ASSET}02_warning_centered.png" alt="" /><div><strong>动作平面偏移 <em id="offset-value">12°</em></strong><span>姿态异常</span></div></div>
          <div class="success-callout" id="success-callout"><img src="${ASSET}14_success_check.png" alt="" /><div><strong>动作达标</strong><span>运动平面正常</span></div></div>
        </div>

        <div class="stage-caption"><span id="stage-kicker">01 / 实时映射</span><span class="caption-line"></span><span id="stage-label">实时姿态映射</span></div>
      </div>

      <aside class="metrics" aria-label="动作指标">
        <div class="status-block" id="status-block" data-tone="ok"><span class="status-mark"></span><div><p class="section-label">CURRENT STATE</p><strong id="status-label">正常状态</strong></div></div>
        <div class="metric-hero"><p class="section-label">ELBOW ROM</p><strong id="metric-rom">72°</strong><span>实时活动范围</span></div>
        <div class="metric-grid">
          <div class="metric"><span>当前角度</span><strong id="metric-angle">72°</strong></div>
          <div class="metric"><span>平面偏移</span><strong id="metric-offset">0°</strong></div>
          <div class="metric"><span>稳定性</span><strong id="metric-stability">—</strong></div>
          <div class="metric"><span>传感节点</span><strong>02</strong></div>
        </div>
        <div class="insight-card" id="insight-card"><div class="insight-card__head"><span class="section-label">AI PARAMETER SUGGESTION</span><span class="ai-dot">AI</span></div><p>下一阶段目标角度</p><div class="trend"><strong>60°</strong><span>→</span><strong class="trend__accent">65°</strong></div><small>基于本次 ROM 与稳定性趋势</small></div>
        <div class="detail-note"><span class="detail-note__line"></span><p id="detail-text">右肘运动轨迹已锁定</p></div>
      </aside>
    </section>

    <footer class="control-bar" id="control-bar">
      <div class="controls"><button class="icon-button" id="play-toggle" type="button" aria-label="暂停播放"><span class="pause-icon"></span></button><button class="text-button" id="replay" type="button">重播</button><div class="segment-buttons" role="tablist" aria-label="演示分段"><button class="segment-button is-active" data-segment="0" type="button">映射</button><button class="segment-button" data-segment="1" type="button">异常</button><button class="segment-button" data-segment="2" type="button">纠正</button><button class="segment-button" data-segment="3" type="button">沉淀</button></div></div>
      <div class="timeline"><span id="timeline-label">01 / 04</span><div class="timeline-track"><span id="timeline-progress"></span></div><button class="icon-button icon-button--small" id="fullscreen" type="button" aria-label="进入全屏">⛶</button></div>
    </footer>
  </main>
`

const refs = {
  stage: document.querySelector('#stage'),
  figure: document.querySelector('#state-figure'),
  figureBack: document.querySelector('#state-figure-back'),
  trajectory: document.querySelector('#trajectory'),
  marker: document.querySelector('#joint-marker'),
  title: document.querySelector('#state-title'),
  statusBlock: document.querySelector('#status-block'),
  statusLabel: document.querySelector('#status-label'),
  romValue: document.querySelector('#rom-value'),
  offsetValue: document.querySelector('#offset-value'),
  metricRom: document.querySelector('#metric-rom'),
  metricAngle: document.querySelector('#metric-angle'),
  metricOffset: document.querySelector('#metric-offset'),
  metricStability: document.querySelector('#metric-stability'),
  detail: document.querySelector('#detail-text'),
  kicker: document.querySelector('#stage-kicker'),
  label: document.querySelector('#stage-label'),
  step: document.querySelector('#state-step'),
  timelineLabel: document.querySelector('#timeline-label'),
  timelineProgress: document.querySelector('#timeline-progress'),
  insightCard: document.querySelector('#insight-card'),
  errorCallout: document.querySelector('#error-callout'),
  successCallout: document.querySelector('#success-callout'),
  plane: document.querySelector('#plane'),
  planeNote: document.querySelector('#plane-note'),
  playToggle: document.querySelector('#play-toggle'),
  replay: document.querySelector('#replay'),
  fullscreen: document.querySelector('#fullscreen'),
}

let activeIndex = 0
let isPlaying = true
let startedAt = performance.now()
let elapsedBeforePause = 0
let raf = 0

function setImage(img, file) {
  img.classList.remove('is-ready')
  img.addEventListener('load', () => img.classList.add('is-ready'), { once: true })
  img.src = `${ASSET}${file}`
}

function renderState(index, { resetClock = true } = {}) {
  activeIndex = index
  const state = states[index]
  refs.stage.dataset.tone = state.statusTone
  refs.statusBlock.dataset.tone = state.statusTone
  refs.title.textContent = state.title
  refs.statusLabel.textContent = state.status
  refs.romValue.textContent = state.rom
  refs.metricRom.textContent = state.rom
  refs.metricAngle.textContent = state.angle
  refs.metricOffset.textContent = state.offset
  refs.metricStability.textContent = state.stability
  refs.offsetValue.textContent = state.offset
  refs.detail.textContent = state.detail
  refs.kicker.textContent = state.kicker
  refs.label.textContent = state.label
  refs.step.textContent = String(index + 1).padStart(2, '0')
  refs.timelineLabel.textContent = `${String(index + 1).padStart(2, '0')} / 04`
  refs.insightCard.classList.toggle('is-visible', state.id === 'insight')
  refs.errorCallout.classList.toggle('is-visible', state.id === 'error')
  refs.successCallout.classList.toggle('is-visible', state.id === 'corrected' || state.id === 'insight')
  refs.plane.classList.toggle('is-dimmed', state.id === 'insight')
  refs.planeNote.classList.toggle('is-visible', state.id !== 'error')
  setImage(refs.figureBack, state.stateImage)
  setImage(refs.figure, state.stateImage)
  setImage(refs.trajectory, state.trajectory)
  setImage(refs.marker, state.marker)
  document.querySelectorAll('.segment-button').forEach((button) => button.classList.toggle('is-active', Number(button.dataset.segment) === index))
  if (resetClock) {
    startedAt = performance.now()
    elapsedBeforePause = 0
  }
}

function updateTimeline(now) {
  const state = states[activeIndex]
  const elapsed = isPlaying ? elapsedBeforePause + now - startedAt : elapsedBeforePause
  const progress = Math.min(elapsed / state.duration, 1)
  refs.timelineProgress.style.width = `${progress * 100}%`
  if (isPlaying && progress >= 1) {
    if (activeIndex < states.length - 1) renderState(activeIndex + 1)
    else { isPlaying = false; elapsedBeforePause = state.duration; refs.playToggle.setAttribute('aria-label', '播放'); refs.playToggle.innerHTML = '<span class="play-icon"></span>' }
  }
  raf = requestAnimationFrame(updateTimeline)
}

function togglePlay() {
  if (isPlaying) {
    elapsedBeforePause += performance.now() - startedAt
    isPlaying = false
    refs.playToggle.setAttribute('aria-label', '播放')
    refs.playToggle.innerHTML = '<span class="play-icon"></span>'
  } else {
    startedAt = performance.now()
    isPlaying = true
    refs.playToggle.setAttribute('aria-label', '暂停播放')
    refs.playToggle.innerHTML = '<span class="pause-icon"></span>'
  }
}

function replay() {
  isPlaying = true
  refs.playToggle.setAttribute('aria-label', '暂停播放')
  refs.playToggle.innerHTML = '<span class="pause-icon"></span>'
  renderState(0)
}

refs.playToggle.addEventListener('click', togglePlay)
refs.replay.addEventListener('click', replay)
document.querySelectorAll('.segment-button').forEach((button) => button.addEventListener('click', () => {
  isPlaying = false
  refs.playToggle.setAttribute('aria-label', '播放')
  refs.playToggle.innerHTML = '<span class="play-icon"></span>'
  renderState(Number(button.dataset.segment))
}))
refs.fullscreen.addEventListener('click', async () => {
  if (!document.fullscreenElement) await document.documentElement.requestFullscreen?.()
  else await document.exitFullscreen?.()
})

renderState(0)
raf = requestAnimationFrame(updateTimeline)
