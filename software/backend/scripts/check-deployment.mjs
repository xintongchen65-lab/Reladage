#!/usr/bin/env node

const HELP = `RehabMotion deployment smoke test

Usage:
  node scripts/check-deployment.mjs https://your-service.example.com

Alternatively set REHABMOTION_API_URL. The test checks the health endpoint and
verifies that a protected endpoint rejects anonymous access.`

if (process.argv.includes('--help') || process.argv.includes('-h')) {
  console.log(HELP)
  process.exit(0)
}

const input = process.argv[2] || process.env.REHABMOTION_API_URL

if (!input) {
  console.error('Missing service URL. Pass it as the first argument or set REHABMOTION_API_URL.')
  process.exit(1)
}

let baseUrl
try {
  const parsed = new URL(input)
  if (!['http:', 'https:'].includes(parsed.protocol)) throw new Error('unsupported protocol')
  parsed.pathname = parsed.pathname.replace(/\/$/, '')
  parsed.search = ''
  parsed.hash = ''
  baseUrl = parsed.toString().replace(/\/$/, '')
} catch {
  console.error('Invalid service URL. Use an absolute http:// or https:// URL.')
  process.exit(1)
}

async function request(path) {
  const controller = new AbortController()
  const timeout = setTimeout(() => controller.abort(), 10_000)
  try {
    return await fetch(`${baseUrl}${path}`, {
      headers: { accept: 'application/json' },
      signal: controller.signal
    })
  } finally {
    clearTimeout(timeout)
  }
}

try {
  const health = await request('/health')
  if (!health.ok) throw new Error(`/health returned HTTP ${health.status}`)

  const healthBody = await health.json().catch(() => null)
  if (!healthBody || healthBody.ok !== true) {
    throw new Error('/health did not return the expected JSON status')
  }
  console.log(`PASS /health (${health.status})`)

  const protectedResponse = await request('/app/bootstrap')
  if (protectedResponse.status !== 401) {
    throw new Error(`/app/bootstrap should return 401 without a token, received ${protectedResponse.status}`)
  }
  console.log('PASS /app/bootstrap rejects anonymous access (401)')
  console.log('Deployment smoke test passed.')
} catch (error) {
  const message = error instanceof Error ? error.message : String(error)
  console.error(`Deployment smoke test failed: ${message}`)
  process.exit(1)
}