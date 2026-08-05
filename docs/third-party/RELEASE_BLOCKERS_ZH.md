# 发布阻塞项

当前开发构建可以继续使用，但 `ZZ_RELEASE_BUILD=ON` 必须保持失败关闭。删除 JSON 数组中的条目、填写未经核验的文本或改写本文件，都不能关闭阻塞项；只有证据文件真实存在、SHA-256 匹配并完成具名审核后，才能同步更新清单。

## qwindowkit.upstream-provenance

- 已有证据：QWindowKit 上游 commit 已定位为 `2813c1f810cb3fb1999a14ad524124562081f2c2`，对应 GitHub 源码归档和 SHA-256 已写入 manifest；qmsetup 与 syscmdline 子模块 commit 也已逐文件核对。
- 缺失证据：包含具名审核人和 UTC 时间的来源审核记录。
- 责任角色：第三方依赖维护者提交证据，发布合规审核人复核来源和摘要。
- 当前状态：技术核对完成、人工审核未签署，不得声明 QWindowKit 来源已经完成发布审核。

## qmsetup.windeployqt-5.15.2-derived-work

- 已有证据：Qt 5.15.2 `qttools/src/shared/winutils/utils.cpp`、`LICENSE.GPL3-EXCEPT` 和本地派生文件的 SHA-256 已写入 manifest；原文件明确采用 GPLv3 加 Qt GPL Exception。
- 缺失证据：具名审核人签署的再分发结论与 UTC 时间。
- 责任角色：第三方依赖维护者准备逐字节来源，发布合规审核人签署再分发结论与日期。
- 当前状态：技术核对完成、再分发审核未签署；本地 `ZzThirdParty/qwindowkit/qmsetup/src/corecmd/utils_win.cpp` 不能仅凭源码注释获得发布放行，也不能整体按 qmsetup 的 MIT 结论覆盖该派生代码。

## project.license

- 已有证据：仓库根 `LICENSE` 已采用 MIT 正文，SPDX 表达式为 `MIT`，文件 SHA-256 已锁定在 `release-evidence.json`。
- 缺失证据：包含具名 `owner`、UTC 批准时间、`approved` 结论和 `MIT` SPDX 表达式的项目所有者批准记录。
- 责任角色：项目所有者选择许可证并签署批准记录，发布合规审核人核验文件摘要。
- 当前状态：部分完成；许可证选择和正文已落地，但在具名批准记录签署前仍独立阻止所有正式发布包。

## 关闭规则

每个阻塞项必须在 `release-evidence.json` 中引用真实的 repository 或 external 文件对象。路径必须是受控根目录下的相对路径，文件必须非空，声明的 SHA-256 必须等于实际字节；审核记录还必须包含具名审核人和 UTC 时间。任一条件不满足时，CMake 会重新生成同一阻塞结论。

Qt 派生代码的审核记录使用 JSON，至少包含 `reviewer`、`reviewedAt` 和值为 `approved` 的 `conclusion`。项目许可证批准记录同样使用 JSON，至少包含具名 `owner`、UTC `reviewedAt`、值为 `approved` 的 `conclusion`，以及与 manifest 完全一致的 `spdxExpression`。这些记录本身也必须由 manifest 中的 SHA-256 锁定。
