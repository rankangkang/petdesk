# 本地打包

`scripts/package.py` 是不依赖 Agent 的统一入口。它复用构建缓存、默认运行自动化测试，并将每次成品放进独立目录，避免覆盖之前的包。内置宠物和临时组包文件位于配置指定的仓库外目录。

## 准备配置

把 [示例配置](../packaging/profile.example.json) 复制到 `~/.config/petdesk/packaging.json`，填写 Qt SDK、LLVM-MinGW、工作目录和输出目录。也可放在其他位置，通过 `--config` 指定。路径可以是绝对路径，也可以相对于配置文件；`work_dir` 和 `output_dir` 必须在本仓库外。不要把包含私人素材路径的配置提交到仓库。

`pets` 是可选数组，每项的 `id` 使用小写英文、数字、连字符或下划线，`path` 指向可由 PetDesk 导入的目录。脚本会调用宿主程序校验每只宠物，生成最小素材和内置清单；同名同图的重复宠物会被拒绝。内置宠物不可在库中移出。省略 `pets` 或使用空数组则不预置宠物。

`icons` 是可选对象：`application` 指向应用图稿，`status` 指向带透明通道的托盘图稿。打包时会按原始比例缩放并居中，生成各平台尺寸。省略该项时使用仓库默认图标。发行包图标与运行时用户选择的图标相互独立。

需要 Python 3.9+、CMake 和对应 Qt SDK。Mac 包需要 macOS、Xcode 命令行工具和 `iconutil`；在 Mac 上交叉构建 Windows 包还需要相同 Qt 版本的 Windows LLVM-MinGW SDK、匹配的 LLVM-MinGW 工具链和 NSIS 的 `makensis`。脚本不会自动安装或升级依赖。

## 检查与打包

```sh
# 只检查配置路径，不构建、不生成安装包；素材内容在正式打包时校验。
python3 scripts/package.py windows --dry-run

# 构建宿主工具并检查宠物和图标素材，但不生成安装包。
python3 scripts/package.py windows --check-content

# 构建 Windows x64 安装包或 macOS 应用。
python3 scripts/package.py windows
python3 scripts/package.py macos

# 生成包含应用和“应用程序”快捷入口的 macOS DMG。
python3 scripts/package.py macos --dmg
```

首次构建需要配置和编译 Qt 宿主及目标应用；后续使用同一套 SDK 会复用缓存。每次默认运行宿主测试；已经验证过的小改动可以传 `--skip-tests`。输出位置会打印在命令末尾。Windows 包和 Mac 应用仍需要在目标系统上进行实际运行验证；本地 Mac 包默认只做 ad-hoc 签名。

底层脚本仍可单独使用：`scripts/package-windows.sh` 接受 Qt、工具链、可选图标及内置包目录环境变量；`scripts/package-macos.sh` 接受可选图标及内置包目录。两者遇到已有同名成品会停止，需指定新的输出目录。通常建议使用统一入口，由它生成这些目录并设置变量。
