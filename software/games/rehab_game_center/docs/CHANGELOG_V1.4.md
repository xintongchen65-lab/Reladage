# RehabMotion 康复游戏 V1.4 更新说明

## 地鼠大作战

- 重做游戏主场景，移除原先的纯 CSS 低清地鼠/山丘/固定锤子方案，统一为与摘水果、点球大战、活力公园一致的明亮 2D 卡通素材风格。
- 场上固定 5 个洞口，每个洞口均有独立地鼠形象；单人默认中央洞口为本地玩家，其余洞口为 NPC 视觉角色。
- 多人房间继续支持 2–5 人、每人独立 RehabMotion 设备和 6 位房间码。玩家按加入时间固定映射到 1–5 号洞，不会因为实时排名变化而换洞。
- 锤子在五个洞口之间移动；只有锁定本地洞口时才提示用户下蹲。其他洞口为自动视觉回合。
- `motion_progress` 实时驱动本地地鼠钻入/探出；达到下蹲位置先产生“躲入洞口”的视觉反馈，`both_rep_done` 仍作为完整有效箱式深蹲的最终确认和游戏得分依据。
- `motion_stage` 使用 `STANDING / DESCENDING / BOTTOM / RISING`；主控继续输出 `symmetry_percent / quality / warning` 负责左右动作质量判断。
- 新增锤子移动、目标警示光圈、命中特效、五洞地鼠皮肤和更完整的 PK/合作玩家标识。
- 游戏页补齐“暂停 + 退出”，退出二次确认并保留本次训练数据。

## 活力公园

- HUD 新增“退出”按钮，与摘水果、点球大战保持一致。
- 暂停、REST、数据中断遮罩内均提供退出入口；系统返回键改为二次确认退出。

## QA

本包已执行：

- 分包边界与运行素材体积检查
- 全游戏素材存在性检查
- 活力公园微信横屏静态规则检查
- 地鼠纯 TypeScript 核心编译与五洞锤子状态机 smoke test
- pages-mole-game / pages-vitality-park 全部 Vue `<script lang="ts">` 语法解析

完整 `pnpm type-check / test:run / build:h5 / build:mp-weixin / build:mp-weixin:test` 仍建议在项目 Windows 开发机重新安装依赖后执行。
