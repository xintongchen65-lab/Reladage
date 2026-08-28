#!/usr/bin/env node

const HELP = `RehabMotion backend environment check

Usage:
  node --env-file=.env.local scripts/check-env.mjs

The command validates required variables without printing their values.`

if (process.argv.includes('--help') || process.argv.includes('-h')) {
  console.log(HELP)
  process.exit(0)
}

const rules = [
  ['CLOUDBASE_ENV_ID', value => value.length >= 8, 'must be a valid CloudBase environment ID'],
  ['CLOUDBASE_API_KEY', value => value.length >= 20, 'must contain at least 20 characters'],
  ['WECHAT_APP_ID', value => /^wx[0-9a-f]{16}$/i.test(value), 'must match wx followed by 16 hexadecimal characters'],
  ['WECHAT_APP_SECRET', value => value.length >= 16, 'must contain at least 16 characters'],
  ['TOKEN_SECRET', value => value.length >= 32, 'must contain at least 32 characters']
]

const errors = []

for (const [name, validate, message] of rules) {
  const value = String(process.env[name] || '').trim()
  if (!value) {
    errors.push(`${name}: missing`)
  } else if (!validate(value)) {
    errors.push(`${name}: ${message}`)
  }
}

const port = String(process.env.PORT || '80')
if (!/^\d+$/.test(port) || Number(port) < 1 || Number(port) > 65535) {
  errors.push('PORT: must be an integer from 1 to 65535')
}

const host = String(process.env.HOST || '0.0.0.0').trim()
if (!host) {
  errors.push('HOST: must not be empty')
}

if (errors.length > 0) {
  console.error('Environment validation failed:')
  for (const error of errors) console.error(`- ${error}`)
  process.exit(1)
}

console.log(`Environment validation passed (${rules.length + 2} checks).`)