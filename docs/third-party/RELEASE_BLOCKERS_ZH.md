# 发布阻塞项

当前开发构建可以继续使用，但 `ZZ_RELEASE_BUILD=ON` 必须保持失败关闭。删除 JSON 数组中的条目、填写未经核验的文本或改写本文件，都不能关闭阻塞项；只有证据文件真实存在、SHA-256 匹配并完成具名审核后，才能同步更新清单。

## qwindowkit.upstream-provenance

- 缺失证据：QWindowKit 精确的 40 位上游 commit、对应源码归档、归档 SHA-256 和来源审核记录。
- 责任角色：第三方依赖维护者提交证据，发布合规审核人复核来源和摘要。
- 当前状态：未解决，不得声明 QWindowKit 来源已经完成发布审核。

## qmsetup.windeployqt-5.15.2-derived-work

- 缺失证据：Qt 5.15.2 `qttools/src/windeployqt/utils.cpp` 上游源码、对应许可证、本地派生文件摘要和再分发审核结论。
- 责任角色：第三方依赖维护者准备逐字节来源，发布合规审核人签署再分发结论与日期。
- 当前状态：未解决，本地 `ZzThirdParty/qwindowkit/qmsetup/src/corecmd/utils_win.cpp` 不能仅凭源码注释获得发布放行。

## project.license

- 已有证据：仓库根 `LICENSE` 已采用 MIT 正文，SPDX 表达式为 `MIT`，文件 SHA-256 已锁定在 `release-evidence.json`。
- 缺失证据：包含具名 `owner`、UTC 批准时间、`approved` 结论和 `MIT` SPDX 表达式的项目所有者批准记录。
- 责任角色：项目所有者选择许可证并签署批准记录，发布合规审核人核验文件摘要。
- 当前状态：部分完成；许可证选择和正文已落地，但在具名批准记录签署前仍独立阻止所有正式发布包。

## 关闭规则

每个阻塞项必须在 `release-evidence.json` 中引用真实的 repository 或 external 文件对象。路径必须是受控根目录下的相对路径，文件必须非空，声明的 SHA-256 必须等于实际字节；审核记录还必须包含具名审核人和 UTC 时间。任一条件不满足时，CMake 会重新生成同一阻塞结论。

Qt 派生代码的审核记录使用 JSON，至少包含 `reviewer`、`reviewedAt` 和值为 `approved` 的 `conclusion`。项目许可证批准记录同样使用 JSON，至少包含具名 `owner`、UTC `reviewedAt`、值为 `approved` 的 `conclusion`，以及与 manifest 完全一致的 `spdxExpression`。这些记录本身也必须由 manifest 中的 SHA-256 锁定。
