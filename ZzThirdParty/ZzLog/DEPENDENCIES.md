# Dependency provenance / 依赖溯源

ZzLog keeps dependency snapshots in-tree to guarantee offline and reproducible
configuration. Dependency updates are explicit maintenance operations.

ZzLog 将依赖源码快照保存在组件内部，以保证离线、可复现配置。依赖升级必须作为明确的
维护操作执行。

## spdlog

- Upstream: `https://github.com/gabime/spdlog.git`
- Branch at import: `v2.x`
- Revision: `d24088deaa441a79267df8ae3dbc567fbe2a5e03`
- Declared version: `2.0.0` (unreleased development branch)
- License: MIT
- Included content: public headers, compiled sources, upstream README,
  changelog, and license
- ZzLog integration: compiled directly into `ZzLog::ZzLog` with the private
  C++ namespace `zzlog_spdlog`; spdlog tests, examples, benchmarks, installation,
  and global logger are disabled/not included

## fmt

- Upstream: `https://github.com/fmtlib/fmt`
- Version: `12.1.0`
- Source archive:
  `https://github.com/fmtlib/fmt/archive/refs/tags/12.1.0.tar.gz`
- SHA-256:
  `ea7de4299689e12b6dddd392f9896f08fb0777ac7168897a244a6d6085043fea`
- License: MIT
- Included content: public headers, `src/format.cc`, upstream README, and license
- ZzLog integration: `format.cc` and fmt headers are private implementation
  dependencies of the vendored spdlog backend; the public formatting API uses
  C++20 `std::format_string`, and fmt headers are not installed

The verbatim dependency licenses are under `licenses/spdlog/LICENSE.txt` and
`licenses/fmt/LICENSE.txt` and are also retained in each source snapshot.

依赖许可证原文位于 `licenses/spdlog/LICENSE.txt` 与
`licenses/fmt/LICENSE.txt`，各自的源码快照中也保留了原始许可证。
