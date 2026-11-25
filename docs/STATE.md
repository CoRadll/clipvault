# ClipVault 项目状态

> 新会话接续先读这个文件。它记录：项目是什么、做到哪了、下一步做什么。

## 项目

Windows 剪贴板历史管理器。常驻后台记录剪贴板文本，热键唤起浮层，输入过滤，回车粘回。

- 仓库：https://github.com/CoRadll/clipvault
- 本地路径：`C:\Users\72673\Desktop\clipvault`

## 技术栈

| 项 | 选择 |
|---|---|
| 语言 | C++20 |
| 编译器 | MSVC 19.34（VS 2022 17.4，装在 `D:\Visual studio`） |
| 构建 | CMake 4.2 + Ninja（VS 自带） |
| 依赖 | SQLite 3.54.1 amalgamation、Catch2 3.16.1 amalgamated，都 vendor 在 `external/` |
| 平台 | 仅 Windows |

## 目录结构

```
include/clipvault/
  core/              纯逻辑，禁止 include windows.h
  platform/win32/    Win32 实现
src/
  core/              与 include 同构
  platform/win32/
tests/               单元测试
external/            vendor 的第三方源码
docs/                本目录
.github/workflows/   CI 配置
```

## 构建

```
build.bat           构建
build.bat test      构建 + 跑测试
build.bat clean     清空 build 目录重新构建
```

`build.bat` 内部会调 vcvars，所以在任何终端里执行都行。

构建目标三个：`third_party_sqlite3`（静态库）、`clipvault_core`（静态库）、`clipvault` / `clipvault_tests`（可执行）。

CI：每次 push 到 main 会在 `windows-latest` 上构建并跑测试。

## 模块状态

| 模块 | 位置 | 状态 |
|---|---|---|
| textNormalize | core | 完成，11 个用例 |
| clipRecord | core | 完成（字段定义） |
| clipStore | core | 完成，9 个用例（insert / trimTo / recent） |
| mainWindow | platform/win32 | 窗口、拖动、圆角、双色背景完成 |
| clipboardReader | platform/win32 | 监听 + 读取 + 归一化 + 入库完成 |
| CI | `.github/workflows/ci.yml` | 构建 + 跑测试 |
| 收藏库 FavoriteStore | core | 未开始 |
| UI 列表显示 | platform/win32 | 未开始 |
| 全局热键 | platform/win32 | 未开始 |
| 托盘图标 | platform/win32 | 未开始 |

## 下一步

**M1 第 2 步：列表显示。** 把 `recent()` 取到的记录画到窗口里。

要做的事：

1. 在 `WM_PAINT` 的内容区里按行绘制每条记录（GDI 的 `DrawTextW` 或 `TextOutW`）
2. 一条一行，超宽截断加省略号
3. 行高固定；超出窗口高度时先不做滚动，只显示前几条
4. 数据来源 `ClipStore::recent(clipvault::core::kClipLimit)`，通过 `GWLP_USERDATA` 取到的 store 调用

验证标准：复制几条不同内容，窗口里能看到对应文字，最近的在最上面。

注意：`WM_PAINT` 可能被频繁触发（拖动、遮挡恢复），不要每次都查库——把 `recent()` 的结果缓存在文件级变量里，只在收到 `WM_CLIPBOARDUPDATE` 时刷新。

## 工作约定

- 每次会话结束后更新本文件的「模块状态」和「下一步」
- 出现新的架构决定时，在 `DECISIONS.md` 对应分类末尾追加一条

## 未决问题

- **数据库路径**：已定使用相对路径 `clips.db`（落在工作目录）。正式分发前要改成 exe 同目录（用 `GetModuleFileNameW`）或 `LOCALAPPDATA`
- **收藏的跨 Store 事务**：收藏动作要同时"插入 favorites + 标记 clips"，两个 Store 独立连接时无法原子。方案未定
- **窗口总在最前**（`WS_EX_TOPMOST`）会挡住其他窗口，调试时不方便

## 踩过的坑

1. 没调 vcvars 就构建 → `LNK1104 无法打开 kernel32.lib`
2. CMake 缓存被污染（`CMAKE_MAKE_PROGRAM-NOTFOUND`）→ 必须删掉整个 `build/` 重新配置
3. MSVC 默认不定义 `UNICODE` 宏 → `IDC_ARROW` 展开成 ANSI 版，与 `LoadCursorW` 类型不匹配
4. `switch` 的某个 `case` 里声明变量，必须用大括号包住，否则报 C2361
5. 构造函数抛异常时析构函数不执行，已获取的资源必须手动释放
6. VS Code 的 IntelliSense 会误报（`starts_with` 找不到、函数重载冲突），以 `cl` 编译结果为准
