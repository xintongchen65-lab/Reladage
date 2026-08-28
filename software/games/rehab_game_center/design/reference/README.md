# 设计母版说明

- `fruit-game-visual-authority.png`：用户确认的唯一视觉规范原稿。
- `character-full-master.png`：用户提供的图 2 原图；中央静态人物直接由此图去除洋红背景、等比裁切生成，不进行 AI 重绘。
- `orchard-master.png`、`character-master.png`、`arms-master.png`、`fruits-master.png`、`baskets-master.png`：其他运行素材裁切前的高分辨率母版。
- 本目录不随最终普通分包迁移；运行时优化素材只存放在 `src/pages-fruit-game/static/`。

所有运行图片使用 `aspectFit`、`heightFix` 或铺满裁切模式，不使用非等比拉伸。

## 素材来源与处理摘要

除用户提供的中央人物外，既有游戏素材以 `fruit-game-visual-authority.png` 为风格参考：

1. 16:9 明亮白天卡通果园，苹果树位于两侧，中间留出人物和篮子区域，不含人物、文字与 UI。
2. 中央人物采用用户提供的完整正面人物，保留自然下垂的双臂和手部；两侧训练手臂继续使用独立三档素材并置于主体后层。
3. 八种水果 4×2 图集：苹果、橙子、香蕉、葡萄、桃子、梨、草莓、西瓜。
4. 篮子三状态图集：空、半满、满。
5. 手臂六姿态图集：左右侧分别包含低、中、高三档肘关节屈伸姿态。

运行 PNG 通过确定性色键去底、边缘收缩和等比缩放处理，再按实际显示尺寸导出；果园背景压缩为 WebP。中央人物不经过生成模型；高分辨率母版保留在本目录。
