# PetDesk

为用户自己的桌宠准备的轻量跨平台底座。导入本地 Codex pet 目录，在原生透明窗口中运行；无需 Codex、Agent、账号、模型或 API key。当前以 macOS Apple Silicon 为实测平台。

## 使用自己的桌宠

1. 打开 PetDesk，默认显示控制台。在「宠物库」点击「导入桌宠」，也可把文件夹拖入窗口。
2. 选择包含最终素材的宠物文件夹。预览会显示形象、名称和可用能力，并检查素材是否完整。
3. 选择「导入并使用」直接放到桌面，或「仅导入」先保存在宠物库。
4. 在宠物库选择任意已导入的桌宠，点击「使用桌宠」即可切换。当前只显示一只，库中可以保存多只。

没有宠物时会显示首只桌宠的导入引导。下载的 ZIP 请先解压，再选择宠物文件夹。缺失或无效素材会显示具体原因。

## 控制台

| 页面 | 能力 |
|---|---|
| 宠物库 | 导入、预览、切换、查看本地文件，移出与恢复 |
| 桌面设置 | 按每只宠物分别保存大小、注视、自动播放动作、自动散步和活动频率；选择屏幕、找回位置、暂停与试播动作 |
| 应用偏好 | 再次启动时是否打开控制台、是否恢复上次桌宠；关闭控制台时继续运行或退出；查看数据文件夹 |
| 关于 | 支持范围与开源仓库入口 |

macOS 运行时在菜单栏显示 PetDesk 图标，不占用 Dock；点击图标可打开菜单，再选择「打开控制台」。Windows 在通知区域显示图标，左键打开控制台，右键打开菜单；图标也可能位于系统的折叠区域。右键桌宠或再次打开 PetDesk 也能找回控制台。默认关闭窗口只收起控制台，桌宠继续运行；「退出」会结束整个应用。单击桌宠挥手、双击跳跃、按住拖动移动。

宠物移出后保存在「已移出」中，可以恢复；不会删除原始素材。重复导入同一只不会新增副本，重新导入已移出的宠物会恢复原记录。

## 设置与本地数据

每只宠物独立保存 75%–200% 大小、注视开关、自动活动、自动散步和三档活动频率。v1 素材没有注视姿势，对应选项会禁用并说明原因；v2 支持 16 向注视。关闭自动播放动作后，自动散步和频率选项随之禁用，手动动作仍可使用。

新宠物默认开启注视与自动播放动作，自动散步默认关闭。0.1/0.2 的已选宠物及其设置会迁移，其他新导入的宠物使用各自默认值。启动偏好和桌宠设置自动保存在应用数据目录中的 `settings.ini`。

导入时复制原始编码图、元数据和缩略图，之后不依赖原始目录。图片不转码、不补帧，源文件不改动。库列表只读取元数据与缩略图，只有当前桌宠持有解码后的完整图集。

控制台使用统一的浅色主题、Qt Fusion 控件和自绘开关，让不同系统上的应用内容保持接近的视觉效果；窗口边框与系统文件选择器仍跟随操作系统。宠物详情展示原图的首帧，只缓存裁剪后的预览，不更改素材。

三档自动活动间隔约为：安静 45–90 秒、适中 18–34 秒、活泼 8–15 秒；启动或收起控制台后约 8–12 秒开始。活动有冷却且避免紧邻重复。完整动作映射见 [动作系统](docs/action-system.md)，控制台入口与数据规则见 [控制台设计](docs/console.md)。

## 支持的目录

- 标准 `pet.json`：`displayName`、`description`、`spritesheetPath` 和可选的 `spriteVersionNumber`。
- 项目目录：`name`、`slug`、`spritesheet` 指向图集；胡桃仓库是一个兼容示例。
- create-pet 工作目录：`pet_request.json`，优先使用 `final/spritesheet-extended.png` / `.webp`。
- 最终交付目录：相邻 `spritesheet.png` / `.webp`，可读取 `pet.json` 中的宠物名称。

支持透明 PNG 和静态 WebP：v1 为 1536×1872，v2 为 1536×2288，每格 192×208、8 列。校验版本、尺寸、透明通道、必需帧和可选校验值；图片限制 20 MiB、JSON 限制 128 KiB。拒绝越出所选目录的图片路径。目录中的脚本、提示词和其他代码不会执行。

## 构建

需要 C++20、CMake 3.21+ 和 Qt Core/Gui/Widgets/Test，以及 Qt Image Formats 的 WebP 插件。macOS 要求 Qt 6.9.1+，本次使用 Qt 6.9.3；其他平台最低 Qt 6.5。Qt 6.9.0 的 Cocoa 辅助功能有已知崩溃，不能用于此 Mac 应用。

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/path/to/Qt/6.9.3/macos
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
```

不打开界面检查素材：

```sh
build/PetDesk.app/Contents/MacOS/PetDesk --validate /path/to/pet-directory
```

开发时使用独立数据目录，避免改动个人宠物库：

```sh
build/PetDesk.app/Contents/MacOS/PetDesk --data-dir /path/to/test-profile
```

打包自带运行库的 Mac 应用：

```sh
bash scripts/package-macos.sh
```

脚本使用 CMake 当前配置的 Qt SDK，输出 `dist/PetDesk.app`，并限制插件只从包内加载。当前构建为 macOS 14+、Apple Silicon；Windows、Linux 和 Intel Mac 尚未实测，Wayland 的窗口定位也需要平台验证。

应用图标使用胡桃形象，菜单栏及通知区域使用同一形象的头像；这不会预置或限制可导入的桌宠。原始图稿和各平台图标保存在 `assets`。需要重新打包图标时：

```sh
cmake --build build --target petdesk_icon_generator
build/petdesk_icon_generator assets/PetDesk-artwork.png assets/PetDesk-status-artwork.png assets/PetDesk.png build/PetDesk.iconset assets/PetDesk-status.png assets/PetDesk.ico
iconutil -c icns build/PetDesk.iconset -o assets/PetDesk.icns
```

## 验证与范围

导入、动作、窗口生命周期和配置测试覆盖空库入口、多宠物独立设置、旧配置迁移、移出恢复、重复导入、再次打开控制台，以及原有动画与注视契约。图标和 Mac 验证见 `qa/verification-0.4.5.json`；此前版本的验证记录见 `qa/verification-0.4.4.json`、`qa/verification-0.4.3.json`、`qa/verification-0.4.2.json`、`qa/verification-0.4.1.json`、`qa/verification-0.4.0.json` 和 `qa/verification.json`。

播放器使用 QWidget + QPainter，不创建全屏画布。没有商店、账号、联网服务、插件运行时或 Agent 接入；本版的启动偏好指再次打开应用，不包含登录自启动。Qt 与依赖许可见 `THIRD_PARTY_NOTICES.txt`。Mac 原型为本地 ad-hoc 签名，尚未进行 Apple 公证。
