# 地鼠大作战（箱式深蹲）使用版

- 单人、2–5 人 PK、2–5 人合作；每位玩家使用自己的 RehabMotion 设备，通过 6 位房间码进入。
- 双侧同步参与一次完整箱式深蹲，不切左右腿。
- motion_progress 0~1 驱动地鼠探出/下沉；motion_stage 推荐 STANDING / DESCENDING / BOTTOM / RISING。
- both_rep_done 是单帧有效动作确认；游戏不自行判断医学动作。
- 主控建议新增 symmetry_percent、quality、warning；左右不一致由主控判定。
- 主控参数仍作为权威：target_count、target_sets、target_angle_deg、valid_angle_deg、return_angle_deg。
- PK 排游戏分数与闪避；合作按全员成功闪避数达到 teamTarget 结算。

## V1.4 交互与视觉更新（五洞地鼠）

- 场上固定 5 个洞口，每个洞口始终有独立地鼠角色；多人房间按玩家加入顺序映射到 1–5 号洞，未占用洞口作为 NPC 视觉角色。
- 单人训练默认本地玩家为中央 3 号洞；锤子在五个洞口之间移动。只有锤子锁定本地洞口时，完成一次有效箱式深蹲才计为游戏“闪避”；在其他洞口时完成的康复动作仍由主控正常记录，但不额外获得本轮游戏闪避分。
- 地鼠实时下沉优先使用 `motion_progress`（0=站立/完全探出，1=下蹲目标/完全躲入），状态提示使用 `motion_stage`：`STANDING | DESCENDING | BOTTOM | RISING`。
- 主控用 `rep_event = both_rep_done` 确认左右两侧共同完成一次有效箱式深蹲；游戏不自行替代医学动作判断。
- 左右质量由主控计算，游戏读取 `symmetry_percent`、`quality`、`warning`。建议 `warning` 至少支持 `none | left_right_asymmetry | resting`。
- 多人统一 2–5 人、每人独立 RehabMotion 设备、6 位房间码加入；PK/合作均沿用现有多人房间协议。
- 游戏页与活力公园均同时提供“暂停”和“退出”，退出前二次确认并保留本次训练数据。
