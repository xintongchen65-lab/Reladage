/// <reference types="vite/client" />

declare const process: {
  env: {
    NODE_ENV?: string
  }
}

declare const __RFG_WEIXIN_TEST_BUILD__: boolean

declare module '*.vue' {
  import { DefineComponent } from 'vue'
  // eslint-disable-next-line @typescript-eslint/no-explicit-any, @typescript-eslint/ban-types
  const component: DefineComponent<{}, {}, any>
  export default component
}
