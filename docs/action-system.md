# 动作系统 0.2

Codex v1 有 9 组动作、57 个动画帧；v2 另有 16 个静态注视姿势，共 73 个有效格。素材描述动作外观，桌宠宿主负责决定何时播放、持续多久、如何切换以及是否移动窗口。

## 参考实现

这次检查的源代码版本：

- [OpenPets c8dfeb6 的动画定义](https://github.com/OpenPetsHQ/openpets/blob/c8dfeb661ee60e9c0f270316dd4e0f9c9158eb3d/packages/pet-format/src/animation.ts)：分别描述反应与位置移动，明确有限次数和循环动作；待机周期为 5.5 秒。它的工作状态可以由 Agent 事件驱动，PetDesk 将相同素材语义用于本地小活动。
- [OpenPets 的注视调度](https://github.com/OpenPetsHQ/openpets/blob/c8dfeb661ee60e9c0f270316dd4e0f9c9158eb3d/apps/desktop/src/pet-window-gaze.ts)与[方向计算](https://github.com/OpenPetsHQ/openpets/blob/c8dfeb661ee60e9c0f270316dd4e0f9c9158eb3d/apps/desktop/src/codex-pets-core.ts)：仅符合条件的待机宠物参与，100 ms 检查，鼠标停止后 1.2 秒恢复。
- [OpenPets 的活动策略](https://github.com/OpenPetsHQ/openpets/blob/c8dfeb661ee60e9c0f270316dd4e0f9c9158eb3d/apps/desktop/src/pet-activity-policy.ts)：延后空闲活动、单个活动冷却、避免紧邻重复。其扩展活动使用额外活动素材；PetDesk 的小活动只组合标准 Codex 帧，不导入它的扩展活动格式。
- [WindowPet 0.0.9 的 Pets 场景](https://github.com/SeakMengs/WindowPet/blob/aba8d3ab94d30f4f5abf92b3f3ec7dff50d3326b/src/scenes/Pets.ts)与[动作注册](https://github.com/SeakMengs/WindowPet/blob/aba8d3ab94d30f4f5abf92b3f3ec7dff50d3326b/src/scenes/manager.ts)：按每只宠物 availableStates 播放，随机活动、冷却、方向移动和拖放反馈分别处理。爬墙/悬挂/掉落需要对应素材，并非导入任意 Codex 图集就自动拥有。

借鉴的是调度原则。PetDesk 使用自己实现的 C++/Qt 时间线，没有引入 Electron、Tauri、Phaser、上述项目的插件运行时或 Agent 接口。

## 播放与优先级

`Playback` 是无计时器的确定性状态机，`PetWindow` 驱动计时并绘制。每一步保存动作、有限循环次数、可选水平位移。每帧只访问该行动作的有效格，最后一帧播放完再切换下一步，活动结束回到原始待机。

优先级：拖动 > 点击/双击/手动活动 > 自动活动 > 待机/注视。高优先级立即打断，旧活动不排队、不恢复。移动被打断时停在当前位置。松开拖动先释放独占优先级，再播放跳跃反馈。

## 小活动

| 活动 | 动作顺序 |
|---|---|
| 打招呼 | 挥手 ×2 |
| 蹦一蹦 | 跳跃 ×2 |
| 等一等 | 等待 ×2 → 挥手 |
| 认真琢磨 | 等待 → 专注 ×3 → 观察 ×2 → 跳跃 |
| 东看看 | 观察 ×2 → 等待 → 挥手 |
| 再试一次 | 专注 → 沮丧 → 观察 → 跳跃 |
| 向右/向左散步 | 对应走路 ×2，同时移动最多 112 逻辑像素 → 观察 |

自由活动默认开启，启动/关闭管理面板后约 8–12 秒首次触发，之后在待机时约 18–34 秒触发。每个活动冷却 60 秒，不紧邻重复。自动散步默认关闭；手动选散步始终是明确的移动指令。打开管理面板时自动活动暂歇，手动动作仍可播放。

单击挥手，双击跳跃；拖动时按即时移动方向切换左右走路，松开后跳跃。走路只在当前屏幕工作区内水平移动，保留用户选择的高度；这版没有桌面重力、爬墙或窗口边缘攀爬。

## 注视和资源调度

v2 注视只在待机时覆盖画面，16 向从屏幕上方开始顺时针，每格 22.5°。视线中心位于精灵上半身，近处死区与观察半径随缩放改变。鼠标停下 1.2 秒后恢复正常待机，修复旧版长时间凝固在一个注视帧的问题。

没有全屏画布或常驻游戏渲染循环。动画使用单次计时器；自由活动等待使用单次粗计时器；仅散步约 2 秒期间启动 33 ms 移动计时器；注视启用且处于待机才使用 100 ms 检查，方向不变不重绘。暂停和隐藏会清空活动、停止相关计时器；恢复后从待机开始，不补播积压活动。

## 范围

完整复用已有 Codex 图集，源图片不改动。专注、等待、沮丧和观察是姿态素材，不表示正在执行真实计算或连接外部服务。新动作（例如睡觉、攀爬、悬挂）需要额外美术与格式约定。当前版本仅在 macOS Apple Silicon 实测，其他平台窗口定位仍需验证。
