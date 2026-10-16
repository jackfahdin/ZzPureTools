# 发布合规状态

项目许可证和历史依赖审核记录由 Jackfahdin 于 2026-08-05 签署，按原快照保留。2026-10-16 更新依赖后的源码身份、目录迁移和分发范围由独立的 Codex 技术核对记录说明，不改写历史签名。`ZZ_RELEASE_BUILD=ON` 必须逐字节验证当前外部证据目录和仓库记录；缺失文件、摘要变化或重新加入 blocker 时继续失败关闭。平台真机证据由独立门禁管理。

## qwindowkit.upstream-provenance

- 证据：QWindowKit 上游 commit 为 `5626b3ae5f22508d77361495f589abedb39c2ee4`，对应源码归档和 SHA-256 已写入 manifest；qmsetup 与 stdcorelib 子模块 commit 也已逐文件核对。
- 当前技术记录：`docs/third-party/reviews/qwindowkit-source-verification.json`；历史具名记录仍为 `docs/third-party/reviews/qwindowkit-provenance-review.json`。
- 当前状态：来源技术核对通过，历史具名审核保持原快照，不声明新的人工签署。

## qmsetup.windeployqt-5.15.2-derived-work

- 证据：Qt 5.15.2 `qttools/src/shared/winutils/utils.cpp`、`LICENSE.GPL3-EXCEPT` 和本地派生文件的 SHA-256 已写入 manifest；原文件明确采用 GPLv3 加 Qt GPL Exception。
- 当前技术记录：`docs/third-party/reviews/windeployqt-source-verification.json`；历史具名记录仍为 `docs/third-party/reviews/windeployqt-redistribution-review.json`。
- 当前状态：已核对迁移后的 `corecmd/utils/utils_win.cpp` 及三个错误文本调用变化，继续保留 `GPL-3.0-only WITH Qt-GPL-exception-1.0` 通知和原分发条件。该代码仅限 vendored qmsetup 构建工具，不进入 ZzPureToolsFrame 运行时或开发安装包，也不得被描述为 MIT 代码。

## project.license

- 证据：仓库根 `LICENSE` 已采用 MIT 正文，版权主体为 Jackfahdin，SPDX 表达式为 `MIT`，文件 SHA-256 已锁定在 `release-evidence.json`。
- 批准记录：`docs/third-party/reviews/project-license-approval.json`。
- 当前状态：已解决，项目所有者为 Jackfahdin，结论为 `approved`。

## 关闭规则

每项证据必须在 `release-evidence.json` 中引用真实的 repository 或 external 文件对象。路径必须是受控根目录下的相对路径，文件必须非空，声明的 SHA-256 必须等于实际字节；审核记录还必须包含具名审核人和 UTC 时间。任一条件不满足时，CMake 会重新生成对应阻塞结论。

两个 `historicalHumanReview` 文件对象单独锁定原人工审核记录，必须引用仓库内固定路径并通过 SHA-256 校验；新增技术记录不能替代或绕过历史记录的摘要门禁。

Qt 派生代码的审核记录使用 JSON，至少包含 `reviewer`、`reviewedAt` 和值为 `approved` 的 `conclusion`。项目许可证批准记录同样使用 JSON，至少包含具名 `owner`、UTC `reviewedAt`、值为 `approved` 的 `conclusion`，以及与 manifest 完全一致的 `spdxExpression`。这些记录本身也必须由 manifest 中的 SHA-256 锁定。
