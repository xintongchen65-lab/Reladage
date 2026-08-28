import fs from 'node:fs'
import path from 'node:path'
import { createRequire } from 'node:module'

const projectRoot = process.cwd()
const builds = [
  { name: '正式微信构建', root: path.join(projectRoot, 'dist', 'build', 'mp-weixin'), debugAllowed: false },
  { name: '微信测试构建', root: path.join(projectRoot, 'dist', 'build', 'mp-weixin-test'), debugAllowed: true }
]
const errors = []
const requireFromHere = createRequire(import.meta.url)

function walk(directory) {
  return fs.readdirSync(directory, { withFileTypes: true }).flatMap((entry) => {
    const fullPath = path.join(directory, entry.name)
    return entry.isDirectory() ? walk(fullPath) : [fullPath]
  })
}

for (const build of builds) {
  if (!fs.existsSync(build.root)) {
    errors.push(`${build.name}不存在：${path.relative(projectRoot, build.root)}`)
    continue
  }
  const files = walk(build.root)
  if (files.some((file) => file.replaceAll('\\', '/').endsWith('/pages-fruit-game/core/training-accumulator.js'))) {
    errors.push(`${build.name}摘水果分包仍包含独立 training-accumulator.js`)
  }
  for (const file of files.filter((candidate) => candidate.endsWith('.js'))) {
    const source = fs.readFileSync(file, 'utf8')
    for (const match of source.matchAll(/require\(["'](\.{1,2}\/[^"']+)["']\)/g)) {
      const target = path.resolve(path.dirname(file), match[1])
      if (!fs.existsSync(target) && !fs.existsSync(`${target}.js`) && !fs.existsSync(path.join(target, 'index.js'))) {
        errors.push(`${path.relative(projectRoot, file)} 引用了不存在的模块 ${match[1]}`)
      }
    }
    const relative = path.relative(build.root, file).replaceAll('\\', '/')
    if (relative.startsWith('pages-penalty-game/') && /require\(["'][^"']*pages-fruit-game/.test(source)) {
      errors.push(`${build.name}点球分包存在跨摘水果分包引用：${relative}`)
    }
    if (relative.startsWith('pages-fruit-game/') && /require\(["'][^"']*pages-penalty-game/.test(source)) {
      errors.push(`${build.name}摘水果分包存在跨点球分包引用：${relative}`)
    }
    if (relative.startsWith('pages-mole-game/') && /require\(["'][^"']*pages-(fruit|penalty|vitality-park)/.test(source)) {
      errors.push(`${build.name}地鼠分包存在跨游戏分包引用：${relative}`)
    }
    if (relative.startsWith('pages-vitality-park/') && /require\(["'][^"']*pages-(fruit|penalty|mole-game)/.test(source)) {
      errors.push(`${build.name}活力公园分包存在跨游戏分包引用：${relative}`)
    }
  }

  const launchConfigPath = path.join(build.root, 'game-platform', 'runtime', 'debug-gate.js')
  try {
    const launchConfig = requireFromHere(launchConfigPath)
    if (launchConfig.resolveDebugEnabled('1') !== build.debugAllowed) {
      errors.push(`${build.name}的 debug=1 门控结果不正确`)
    }
    if (launchConfig.resolveDebugEnabled(undefined) !== false) {
      errors.push(`${build.name}在未传 debug=1 时错误开启了调试能力`)
    }
  } catch (error) {
    errors.push(`${build.name}无法验证调试门控：${error instanceof Error ? error.message : String(error)}`)
  }

  const moleLaunchPath = path.join(build.root, 'pages-mole-game', 'runtime', 'launch-config.js')
  try {
    const launchConfig = requireFromHere(moleLaunchPath)
    if (launchConfig.configureMoleFromQuery({ debug: '1' }).debugEnabled !== build.debugAllowed) errors.push(`${build.name}地鼠 debug=1 门控结果不正确`)
    if (launchConfig.configureMoleFromQuery({}).debugEnabled !== false) errors.push(`${build.name}地鼠在未传 debug=1 时错误开启调试能力`)
  } catch (error) {
    errors.push(`${build.name}无法验证地鼠调试门控：${error instanceof Error ? error.message : String(error)}`)
  }

  const vitalityPrepare = fs.readFileSync(path.join(build.root, 'pages-vitality-park', 'prepare', 'index.js'), 'utf8')
  if (!/game-platform\/runtime\/debug-gate\.js/.test(vitalityPrepare) || !/resolveDebugEnabled/.test(vitalityPrepare)) {
    errors.push(`${build.name}活力公园未使用共享调试门控`)
  }

  const penaltyLaunchPath = path.join(build.root, 'pages-penalty-game', 'runtime', 'launch-config.js')
  try {
    const launchConfig = requireFromHere(penaltyLaunchPath)
    if (launchConfig.configurePenaltyFromQuery({ debug: '1' }).debugEnabled !== build.debugAllowed) errors.push(`${build.name}点球 debug=1 门控结果不正确`)
    if (launchConfig.configurePenaltyFromQuery({}).debugEnabled !== false) errors.push(`${build.name}点球在未传 debug=1 时错误开启调试能力`)
  } catch (error) {
    errors.push(`${build.name}无法验证点球调试门控：${error instanceof Error ? error.message : String(error)}`)
  }

  for (const packageName of ['pages-fruit-game','pages-penalty-game','pages-vitality-park','pages-mole-game']) {
    const packageRoot=path.join(build.root,packageName)
    const bytes=walk(packageRoot).reduce((sum,file)=>sum+fs.statSync(file).size,0)
    if(bytes>1.8*1024*1024)errors.push(`${build.name} ${packageName} ${(bytes/1024/1024).toFixed(2)} MiB，超过1.8 MiB`)
  }
}

if (errors.length) {
  console.error(errors.map((error) => `- ${error}`).join('\n'))
  process.exit(1)
}

console.log('WeChat build check passed: module graph is complete and debug gates are isolated.')
