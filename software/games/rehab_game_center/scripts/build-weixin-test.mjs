import { spawnSync } from 'node:child_process'
import path from 'node:path'

const projectRoot = process.cwd()
const cliPath = path.join(projectRoot, 'node_modules', '@dcloudio', 'vite-plugin-uni', 'bin', 'uni.js')
const outputDir = path.join(projectRoot, 'dist', 'build', 'mp-weixin-test')

const result = spawnSync(process.execPath, [cliPath, 'build', '-p', 'mp-weixin'], {
  cwd: projectRoot,
  env: {
    ...process.env,
    RFG_WEIXIN_TEST_BUILD: '1',
    UNI_OUTPUT_DIR: outputDir
  },
  stdio: 'inherit'
})

if (result.error) throw result.error
process.exit(result.status ?? 1)
