> V1.4：地鼠大作战已升级为五洞五地鼠 + 移动锤子玩法，并为活力公园/地鼠大作战补齐暂停与退出。详见 `docs/CHANGELOG_V1.4.md`。

# RehabMotion 康复游戏中心

这是一个可独立运行、也可迁移为微信小程序普通分包的 uni-app Vue 3 + TypeScript 工程。主包 `/pages/game-center/index` 只负责游戏选择；摘水果位于 `src/pages-fruit-game/`，点球大战位于 `src/pages-penalty-game/`，通用协议、记录、导航与随机工具位于 `src/game-platform/`。

## 运行

```bash
pnpm install
pnpm dev:h5
pnpm dev:mp-weixin
```

多人开发联调另开终端：

```bash
pnpm server:mock
pnpm server:smoke
```

Mock 服务监听 `ws://127.0.0.1:8787`，只在内存保存房间，重启即清空，不是生产后台。

## v3 动作协议

正式 `MotionFrame` 包含原动作字段以及 `body_mode`、`train_mode`、`set_index`、`target_sets`、`rest_remaining_sec`、`target_angle_deg`、`valid_angle_deg`、`return_angle_deg`、`overall_completion_percent`、`quality` 和 `warning`。训练状态支持：

```text
IDLE | RUNNING | PAUSED | REST | FINISHED | STOPPED
```

- `target_count`、左右次数与 `completion_percent` 是当前组数据。
- `overall_completion_percent` 是整次训练数据，主 HUD 进度条使用该字段。
- 正式角度阈值、组数和次数服从主控帧；页面不会覆盖 RealDataSource 参数。
- REST 不允许携带 `rep_event`；合法换组为 `RUNNING → REST → RUNNING`，下一组 `set_index + 1`，组内次数、ROM 和当前组完成度可归零，整体完成度不得倒退。
- 游戏只消费主控已判定的单帧 `rep_event`，不替代真实康复算法。

FakeDataSource 默认 25 Hz、3 组、每组左右各 10 次，目标角 80°、有效角 60°、返回角 20°、休息 30 秒。完整模拟周期是：

```text
低角度伸肘准备 → 屈肘达到有效角度 → 重新伸肘回到返回角以下 → 单帧 rep_event
```

| 输入 | 动作 |
| --- | --- |
| W / ↑ | 左 / 右屈肘，角度增大，手臂抬高 |
| S / ↓ | 左 / 右伸肘，角度减小，手臂放低 |
| P | 暂停 / 继续 |

摘水果的 Q/E/B 没有强制事件入口。点球大战在开发构建或专用微信测试构建且 URL 显式带 `debug=1` 时，Q/E 会自动播放一遍完整屈膝—伸膝周期，仍必须经过周期检测器，不能直接注入 `rep_event`。正式微信构建即使收到 `debug=1` 也不会开启调试盘。

## 游戏中心与点球大战

- 游戏中心：`/pages/game-center/index`
- 摘水果首页：`/pages-fruit-game/home/index`
- 点球首页：`/pages-penalty-game/home/index`
- 点球训练链路：`home → prepare → calibrate → game → result`

点球对应膝关节屈伸。0°为伸膝零位，W/↑增加左/右膝角度，S/↓减小角度；达到有效角后回到返回角以下才产生一次动作。`active_side`存在时服从主控，缺失时按有效动作从左侧开始交替推导。射门方向左/中/右等概率；结果为70%进球、20%被扑、10%射偏。连续进球第N球得分为 `100 + (N-1)×20`，失败或错侧会清空连击。

点球 REST 使用游戏页内遮罩。事件优先触发射门；若事件丢失而当前有效侧次数恰好增加1，则只补射一次，次数跳增不会批量补射。

## 单人与多人

单人入口：

```ts
uni.navigateTo({
  url: '/pages-fruit-game/prepare/index?returnUrl=' + encodeURIComponent('/pages/training/index'),
  events: {
    fruitGameResult(result) {
      saveTrainingResult(result)
    }
  }
})
```

`returnUrl`只能是小程序内部页面路径。结果页优先返回原页面栈；H5刷新、直接打开结果页或页面栈丢失时，会 `reLaunch` 到该地址。未传时默认回 `/pages/game-center/index`，“再练一次”会继续保留同一个返回地址。

多人入口通过专属 EventChannel 传身份、短期 token、WebSocket 地址和当前训练组数/次数：

```ts
uni.navigateTo({
  url: '/pages-fruit-game/multiplayer/lobby/index',
  events: {
    fruitGameMultiplayerResult(result) {
      saveMultiplayerResult(result)
    },
    fruitGameResult(result) {
      // 多人断线超过 15 秒后，用户选择“继续单人”时走这里。
      saveTrainingResult(result)
    }
  },
  success(navigation) {
    navigation.eventChannel.emit('fruitGameMultiplayerBootstrap', {
      identity: { playerId: 'user-123', displayName: '小明', avatarUrl: '' },
      authToken: 'short-lived-token',
      wsEndpoint: 'wss://example.com/rehabmotion',
      trainingConfig: { targetSets: 3, targetCount: 10 }
    })
  }
})
```

多人支持 2～5 人私有房间、6 位房间码、PK 与合作。少于 2 人、有人未准备、第 6 人、参数不一致或开局后加入都会被拒绝。服务端发送统一 `starts_at_ms`，客户端用心跳估算服务端时差。断线 15 秒内重连并补发未确认事件；超时后明确让用户选择继续单人或结束。

多人大厅常驻显示“正在连接 / 已连接 / 连接失败”。首次连接失败按 500 ms、1 s、2 s 自动重试 3 次，仍失败后可点击“重新连接”；只有已连接时才能创建或加入房间，房间命令不会在离线状态下静默排队。训练中的未确认进度事件仍保留 15 秒补发机制。

单人准备页、多人大厅和等待房间页使用分包自带返回按钮。等待房间返回大厅时会先离开当前房间但保留大厅连接；离开整个多人板块时彻底关闭 Socket。只有从有效房间正式进入训练后才启用多人游戏状态，因此退出合作房间后进入单人训练不会残留共享篮子、房间提示或多人进度上报。

WebSocket 只同步动作事件和低频成绩，不上传角度、ROM 或原始 IMU。服务端按 `player_id + event_id`、`client_seq`、`motion_seq` 去重并广播递增 `room_seq` 的完整快照。PK 排名依次按完成、整体进度、得分、有效时长和加入顺序；合作目标在开局时固定，退出玩家不会降低目标。

## 游戏与结果

- 白天/夕阳背景取手机本地时间；彩旗场景仅用于水果采集册。
- 主水果按 82% 普通、12% 金苹果、6% 彩虹果随机；普通/金/彩分别 100/200/300 分。
- 同一 PK 房间使用相同种子；每次主目标成功都消费一次基础随机数，连击强制金苹果不会使玩家间基础序列错位。
- REST、暂停、后台、断流和采集册期间停止生成、计分、奖励计时和有效训练计时，恢复后必须重新回到低角度返回区。
- 康复次数与水果奖励分离。训练完成只服从主控 `FINISHED`，错侧动作可增加康复次数但不摘水果且会清空连击。
- 主目标水果成功后连击保留 8 秒有效游戏时间；超时自动清零但保留最大连击。REST、暂停、后台、断流和采集册期间连击计时冻结，双手西瓜不刷新连击时限。
- 结果同时保留最终组数据、跨组左右总次数、整次最大 ROM、整体完成度、总用时和有效训练时长。

摘水果继续使用 `rehabmotion-fruit-game:records:v1`，点球使用 `rehabmotion-penalty-game:records:v1`。旧摘水果记录不会迁移或删除；正式迁移后的持久化仍由主项目负责。

《活力公园》是单人坐到站训练，入口为 `/pages-vitality-park/home/index`，记录键为 `rehabmotion-vitality-park:records:v1`。它只接收主控已经判定的 `sit_to_stand_done`、次数和可选 `motion_progress`/`motion_stage`，不自行判断医学动作。Fake 模式使用 25 Hz，`W`/`↑` 起立、`S`/`↓` 下坐，必须完成“坐姿回位 → 达到有效幅度 → 返回坐姿”周期才计一次；`P/F/X/R` 仅在开发或微信测试构建且入口带 `debug=1` 时启用。每次有效动作依次激活小鸟、花朵、灯、蝴蝶、喷泉、风筝、小狗、彩旗、彩虹和庆祝状态，活力值每个事件增加 10，不按速度奖励。活力公园不包含多人模式、WebSocket 或多人结果事件。

### 活力公园视觉层 V1.1

当前版本已将人物与长椅彻底解耦：六姿态均为统一透明画布，长椅作为固定层贯穿训练；公园改为远景/中景/前景三层，并按小鸟→花坛→路灯→蝴蝶→喷泉→风筝→小狗→彩旗→彩虹→庆祝逐步激活。动作姿态按 `motion_progress → motion_stage → 角度` 降级读取，下降阶段会按站立姿态序列反向回放，不再把单一进度区间错误映射成 `sit-back`。详细说明见 `docs/RehabMotion-活力公园-V1-视觉升级说明.md`。


## 质量检查

```bash
pnpm type-check
pnpm test:run
pnpm check:boundary
pnpm check:assets
pnpm build:h5
pnpm build:mp-weixin
pnpm build:mp-weixin:test
pnpm check:weixin-build
```

正式稳定构建输出到 `dist/build/mp-weixin`，调试盘始终关闭。专用测试构建同样走生产编译流程，输出到 `dist/build/mp-weixin-test`；只有测试构建入口同时携带 `debug=1` 才显示调试盘。微信构建必须保留两个普通 `subPackages` 且不得设置 `independent`。两个游戏分包均必须低于 1.8 MiB；点球母版保存在 `design/penalty`，运行素材保存在 `pages-penalty-game/static`。

## 迁移边界

1. 按需复制 `src/pages-fruit-game/`、`src/pages-penalty-game/`、`src/pages-vitality-park/` 与 `src/game-platform/`，并合并 `pages.json` 普通分包配置。
2. 不复制 demo、临时 App 外壳、测试 AppID 或根目录 Mock 服务。
3. 用主项目传输服务实例化 `real-data-source.ts`，并只在 `source-factory.ts` 装配；核心规则不引用 Vue、Pinia、globalData 或主项目页面。
4. 生产多人服务可替换为云或自建 WebSocket，但保持 `types/multiplayer.ts` 协议。
5. 当前真实主控尚无手机到主控的校准/开始控制通道，因此本阶段不宣称多人倒计时能自动驱动真实设备同步开练。

详细设计见 `docs/RehabMotion-摘水果-V1-开发计划.md` 与 `docs/RehabMotion-游戏中心与点球大战-V1.md`。

## 地鼠大作战（箱式深蹲）

当前版本新增 `pages-mole-game`：支持单人训练、2–5 人 PK 与 2–5 人合作模式。多人采用 6 位房间码，每位玩家使用自己的 RehabMotion 设备；箱式深蹲按双侧同步动作处理，推荐主控输出 `motion_progress`、`motion_stage`、`symmetry_percent`、`quality`、`warning`，有效动作使用单帧 `both_rep_done`。

详细接口与玩法见 `docs/MOLE_GAME_V1.md`。
