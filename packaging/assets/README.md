# 应用图标资产

`ZzPureToolsExample` 的初始应用图标由 Jackfahdin 所有，并已获准用于本项目的三个
桌面平台。导入过程未修改旧项目文件。

## 来源记录

- 导入日期：2026-08-28
- 迁移工作区中的来源：`../ZzPureToolsExample/Resource/Image/APPICON.png`
- 原始 SHA-256：`5754af3a83e9280d82da6bef26a8e773a1d4a16edbf32e5e363a9bfbe15b53cc`
- 仓库主图标：`examples/ZzPureToolsExample/resources/application/ZzPureToolsExample.png`

## 重新生成平台图标

生成命令需要 Python 3 和 Pillow。先将 `source.png` 替换为新的方形 RGBA 主图标，再从
仓库根目录运行：

```bash
python3 -c 'from PIL import Image; p="source.png"; i=Image.open(p).convert("RGBA").resize((1024, 1024), Image.Resampling.LANCZOS); i.save("examples/ZzPureToolsExample/resources/application/ZzPureToolsExample.ico", format="ICO", sizes=[(16, 16), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)]); i.save("examples/ZzPureToolsExample/resources/application/ZzPureToolsExample.icns", format="ICNS")'
```

Windows、macOS 和 Linux 可以在以后分别换用不同的品牌变体，但必须同步更新本记录、
平台元数据合同和实际部署包验证。
