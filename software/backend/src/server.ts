import { buildApp } from './app.js'
import { createWechatAuthClient } from './auth/wechat.js'
import { createTokenService } from './auth/token.js'
import { loadConfig } from './config.js'
import { CloudBaseRestRepository } from './repositories/cloudbase-rest.js'

const config = loadConfig()
const repository = new CloudBaseRestRepository(config.CLOUDBASE_ENV_ID, config.CLOUDBASE_API_KEY)
const app = buildApp({
  repository,
  tokenService: createTokenService(config.TOKEN_SECRET),
  wechatAuth: createWechatAuthClient(config.WECHAT_APP_ID, config.WECHAT_APP_SECRET),
  logger: true
})

async function shutdown(signal: string) {
  app.log.info({ signal }, '服务器正在关闭')
  await app.close()
}

process.once('SIGTERM', () => { void shutdown('SIGTERM') })
process.once('SIGINT', () => { void shutdown('SIGINT') })

try {
  await app.listen({ port: config.PORT, host: config.HOST })
} catch (error) {
  app.log.error(error)
  process.exit(1)
}
