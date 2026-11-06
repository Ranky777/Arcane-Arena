# Arcane Arena

Arcane Arena 是一个使用 Unreal Engine 5.8 制作的 3v3 英雄竞技场 Demo。项目主要练习网络联机、Gameplay Ability System（GAS）和战斗系统的搭建，包含大厅、选阵营、比赛流程、AI、技能战斗和观战。

项目的美术资源主要来自 UE 模板和原型网格，重点放在玩法、网络同步和代码结构上，没有专门制作角色和场景美术。

## 功能

- 3v3 局域网对战，支持 Listen Server 和 AI 补位。
- 创建、搜索和加入房间；选择阵营，调整每队人数和胜利局数。
- 完整比赛流程：热身、回合开始、团灭判定、比分、结算、再来一局和返回房间。
- 近战连段、蓄力攻击、冲刺、远程锁定弹和护盾。
- 伤害、击退、震屏、受击反馈、死亡和观战。
- 玩家和 AI 共用角色、属性和技能流程；AI 使用 State Tree 进行战斗决策。

## 技术点

- Unreal Engine 5.8，C++ 和 Blueprint 混合开发。
- GAS：Ability、GameplayEffect、AttributeSet、GameplayCue，以及冷却和能量消耗。
- 数据驱动：技能数值、冷却、伤害、击退和冲击表现通过 CSV 导入 DataTable/CurveTable 配置。
- 网络：Listen Server 和 OnlineSubsystemNull；服务器负责伤害、死亡和比赛状态；移动和冲刺使用客户端预测；近战命中在服务器做短时间回退，减少高延迟下的命中偏差。
- UI：UMG 实现 HUD、大厅、设置、暂停、结算和头顶血条。
- AI：State Tree 和 AI Controller，支持近战和远程技能。

## 操作

- WASD：移动
- 鼠标：瞄准和视角
- 鼠标左键：普通攻击
- 鼠标右键：蓄力攻击
- Space：冲刺
- 技能栏对应按键：远程技能和护盾（以 HUD 显示为准）
- Esc：暂停
- 死亡后：E、Q 或滚轮切换观战目标

## 运行

1. 使用 Unreal Engine 5.8 打开 `Arcane_Arena.uproject`。
2. 编译 C++ 模块。
3. 在编辑器中选择 Play As Listen Server 运行。
4. 一个窗口作为主机创建或加入局域网会话，另一个窗口作为客户端加入；进入大厅后选择阵营并开始比赛。

局域网联机使用 OnlineSubsystemNull，仅用于本机或同一局域网测试。项目没有实现 Dedicated Server、账号系统、匹配和断线重连。

## 目录

- `Source/Arcane_Arena/`：C++ 源码
  - `AI`：AI 控制器和 State Tree 任务
  - `Character`：角色、相机和移动组件
  - `Combat`：近战、弹体、伤害和战斗数据
  - `Framework`：GameMode、GameState 和比赛流程
  - `GAS`：ASC、AttributeSet、Ability 和伤害计算
  - `Net`：网络同步和延迟补偿
  - `Online`：局域网会话
  - `Player`：PlayerController、PlayerState 和观战
  - `UI`：UI 基类和 HUD
- `Content/Arcane/`：项目蓝图、数据表、关卡和 UI
- `Content/Variant_Combat/`：UE 模板的战斗角色和动画资源
- `Config/`：项目配置

## 已知限制

- 仅支持 Listen Server 和局域网，没有 Dedicated Server、匹配和断线重连。
- 美术使用 UE 模板和原型资源。
- 近战延迟补偿只回退角色胶囊，不回退骨骼和武器；回退窗口有上限。
- 这是一个个人 Demo，不是完整商业项目。
