# clipvault

Windows 剪贴板历史管理器。

> 状态：M1 开发中，未发布。进度和下一步见 [`docs/STATE.md`](docs/STATE.md)。

## 它解决什么

复制一段代码，再复制别的东西，前一段就没了。clipvault 常驻后台记录剪贴板历史，
按全局热键唤起一个置顶列表，输入几个字过滤，回车把选中的内容写回剪贴板。

## 功能

- [x] 监听剪贴板变化并持久化
- [ ] 全局热键唤出无边框置顶浮层
- [ ] 关键词过滤
- [ ] 回车写回剪贴板
- [x] 历史上限 100 条，超限按最近使用淘汰
- [ ] 托盘图标与退出入口

## 当前范围

只处理**文本**剪贴板。单条超过 2 MB 的内容静默忽略。图片与文件类型暂不支持。

## 构建

```
build.bat          构建
build.bat test     构建 + 跑测试
build.bat clean    清空 build 重新构建
```

需要 VS 2022（MSVC）、CMake 3.28+、Ninja 在 PATH。`build.bat` 内部调 `vcvars64.bat`，所以在任何终端里执行都行，不用先开 Developer Command Prompt。

## 目录结构

```
include/clipvault/
  core/              纯逻辑，禁止 include windows.h
  platform/win32/    Win32 实现
src/
  core/              与 include 同构
  platform/win32/
tests/               单元测试
external/            vendor 的第三方源码（SQLite、Catch2）
docs/                STATE.md 与 DECISIONS.md
.github/workflows/   CI 配置
```

## 第三方依赖

依赖以源码形式 vendor 在 `external/` 下，随仓库一起提交，构建时不联网下载。

| 库 | 版本 | 许可证 | 用途 |
|---|---|---|---|
| [SQLite](https://www.sqlite.org/) | 3.54.1 (amalgamation) | Public Domain | 本地存储 |
| [Catch2](https://github.com/catchorg/Catch2) | 3.16.1 (amalgamated) | BSL-1.0 | 单元测试 |

各自的许可证文本保存在对应目录下的 `LICENSE.txt`。

## 许可

[MIT](LICENSE)
