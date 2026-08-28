import { jwtVerify, SignJWT } from 'jose'
import { UnauthorizedError } from '../errors.js'
import type { TokenClaims, TokenService } from '../types.js'

const issuer = 'rehabmotion-server'
const audience = 'rehabmotion-miniapp'

export function createTokenService(secret: string): TokenService {
  const key = new TextEncoder().encode(secret)
  return {
    async sign(claims) {
      return new SignJWT({ openId: claims.openId })
        .setProtectedHeader({ alg: 'HS256' })
        .setSubject(claims.userId)
        .setIssuer(issuer)
        .setAudience(audience)
        .setIssuedAt()
        .setExpirationTime('7d')
        .sign(key)
    },
    async verify(token) {
      try {
        const { payload } = await jwtVerify(token, key, { issuer, audience })
        if (!payload.sub || typeof payload.openId !== 'string') throw new Error('令牌字段不完整')
        return { userId: payload.sub, openId: payload.openId } satisfies TokenClaims
      } catch {
        throw new UnauthorizedError()
      }
    }
  }
}
