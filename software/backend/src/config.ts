import { z } from 'zod'

const environmentSchema = z.object({
  NODE_ENV: z.enum(['development', 'test', 'production']).default('development'),
  PORT: z.coerce.number().int().min(1).max(65535).default(80),
  HOST: z.string().default('0.0.0.0'),
  CLOUDBASE_ENV_ID: z.string().min(1),
  CLOUDBASE_API_KEY: z.string().min(20),
  WECHAT_APP_ID: z.string().regex(/^wx[0-9a-f]{16}$/i),
  WECHAT_APP_SECRET: z.string().min(1),
  TOKEN_SECRET: z.string().min(32)
})

export type AppConfig = z.infer<typeof environmentSchema>

export function loadConfig(source: NodeJS.ProcessEnv = process.env): AppConfig {
  const result = environmentSchema.safeParse(source)
  if (result.success) return result.data
  const fields = result.error.issues.map(issue => issue.path.join('.')).filter(Boolean).join('、')
  throw new Error(`服务器环境变量不完整：${fields}`)
}
