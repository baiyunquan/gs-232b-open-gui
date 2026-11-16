# 内置 DXCC 数据

- 数据：Jim Reisert AD1C 的 [Amateur Radio Country Files](https://www.country-files.com/contest/)，原始下载地址 <https://www.country-files.com/cty/cty.dat>。
- 快照：2026-11-14（文件内 `=VER20261114`），获取于 2026-11-16；340 个 DXCC 实体，另含 6 个仅用于 WAE 的实体。
- SHA-256：`5e9dad3e4a04d868dda22bf42cfbebc196c6df655f3f10b641d851ff22a1784b`。
- 授权：上游 [Copyright](https://www.country-files.com/copyright/) 页的许可原文见 `COPYING`，与资源一起嵌入应用，并复制到发布包的 `licenses` 目录。
- 格式：[CTY.DAT Format](https://www.country-files.com/cty-dat-format/)。文件保持上游原样，CTY 经度以西经为正，加载时转换成东经为正。

更新时替换 `cty.dat`，同步这里的版本和校验值，并运行 `target_bearing_tests`。解析器跳过带 `*` 的 WAE 条目、`VERSION`/`VER…` 版本标记，保留完整呼号例外与最长前缀规则；所有呼号均使用 DXCC 实体头部坐标，不使用别名的区域坐标覆盖。不在运行或构建时下载数据。
