# ClipVault 项目状态

> 新会话接续先读这个文件。它记录：项目是什么、做到哪了、下一步做什么。

## 项目

Windows 剪贴板历史管理器。常驻后台记录剪贴板文本，热键唤起浮层，输入过滤，回车粘回。

- 仓库：https://github.com/CoRadll/clipvault
- 本地路径：`apps/clipvault/`（在 personal-project 工作区内）

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

构建目标四个：`third_party_sqlite3`、`clipvault_core`（静态库），`clipvault`、`clipvault_tests`（可执行）。

CI：每次 push 到 main 会在 `windows-latest` 上构建并跑测试。

## 模块状态

| 模块 | 位置 | 状态 |
|---|---|---|
| textNormalize | core | 完成，11 个用例 |
| textPreview | core | 完成，11 个用例（去首部空白 + UTF-8 边界截断） |
| clipRecord | core | 完成（字段定义） |
| clipStore | core | 完成，9 个用例（insert / trimTo / recent） |
| mainWindow | platform/win32 | 窗口、拖动、圆角、双色背景完成 |
| clipboardReader | platform/win32 | 监听 + 读取 + 归一化 + 入库完成 |
| CI | `.github/workflows/ci.yml` | 构建 + 跑测试 |
| 收藏库 FavoriteStore | core | 未开始 |
| UI 列表显示 | platform/win32 | 完成：卡片列表、空状态、悬停/按下反馈、双缓冲 |
| 全局热键 | platform/win32 | 未开始 |
| 托盘图标 | platform/win32 | 未开始 |

## 下一步

**点击写回剪贴板。** 悬停/按下反馈和鼠标捕获都已就位（`SetCapture` / `ReleaseCapture` 在按下、松开里配对），只差松开时把那条内容写回剪贴板。

有个必须先解决的问题：`g_rows` 里存的是**截断过的预览宽字符**，不能拿来写回剪贴板。要拿原文，`ClipStore` 得补一个「按 id 取一条」的接口（现在只有 `recent()`），并且写回时把 `last_used_at` 更新成当前时间，让这条沉到最上面。

**滚动**：窗口只能放 6 条，超出的看不到。

**时间小字**：条目角落的浅灰 10 px 字（`刚刚` / `3分钟前`），方案已定、未实现。

再往后：关键词过滤、全局热键、托盘图标。

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
7. `std::min` / `std::max` 撞上 windows.h 的 `min`/`max` 宏 → C2589，`CMakeLists.txt` 给 target 加 `NOMINMAX`
8. 改了源文件但 ninja 不重编 → 现象是「改完代码、构建显示成功、跑起来还是旧行为」。原因是编辑工具改了内容却没更新 mtime，ninja 只看 mtime，认为 `.obj` 比源文件新就跳过。处理：删掉对应的 `.obj` 再构建（实测比 `build.bat clean` 快得多），确认时看构建日志里有没有 `Building CXX object ...` 那一行
9. 点击条目时卡片「闪一下」→ 单缓冲直接往窗口 DC 上画，顺序是「整片铺底色 → 再画卡片」，两步之间卡片处于消失状态。处理：`WM_PAINT` 里建内存 DC + 兼容位图，全部画完再 `BitBlt` 一次性贴屏。注意 `DeleteObject(位图)` 之前必须先 `SelectObject` 换回原位图
10. 鼠标拖出窗口再松开，按下态会一直挂着 → 没有捕获时窗口收不到 `WM_LBUTTONUP`。想用 `WM_MOUSEMOVE` 里查 `MK_LBUTTON` 打补丁没用，因为鼠标在窗口外时根本收不到移动消息。正确做法是按下时 `SetCapture`、松开时 `ReleaseCapture`，`WM_DESTROY` 里再兜底释放一次
11. 拖动窗口时列表高亮乱跳 → 按住顶部拖窗口是系统自己的移动循环，但期间**照样往窗口送 `WM_MOUSEMOVE`**；窗口跟着鼠标跑，鼠标相对窗口的位置一直在变，换算出来的条目也跟着变。处理：`WM_MOUSEMOVE` 开头加 `if (wParam & MK_LBUTTON) return 0;`，按住时不更新悬停。代价是松手后要再移动一次鼠标，高亮才跟上来
