<template>
  <view class="agent-page">
    <scroll-view
      class="conversation"
      scroll-y
      :scroll-into-view="scrollTarget"
      :show-scrollbar="false"
    >
      <view class="agent-profile">
        <view class="agent-avatar">思</view>
        <view class="agent-profile-copy">
          <text class="agent-name">小思</text>
          <text class="agent-role">RehabMotion 康复智能助手</text>
        </view>
        <view class="context-state">
          <view class="state-dot"></view>
          <text>训练数据已就绪</text>
        </view>
      </view>

      <view class="context-card">
        <text class="context-label">本次演示数据</text>
        <text class="context-title">{{ member.relationship }} · {{ member.name }}　训练记录</text>
        <view class="context-grid">
          <view class="context-item">
            <text class="context-value">30</text>
            <text class="context-name">哑铃弯举</text>
          </view>
          <view class="context-divider"></view>
          <view class="context-item">
            <text class="context-value">20</text>
            <text class="context-name">肱三头肌伸展</text>
          </view>
          <view class="context-divider"></view>
          <view class="context-item">
            <text class="context-value">20</text>
            <text class="context-name">膝关节屈伸</text>
          </view>
        </view>
        <text class="context-frequency">三项动作均按每周 5 天安排</text>
      </view>

      <view class="message-list">
        <view
          v-for="(message, index) in messages"
          :id="`message-${index}`"
          :key="`${message.role}-${index}`"
          class="message-row"
          :class="message.role"
        >
          <view v-if="message.role === 'agent'" class="mini-avatar">思</view>
          <view class="message-bubble">
            <text class="message-text">{{ message.content }}</text>
            <text
              v-if="streaming && index === messages.length - 1 && message.role === 'agent'"
              class="stream-cursor"
            ></text>
            <view v-if="message.evidence" class="evidence-link" @tap="openWeekReport">
              <text>查看训练数据</text>
              <text>›</text>
            </view>
          </view>
        </view>

        <view v-if="loading" id="message-loading" class="message-row agent">
          <view class="mini-avatar">思</view>
          <view class="thinking-card">
            <view class="thinking-head">
              <view class="thinking-spinner"><view class="thinking-core"></view></view>
              <view class="thinking-copy">
                <text class="thinking-title">小思正在分析</text>
                <text class="thinking-stage">{{ thinkingStep }}</text>
              </view>
            </view>
            <view class="thinking-track">
              <view class="thinking-progress" :style="{ width: `${thinkingProgress}%` }"></view>
            </view>
          </view>
        </view>
      </view>

      <view v-if="messages.length === 1 && !busy" class="suggestions">
        <view class="suggestion-heading">
          <text class="suggestion-title">先试试这两项能力</text>
          <text class="suggestion-tip">也可以在下方输入其他问题</text>
        </view>
        <view class="suggestion-list">
          <view
            v-for="item in suggestions"
            :key="item.id"
            class="suggestion-item"
            @tap="sendMessage(item.label)"
          >
            <view class="suggestion-icon">{{ item.id === 'report-analysis' ? '析' : '荐' }}</view>
            <view class="suggestion-copy">
              <text class="suggestion-label">{{ item.label }}</text>
              <text class="suggestion-description">{{ item.description }}</text>
            </view>
            <text class="suggestion-arrow">›</text>
          </view>
        </view>
      </view>

      <text class="safety-note">小思根据训练记录提供辅助解读，不能替代医生或康复师的诊断与处方。</text>
      <view class="conversation-spacer"></view>
    </scroll-view>

    <view class="composer">
      <view class="input-wrap" :class="{ disabled: busy }">
        <input
          v-model="question"
          class="question-input"
          confirm-type="send"
          :disabled="busy"
          :adjust-position="true"
          placeholder="还可以继续问小思其他问题"
          @confirm="sendMessage()"
        />
        <button class="send-button" :disabled="busy || !question.trim()" @tap="sendMessage()">发送</button>
      </view>
    </view>
  </view>
</template>

<script>
import { getReportHomeData } from '../../services/report-dashboard.js'
import {
  XIAOSI_SHOWCASE_PROMPTS,
  getXiaosiShowcaseContext,
  getXiaosiThinkingSteps,
  sendToXiaosi
} from '../../services/xiaosi-agent.js'
import { getCurrentMember } from '../../services/members.js'

export default {
  data() {
    const dashboard = getReportHomeData()
    const member = getCurrentMember()
    const showcase = getXiaosiShowcaseContext()
    return {
      dashboard,
      member,
      showcase,
      question: '',
      loading: false,
      streaming: false,
      thinkingStep: '',
      thinkingProgress: 0,
      scrollTarget: 'message-0',
      generationId: 0,
      timerIds: [],
      suggestions: XIAOSI_SHOWCASE_PROMPTS,
      messages: [{
        role: 'agent',
        content: `我已经读取到 ${member.relationship}的三项训练记录，共 ${showcase.totalCount} 次。你可以让我分析这份报告、推荐下一次参数，也可以直接问其他康复训练问题。`,
        evidence: false
      }]
    }
  },
  computed: {
    busy() {
      return this.loading || this.streaming
    }
  },
  onUnload() {
    this.generationId += 1
    this.timerIds.forEach(timer => clearTimeout(timer))
    this.timerIds = []
  },
  methods: {
    openWeekReport() {
      uni.navigateTo({ url: '/pages/report/week' })
    },
    normalizeAnswer(result) {
      if (typeof result === 'string') return result
      return result && (result.answer || result.content || result.message)
        ? (result.answer || result.content || result.message)
        : '我暂时无法完成这次解读，请稍后再试。'
    },
    wait(milliseconds) {
      return new Promise(resolve => {
        const timer = setTimeout(() => {
          this.timerIds = this.timerIds.filter(item => item !== timer)
          resolve()
        }, milliseconds)
        this.timerIds.push(timer)
      })
    },
    async runThinking(steps, requestId) {
      this.thinkingProgress = 12
      for (let index = 0; index < steps.length; index += 1) {
        if (requestId !== this.generationId) return false
        this.thinkingStep = steps[index]
        this.thinkingProgress = Math.round(((index + 1) / steps.length) * 88)
        this.scrollToLast()
        await this.wait(index === steps.length - 1 ? 620 : 720)
      }
      return requestId === this.generationId
    },
    getCharacterDelay(character) {
      if (/。|！|？/.test(character)) return 120
      if (/，|：|；|\n/.test(character)) return 70
      return 24 + Math.floor(Math.random() * 18)
    },
    async streamAnswer(content, evidence, requestId) {
      const messageIndex = this.messages.length
      this.messages.push({ role: 'agent', content: '', evidence: false })
      this.loading = false
      this.streaming = true
      this.scrollToLast()

      const characters = Array.from(content)
      for (let index = 0; index < characters.length; index += 1) {
        if (requestId !== this.generationId) return
        const character = characters[index]
        this.messages[messageIndex].content += character
        if (index % 4 === 0 || /。|！|？|\n/.test(character)) this.scrollToLast()
        await this.wait(this.getCharacterDelay(character))
      }

      if (requestId === this.generationId) {
        this.messages[messageIndex].evidence = evidence
        this.streaming = false
        this.scrollToLast()
      }
    },
    async sendMessage(preset) {
      const content = String(preset || this.question || '').trim()
      if (!content || this.busy) return

      const requestId = ++this.generationId
      this.question = ''
      this.messages.push({ role: 'user', content })
      this.loading = true
      this.thinkingStep = '正在理解你的问题'
      this.thinkingProgress = 8
      this.scrollToLast()

      try {
        const steps = getXiaosiThinkingSteps(content)
        const requestPromise = sendToXiaosi(content, this.dashboard.week)
        const active = await this.runThinking(steps, requestId)
        if (!active) return
        const result = await requestPromise
        if (requestId !== this.generationId) return
        await this.streamAnswer(
          this.normalizeAnswer(result),
          result && result.evidence !== false,
          requestId
        )
      } catch (error) {
        if (requestId !== this.generationId) return
        this.loading = false
        this.streaming = false
        this.messages.push({ role: 'agent', content: '暂时无法完成这次解读，请稍后再试。', evidence: false })
        this.scrollToLast()
      }
    },
    scrollToLast() {
      this.scrollTarget = ''
      this.$nextTick(() => {
        this.scrollTarget = this.loading ? 'message-loading' : `message-${Math.max(0, this.messages.length - 1)}`
      })
    }
  }
}
</script>

<style scoped>
.agent-page{min-height:100vh;background:#f5f6f3;color:#17221f}.conversation{height:100vh;box-sizing:border-box;padding:28rpx 28rpx 0}.agent-profile{display:flex;align-items:center}.agent-avatar{width:72rpx;height:72rpx;flex:none;border-radius:22rpx;background:#0c5a46;color:#ccee69;display:flex;align-items:center;justify-content:center;font-size:32rpx;font-weight:700}.agent-profile-copy{flex:1;min-width:0;margin-left:18rpx}.agent-name,.agent-role{display:block}.agent-name{font-size:34rpx;line-height:1.25;font-weight:700}.agent-role{margin-top:6rpx;color:#7c8783;font-size:24rpx;line-height:1.35;font-weight:400}.context-state{display:flex;align-items:center;gap:8rpx;color:#5d756c;font-size:24rpx;font-weight:500}.state-dot{width:12rpx;height:12rpx;border-radius:50%;background:#67ad58}.context-card{margin-top:28rpx;padding:24rpx 26rpx;border:1rpx solid #dce6e1;border-radius:24rpx;background:#fff}.context-label,.context-title,.context-frequency{display:block}.context-label{color:#668078;font-size:24rpx;line-height:1.3;font-weight:500}.context-title{margin-top:9rpx;color:#173d33;font-size:30rpx;line-height:1.4;font-weight:600}.context-grid{display:flex;align-items:center;margin-top:22rpx}.context-item{flex:1;min-width:0}.context-value,.context-name{display:block}.context-value{color:#125a47;font-size:44rpx;line-height:1.05;font-weight:700;font-variant-numeric:tabular-nums}.context-name{margin-top:8rpx;color:#66736f;font-size:24rpx;line-height:1.35;font-weight:400;white-space:nowrap}.context-divider{width:1rpx;height:68rpx;margin:0 16rpx;background:#dce5e1}.context-frequency{margin-top:20rpx;padding-top:18rpx;border-top:1rpx solid #e8eeeb;color:#62716c;font-size:26rpx;line-height:1.4;font-weight:400}.message-list{margin-top:32rpx}.message-row{display:flex;align-items:flex-start;margin-bottom:24rpx}.message-row.user{justify-content:flex-end}.mini-avatar{width:52rpx;height:52rpx;flex:none;margin-right:14rpx;border-radius:17rpx;background:#145f4c;color:#d2ee7c;display:flex;align-items:center;justify-content:center;font-size:24rpx;font-weight:700}.message-bubble{max-width:548rpx;box-sizing:border-box;padding:20rpx 22rpx;border-radius:8rpx 24rpx 24rpx 24rpx;background:#fff;box-shadow:0 5rpx 16rpx rgba(21,58,47,.05)}.message-row.user .message-bubble{border-radius:24rpx 8rpx 24rpx 24rpx;background:#175b49;color:#fff}.message-text{font-size:28rpx;line-height:1.68;font-weight:400;white-space:pre-wrap}.stream-cursor{display:inline-block;width:4rpx;height:30rpx;margin-left:4rpx;vertical-align:-5rpx;border-radius:2rpx;background:#4f8e73;animation:cursorBlink .8s steps(1) infinite}.evidence-link{display:flex;align-items:center;justify-content:space-between;margin-top:18rpx;padding-top:16rpx;border-top:1rpx solid #e6ece9;color:#1e7059}.evidence-link text{font-size:24rpx;line-height:1.3;font-weight:500}.message-row.user .evidence-link{display:none}.thinking-card{width:468rpx;box-sizing:border-box;padding:22rpx;border:1rpx solid #e1e9e5;border-radius:8rpx 24rpx 24rpx 24rpx;background:#fff}.thinking-head{display:flex;align-items:center}.thinking-spinner{width:40rpx;height:40rpx;box-sizing:border-box;flex:none;margin-right:16rpx;border:3rpx solid #d7e6df;border-top-color:#1c725a;border-radius:50%;display:flex;align-items:center;justify-content:center;animation:spin 1.05s linear infinite}.thinking-core{width:9rpx;height:9rpx;border-radius:50%;background:#a6d45c}.thinking-copy{min-width:0}.thinking-title,.thinking-stage{display:block}.thinking-title{color:#1c493d;font-size:28rpx;line-height:1.3;font-weight:600}.thinking-stage{margin-top:6rpx;color:#778680;font-size:24rpx;line-height:1.4;font-weight:400}.thinking-track{height:6rpx;margin-top:18rpx;overflow:hidden;border-radius:999rpx;background:#e8eeeb}.thinking-progress{height:100%;border-radius:999rpx;background:linear-gradient(90deg,#1e7059,#a8d957);transition:width .42s ease}.suggestions{margin:8rpx 0 0 66rpx}.suggestion-heading{display:flex;align-items:flex-end;justify-content:space-between}.suggestion-title{color:#4f5f59;font-size:26rpx;line-height:1.4;font-weight:600}.suggestion-tip{color:#939d99;font-size:22rpx;line-height:1.4;font-weight:400}.suggestion-list{margin-top:14rpx}.suggestion-item{display:flex;align-items:center;min-height:104rpx;margin-bottom:14rpx;padding:0 18rpx;border:1rpx solid #d8e3de;border-radius:22rpx;background:#fff;color:#285e4f}.suggestion-icon{width:54rpx;height:54rpx;flex:none;border-radius:17rpx;background:#eaf3ee;color:#1b6a55;display:flex;align-items:center;justify-content:center;font-size:24rpx;font-weight:600}.suggestion-copy{flex:1;min-width:0;margin-left:16rpx}.suggestion-label,.suggestion-description{display:block}.suggestion-label{font-size:28rpx;line-height:1.35;font-weight:600}.suggestion-description{margin-top:5rpx;color:#7b8883;font-size:24rpx;line-height:1.35;font-weight:400}.suggestion-arrow{margin-left:12rpx;color:#7d9089;font-size:36rpx;line-height:1;font-weight:400}.safety-note{display:block;margin:34rpx 12rpx 0;color:#87928e;font-size:24rpx;line-height:1.55;font-weight:400;text-align:center}.conversation-spacer{height:210rpx}.composer{position:fixed;z-index:20;left:0;right:0;bottom:0;padding:18rpx 24rpx calc(14rpx + env(safe-area-inset-bottom));border-top:1rpx solid #e1e7e4;background:rgba(250,251,249,.98)}.input-wrap{display:flex;align-items:center;height:82rpx;padding-left:22rpx;border:1rpx solid #d4dfda;border-radius:24rpx;background:#fff}.input-wrap.disabled{background:#f1f3f1}.question-input{flex:1;height:82rpx;font-size:28rpx;font-weight:400}.send-button{width:112rpx;height:70rpx;margin-right:6rpx;border-radius:20rpx;background:#155b48;color:#cfed74;display:flex;align-items:center;justify-content:center;font-size:28rpx;font-weight:600}.send-button[disabled]{background:#a9b8b2;color:#edf2ef}@keyframes spin{to{transform:rotate(360deg)}}@keyframes cursorBlink{0%,48%{opacity:1}49%,100%{opacity:0}}
</style>
