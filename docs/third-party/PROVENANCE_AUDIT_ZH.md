# QWindowKit 与 Qt 派生代码来源核对

## 审计边界

本文记录 2026-08-05 完成的技术核对，只证明可由 Git 历史、逐文件比较和 SHA-256 复现的事实。本文不是法律意见，也不代替发布 manifest 要求的具名来源审核、再分发审核或项目所有者批准记录。

未跟踪的证据根目录约定为 `build/provenance-audit/evidence`。正式发布配置必须通过绝对路径 `ZZ_RELEASE_EVIDENCE_ROOT` 指向该目录或包含相同字节的受控目录。

## QWindowKit 快照

本地 `ZzThirdParty/qwindowkit` 与以下上游身份匹配：

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

## Qt 5.15.2 派生代码

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

## 当前结论

技术身份、上游字节和摘要已经固定，Jackfahdin 于 `2026-08-05T06:05:49Z` 签署以下记录：

- `docs/third-party/reviews/qwindowkit-provenance-review.json`：QWindowKit 来源审核结论为 `approved`。
- `docs/third-party/reviews/windeployqt-redistribution-review.json`：Qt 派生构建工具按 `GPL-3.0-only WITH Qt-GPL-exception-1.0` 审核，结论为 `approved`。

第二项批准仅适用于 vendored qmsetup 构建工具：必须保留来源和许可证通知，不得将派生片段描述为 MIT，不得把 `qmcorecmd` 或派生源码安装到 ZzPureToolsFrame 二进制包。摘要或分发范围变化后必须重新审核。
