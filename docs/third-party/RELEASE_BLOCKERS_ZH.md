# 发布合规状态

项目许可证和历史依赖审核记录由 Jackfahdin 于 2026-08-05 签署，按原快照保留。2026-11-09 同步上游后的源码身份、兼容补丁和依赖移除由独立的 Codex 技术核对记录说明，不改写历史签名。`ZZ_RELEASE_BUILD=ON` 必须逐字节验证当前外部证据目录和仓库记录；缺失文件、摘要变化或重新加入 blocker 时继续失败关闭。平台真机证据由独立门禁管理。

## qwindowkit.upstream-provenance

- 证据：QWindowKit 上游 commit 为 `2072958b04820e9e7629f6c40068dee0ce0e9915`，固定归档与全部 157 个文件逐字节核对；vendor 仅保留已登记的 Windows 长路径补丁，当前无子模块。
- 当前技术记录：`docs/third-party/reviews/qwindowkit-source-verification.json`；历史具名记录仍为 `docs/third-party/reviews/qwindowkit-provenance-review.json`。
- 当前状态：来源技术核对通过，历史具名审核保持原快照，不声明新的人工签署。

## qmsetup.windeployqt-5.15.2-derived-work

- 证据：当前 manifest 标记 `status: removed`，移除提交与 QWindowKit 固定提交一致；源码树中 qmsetup 已不存在，submodules 声明为空。
- 历史技术记录：`docs/third-party/reviews/windeployqt-source-verification.json`；历史具名记录为 `docs/third-party/reviews/windeployqt-redistribution-review.json`。二者仍由 manifest 锁定摘要并随正式 SDK 安装。
- 当前状态：派生构建工具已移除，不再下载或安装其源文件。历史许可证表达式仍为 `GPL-3.0-only WITH Qt-GPL-exception-1.0`，不改写为 MIT；安装审计仍禁止 `qmcorecmd` 进入产品包。

## project.license

- 证据：仓库根 `LICENSE` 已采用 MIT 正文，版权主体为 Jackfahdin，SPDX 表达式为 `MIT`，文件 SHA-256 已锁定在 `release-evidence.json`。
- 批准记录：`docs/third-party/reviews/project-license-approval.json`。
- 当前状态：已解决，项目所有者为 Jackfahdin，结论为 `approved`。

## 关闭规则

每项证据必须在 `release-evidence.json` 中引用真实的 repository 或 external 文件对象。路径必须是受控根目录下的相对路径，文件必须非空，声明的 SHA-256 必须等于实际字节；审核记录还必须包含具名审核人和 UTC 时间。任一条件不满足时，CMake 会重新生成对应阻塞结论。

两个 `historicalHumanReview` 文件对象单独锁定原人工审核记录，必须引用仓库内固定路径并通过 SHA-256 校验；新增技术记录不能替代或绕过历史记录的摘要门禁。

旧的 present 状态继续校验 Qt 派生源码、许可和 approved 审核；removed 状态必须额外验证源码缺席、移除提交一致及空子模块声明，不能仅填写状态就跳过检查。正反 fixture 覆盖残留源码、伪造提交和历史摘要篡改。

项目许可证批准记录使用 JSON，至少包含具名 `owner`、UTC `reviewedAt`、值为 `approved` 的 `conclusion`，以及与 manifest 完全一致的 `spdxExpression`。这些记录本身也必须由 manifest 中的 SHA-256 锁定。
