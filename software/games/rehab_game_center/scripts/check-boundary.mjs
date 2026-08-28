import fs from 'node:fs'
import path from 'node:path'

const projectRoot = process.cwd()
const moduleRoot = path.join(projectRoot, 'src', 'pages-fruit-game')
const penaltyRoot = path.join(projectRoot, 'src', 'pages-penalty-game')
const vitalityRoot = path.join(projectRoot, 'src', 'pages-vitality-park')
const moleRoot = path.join(projectRoot, 'src', 'pages-mole-game')
const staticRoot = path.join(moduleRoot, 'static')
const maxBytes = Math.floor(1.8 * 1024 * 1024)
const textExtensions = new Set(['.ts', '.vue', '.json', '.md', '.scss', '.css'])
const forbidden = [
  [/App\.vue/i, '不得导入 App.vue'],
  [/\bpinia\b/i, '不得引用 Pinia'],
  [/\bvuex\b/i, '不得引用 Vuex'],
  [/getApp\s*\(/, '不得调用 getApp()'],
  [/globalData/i, '不得引用 globalData'],
  [/uni\.\$(?:emit|on|once|off)/, '不得使用 uni 全局事件总线'],
  [/forceRepEvent/, '不得保留强制动作事件入口'],
  [/(?:get|set|remove)StorageSync\s*\(/, '游戏分包不得直接读写本地持久化'],
  [/wx\.cloud|uniCloud/i, '不得包含云开发逻辑'],
  [/\bwx[0-9a-z]{16}\b/i, '不得包含微信 AppID']
]

function walk(directory) {
  return fs.readdirSync(directory, { withFileTypes: true }).flatMap((entry) => {
    const fullPath = path.join(directory, entry.name)
    return entry.isDirectory() ? walk(fullPath) : [fullPath]
  })
}

const errors = []
const files = walk(moduleRoot)
const penaltyFiles = walk(penaltyRoot)
const vitalityFiles = walk(vitalityRoot)
const moleFiles = walk(moleRoot)
for (const file of files) {
  if (!textExtensions.has(path.extname(file))) continue
  const relative = path.relative(projectRoot, file)
  const content = fs.readFileSync(file, 'utf8')
  if (/pages[\\/]demo/i.test(content) && !relative.endsWith(path.join('runtime', 'navigation-runtime.ts'))) {
    errors.push(`${relative}: 除可配置返回工具的默认兜底外，不得引用临时主包页面`)
  }
  if (content.includes('process.env.NODE_ENV') && !relative.endsWith(path.join('runtime', 'launch-config.ts'))) {
    errors.push(`${relative}: 调试构建能力只能由 launch-config 统一判定`)
  }
  for (const [pattern, message] of forbidden) {
    if (pattern.test(content)) errors.push(`${relative}: ${message}`)
  }
  if (file.endsWith('.vue')) {
    const styleTags = [...content.matchAll(/<style([^>]*)>/g)]
    if (!styleTags.length || styleTags.some((match) => !/\bscoped\b/.test(match[1]))) {
      errors.push(`${relative}: 所有样式必须使用 <style scoped>`)
    }
    if (/<script\s+setup\b/i.test(content)) errors.push(`${relative}: 不得使用 <script setup>`)
    if (/(^|\s)(page|view|text|image|button)\s*[,{]/m.test(content)) {
      errors.push(`${relative}: 不得使用无前缀标签选择器`)
    }
  }
}

const coreFiles = walk(path.join(moduleRoot, 'core')).filter((file) => file.endsWith('.ts'))
for (const file of coreFiles) {
  const content = fs.readFileSync(file, 'utf8')
  if (/from\s+['"](?:vue|pinia|@dcloudio)|\buni\.|\bwx\./.test(content)) {
    errors.push(`${path.relative(projectRoot, file)}: core 只能依赖纯 TypeScript 与分包 types`)
  }
}

const sourceFactoryReferences = files
  .filter((file) => file.endsWith('.ts') || file.endsWith('.vue'))
  .filter((file) => fs.readFileSync(file, 'utf8').includes('createMotionDataSource'))
if (sourceFactoryReferences.length !== 2) {
  errors.push(`数据源工厂应只有定义与游戏页装配两个引用，当前为 ${sourceFactoryReferences.length}`)
}

const staticBytes = walk(staticRoot).reduce((sum, file) => sum + fs.statSync(file).size, 0)
if (staticBytes > maxBytes) {
  errors.push(`运行素材 ${(staticBytes / 1024 / 1024).toFixed(2)} MiB，超过 1.8 MiB 目标`)
}

for (const buildName of ['mp-weixin', 'mp-weixin-test']) {
  const builtSubpackageRoot = path.join(projectRoot, 'dist', 'build', buildName, 'pages-fruit-game')
  if (fs.existsSync(builtSubpackageRoot)) {
    const builtBytes = walk(builtSubpackageRoot).reduce((sum, file) => sum + fs.statSync(file).size, 0)
    if (builtBytes > maxBytes) {
      errors.push(`${buildName} 分包 ${(builtBytes / 1024 / 1024).toFixed(2)} MiB，超过 1.8 MiB 目标`)
    }
  }
}
const penaltyStaticBytes = walk(path.join(penaltyRoot, 'static')).reduce((sum, file) => sum + fs.statSync(file).size, 0)
if (penaltyStaticBytes > maxBytes) errors.push(`点球运行素材 ${(penaltyStaticBytes / 1024 / 1024).toFixed(2)} MiB，超过 1.8 MiB 目标`)
const vitalityStaticBytes = walk(path.join(vitalityRoot, 'static')).reduce((sum, file) => sum + fs.statSync(file).size, 0)
if (vitalityStaticBytes > maxBytes) errors.push(`活力公园运行素材 ${(vitalityStaticBytes / 1024 / 1024).toFixed(2)} MiB，超过 1.8 MiB 目标`)
const moleStaticBytes = walk(path.join(moleRoot, 'static')).reduce((sum, file) => sum + fs.statSync(file).size, 0)
if (moleStaticBytes > maxBytes) errors.push(`地鼠大作战运行素材 ${(moleStaticBytes / 1024 / 1024).toFixed(2)} MiB，超过 1.8 MiB 目标`)
for (const file of penaltyFiles) {
  if (!textExtensions.has(path.extname(file))) continue
  const relative = path.relative(projectRoot, file)
  const content = fs.readFileSync(file, 'utf8')
  for (const [pattern, message] of forbidden) if (pattern.test(content)) errors.push(`${relative}: ${message}`)
  if (file.endsWith('.vue')) {
    const styleTags = [...content.matchAll(/<style([^>]*)>/g)]
    if (!styleTags.length || styleTags.some((match) => !/\bscoped\b/.test(match[1]))) errors.push(`${relative}: 所有样式必须使用 <style scoped>`)
    if (/<script\s+setup\b/i.test(content)) errors.push(`${relative}: 不得使用 <script setup>`)
  }
}
for (const file of vitalityFiles) {
  if (!textExtensions.has(path.extname(file))) continue
  const relative = path.relative(projectRoot, file)
  const content = fs.readFileSync(file, 'utf8')
  for (const [pattern, message] of forbidden) if (pattern.test(content)) errors.push(`${relative}: ${message}`)
  if (file.endsWith('.vue')) {
    const styleTags = [...content.matchAll(/<style([^>]*)>/g)]
    if (!styleTags.length || styleTags.some((match) => !/\bscoped\b/.test(match[1]))) errors.push(`${relative}: 所有样式必须使用 <style scoped>`)
    if (/<script\s+setup\b/i.test(content)) errors.push(`${relative}: 不得使用 <script setup>`)
  }
}

for (const file of moleFiles) {
  if (!textExtensions.has(path.extname(file))) continue
  const relative = path.relative(projectRoot, file)
  const content = fs.readFileSync(file, 'utf8')
  for (const [pattern, message] of forbidden) if (pattern.test(content)) errors.push(`${relative}: ${message}`)
  if (file.endsWith('.vue')) {
    const styleTags = [...content.matchAll(/<style([^>]*)>/g)]
    if (!styleTags.length || styleTags.some((match) => !/\bscoped\b/.test(match[1]))) errors.push(`${relative}: 所有样式必须使用 <style scoped>`)
    if (/<script\s+setup\b/i.test(content)) errors.push(`${relative}: 不得使用 <script setup>`)
  }
}

if (files.some((file) => path.basename(file) === 'training-accumulator.ts')) {
  errors.push('训练累计器不得继续作为独立微信运行模块')
}

const pagesJson = fs.readFileSync(path.join(projectRoot, 'src', 'pages.json'), 'utf8')
if (!pagesJson.includes('"root": "pages-fruit-game"')) errors.push('pages.json 缺少普通分包 root')
if (/"independent"\s*:\s*true/.test(pagesJson)) errors.push('pages-fruit-game 不得设置 independent:true')
const landscapeDeclarations = pagesJson.match(/"pageOrientation"\s*:\s*"landscape"/g) || []
if (landscapeDeclarations.length < 24) {
  errors.push(`四个游戏的横屏页面应在双端注册中包含完整声明，当前为 ${landscapeDeclarations.length}`)
}

const viewportLayoutFiles = [
  path.join(projectRoot, 'src', 'pages', 'demo', 'index.vue'),
  path.join(moduleRoot, 'prepare', 'index.vue'),
  path.join(moduleRoot, 'game', 'index.vue'),
  path.join(moduleRoot, 'result', 'index.vue'),
  path.join(moduleRoot, 'components', 'GameHud.vue'),
  path.join(moduleRoot, 'components', 'CharacterRig.vue'),
  path.join(moduleRoot, 'components', 'DebugControls.vue'),
  path.join(moduleRoot, 'multiplayer', 'result', 'index.vue')
]
for (const file of viewportLayoutFiles) {
  if (/\d+rpx\b/.test(fs.readFileSync(file, 'utf8'))) {
    errors.push(`${path.relative(projectRoot, file)}: 横屏关键布局不得使用按屏幕宽度放大的 rpx`)
  }
}

if (errors.length) {
  console.error(errors.map((error) => `- ${error}`).join('\n'))
  process.exit(1)
}

console.log(`Boundary check passed: fruit ${files.length} files/${(staticBytes/1024/1024).toFixed(2)} MiB; penalty ${penaltyFiles.length} files/${(penaltyStaticBytes/1024/1024).toFixed(2)} MiB; vitality ${vitalityFiles.length} files/${(vitalityStaticBytes/1024/1024).toFixed(2)} MiB; mole ${moleFiles.length} files/${(moleStaticBytes/1024/1024).toFixed(2)} MiB.`)
