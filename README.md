# PetDesk

一个只读取本地素材的 Qt 桌宠原型。直接导入 Codex pet 目录，原生窗口播放精灵图；无需 Codex、Agent、账号、模型、API key、Python 或后台服务。

## 使用

1. 打开 PetDesk，点击「导入宠物目录…」。
2. 选择一个宠物目录，确认预览后点击「使用这只宠物」。
3. 拖动宠物移动，单击挥手，右键打开管理面板。也可以使用菜单栏的宠物图标管理宠物。

管理面板与菜单栏入口支持切换宠物、75%–200% 大小、暂停、隐藏、九种动作，以及 v2 宠物的「看向鼠标」。跟随鼠标默认关闭；开启后以 100 ms 间隔检测光标，只有视线方向变化时才重绘。暂停使用待机首帧，隐藏或暂停会停止相关计时器。

导入时会在应用自己的数据目录保存动画图、元数据和缩略图，之后启动不再读取原始目录。原图保留原始编码字节，不转码、不补帧、不改动源文件。关闭管理窗口后桌宠继续运行；从宠物菜单选择「退出 PetDesk」退出。

## 支持的目录

- 标准 `pet.json`：`displayName`、`description`、`spritesheetPath` 和可选的 `spriteVersionNumber`。
- 胡桃素材仓库：`name`、`slug`、`spritesheet` 指向 `assets/spritesheet.png`。
- 当前 create-pet 技能工作目录：`pet_request.json`，优先读取 `final/spritesheet-extended.png` / `.webp`，避免误选中间的 v1 图。
- 当前技能交付目录：相邻的 `spritesheet.png` / `.webp`，可读取 `pet.json` 中的宠物名称。

支持透明 PNG 和单张 WebP：v1 为 1536×1872，v2 为 1536×2288，每格 192×208、8 列。检查版本、尺寸、透明通道和所有必需帧；限制图像文件 20 MiB、JSON 128 KiB，并拒绝指向所选目录之外的图片路径。目录中的脚本、提示词和其他代码不会执行。

第一版以单宠为范围，不包含宠物商店、联网功能、账户、插件宿主或 Agent 接入。播放器使用 QWidget + QPainter，窗口贴合宠物尺寸，不创建全屏画布。

## 构建

需要 C++20 编译器、CMake 3.21+、Qt 6.5+（Core、Gui、Widgets、Test），以及 Qt Image Formats 的 WebP 解码插件。

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$(qtpaths --query QT_INSTALL_PREFIX)"
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
```

当前机器使用 Homebrew Qt 6.9.0。该版本依赖 AGL，配合已有的 macOS 15.4 SDK 构建；较新的 macOS SDK 已移除它的链接存根。本机配置命令为：

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/opt/homebrew \
  -DCMAKE_OSX_SYSROOT=/Library/Developer/CommandLineTools/SDKs/MacOSX15.4.sdk
```

不启动界面，检查一个真实目录：

```sh
build/PetDesk.app/Contents/MacOS/PetDesk --validate /path/to/pet-directory
```

打包自带 Qt 运行库的 Mac 应用：

```sh
bash scripts/package-macos.sh
```

输出 `dist/PetDesk.app`。当前构建为 macOS 14+、Apple Silicon；尚未验证 Windows、Linux 和 Intel Mac。Linux 的 Wayland 对程序控制窗口位置存在限制，完整自由拖动需要平台验证。

## 检查

导入测试覆盖项目目录、标准 WebP、技能最终图选择、旧版 v1、缺帧/版本错误、目录越界/符号链接、错误校验值，以及导入去重和删除源图后的独立加载。真实胡桃的四种目录形态也可使用上述验证命令检查。

## 素材与依赖

应用图标由用户已有的胡桃待机帧制作，仅作这个本地原型使用；胡桃图集本身通过目录导入。Qt 和打包依赖保留各自许可，见 `THIRD_PARTY_NOTICES.txt`。原型未经过 Developer ID 签名或 Apple 公证。

本机的实际检查结果见 `qa/verification.json`。资源占用和耗电对比未在本轮继续进行。
