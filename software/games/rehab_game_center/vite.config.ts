import { defineConfig } from "vite";
import uni from "@dcloudio/vite-plugin-uni";

// https://vitejs.dev/config/
export default defineConfig({
  plugins: [uni()],
  define: {
    __RFG_WEIXIN_TEST_BUILD__: JSON.stringify(process.env.RFG_WEIXIN_TEST_BUILD === '1')
  }
});
