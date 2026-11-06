# Arcane Arena

Arcane Arena 是一个基于 Unreal Engine 5.8 的多人英雄竞技场 Demo。项目主要围绕网络联机、Gameplay Ability System（GAS）和可扩展的战斗框架展开，目标是验证一套能支撑多人对战、AI 补位和后续英雄扩展的工程结构。

项目使用 UE 模板和原型资源作为美术占位，重点放在玩法系统、网络同步、数据驱动和 C++ 与 Blueprint 的职责划分上，没有专门制作角色和场景美术。

## 项目概览

- 两队多人对战，每队人数可在房间中配置，硬上限 7 人；实际人数受关卡出生点数量限制。
- 支持 Listen Server 局域网联机，房主创建房间，其他玩家搜索或加入。
- 支持 AI 补位、队伍平衡，以及房间人数和胜利局数的调整。
- 完整比赛流程：热身选阵营、回合开始、团灭判定、比分、回合结算、整场结算、再来一局和返回房间。
- 角色能力：近战连段、蓄力攻击、冲刺、远程锁定弹和护盾。
- 死亡后进入观战，支持在目标之间切换。
- 玩家和 AI 共用同一套角色、PlayerState、ASC 和技能流程。

## 技术架构

### 分层与依赖

项目按职责拆分，依赖方向保持单向：

Core → GAS → Combat → Character → Framework → Player / UI / AI / Online

- Core：阵营解析、能力输入槽等基础类型。
- GAS：ASC、AttributeSet、Ability 基类、伤害计算和 GameplayCue。
- Combat：统一伤害入口、近战和弹体、冲击配置、选敌任务。
- Character：玩家和 Bot 角色、相机、移动组件。
- Framework：GameMode、GameState、房间名单和比赛流程。
- Player / UI / AI / Online：控制器、观战、界面、AI 决策和会话。

Combat、AI 和 UI 通过接口读取角色与比赛数据，避免为了获取存活、阵营、ASC 或比赛命令而 Cast 具体类。GameMode 只保留引擎回调和对外入口，队伍和人数交给 RosterComponent，阶段、比分、回合和重生交给 MatchFlowComponent。

### 网络模型

- Listen Server 加 OnlineSubsystemNull，面向局域网和本机联调。
- 玩家和 Bot 的 ASC、AttributeSet 统一挂在 PlayerState；PlayerState 跨重生存活，避免角色销毁导致属性和技能状态丢失。
- 伤害、死亡、回合和比分等影响胜负的状态由服务器裁决；客户端只负责输入和表现。
- 移动和冲刺使用客户端预测；能力通过 GAS 的预测和复制结果同步。
- 一次性表现使用 GameplayCue 或 NetMulticast，不为纯表现开每帧复制。
- 远程技能的目标由客户端选出，但服务器会重新校验阵营、存活、距离和视线；客户端上报的目标只作为建议。
- 近战命中在服务器按客户端延迟做短时间回退，减少高延迟下“屏幕看着中了、服务器没中”的偏差。

### GAS 设计

- 所有角色能力继承统一的 Ability 基类，输入槽、AbilityID、冷却和耗能逻辑集中处理。
- 冷却和耗能使用共享的 GameplayEffect 容器，具体数值通过 SetByCaller 在运行时注入；每个技能只配置自己的冷却时长和冷却标签。
- 近战、冲刺、远程弹和护盾使用不同的执行策略：位移优先本地预测，伤害和状态由服务器确认。
- AttributeSet 负责 Health、Energy、Shield、DamageReduction 等属性；伤害通过统一的 Execution 处理减伤、护盾吸收和扣血。
- GameplayCue 负责命中、护盾等一次性表现，避免把特效逻辑写进伤害代码。

### 数据驱动战斗

技能数值不写死在 C++ 或蓝图里，统一放在 CSV 导入的 DataTable 和 CurveTable 中：

- AbilityTable：技能伤害、冷却、耗能、射程或半径、弹体类、冲击配置 ID。
- ImpactTable：击退方向、水平或竖直冲量、控制标签、受击表现和顿帧。
- FalloffCurve：距离衰减曲线。

调整数值和冲击配置只需要修改 CSV 并 Reimport，不需要重新编译。新增技能时在技能表增加一行，并给对应的 Ability 配置 AbilityID；战斗代码只负责执行规则，不负责保存数值。

### 战斗与表现分离

- IArcaneDamageable：统一伤害入口，角色和可破坏物都实现。
- IArcaneCombatant：存活、输入锁、击退和受击反应等战斗契约。
- IArcaneAbilityAvatar：技能只通过这个接口攻击，不依赖具体角色类。
- 近战通过一次 MultiSphereTrace 读取技能行和冲击行，再分别处理伤害、击退、Cue、受击和顿帧。
- 弹体由服务器生成并结算；远程锁定技的选敌结果由服务器重算。
- 血条、HUD、大厅和结算界面只读 PlayerState、GameState 和 ASC，不反向驱动 gameplay。

### AI 与比赛流程

- AI 使用 AI Controller 加 State Tree，任务和评估器负责选敌、接近、攻击和撤退。
- Bot 持有 PlayerState 和 ASC，和真人玩家走同一套属性、技能、死亡和回合流程。
- RosterComponent 负责分队、人数上限、Bot 增删和平衡。
- MatchFlowComponent 负责阶段、回合、比分、重生和输入锁定。
- GameState 复制阶段、比分、回合数、胜方和名单版本，UI 通过委托和 OnRep 刷新。

### 界面

- UMG 加 C++ 基类：ArcaneUserWidget 负责 GAS 属性桥接，ArcaneOwnerWidget 负责归属角色和敌我关系。
- HUD、选阵营、大厅、设置、暂停、比赛结算、观战提示和头顶血条独立拆分。
- 文本统一配置换行、省略和裁剪策略，适配不同的分辨率和文本长度。

## 工程取舍

- 服务器权威优先：伤害和胜负状态不在客户端结算；客户端只做移动和冲刺预测，保证手感和一致性。
- 数据驱动优先：技能和冲击参数放在数据表里，代码只保留规则，便于快速迭代和扩展英雄。
- 接口优先：Combat、AI 和 UI 通过接口取数据，减少跨层 Cast 和模块耦合。
- 范围收敛：当前只做 Listen Server 加局域网，不引入 Dedicated Server、账号、匹配和断线重连，这些属于后续工程化范围。

## 目录

- `Source/Arcane_Arena/`：C++ 源码
  - `AI`：AI 控制器和 State Tree 任务
  - `Character`：角色、相机和移动组件
  - `Combat`：近战、弹体、伤害和战斗数据
  - `Framework`：GameMode、GameState、房间名单和比赛流程
  - `GAS`：ASC、AttributeSet、Ability 和伤害计算
  - `Net`：网络同步和延迟补偿
  - `Online`：局域网会话
  - `Player`：PlayerController、PlayerState 和观战
  - `UI`：UI 基类、HUD 和各类面板
- `Content/Arcane/`：项目蓝图、数据表、关卡和 UI
- `Content/Variant_Combat/`：UE 模板的战斗角色和动画资源
- `Config/`：项目配置

## 已知限制

- 仅支持 Listen Server 和局域网，没有 Dedicated Server、匹配、账号和断线重连。
- 美术资源来自 UE 模板和原型网格，没有专门的场景和角色美术。
- 近战延迟补偿只回退角色胶囊，不回退骨骼和武器；回退窗口有上限。
- 英雄数量和地图数量保持在 Demo 规模。
