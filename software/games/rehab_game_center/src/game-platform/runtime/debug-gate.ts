export function isDebugBuild(
  nodeEnv = process.env.NODE_ENV,
  weixinTestBuild = typeof __RFG_WEIXIN_TEST_BUILD__ !== 'undefined' && __RFG_WEIXIN_TEST_BUILD__
): boolean {
  return nodeEnv !== 'production' || weixinTestBuild
}

export function resolveDebugEnabled(requested: string | undefined, debugBuild = isDebugBuild()): boolean {
  return requested === '1' && debugBuild
}
