import 'fastify'
import type { TokenClaims } from './types.js'

declare module 'fastify' {
  interface FastifyRequest {
    auth: TokenClaims | null
  }
}
