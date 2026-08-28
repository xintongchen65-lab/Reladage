import { AppError } from '../errors.js'
import type { WechatAuthClient, WechatSession } from '../types.js'

interface WechatCodeResponse {
  openid?: string
  unionid?: string
  session_key?: string
  errcode?: number
  errmsg?: string
}

export function createWechatAuthClient(appId: string, appSecret: string, request: typeof fetch = fetch): WechatAuthClient {
  return {
    async exchangeCode(code: string): Promise<WechatSession> {
      const url = new URL('https://api.weixin.qq.com/sns/jscode2session')
      url.searchParams.set('appid', appId)
      url.searchParams.set('secret', appSecret)
      url.searchParams.set('js_code', code)
      url.searchParams.set('grant_type', 'authorization_code')
      const response = await request(url)
      if (!response.ok) throw new AppError(502, 'WECHAT_UNAVAILABLE', '微信登录服务暂时不可用')
      const payload = await response.json() as WechatCodeResponse
      if (!payload.openid || payload.errcode) throw new AppError(401, 'WECHAT_LOGIN_FAILED', payload.errmsg || '微信登录凭证无效')
      return {
        openId: payload.openid,
        ...(payload.unionid ? { unionId: payload.unionid } : {}),
        ...(payload.session_key ? { sessionKey: payload.session_key } : {})
      }
    }
  }
}
