<template>
  <view class="assignment-page">
    <app-page-header class="main-header" title="训练任务" :subtitle="`为${member.relationship} · ${member.name}安排居家康复`">
      <template #action><view class="sync-pill"><view class="status-dot" :class="{ pending: syncState !== '设备已确认' }"></view><text>{{ syncState }}</text></view></template>
    </app-page-header>

    <view class="assignment-content">

    <view class="overview-card">
      <view class="overview-head"><view><text>本次训练计划</text><text>自由组合标准动作和自定义动作</text></view><text class="safe-tag">保守默认</text></view>
      <view class="overview-metrics">
        <view><text>{{ plan.tasks.length }}</text><text>个动作</text></view>
        <view><text>{{ totalMoves }}</text><text>次动作</text></view>
        <view><text>{{ maxFrequency }}</text><text>天 / 周</text></view>
      </view>
    </view>

    <view class="section-head action-section-head"><text class="section-title">选择动作</text><text class="section-link">点击卡片查看详情</text></view>
    <view class="filter-row"><text v-for="item in filters" :key="item" :class="{ active: filter === item }" @tap="filter = item">{{ item }}</text></view>

    <scroll-view class="exercise-scroll" scroll-x :show-scrollbar="false">
      <view class="exercise-row">
        <view v-for="item in filteredExercises" :key="item.id" class="exercise-option pressable" :class="{ selected: isSelected(item.id) }" @tap="openExerciseDetail(item)">
          <view class="exercise-visual"><image :src="item.image" mode="aspectFit"></image><text class="select-mark" @tap.stop="toggleExercise(item)">{{ isSelected(item.id) ? '✓' : '＋' }}</text></view>
          <text class="exercise-option-name">{{ item.shortName }}</text>
          <view class="exercise-option-footer"><text>{{ item.deviceSupported ? '设备计数' : '动作指导' }}</text><text @tap.stop="toggleExercise(item)">{{ isSelected(item.id) ? '已加入' : '加入' }}</text></view>
        </view>
        <view class="exercise-option custom-option pressable" @tap="addCustomAction">
          <view class="custom-visual">＋</view><text class="exercise-option-name">自定义动作</text><view class="exercise-option-footer"><text>扩展接口</text><text>添加</text></view>
        </view>
      </view>
    </scroll-view>

    <view class="section-head"><text class="section-title">已选任务</text><text class="section-link">{{ plan.tasks.length }} 项 · 点击展开设置</text></view>
    <view v-if="!plan.tasks.length" class="empty-card card"><text>还没有选择动作</text><text>从上方动作库中选择至少一个训练动作</text></view>

    <view v-for="(task, index) in plan.tasks" :key="task.task_id" class="task-card card">
      <view class="task-head">
        <image v-if="task.image" class="task-thumb pressable" :src="task.image" mode="aspectFit" @tap="openTaskDetail(task)"></image>
        <view v-else class="task-thumb custom-thumb pressable" @tap="openTaskDetail(task)">自</view>
        <view class="task-copy pressable" @tap="openTaskDetail(task)"><text>{{ task.name }}</text><text>{{ task.region }} · {{ task.joint }} · {{ task.device_supported ? '设备自动计数' : '手动确认完成' }}</text></view>
        <text class="remove-action pressable" @tap="removeTask(index)">×</text>
      </view>

      <view class="task-summary pressable" @tap="toggleTaskSettings(task.task_id)">
        <text>{{ task.sets }} 组 × {{ task.reps }} 次 · 每周 {{ task.frequency_per_week }} 天 · {{ task.target_angle_deg }}°</text>
        <view class="task-expand-control"><text>{{ isTaskExpanded(task.task_id) ? '收起设置' : '展开设置' }}</text><view class="expand-chevron" :class="{ expanded: isTaskExpanded(task.task_id) }"><view></view></view></view>
      </view>

      <view v-if="isTaskExpanded(task.task_id)" class="task-settings">
        <view class="setting-row"><view class="setting-copy"><text>组数</text><text>范围 1—5</text></view><view class="setting-control"><view class="number-stepper"><text @tap="stepTask(index, 'sets', -1, 1, 5)">−</text><input type="number" maxlength="2" :value="task.sets" @input="manualTaskInput(index, 'sets', $event)" @blur="normalizeTaskInput(index, 'sets', 1, 5)"/><text @tap="stepTask(index, 'sets', 1, 1, 5)">＋</text></view><text class="setting-unit">组</text></view></view>
        <view class="setting-row"><view class="setting-copy"><text>每组次数</text><text>范围 1—30</text></view><view class="setting-control"><view class="number-stepper"><text @tap="stepTask(index, 'reps', -1, 1, 30)">−</text><input type="number" maxlength="2" :value="task.reps" @input="manualTaskInput(index, 'reps', $event)" @blur="normalizeTaskInput(index, 'reps', 1, 30)"/><text @tap="stepTask(index, 'reps', 1, 1, 30)">＋</text></view><text class="setting-unit">次</text></view></view>
        <view class="setting-row"><view class="setting-copy"><text>训练频率</text><text>范围每周 1—7 天</text></view><view class="setting-control"><view class="number-stepper"><text @tap="stepTask(index, 'frequency_per_week', -1, 1, 7)">−</text><input type="number" maxlength="1" :value="task.frequency_per_week" @input="manualTaskInput(index, 'frequency_per_week', $event)" @blur="normalizeTaskInput(index, 'frequency_per_week', 1, 7)"/><text @tap="stepTask(index, 'frequency_per_week', 1, 1, 7)">＋</text></view><text class="setting-unit wide-unit">天/周</text></view></view>
        <view class="setting-row"><view class="setting-copy"><text>目标角度</text><text>范围 30°—120°</text></view><view class="setting-control"><view class="number-stepper"><text @tap="stepTask(index, 'target_angle_deg', -5, 30, 120)">−</text><input type="number" maxlength="3" :value="task.target_angle_deg" @input="manualTaskInput(index, 'target_angle_deg', $event)" @blur="normalizeTaskInput(index, 'target_angle_deg', 30, 120)"/><text @tap="stepTask(index, 'target_angle_deg', 5, 30, 120)">＋</text></view><text class="setting-unit">°</text></view></view>
        <view class="setting-row"><view class="setting-copy"><text>保持时间</text><text>达到目标角度后，范围 0—15 秒</text></view><view class="setting-control"><view class="number-stepper"><text @tap="stepTask(index, 'hold_sec', -1, 0, 15)">−</text><input type="number" maxlength="2" :value="task.hold_sec" @input="manualTaskInput(index, 'hold_sec', $event)" @blur="normalizeTaskInput(index, 'hold_sec', 0, 15)"/><text @tap="stepTask(index, 'hold_sec', 1, 0, 15)">＋</text></view><text class="setting-unit">秒</text></view></view>
        <view class="setting-row total-row"><view class="setting-copy"><text>预计动作量</text><text>组数 × 每组次数</text></view><view class="read-value"><text>{{ Number(task.sets || 0) * Number(task.reps || 0) }}</text><text>次</text></view></view>
      </view>
    </view>

    <view class="section-head"><text class="section-title">高级设置</text><text class="section-link">专业人员选填</text></view>
    <view class="advanced-card card">
      <view class="advanced-toggle"><view><text>手动设置专业参数</text><text>关闭时使用保守预设</text></view><switch :checked="plan.advanced.enabled" color="#2F7867" @change="toggleAdvanced" /></view>
      <view v-if="!plan.advanced.enabled" class="safe-defaults"><text>缓慢节奏</text><text>休息 60 秒</text><text>疼痛 ≥ 4 停止</text><text>最长 20 分钟</text></view>
      <view v-else class="advanced-settings">
        <view class="setting-row"><view class="setting-copy"><text>动作节奏</text><text>范围 3—12 秒</text></view><view class="setting-control"><view class="number-stepper"><text @tap="stepAdvanced('tempo_sec_per_rep', -1, 3, 12)">−</text><input type="number" maxlength="2" :value="plan.advanced.tempo_sec_per_rep" @input="manualAdvancedInput('tempo_sec_per_rep', $event)" @blur="normalizeAdvancedInput('tempo_sec_per_rep', 3, 12)"/><text @tap="stepAdvanced('tempo_sec_per_rep', 1, 3, 12)">＋</text></view><text class="setting-unit">秒</text></view></view>
        <view class="setting-row"><view class="setting-copy"><text>组间休息</text><text>范围 20—180 秒</text></view><view class="setting-control"><view class="number-stepper"><text @tap="stepAdvanced('rest_sec', -10, 20, 180)">−</text><input type="number" maxlength="3" :value="plan.advanced.rest_sec" @input="manualAdvancedInput('rest_sec', $event)" @blur="normalizeAdvancedInput('rest_sec', 20, 180)"/><text @tap="stepAdvanced('rest_sec', 10, 20, 180)">＋</text></view><text class="setting-unit">秒</text></view></view>
        <view class="setting-row"><view class="setting-copy"><text>角度容差</text><text>范围 5°—30°</text></view><view class="setting-control"><view class="number-stepper"><text @tap="stepAdvanced('angle_tolerance_deg', -5, 5, 30)">−</text><input type="number" maxlength="2" :value="plan.advanced.angle_tolerance_deg" @input="manualAdvancedInput('angle_tolerance_deg', $event)" @blur="normalizeAdvancedInput('angle_tolerance_deg', 5, 30)"/><text @tap="stepAdvanced('angle_tolerance_deg', 5, 5, 30)">＋</text></view><text class="setting-unit">°</text></view></view>
        <view class="setting-row"><view class="setting-copy"><text>疼痛停止阈值</text><text>范围 1—10</text></view><view class="setting-control"><view class="number-stepper"><text @tap="stepAdvanced('pain_stop_score', -1, 1, 10)">−</text><input type="number" maxlength="2" :value="plan.advanced.pain_stop_score" @input="manualAdvancedInput('pain_stop_score', $event)" @blur="normalizeAdvancedInput('pain_stop_score', 1, 10)"/><text @tap="stepAdvanced('pain_stop_score', 1, 1, 10)">＋</text></view><text class="setting-unit wide-unit">/ 10</text></view></view>
        <view class="setting-row"><view class="setting-copy"><text>单次最长时间</text><text>范围 10—60 分钟</text></view><view class="setting-control"><view class="number-stepper"><text @tap="stepAdvanced('max_session_min', -5, 10, 60)">−</text><input type="number" maxlength="2" :value="plan.advanced.max_session_min" @input="manualAdvancedInput('max_session_min', $event)" @blur="normalizeAdvancedInput('max_session_min', 10, 60)"/><text @tap="stepAdvanced('max_session_min', 5, 10, 60)">＋</text></view><text class="setting-unit">分</text></view></view>
        <view class="switch-row"><view class="setting-copy"><text>设备异常自动停止</text><text>信号丢失、未标定或设备报警时停止</text></view><switch :checked="plan.advanced.auto_stop_on_warning" color="#2F7867" @change="toggleAutoStop" /></view>
      </view>
    </view>

    <view class="game-card card"><view class="game-mark">游</view><view class="game-copy"><text>趣味训练</text><text>按已选动作匹配对应的康复游戏</text></view><switch :checked="plan.game.enabled" color="#2F7867" @change="toggleGame" /></view>
    <view v-if="plan.game.enabled && availableGameEntries.length" class="game-launch-list">
      <view v-for="entry in availableGameEntries" :key="entry.task.task_id" class="game-launch-card card">
        <view><text>{{ entry.title }}</text><text>{{ entry.task.name }} · {{ entry.task.sets }} 组 × {{ entry.task.reps }} 次</text></view>
        <button class="game-launch-button pressable" @tap="openRehabGame(entry)">进入游戏</button>
      </view>
    </view>
    <view v-else-if="plan.game.enabled" class="game-note">当前方案中还没有可匹配的游戏动作。</view>
    <view class="safety-note"><text>i</text><text>保守预设不能替代医生或康复师评估。目标角度、训练剂量和疼痛阈值应结合老人实际情况确认。</text></view>

    <button class="dispatch-button pressable" :disabled="!plan.tasks.length" @tap="dispatchPlan">保存并下发</button>
    <button class="live-training-entry pressable" @tap="goLiveTraining">进入实时训练</button>

    <view v-if="detailExercise && detail" class="detail-overlay" @tap="closeDetail" @touchmove.stop.prevent>
      <view class="detail-dialog" @tap.stop>
        <view class="dialog-head"><view><text>{{ detailExercise.name }}</text><text>{{ detailExercise.region }} · {{ detailExercise.joint }}</text></view><button class="dialog-close pressable" @tap="closeDetail">×</button></view>
        <scroll-view class="detail-scroll" scroll-y :show-scrollbar="false">
          <video v-if="detail.demoVideo" class="dialog-media" :src="detail.demoVideo" controls></video>
          <image v-else class="dialog-media" :src="detailExercise.image" mode="aspectFit"></image>
          <view class="media-note"><text>{{ detail.demoVideo ? '动画演示' : '高清动作示意' }}</text></view>
          <text class="dialog-summary">{{ detail.summary }}</text>
          <text class="dialog-section-title">动作步骤</text>
          <view class="dialog-steps"><view v-for="(step, stepIndex) in detail.steps" :key="step"><text>{{ stepIndex + 1 }}</text><text>{{ step }}</text></view></view>
          <text class="dialog-section-title">动作要点</text>
          <view class="dialog-tips"><text v-for="tip in detail.tips" :key="tip">✓ {{ tip }}</text></view>
          <view class="dialog-caution"><text>注意</text><text>{{ detail.caution }}</text></view>
        </scroll-view>
        <view class="dialog-footer"><button class="dialog-primary pressable" :class="{ selected: detailSelected }" @tap="toggleDetailExercise">{{ detailSelected ? '已加入训练任务' : '加入训练任务' }}</button></view>
      </view>
    </view>
    </view>
  </view>
</template>

<script>
import AppPageHeader from '../../components/app-page-header/app-page-header.vue'
import { exerciseCatalog } from '../../data/catalog.js'
import { getExerciseDetail } from '../../data/exercise-details.js'
import { conservativeAdvancedDefaults, createTrainingTask, getTrainingTaskPlan, writeTrainingTaskPlan } from '../../services/training-plan.js'
import { getDeviceSnapshot } from '../../services/device.js'
import { getAvailableGameEntries, launchRehabGame } from '../../services/game-launcher.js'
import { getCurrentMember } from '../../services/members.js'

const clone = value => JSON.parse(JSON.stringify(value))

export default {
  components: { AppPageHeader },
  data() {
    const plan = clone(getTrainingTaskPlan())
    const device = getDeviceSnapshot()
    plan.advanced = { ...conservativeAdvancedDefaults, ...(plan.advanced || {}) }
    plan.game = { enabled: false, provider: '', scene_id: '', payload: {}, ...(plan.game || {}) }
    return { plan, device, member: getCurrentMember(), filter: '全部', filters: ['全部', '上肢', '下肢'], exercises: exerciseCatalog, syncState: plan.updated_at ? '已保存' : '未保存', expandedTaskIds: [], detailExercise: null, detail: null }
  },
  computed: {
    filteredExercises() { return this.filter === '全部' ? this.exercises : this.exercises.filter(item => item.region === this.filter) },
    totalMoves() { return this.plan.tasks.reduce((sum, task) => sum + Number(task.sets || 0) * Number(task.reps || 0), 0) },
    maxFrequency() { return this.plan.tasks.length ? Math.max(...this.plan.tasks.map(task => Number(task.frequency_per_week || 0))) : 0 },
    availableGameEntries() { return getAvailableGameEntries(this.plan.tasks) },
    detailSelected() { return this.detailExercise ? this.isSelected(this.detailExercise.id) : false }
  },
  methods: {
    isSelected(exerciseId) { return this.plan.tasks.some(task => task.exercise_id === exerciseId && task.source !== 'custom') },
    toggleExercise(exercise) {
      const index = this.plan.tasks.findIndex(task => task.exercise_id === exercise.id && task.source !== 'custom')
      if (index >= 0) this.plan.tasks.splice(index, 1)
      else this.plan.tasks.push(createTrainingTask(exercise))
      this.syncState = '未保存'
    },
    openExerciseDetail(item) { this.detailExercise = item; this.detail = getExerciseDetail(item.id) },
    openTaskDetail(task) {
      if (task.source === 'custom') { uni.showToast({ title: '自定义动作暂无示意资料', icon: 'none' }); return }
      const item = this.exercises.find(exercise => exercise.id === task.exercise_id)
      if (item) this.openExerciseDetail(item)
    },
    closeDetail() { this.detailExercise = null; this.detail = null },
    toggleDetailExercise() { if (!this.detailExercise) return; this.toggleExercise(this.detailExercise) },
    addCustomAction() {
      uni.showModal({ title: '添加自定义动作', editable: true, placeholderText: '请输入动作名称', confirmText: '添加', success: result => {
        const name = String(result.content || '').trim()
        if (!result.confirm) return
        if (!name) { uni.showToast({ title: '请输入动作名称', icon: 'none' }); return }
        const stamp = Date.now()
        this.plan.tasks.push(createTrainingTask({ id: 'custom', name, shortName: name, region: '自定义', joint: '待设置', deviceSupported: false, image: '', source: 'custom' }, { task_id: 'custom-' + stamp, exercise_id: 'custom-' + stamp, source: 'custom' }))
        this.syncState = '未保存'
      } })
    },
    removeTask(index) {
      const removed = this.plan.tasks[index]
      uni.showModal({
        title: '移除训练动作？',
        content: `将从本次任务中移除“${removed.name}”，其他动作设置不会改变。`,
        confirmText: '移除',
        confirmColor: '#B0634D',
        success: result => {
          if (!result.confirm) return
          this.plan.tasks.splice(index, 1)
          this.expandedTaskIds = this.expandedTaskIds.filter(id => id !== removed.task_id)
          this.syncState = '未保存'
        }
      })
    },
    isTaskExpanded(taskId) { return this.expandedTaskIds.includes(taskId) },
    toggleTaskSettings(taskId) {
      this.expandedTaskIds = this.isTaskExpanded(taskId) ? this.expandedTaskIds.filter(id => id !== taskId) : [...this.expandedTaskIds, taskId]
    },
    normalizeNumber(value, min, max) {
      const raw = String(value == null ? '' : value).trim()
      if (!raw) return min
      const numeric = Number(raw)
      return Number.isFinite(numeric) ? Math.min(max, Math.max(min, Math.round(numeric))) : min
    },
    manualTaskInput(index, key, event) { this.plan.tasks[index][key] = event.detail.value; this.syncState = '未保存' },
    normalizeTaskInput(index, key, min, max) { this.plan.tasks[index][key] = this.normalizeNumber(this.plan.tasks[index][key], min, max) },
    stepTask(index, key, amount, min, max) { const task = this.plan.tasks[index]; task[key] = Math.min(max, Math.max(min, this.normalizeNumber(task[key], min, max) + amount)); this.syncState = '未保存' },
    toggleAdvanced(event) { this.plan.advanced.enabled = event.detail.value; this.syncState = '未保存' },
    manualAdvancedInput(key, event) { this.plan.advanced[key] = event.detail.value; this.syncState = '未保存' },
    normalizeAdvancedInput(key, min, max) { this.plan.advanced[key] = this.normalizeNumber(this.plan.advanced[key], min, max) },
    stepAdvanced(key, amount, min, max) { this.plan.advanced[key] = Math.min(max, Math.max(min, this.normalizeNumber(this.plan.advanced[key], min, max) + amount)); this.syncState = '未保存' },
    toggleAutoStop(event) { this.plan.advanced.auto_stop_on_warning = event.detail.value; this.syncState = '未保存' },
    toggleGame(event) {
      const enabled = event.detail.value
      const entries = getAvailableGameEntries(this.plan.tasks)
      if (enabled && !entries.length) {
        this.plan.game.enabled = false
        uni.showToast({ title: '请先加入支持趣味训练的动作', icon: 'none' })
        return
      }
      this.plan.game = {
        ...this.plan.game,
        enabled,
        provider: enabled ? 'builtin' : '',
        scene_id: enabled ? 'rehab-games-v1.5' : '',
        payload: enabled ? { task_ids: entries.map(entry => entry.task.task_id) } : {}
      }
      this.syncState = '未保存'
      uni.showToast({ title: enabled ? '已开启趣味训练' : '已关闭趣味训练', icon: 'none' })
    },
    openRehabGame(entry) {
      launchRehabGame({ task: entry.task, advanced: this.plan.advanced, returnUrl: '/pages/plan/index' })
        .catch(error => uni.showToast({ title: error.message || '暂时无法进入游戏', icon: 'none' }))
    },
    goLiveTraining() { uni.navigateTo({ url: '/pages/training/index' }) },
    dispatchPlan() {
      if (!this.device.connected) { uni.showToast({ title: '设备未连接，无法下发', icon: 'none' }); return }
      this.plan.patient_name = `${this.member.relationship} · ${this.member.name}`
      uni.showLoading({ title: '写入设备' })
      writeTrainingTaskPlan(clone(this.plan)).then(result => {
        uni.hideLoading()
        this.plan = clone(result.plan)
        this.syncState = result.deviceAcknowledged ? '设备已确认' : '已保存'
        uni.showToast({ title: result.deviceAcknowledged ? '设备已确认任务' : '训练方案已保存', icon: result.deviceAcknowledged ? 'none' : 'success' })
      }).catch(error => { uni.hideLoading(); uni.showToast({ title: error.message, icon: 'none' }) })
    }
  },
  onShow() { this.device = getDeviceSnapshot(); this.member = getCurrentMember() }
}
</script>

<style scoped>
.assignment-page { min-height: 100vh; background: #f5f6f3; padding-bottom: calc(38rpx + env(safe-area-inset-bottom)); }
.assignment-content { padding: 10rpx 30rpx 0; }
.sync-pill { min-height: 44rpx; padding: 0; color: #267261; display: flex; align-items: center; gap: 8rpx; font-size: 28rpx; font-weight: 500; }
.sync-pill .status-dot { background: #2f7867; }.sync-pill .status-dot.pending { background: #d39b43; }
.overview-card { margin-top: 0; padding: 26rpx; border-radius: 29rpx; background: #174f42; color: #fff; box-shadow: 0 11rpx 27rpx rgba(23,79,66,.10); }.overview-head { display: flex; align-items: flex-start; justify-content: space-between; }.overview-head text { display: block; font-size: 32rpx; font-weight: 600; }.overview-head view > text:last-child { margin-top: 6rpx; color: #a7c2ba; font-size: 24rpx; font-weight: 400; }.overview-head .safe-tag { padding: 8rpx 12rpx; border-radius: 999rpx; background: rgba(217,238,127,.14); color: #d9ee7f; font-size: 24rpx; font-weight: 500; }
.overview-metrics { margin-top: 24rpx; display: grid; grid-template-columns: repeat(3,1fr); }.overview-metrics > view { padding-left: 20rpx; border-left: 1rpx solid rgba(255,255,255,.13); }.overview-metrics > view:first-child { padding-left: 0; border-left: 0; }.overview-metrics text { display: block; color: #d9ee7f; font-size: 44rpx; line-height: 1.05; font-weight: 700; letter-spacing: -1rpx; font-variant-numeric: tabular-nums; }.overview-metrics text:last-child { margin-top: 7rpx; color: #a7c2ba; font-size: 24rpx; font-weight: 400; letter-spacing: 0; }
.action-section-head { margin-top: 27rpx; }.filter-row { margin-top: 12rpx; display: flex; gap: 10rpx; }.filter-row text { min-width: 96rpx; height: 54rpx; padding: 0 17rpx; border-radius: 16rpx; background: #e8eeeb; color: #76857f; display: flex; align-items: center; justify-content: center; font-size: 28rpx; font-weight: 500; }.filter-row text.active { background: #174f42; color: #fff; }
.exercise-scroll { width: 100%; margin-top: 16rpx; }.exercise-row { display: flex; gap: 14rpx; padding-bottom: 5rpx; }.exercise-option { flex: none; width: 216rpx; padding-bottom: 15rpx; overflow: hidden; border: 2rpx solid transparent; border-radius: 22rpx; background: #fff; }.exercise-option.selected { border-color: #2f7867; background: #f7faf8; }.exercise-visual { position: relative; height: 142rpx; background: #f1f5f3; }.exercise-visual image { width: 100%; height: 100%; }.select-mark { position: absolute; right: 9rpx; top: 9rpx; width: 40rpx; height: 40rpx; border-radius: 50%; background: rgba(255,255,255,.95); color: #2f7867; display: flex; align-items: center; justify-content: center; font-size: 28rpx; font-weight: 600; }.exercise-option.selected .select-mark { background: #2f7867; color: #fff; }.exercise-option-name { display: block; margin: 13rpx 13rpx 0; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; color: #25483f; font-size: 28rpx; font-weight: 600; }.exercise-option-footer { margin: 7rpx 13rpx 0; display: flex; align-items: center; justify-content: space-between; }.exercise-option-footer text { color: #8b9893; font-size: 24rpx; font-weight: 400; }.exercise-option-footer text:last-child { color: #2f7867; font-weight: 600; }.custom-option { border: 2rpx dashed #b8c8c1; }.custom-visual { height: 142rpx; background: #edf3f0; color: #2f7867; display: flex; align-items: center; justify-content: center; font-size: 52rpx; font-weight: 400; }
.empty-card { padding: 34rpx; text-align: center; }.empty-card text { display: block; color: #395b52; font-size: 28rpx; font-weight: 600; }.empty-card text:last-child { margin-top: 8rpx; color: #8b9792; font-size: 24rpx; font-weight: 400; }
.task-card { margin-top: 15rpx; padding: 0; overflow: hidden; box-shadow: none; }.task-head { min-height: 116rpx; padding: 15rpx 20rpx; display: flex; align-items: center; }.task-thumb { flex: none; width: 84rpx; height: 84rpx; border-radius: 18rpx; background: #edf3f0; }.custom-thumb { color: #2f7867; display: flex; align-items: center; justify-content: center; font-size: 32rpx; font-weight: 600; }.task-copy { min-width: 0; flex: 1; margin-left: 15rpx; }.task-copy > text { display: block; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; color: #24483e; font-size: 30rpx; font-weight: 600; }.task-copy > text:last-child { margin-top: 7rpx; color: #87938e; font-size: 24rpx; font-weight: 400; }.remove-action { flex: none; width: 46rpx; height: 46rpx; margin-left: 10rpx; border-radius: 14rpx; background: #f7e8e2; color: #aa624f; display: flex; align-items: center; justify-content: center; font-size: 30rpx; font-weight: 500; }
.task-summary { min-height: 68rpx; padding: 0 20rpx; border-top: 1rpx solid #e6ece9; background: #fafcfb; display: flex; align-items: center; justify-content: space-between; gap: 14rpx; }.task-summary > text { min-width: 0; flex: 1; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; color: #64766f; font-size: 24rpx; font-weight: 400; font-variant-numeric: tabular-nums; }.task-summary > view { flex: none; display: flex; align-items: center; gap: 5rpx; color: #2f7867; }.task-summary > view text { font-size: 24rpx; font-weight: 600; }.task-expand-control { min-height: 40rpx; }.expand-chevron { flex: none; width: 32rpx; height: 32rpx; display: flex; align-items: center; justify-content: center; transition: transform .2s ease; transform-origin: center; }.expand-chevron > view { width: 11rpx; height: 11rpx; margin-top: -5rpx; border-right: 3rpx solid #2f7867; border-bottom: 3rpx solid #2f7867; transform: rotate(45deg); }.expand-chevron.expanded { transform: rotate(180deg); }
.task-settings,.advanced-settings { border-top: 1rpx solid #e1e8e4; }.setting-row,.switch-row { min-height: 108rpx; padding: 0 20rpx; border-bottom: 1rpx solid #e9eeeb; display: flex; align-items: center; justify-content: space-between; gap: 18rpx; }.setting-row:last-child,.switch-row:last-child { border-bottom: 0; }.setting-copy { min-width: 0; flex: 1; }.setting-copy text { display: block; color: #294c43; font-size: 28rpx; font-weight: 600; }.setting-copy text:last-child { margin-top: 5rpx; color: #8c9893; font-size: 24rpx; font-weight: 400; }.setting-control { flex: none; width: 304rpx; display: flex; align-items: center; justify-content: flex-start; }.number-stepper { flex: none; width: 216rpx; height: 60rpx; overflow: hidden; border: 1rpx solid #dfe7e3; border-radius: 16rpx; background: #fff; display: grid; grid-template-columns: 54rpx 1fr 54rpx; align-items: center; }.number-stepper > text { height: 60rpx; background: #edf3f0; color: #315e52; display: flex; align-items: center; justify-content: center; font-size: 30rpx; font-weight: 500; }.number-stepper input { width: 100%; height: 60rpx; color: #174f42; font-size: 32rpx; line-height: 60rpx; font-weight: 600; text-align: center; font-variant-numeric: tabular-nums; }.setting-unit { flex: none; width: 76rpx; margin-left: 12rpx; color: #5f726a; font-size: 24rpx; font-weight: 500; text-align: right; }.setting-unit.wide-unit { width: 76rpx; }.read-value { flex: none; width: 304rpx; display: grid; grid-template-columns: 54rpx 108rpx 54rpx 12rpx 76rpx; align-items: baseline; }.read-value text:first-child { grid-column: 2; color: #174f42; font-size: 40rpx; line-height: 1; font-weight: 700; text-align: center; font-variant-numeric: tabular-nums; }.read-value text:last-child { grid-column: 5; margin-left: 0; color: #65776f; font-size: 24rpx; font-weight: 500; text-align: right; }
.advanced-card { padding: 0; overflow: hidden; box-shadow: none; }.advanced-toggle { min-height: 112rpx; padding: 0 20rpx; display: flex; align-items: center; justify-content: space-between; }.advanced-toggle text { display: block; color: #284b42; font-size: 28rpx; font-weight: 600; }.advanced-toggle text:last-child { margin-top: 6rpx; color: #8c9893; font-size: 24rpx; font-weight: 400; }.safe-defaults { padding: 0 20rpx 21rpx; display: flex; flex-wrap: wrap; gap: 9rpx; }.safe-defaults text { padding: 8rpx 11rpx; border-radius: 999rpx; background: #e8f0e8; color: #587460; font-size: 24rpx; font-weight: 500; }
.game-card { margin-top: 24rpx; padding: 21rpx 23rpx; display: flex; align-items: center; box-shadow: none; }.game-launch-list { margin-top: 12rpx; display: grid; gap: 12rpx; }.game-launch-list .game-launch-card { margin-top: 0; }.game-mark { flex: none; width: 58rpx; height: 58rpx; border-radius: 18rpx; background: #e4eee9; color: #2f7867; display: flex; align-items: center; justify-content: center; font-size: 28rpx; font-weight: 700; }.game-copy { min-width: 0; flex: 1; margin-left: 15rpx; }.game-copy text { display: block; color: #294c43; font-size: 28rpx; font-weight: 600; }.game-copy text:last-child { margin-top: 6rpx; color: #8d9994; font-size: 24rpx; line-height: 1.45; font-weight: 400; }.game-launch-card { margin-top: 12rpx; padding: 19rpx 21rpx; display: flex; align-items: center; gap: 18rpx; box-shadow: none; }.game-launch-card > view { min-width: 0; flex: 1; }.game-launch-card > view text { display: block; color: #294c43; font-size: 28rpx; font-weight: 600; }.game-launch-card > view text:last-child { margin-top: 5rpx; color: #87938e; font-size: 24rpx; font-weight: 400; }.game-launch-button { flex: none; width: 164rpx; height: 62rpx; margin: 0; padding: 0; border: 0; border-radius: 18rpx; background: #174f42; color: #d9ee7f; display: flex; align-items: center; justify-content: center; font-size: 26rpx; line-height: 1; font-weight: 600; }.game-launch-button::after { border: 0; }
.game-note { margin-top: 10rpx; color: #76857f; font-size: 24rpx; line-height: 1.5; }.safety-note { margin-top: 20rpx; padding: 18rpx; border-radius: 18rpx; background: #f1eee8; color: #7e756b; display: flex; align-items: flex-start; gap: 11rpx; font-size: 24rpx; line-height: 1.55; font-weight: 400; }.safety-note > text:first-child { flex: none; width: 27rpx; height: 27rpx; border: 2rpx solid #93877b; border-radius: 50%; display: flex; align-items: center; justify-content: center; font-size: 20rpx; }
.dispatch-button { width: 100%; height: 84rpx; margin: 24rpx 0 0; padding: 0; border-radius: 22rpx; background: #174f42; color: #d9ee7f; display: flex; align-items: center; justify-content: center; font-size: 28rpx; line-height: 1; font-weight: 600; box-shadow: none; }.dispatch-button::after { border: 0; }.dispatch-button[disabled] { opacity: .45; }
.live-training-entry { width: 100%; height: 70rpx; margin-top: 12rpx; border: 2rpx solid #bfd0c8; border-radius: 20rpx; color: #2f7867; display: flex; align-items: center; justify-content: center; font-size: 28rpx; font-weight: 600; }
.detail-overlay { position: fixed; z-index: 100; inset: 0; padding: 40rpx 34rpx; background: rgba(13,35,29,.38); -webkit-backdrop-filter: blur(10rpx); backdrop-filter: blur(10rpx); display: flex; align-items: center; justify-content: center; }.detail-dialog { width: 640rpx; max-width: 100%; max-height: 80vh; overflow: hidden; border-radius: 28rpx; background: #fff; display: flex; flex-direction: column; box-shadow: 0 28rpx 70rpx rgba(6,31,24,.24); }.dialog-head { flex: none; min-height: 94rpx; padding: 17rpx 19rpx 15rpx 24rpx; border-bottom: 1rpx solid #e7ece9; display: flex; align-items: center; justify-content: space-between; }.dialog-head view { min-width: 0; flex: 1; }.dialog-head view text { display: block; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; color: #1f463c; font-size: 30rpx; font-weight: 600; }.dialog-head view text:last-child { margin-top: 4rpx; color: #87938e; font-size: 24rpx; font-weight: 400; }.dialog-close { flex: none; width: 54rpx; height: 54rpx; border-radius: 16rpx; background: #eef3f0; color: #46665d; font-size: 34rpx; }.detail-scroll { flex: 1; max-height: 61vh; }.dialog-media { width: 100%; height: 300rpx; background: linear-gradient(145deg,#fafcfb,#edf3f0); }.media-note { min-height: 58rpx; padding: 0 22rpx; border-bottom: 1rpx solid #e7ece9; display: flex; align-items: center; justify-content: space-between; }.media-note text { color: #41665b; font-size: 24rpx; font-weight: 600; }.media-note text:last-child { color: #919c97; font-size: 24rpx; font-weight: 400; }.dialog-summary { display: block; padding: 20rpx 22rpx 0; color: #5e716a; font-size: 28rpx; line-height: 1.55; font-weight: 400; }.dialog-section-title { display: block; padding: 23rpx 22rpx 12rpx; color: #24483e; font-size: 28rpx; font-weight: 600; }.dialog-steps { padding: 0 22rpx; }.dialog-steps > view { padding: 13rpx 0; display: flex; align-items: flex-start; gap: 13rpx; }.dialog-steps > view > text:first-child { flex: none; width: 38rpx; height: 38rpx; border-radius: 13rpx; background: #174f42; color: #d9ee7f; display: flex; align-items: center; justify-content: center; font-size: 24rpx; font-weight: 700; }.dialog-steps > view > text:last-child { flex: 1; color: #5e716a; font-size: 28rpx; line-height: 1.5; font-weight: 400; }.dialog-tips { padding: 0 22rpx; }.dialog-tips text { display: block; padding: 8rpx 0; color: #577068; font-size: 28rpx; font-weight: 400; }.dialog-caution { margin: 18rpx 22rpx 24rpx; padding: 16rpx; border-radius: 17rpx; background: #f7e8e1; }.dialog-caution text { display: block; color: #a45f4d; font-size: 24rpx; font-weight: 600; }.dialog-caution text:last-child { margin-top: 6rpx; color: #816b63; font-size: 24rpx; line-height: 1.5; font-weight: 400; }.dialog-footer { flex: none; padding: 14rpx 20rpx calc(14rpx + env(safe-area-inset-bottom)); border-top: 1rpx solid #e7ece9; }.dialog-primary { width: 100%; height: 70rpx; padding: 0; box-sizing: border-box; border: 2rpx solid #174f42; border-radius: 19rpx; background: #174f42; color: #d9ee7f; display: flex; align-items: center; justify-content: center; font-size: 28rpx; line-height: 1; font-weight: 600; box-shadow: none; }.dialog-primary::after { border: 0; }.dialog-primary.selected { background: #e6f0e7; color: #174f42; border-color: #2f7867; opacity: 1; }
</style>
