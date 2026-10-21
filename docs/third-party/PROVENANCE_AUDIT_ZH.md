# QWindowKit 与 Qt 派生代码来源核对

## 当前快照：2026-10-21

2026-10-16 的技术核对确认以下三个固定归档与当时仓库受版本控制的源码逐字节一致。子模块在 vendor 目录中展开，`.gitmodules` 文件保留；比较时排除展开的子模块目录和本机 `.git` 元数据。

当前快照额外保留提交 `b80a5d2` 的一处本地补丁：`qmsetup/src/corecmd/commands/incsync.cpp` 在 Windows 上为接近 MAX_PATH 的转发头引用生成规范化绝对路径，避免 MSVC 打不开头文件。2026-10-21 已将该文件与摘要验证通过的 qmsetup 归档比较，唯一差异为上述补丁。原文件 SHA-256 为 `31befddc251e2028f0949e6c65e4a58f561baa990b4d734433984f30989fad9b`，补丁后为 `b4dffe5543e92b5337a64dedad69628454a0f2ade0d1962c6c85abe1b8c94345`；两份来源 JSON 已登记，不再将当前源码描述为全部未经修改。

| 项目 | 固定 commit | codeload 归档 SHA-256 |
|---|---|---|
| QWindowKit 1.5.1.0 | `5626b3ae5f22508d77361495f589abedb39c2ee4` | `c649e6bfa1e450e79f08372cc24f40d49ce0137ab3e7ed8f76781fc6a9f2c496` |
| qmsetup 1.1.2.0 | `f9cb0153a9dc913b7b8226451cd6760ed83b6ce8` | `75e8790303479edce30337a8ededc75c8e3be47c4e9d528b5d3001aa546eb2a3` |
| stdcorelib 0.2.1.0 | `2fbefa9b152451d9621bf71bb1dfc8f98f8d2120` | `96fe7b6a3f41ad7c4e0d9dc8233a165a37932f3ebbd25d81037ee29317a026f9` |

stdcorelib 替代旧 syscmdline。运行包和 SDK 安装均复制 `qmsetup/src/stdcorelib/LICENSE`，目标名称为 `stdcorelib-LICENSE`。qmsetup 与 stdcorelib 只参与构建，不作为框架运行库安装。

Qt 派生文件现为 `ZzThirdParty/qwindowkit/qmsetup/src/corecmd/utils/utils_win.cpp`，SHA-256 为 `729c2b7cfdf337aa7026081cd8d9dd454f54298575e45bcf8521d74e003fc0fc`。将迁移前后文件中的 `WindowsDeployQt` namespace 单独比较，只有三个错误文本调用从 `winErrorMessage` 改为 `stdc::windows::systemError`。完整文件有其他构建工具调整，manifest 必须锁定新摘要。原 Qt 来源、许可证表达式和不安装构建工具的限制继续保留。

来源记录见 `qwindowkit-vendor.json`、`release-evidence.json`、`reviews/qwindowkit-source-verification.json` 和 `reviews/windeployqt-source-verification.json`。技术核对记录的 reviewer 明确为 Codex，不改写下文历史人工审核，不表示 Jackfahdin 为新快照重新签署了审核。原审核 JSON 保持原字节不变，随安装包一并保留。

最新验证矩阵只记录已实跑的 Linux / GCC / Qt 6.11.1 shared Debug，不沿用旧快照的 Clang、static、LTO 或 Sanitizer 结论。

当前归档和 Qt 许可证可在空证据目录中重新准备，脚本会验证 manifest 中的固定摘要：

```bash
mkdir -p build/provenance-audit/current-evidence
cmake -DZZ_OUTPUT_DIR="$PWD/build/provenance-audit/current-evidence" \
  -P scripts/package/PrepareReleaseEvidence.cmake
```

## 历史审计边界：2026-08-05

本文记录 2026-08-05 完成的技术核对，只证明可由 Git 历史、逐文件比较和 SHA-256 复现的事实。本文不是法律意见，也不代替发布 manifest 要求的具名来源审核、再分发审核或项目所有者批准记录。

未跟踪的证据根目录约定为 `build/provenance-audit/evidence`。正式发布配置必须通过绝对路径 `ZZ_RELEASE_EVIDENCE_ROOT` 指向该目录或包含相同字节的受控目录。

## 历史 QWindowKit 快照

当时的 `ZzThirdParty/qwindowkit` 与以下上游身份匹配；以下命令仅用于复核旧快照：

| 项目 | 固定值 |
|---|---|
| 上游 | `https://github.com/stdware/qwindowkit.git` |
| QWindowKit commit | `2813c1f810cb3fb1999a14ad524124562081f2c2` |
| qmsetup commit | `bd2ce397ee1400e4a72d3ed8ce6b6baed24baeb4` |
| syscmdline commit | `0c9f3de8b11bd2f33b03bea5521bf446af4ead69` |
| GitHub 归档 SHA-256 | `cd0d3ad3c94ce5c0965337f2e59262613d684f46d6ce0c45613726e751d3d90c` |

逐目录比较只有两个布局差异：vendor 目录展开了 submodule 内容，并省略 QWindowKit 根目录和 qmsetup 目录中的 `.gitmodules`。除此之外，QWindowKit、qmsetup 和 syscmdline 的受版本控制文件与上述三个 commit 逐字节一致。该布局转换记录在 `qwindowkit-vendor.json` 的 `localPatches`，不能把它误写为完全未变更的 Git checkout。

复核命令：

```bash
git clone --recurse-submodules https://github.com/stdware/qwindowkit.git \
  build/provenance-audit/upstream-qwindowkit
git -C build/provenance-audit/upstream-qwindowkit checkout --detach \
  2813c1f810cb3fb1999a14ad524124562081f2c2
git -C build/provenance-audit/upstream-qwindowkit submodule update \
  --init --recursive
git -C build/provenance-audit/upstream-qwindowkit submodule status \
  --recursive
diff -qr --exclude=.git ZzThirdParty/qwindowkit \
  build/provenance-audit/upstream-qwindowkit
```

预期 `diff` 只报告两个 `.gitmodules` 文件仅存在于上游 checkout。归档必须从固定 commit 下载，不能使用会移动的分支名称：

```bash
curl --fail --location \
  --output build/provenance-audit/evidence/qwindowkit/qwindowkit-2813c1f810cb3fb1999a14ad524124562081f2c2.tar.gz \
  https://github.com/stdware/qwindowkit/archive/2813c1f810cb3fb1999a14ad524124562081f2c2.tar.gz
sha256sum build/provenance-audit/evidence/qwindowkit/qwindowkit-2813c1f810cb3fb1999a14ad524124562081f2c2.tar.gz
```

## 历史 Qt 5.15.2 派生代码

qmsetup 文件 `ZzThirdParty/qwindowkit/qmsetup/src/corecmd/utils_win.cpp` 第 245 行起明确注明修改自 `windeployqt 5.15.2`。对应 Qt 标签为 `v5.15.2`，解引用后的 qttools commit 是 `cc52debd905e0ed061290d6fd00a5f1ab67478a5`，实际源文件路径是 `qttools/src/shared/winutils/utils.cpp`。

Qt 原文件头声明 GPLv3 加 `LICENSE.GPL3-EXCEPT` 中的 Qt GPL Exception。派生代码把 Qt 类型改写为标准库类型并裁剪为 PE 依赖解析逻辑，但这些修改不构成将该代码自动改为 MIT 的依据。发布前必须由具名审核人确认适用义务并签署 manifest 要求的 `approved` 再分发记录。

| 文件 | SHA-256 |
|---|---|
| Qt 5.15.2 `src/shared/winutils/utils.cpp` | `6620d8eccfa7c3be50ad6040633beecbd536f91d4358e4c891c6015a7162a1dd` |
| Qt 5.15.2 `LICENSE.GPL3-EXCEPT` | `0dbe024961f6ab5c52689cbd036c977975d0d0f6a67ff97762d96cb819dd5652` |
| 本地 qmsetup `utils_win.cpp` | `7009e6061c21dcfcf838960249fbc165d05adcca2b944a85d6fd2d65eb2f5957` |

复核命令：

```bash
git clone --branch v5.15.2 --depth 1 \
  https://github.com/qt/qttools.git \
  build/provenance-audit/upstream-qttools-5.15.2
git -C build/provenance-audit/upstream-qttools-5.15.2 rev-parse HEAD
sha256sum \
  build/provenance-audit/upstream-qttools-5.15.2/src/shared/winutils/utils.cpp \
  build/provenance-audit/upstream-qttools-5.15.2/LICENSE.GPL3-EXCEPT \
  ZzThirdParty/qwindowkit/qmsetup/src/corecmd/utils_win.cpp
```

## 历史结论

技术身份、上游字节和摘要已经固定，Jackfahdin 于 `2026-08-05T06:05:49Z` 签署以下记录：

- `docs/third-party/reviews/qwindowkit-provenance-review.json`：QWindowKit 来源审核结论为 `approved`。
- `docs/third-party/reviews/windeployqt-redistribution-review.json`：Qt 派生构建工具按 `GPL-3.0-only WITH Qt-GPL-exception-1.0` 审核，结论为 `approved`。

第二项批准仅适用于 vendored qmsetup 构建工具：必须保留来源和许可证通知，不得将派生片段描述为 MIT，不得把 `qmcorecmd` 或派生源码安装到 ZzPureToolsFrame 二进制包。摘要或分发范围变化后必须重新审核。
