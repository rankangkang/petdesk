# 生成与配准

在新建或重做画风时读取；仅校验现有图集无需走一遍生成。`init` 生成的 `jobs.json` 已包含身份约束、各行动作语义、参考关系及输出路径，可按具体角色增强 prompt。图片生成工具和存储位置由当前环境决定，生成后复制选定文件到工作目录，勿假设某个插件缓存或机器路径存在。

## 基准形象与源行

从照片制作时保留能决定身份的毛色、面部、耳形、比例、衣物及饰品位置；像素、写实、黏土、毛绒等风格由用户选择或从参考推断。全身紧凑，五官细节应能在 192×208 及 96×104 中看清。基准图检查通过后，后续每个生成 job 都附基准图与决定身份的参考，不依赖纯文字重画角色。

每个标准动作独立生成一条横向透明 strip，完整且互不接触的姿势按时间先后排列；帧数由 `jobs.json` 指定。不要让图像模型生成整个 11 行 atlas。请求真实 alpha；若返回的只是绘制棋盘格、白底或带色边的假透明图，使用图像生成编辑能力修复源行。不能把不干净背景带入提取，也不能为了去背景擦掉角色本来就有的颜色。

可用的通用 prompt 补充：

> Use the attached approved base as the same character, not a redesign. One horizontal strip of exactly N complete poses, chronological from left to right, with clear transparent gaps. Keep drawing scale, torso landmarks, light, markings and prop handedness stable. Transparent background, no shadows, text, slot labels, guide marks, dust, sparkles or floating symbols. Preserve natural joint and limb attachment.

姿态本身表达动作，不添加脱离身体的符号、烟雾、运动线、漂浮星星或新道具。挥手通过附着的手爪等表现；等待是期待输入；专注是在原地思考；观察通过眼神、歪头、倾身表现。跳跃包含准备、起跳、最高点、下降、落地，源行的最高点要真正高于地面。

先检查向右走的步态与身份。只有角色标记、配饰、持物左右和光照允许时，才可将每帧单独水平翻转生成向左走；时间顺序保持不变。不能整条 strip 翻转导致动作倒序。不能从待机复制出等待、专注、观察或沮丧动作。

## 提取与共享变换

默认 `extract` 按透明间隔识别完整姿势组；姿势内部若有未连接的部分，投影分组可能多于实际帧数，此时不会自动丢弃碎片或按预设格数硬切。用看过的原图坐标给出完整帧槽：

```json
[[0, 0, 256, 400], [256, 0, 512, 400], [512, 0, 768, 400], [768, 0, 1024, 400]]
```

这只是四帧示意。实际框数必须符合状态，不能重叠，必须包含全部可见像素。优先用等宽、固定身体中心的真实帧槽；自动的紧边界居中在尾巴或手臂变化较大时可能导致身体左右抖动，需用槽位或修正源图解决。先检查 `extract` 输出的 `contact-sheet.png`、`preview.gif` 和 `registration.json`，再接受该行。

第一组待机默认中性身高约 160 px、基线 196 px，留出头顶跳跃空间。可用 `--height 148` 等调整整只角色的共同尺寸。后续标准动作通过 `--reference "$RUN_DIR/frames/idle/00.png"` 对齐中性身高和基线，不对每一帧独立填满画布。跳跃默认以最后一帧作为落地参考，必要时用 `--anchor` 指定实际中性帧（从 0 计数），保留行内所有纵向差值。

超出边界时脚本停止；先修正源图或共同尺寸，再重新提取受影响行。不要只缩小某个跳跃帧，也不要把跳跃每帧都贴到地面。像素风使用 `--resample nearest`，其他画风默认 smooth。

全部标准行通过后：

```sh
python3 "$SKILL_DIR/scripts/petdesk_pet.py" assemble \
  --frames "$RUN_DIR/frames" --version 1 --output "$RUN_DIR/final/standard.png"
python3 "$SKILL_DIR/scripts/petdesk_pet.py" preview \
  --atlas "$RUN_DIR/final/standard.png" --output "$RUN_DIR/qa/standard"
```

检查全部九行动作及待机→跳跃→待机，再以这里的 `contact-light.png` 辅助生成注视。

## 四主方向及 16 向注视

先说明角色如何转头/眼睛、什么部位保持稳定、饰品如何遮挡或跟随；保留原有眼睛结构。头与上身自然微转，禁止把整个宠物旋转成转台、逐帧换比例或用扭曲贴图冒充转头。

先生成四帧 cardinal strip，按 **上、屏幕右、下、屏幕左** 排列。结合鼻尖、眼睛、眼睑、头部相对躯干位置确认四方向；无眼睛角色使用其自然指向部件。主方向歧义应先修正，再生成两条注视行。四帧锚点可通过 `extract --state cardinals` 提取预览，不放入最终 atlas。

行 9 使用基准图、四主方向与标准动作 contact sheet；行 10 还必须引用已通过的行 9。两行源图保持相同高度、主体绘制尺度和纵向坐标，才能共享注册变换。

```sh
python3 "$SKILL_DIR/scripts/petdesk_pet.py" extract \
  --strip "$RUN_DIR/source/look-row-9.png" --state look-row-9 \
  --reference "$RUN_DIR/frames/idle/00.png" --output "$RUN_DIR/frames/look-row-9"
python3 "$SKILL_DIR/scripts/petdesk_pet.py" extract \
  --strip "$RUN_DIR/source/look-row-10.png" --state look-row-10 \
  --registration "$RUN_DIR/frames/look-row-9/registration.json" \
  --output "$RUN_DIR/frames/look-row-10"
```

先检查行 9 的完整性、姿态及主体相对待机的尺度，再继续行 10。自动缩放只提供初始配准；向上/下看导致轮廓高度变化时，不能用独立拉伸补偿，应保持躯干、衣服或底座等稳定标记，并修正源图或选择恰当的参考姿态。

最后按顺时针看完整环，特别看 `157.5 → 180` 和 `337.5 → 000` 的接缝。每个方向在 `visual-review.json` 中写出实际观察和地标证据；对角线分别解释水平和垂直分量。明显朝错、四主方向不清或躯干跳动应修复，不能靠写入通过绕过。

注视行中的问题先判断是提取错误还是原画问题。原画错则重做相关完整八帧源行，以保持连续性；不要从另一批生成图拼入一张风格或尺寸不同的最终格。已经通过的标准九行应保留。

## 工具自检

本技能的脚本只依赖 Pillow；图像生成不由 Python 调用。需要检查工具行为时，在装有依赖的环境运行：

```sh
python3 -B -m unittest discover -s "$SKILL_DIR/scripts" -p 'test_*.py'
```

真实导出还必须使用 PetDesk 二进制；正常工作流不需要任何 Pets MCP 接口。App 打包另见仓库 `docs/packaging.md`。
