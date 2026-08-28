<template>
  <view class="chart-shell" :class="{ compact }">
    <canvas class="report-line-chart" :canvas-id="canvasId" :id="canvasId"></canvas>
  </view>
</template>

<script>
export default {
  props: {
    canvasId: { type: String, default: 'reportLineChart' },
    values: { type: Array, default: () => [] },
    labels: { type: Array, default: () => [] },
    suffix: { type: String, default: '%' },
    color: { type: String, default: '#17644F' },
    accentColor: { type: String, default: '#B8DC58' },
    minValue: { type: Number, default: 60 },
    maxValue: { type: Number, default: 100 },
    compact: { type: Boolean, default: false }
  },
  watch: {
    values: { deep: true, handler() { this.queueDraw() } },
    labels: { deep: true, handler() { this.queueDraw() } },
    minValue() { this.queueDraw() },
    maxValue() { this.queueDraw() },
    compact() { this.queueDraw() }
  },
  mounted() { this.queueDraw() },
  methods: {
    queueDraw() { this.$nextTick(() => setTimeout(() => this.draw(), 60)) },
    formatValue(value) {
      const numeric = Number(value)
      return `${Number.isInteger(numeric) ? numeric : numeric.toFixed(1)}${this.suffix}`
    },
    draw() {
      const values = this.values.map(Number).filter(Number.isFinite)
      if (!values.length) return
      const query = uni.createSelectorQuery().in(this)
      query.select('.report-line-chart').boundingClientRect(rect => {
        if (!rect || !rect.width || !rect.height) return
        const width = rect.width
        const height = rect.height
        const px = value => Math.max(1, uni.upx2px(value))
        const left = this.compact ? px(8) : px(72)
        const right = this.compact ? px(8) : px(18)
        const top = this.compact ? px(40) : px(48)
        const bottom = this.compact ? px(38) : px(44)
        const chartWidth = Math.max(1, width - left - right)
        const chartHeight = Math.max(1, height - top - bottom)
        const range = Math.max(1, this.maxValue - this.minValue)
        const ctx = uni.createCanvasContext(this.canvasId, this)
        ctx.clearRect(0, 0, width, height)
        ctx.setLineCap('round')
        ctx.setLineJoin('round')

        const gridCount = 3
        for (let index = 0; index < gridCount; index += 1) {
          const ratio = gridCount === 1 ? 0 : index / (gridCount - 1)
          const y = top + chartHeight * ratio
          ctx.setStrokeStyle(this.compact ? 'rgba(23,100,79,.08)' : '#E5EBE8')
          ctx.setLineWidth(1)
          ctx.beginPath(); ctx.moveTo(left, y); ctx.lineTo(width - right, y); ctx.stroke()
          if (!this.compact) {
            const tick = Math.round(this.maxValue - range * ratio)
            ctx.setFillStyle('#8B9893'); ctx.setFontSize(px(22)); ctx.setTextAlign('right')
            ctx.fillText(`${tick}${this.suffix}`, left - px(12), y + px(7))
          }
        }

        const slotWidth = chartWidth / Math.max(1, values.length)
        const points = values.map((value, index) => ({
          x: left + slotWidth * (index + 0.5),
          y: top + Math.max(0, Math.min(1, (this.maxValue - value) / range)) * chartHeight,
          value
        }))

        if (points.length > 1) {
          const fill = ctx.createLinearGradient(0, top, 0, top + chartHeight)
          fill.addColorStop(0, 'rgba(47,120,103,.16)')
          fill.addColorStop(1, 'rgba(47,120,103,.01)')
          ctx.setFillStyle(fill)
          ctx.beginPath(); ctx.moveTo(points[0].x, top + chartHeight)
          points.forEach(point => ctx.lineTo(point.x, point.y))
          ctx.lineTo(points[points.length - 1].x, top + chartHeight); ctx.closePath(); ctx.fill()
        }

        ctx.setStrokeStyle(this.color); ctx.setLineWidth(this.compact ? px(5) : px(6))
        ctx.beginPath()
        points.forEach((point, index) => index ? ctx.lineTo(point.x, point.y) : ctx.moveTo(point.x, point.y))
        ctx.stroke()

        points.forEach((point, index) => {
          const latest = index === points.length - 1
          ctx.setFillStyle(latest ? '#174F42' : '#FFFFFF')
          ctx.setStrokeStyle(latest ? this.accentColor : this.color)
          ctx.setLineWidth(latest ? px(5) : px(4))
          ctx.beginPath(); ctx.arc(point.x, point.y, latest ? px(10) : px(8), 0, Math.PI * 2); ctx.fill(); ctx.stroke()

          const previous = points[index - 1]
          const crowded = previous && point.x - previous.x < px(88) && Math.abs(point.y - previous.y) < px(18)
          const extraLift = crowded && index % 2 === 1 ? px(10) : 0
          const valueY = Math.max(px(24), point.y - px(18) - extraLift)
          ctx.setFillStyle(latest ? '#174F42' : '#50665F')
          ctx.setFontSize(px(24))
          ctx.setTextAlign('center')
          ctx.fillText(this.formatValue(point.value), point.x, valueY)

          const labelStep = points.length > 8 ? Math.ceil(points.length / 6) : 1
          if (index % labelStep === 0 || latest) {
            ctx.setFillStyle('#7D8B86')
            ctx.setFontSize(px(24))
            ctx.setTextAlign('center')
            ctx.fillText(this.labels[index] || '', point.x, height - px(8))
          }
        })
        ctx.draw()
      }).exec()
    }
  }
}
</script>

<style scoped>
.chart-shell,.report-line-chart { width: 100%; height: 272rpx; }
.chart-shell { overflow: hidden; }
.chart-shell.compact,.compact .report-line-chart { height: 224rpx; }
</style>
