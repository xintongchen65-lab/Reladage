# RehabMotion 活力公园 V1.1 — 交付 QA

## 已通过

- 活力公园分包边界/体积检查：`node scripts/check-boundary.mjs`
- 三款游戏素材完整性检查：`node scripts/check-assets.mjs`
- 本次修改的 TypeScript / Vue script 语法转译检查
- 坐到站方向感知姿态映射行为检查：起立顺序与下坐反向顺序
- 六张人物 PNG 透明边缘检查：未发现白边或洋红残留
- 667×375、844×390、932×430 三种横屏静态布局检查：人物、独立长椅、HUD 均在安全区域内

## 当前执行环境无法完成的项目

上传 ZIP 中携带的是 Windows `node_modules`。当前交付运行环境为 Linux，Rollup 所需的 Linux 原生可选包 `@rollup/rollup-linux-x64-gnu` 不在上传依赖中，因此 Vitest、H5 和微信构建会在加载 Rollup 时停止；这属于上传依赖的平台不匹配，不是本次活力公园源码的运行时逻辑报错。

最终 ZIP **不包含 `node_modules`**。在 Windows 开发机/HBuilderX 工程目录执行一次：

```bash
pnpm install
pnpm type-check
pnpm test:run
pnpm check:boundary
pnpm check:assets
pnpm build:h5
pnpm build:mp-weixin
pnpm build:mp-weixin:test
```

即可用目标平台依赖重新完成完整构建验收。

## 本次关键修改

1. 六姿态人物全部改为高分辨率、透明、无椅子素材，统一画布和脚底锚点。
2. 长椅改为独立固定层，人物起立后不消失、不跟随人物漂移。
3. 公园改为远景 / 中景 / 前景三层，事件作为真实场景元素叠加。
4. `motion_progress → motion_stage → 角度` 三级姿态数据降级；`rep_event` 只确认有效动作完成。
5. 修复下坐方向：站立 → 半站立 → 离椅 → 前倾 → 回坐，不再按起立方向错误切图。
6. HUD 精简，动作提示独立放底部；REST 保留椅子和已激活公园，只覆盖半透明休息层。
7. 10 个激活事件保持：小鸟、花坛、路灯、蝴蝶、喷泉、风筝、小狗、彩旗、彩虹、庆祝。

## V1.2 微信横屏布局补充验收

- 三层背景必须全部使用 `aspectFill`；
- 844×390 微信横屏中人物/长椅比例应与 H5 基本一致；
- 微信右上角胶囊不得覆盖“暂停”按钮；
- 人物/长椅主体尺寸不得依赖 `vw + max-width(px)` 混合约束；
- 花坛、风筝、喷泉不应出现矩形背景残留；
- 重新测试前清理旧 `dist/`、`unpackage/` 和微信开发者工具缓存。
