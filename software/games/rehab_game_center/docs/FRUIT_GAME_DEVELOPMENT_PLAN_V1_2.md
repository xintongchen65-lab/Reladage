# RehabMotion「摘水果」v3 协议与多人模式实施说明

## 1. 技术结论与工程边界

HBuilderX + uni-app + Vue 3 + TypeScript + 微信开发者工具适合当前交付。工程以 `pages-fruit-game` 作为可迁移普通分包，不设置 `independent: true`；H5/App 通过条件编译注册同路径普通页面。核心规则是纯 TypeScript，不依赖 Vue、Pinia、Vuex、`App.vue`、`getApp().globalData` 或主项目页面。

- 临时主包：demo 菜单、训练记录、测试 AppID。
- 可迁移分包：单人页面、多人页面、组件、核心规则、DataSource、runtime、协议类型和运行素材。
- 开发服务：根目录 `server/mock-room-server.ts`，仅供本地 2～5 人联调，不随分包迁移、不接数据库。
- 正式接入：RealDataSource 替换点仍只有 `data-sources/source-factory.ts`；生产 WebSocket 服务保持 `types/multiplayer.ts` 协议即可替换。

## 2. RehabMotion v3 数据协议

正式帧以主控给出的处理后 JSON 为准。`TrainingState` 为 `IDLE | RUNNING | PAUSED | REST | FINISHED | STOPPED`。除原角度、ROM、次数、速度、`seq`、`timestamp_ms`、`rep_event` 外，帧必须包含：

```text
body_mode, train_mode, set_index, target_sets, rest_remaining_sec,
target_angle_deg, valid_angle_deg, return_angle_deg,
overall_completion_percent, quality, warning
```

字段职责：

- `left_count`、`right_count`、`target_count`、`completion_percent` 是当前组事实。
- `set_index` 从 1 开始，`overall_completion_percent` 是整次训练进度。
- 正式组数、次数与角度参数均来自主控帧；Fake 的 URL 参数仅服务临时调试。
- 主 HUD 使用整体完成度，同时显示第 X/Y 组及左右当前组次数。
- RealDataSource 提供已经判定好的 `rep_event`；游戏不做正式医学动作识别。

MotionFrameAdapter 严格执行结构、顺序与语义校验：

- `seq <= lastSeq`、时间戳倒退、同组次数/完成度倒退、整体进度倒退、参数无故变化、非法状态转换和终态后数据直接拒绝。
- 合法换组只能是 `RUNNING → REST → RUNNING`，下一组必须 `set_index + 1`；此时组内次数、ROM 和当前组完成度可归零。
- REST 帧必须 `rep_event = none`。
- 跳号、较大时间间隔、次数跳增与到达延迟漂移记录诊断，不重复消费事件。

## 3. FakeDataSource 与视觉映射

Fake 默认：25 Hz、0° 初始伸肘、目标 80°、有效 60°、返回 20°、3 组、每组左右各 10 次、组间休息 30 秒。

每侧独立周期：

```text
低角度返回区准备
→ 屈肘并增大角度
→ 达到 valid_angle_deg
→ 重新伸肘并回到 return_angle_deg 以下
→ 发出一次 rep_event 并锁定本周期
```

W/↑ 增大左/右角度，S/↓ 减小左/右角度。持续高位、未达到有效角、未返回低角或低角抖动都不能重复计次。左右同帧返回可输出 `both_rep_done`。Q/E/B 没有事件注入入口。

动画采用 0° 低位、目标角附近高位。真实 5 Hz 帧在页面运行层用最近两帧做不超过 250 ms 的线性视觉插值，UI 定时上限 25 FPS；插值不进入 Adapter、GameEngine 或次数统计。

## 4. REST、运行保护与跨组结果

REST 期间暂停水果生成、得分、连击、双手奖励倒计时和有效训练计时，显示“第 X/Y 组完成，休息 N 秒”。REST 结束后左右侧必须分别回到 `return_angle_deg` 以下，才重新允许游戏奖励。

主水果摘取成功后启动 8 秒有效游戏时间的连击窗口。窗口内再次成功摘取会续满 8 秒；超时只清空当前连击，保留最大连击和全部训练事实。REST、用户暂停、后台、断流、数据源暂停及采集册期间冻结该计时，双手西瓜奖励不续时。

SessionGuard 还处理用户暂停、应用后台、数据源暂停、1 秒断流、重新回位和终止。保护状态期间继续接收并以 Adapter 去重，但不计分。恢复后先更新视觉，再完成回位门控。

TrainingAccumulator 跨组保存：左右总有效次数、左右整次最大 ROM、最终组号和整体完成度。`TrainingResult.training` 同时保留最终组原字段及：

```text
left_total_count, right_total_count,
session_left_rom_deg, session_right_rom_deg, session_lr_rom_diff_deg,
set_index, target_sets, overall_completion_percent
```

`elapsedMs` 是总用时，`activeElapsedMs` 排除暂停、REST、后台和断流。旧 v1 历史记录读取时按单组数据补齐，不更换存储键。

## 5. 单人游戏规则

左右主目标交替，水果种类独立随机：8 种普通水果合计 82%、金苹果 12%、彩虹果 6%，对应 100/200/300 分。每 5 连击的下一颗强制为金苹果。整体进度首次跨过 35% 和 70% 时出现 6 秒双手西瓜奖励，合法 `both_rep_done` 加 500 分但不改变主目标。

错侧有效动作保留主控康复次数，但不摘水果、连击归零并增加错侧数。训练完成只服从 `training_state = FINISHED`，不以水果数或游戏分数推断。采集册、白天/夕阳主题和所有暂停保护继续沿用现有实现。

## 6. 多人架构与协议

多人采用旁路同步，不上传高频角度、ROM 或原始 IMU：

```text
本地 DataSource → Adapter → GameEngine → 本地即时画面
                         ↘ MultiplayerClient → WebSocket → RoomServer
```

客户端只发送：`event_id`、递增 `client_seq`、`motion_seq`、`rep_event`、训练状态、当前组、组内及累计次数、整体完成度、有效时长、得分和水果数。服务端按玩家 + 事件 ID、客户端序号和动作序号去重，并广播递增 `room_seq` 的完整房间快照。

房间状态为 `WAITING → COUNTDOWN → RUNNING → FINISHED/CLOSED`。规则：

- 6 位数字码，2～5 人；少于 2 人、有人未准备、第 6 人、训练组数/次数不一致或开局后加入均拒绝。
- 开始时锁定名单、PK/合作模式、组数、次数、随机种子及合作总目标。
- 每个玩家服从自己的 RUNNING/REST/PAUSED/FINISHED，不强制全房同步休息。
- 服务端下发 `starts_at_ms`；客户端根据 connected/pong 时间估算服务端时差。
- 心跳 5 秒。短线期间本地继续训练并缓存未确认事件；15 秒内凭 resume token 重连补发，服务端仍只结算一次。
- 超时玩家标记 LEFT；房主超时后转移给最早加入的在线玩家。合作目标不因退出下降。
- 超过恢复窗口时，训练页明确让用户选择保留进度继续单人或结束，不静默切换。

服务端不能证明动作真实有效，只信任各自主控已判定的康复事实；其职责是房间一致性、去重、排名和结算。

## 7. PK、合作与共享随机种子

PK 排名顺序：已完成优先、整体完成度降序、得分降序、有效训练时长升序、加入顺序稳定排序。

同一 PK 房间使用服务端随机种子。每次主目标成功都固定消费一次基础随机数，即使该水果被五连击金苹果覆盖也照常消费，因此不同玩家第 N 颗基础水果保持一致；错侧、特殊奖励和完成速度仍可造成成绩差异。

多人大厅显示连接状态。初次连接失败按 500 ms、1 s、2 s 自动重试 3 次，随后允许用户手动重试；创建、加入、准备和开始等房间命令仅在 Socket 已连接时发送，不做无提示离线排队。训练进度事件继续保留 15 秒短线缓存与去重补发。

准备页、多人大厅和等待房间页提供适配微信安全区的返回按钮。房间返回大厅时先发送 `leave_room`、清空房间快照并复用大厅 Socket；大厅返回主菜单时关闭 Socket 和全部订阅。多人训练必须由房间页显式激活，单人游戏不得根据历史房间快照推断模式；失效的浏览器前进页面应提示“房间已退出”后安全返回。

合作贡献等于左右累计有效次数之和，`both_rep_done` 自然贡献 2。团队目标在开局时固定为所有开局成员 `target_sets × target_count × 2` 之和。共享面板展示团队动作进度、总得分和个人贡献；错侧动作增加康复贡献但不增加水果与游戏得分。

## 8. 页面与主项目对接

- 竖屏：多人大厅、创建/加入、房间等待与统一倒计时。
- 横屏：准备、训练、单人结果、多人结果。
- 游戏页复用同一 GameEngine/DataSource，只叠加 PK 排行或合作进度组件。
- 房间状态由纯 TypeScript RoomStore 订阅，不引入全局 store 或事件总线。

主项目使用专属 EventChannel 发送 `playerId`、昵称、头像、短期 token、WebSocket 地址和训练组数/次数；单人结果事件是 `fruitGameResult`，多人权威结算是 `fruitGameMultiplayerResult`。多人断线后转单人时回落到单人结果事件。完整调用示例见根目录 README。

## 9. 资源、分包与安全边界

运行素材全部在分包 `static`。三张篮子图由 PNG 等比压缩为透明 WebP 后，静态素材约 1.44 MiB；当前微信构建的整个 `pages-fruit-game` 分包约 1.55 MiB，低于 1.8 MiB 门槛。原始和生成母版仍在 `design/reference` 或 Git 历史中。

结果页返回优先使用原页面栈；页面栈缺失或返回失败时，使用经校验的内部 `returnUrl` 执行 `reLaunch`。临时工程默认返回 `/pages/demo/index`，迁移到主项目后由入口参数指定主项目菜单。“再练一次”和结果页刷新均保留该地址。

自动边界扫描禁止 AppID、云开发、数据库、主包页面、globalData、Pinia/Vuex、全局事件总线、强制 rep 接口、分包本地持久化、非 scoped 样式和污染性标签选择器。普通分包不设置 `independent`。

## 10. 验收与已知边界

自动测试覆盖 v3 全状态序列、低→高→低周期、换组归零、整体进度单调、REST 冻结、5 Hz 视觉方向、历史兼容、2/3/5 人容量、开局锁定、事件/序号去重、PK 排名、合作固定目标、15 秒重连与房主转移、共享随机流等。

发布前执行：

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

正式微信产物为 `dist/build/mp-weixin`，其中调试能力编译关闭；测试产物为 `dist/build/mp-weixin-test`，仍使用生产编译流程。测试产物也必须同时由入口传入 `debug=1` 才渲染调试盘。训练累计器合并在既有游戏核心模块中，避免微信启动时额外注册独立模块。

本阶段用 Fake 和本地 Mock 服务完成 2～5 人流程；RealDataSource 接口、REST 和 5 Hz 数据兼容已预留。由于现有 v3 主控尚无手机到主控的校准/开始控制通道，不能把服务端统一倒计时解释为真实设备已经自动同步启动，真机联调仍是下一阶段验收项。
