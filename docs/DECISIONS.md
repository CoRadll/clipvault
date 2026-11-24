# 决策记录

> 每条记录一个已经拍板的决定、理由、以及代价。
> 新会话不要重新讨论这些，除非出现了新证据。
>
> 编号规则：前缀表示类别，序号在类内独立递增，新增只在所属类的末尾追加。
> `B` 构建 / `A` 架构 / `D` 数据 / `U` 界面 / `R` 运行时行为

## 工具链与构建

### B1. CMake + MSVC + Ninja，不用 MinGW

剪贴板、窗口、渲染都依赖官方 Windows SDK，MinGW-w64 对这部分的支持残缺。代价是需要 vcvars 环境，所以用 `build.bat` 封装。

### B2. 依赖 vendor 进仓库

SQLite 用 amalgamation，Catch2 用 amalgamated。理由是 clone 即可构建、不联网、版本由 git 锁定。代价是 `sqlite3.c` 有 9.5 MB 进仓库，首次编译较慢。

### B3. CMakePresets 里不写死本机路径

`CMAKE_MAKE_PROGRAM` 已移除，ninja 通过 PATH 查找。绝对路径会让 CI 和别人的机器 configure 失败。

### B4. C++20 + `/utf-8` + `/Zc:__cplusplus`

`/Zc:__cplusplus` 让 `__cplusplus` 报告真实值（否则永远是 199711）；`/utf-8` 让 MSVC 按 UTF-8 读源码，避免中文注释触发 C4819。

## 架构

### A1. core 与 platform 分层

`core` 不 include `windows.h`，`platform` 依赖 `core`，反向不允许。理由是 core 要能写纯逻辑单元测试，不需要窗口和剪贴板。

### A2. 头文件用前置声明隔离第三方 C API

`clipStore.hpp` 里写 `struct sqlite3;` 而不是 include `sqlite3.h`。理由是避免 C API 沿 include 链扩散到 UI 层。

### A3. 失败用异常表达，不用错误码

存储失败基本无法补救，静默失败最糟。异常能带上 `sqlite3_errmsg` 的具体原因。约定：异常只用于真错误，"内容重复"是正常分支，不算异常。

### A4. 不抽 `IStorage` 那样的纯虚接口

接口的价值是"支持多种实现"，本项目只有 SQLite 一种。替代方案是具体类 `ClipStore` + 前置声明隐藏实现。

### A5. `ClipRecord` 与 `ClipStore` 分离

`ClipRecord` 是纯数据（无方法），`ClipStore` 管存取。理由是 UI 拿 `ClipRecord` 渲染时不该被迫链接 sqlite3。

### A6. 不用单例

store 天生可能多个（clips / favorites / 测试用的内存库），单例会挡路；且单例隐藏依赖、难以替换。方案：`main` 创建、传引用、挂窗口 `GWLP_USERDATA`。

## 数据设计

### D1. 只做纯文本

去掉 `kind` 字段。2 MB 上限对图片没有意义，引入图片会带来大量格式处理和 bug。

### D2. 不记录使用次数

排序只按 `last_used_at`。首次插入时 `last_used_at` 等于 `created_at`。

### D3. `clips` 表字段

`id` / `content` / `byte_size` / `created_at` / `last_used_at` / `is_favorite`

### D4. 收藏用独立表，同一个 db 文件

`clips` 上限 100 条，`favorites` 上限 200 条，`favorites` 没有 `is_favorite` 列（进了这张表就意味着已收藏）。同一文件便于事务，两张表便于分别处理。

### D5. 时间戳用 Unix 毫秒，类型 `std::int64_t`

Windows 上 `long` 是 32 位，跨平台必须用定宽类型。秒级时间戳的 2038 问题在毫秒精度下会更早出现。

## 界面

### U1. 无边框圆角窗口

`WS_POPUP` + `WS_EX_TOPMOST` + `SetWindowRgn(CreateRoundRectRgn(...))`。已知局限：`SetWindowRgn` 是 1 位裁剪，边缘有锯齿，要平滑必须换分层窗口 + 自绘。

### U2. 标题栏只存在于命中测试

`WM_NCHITTEST` 在顶部区域返回 `HTCAPTION`，视觉上没有任何标题栏痕迹。拖动由系统处理，不用自己算位移。

### U3. 视觉方案：圆角矩形套圆角矩形

外层淡蓝 `RGB(208,228,246)`，内层柔白 `RGB(247,249,251)`，四周等距 6 px，外层圆角直径 16、内层 8。

## 运行时行为

### R1. 剪贴板用事件通知，不轮询

`AddClipboardFormatListener` + `WM_CLIPBOARDUPDATE`。平时零开销。

### R2. 两层去重

- **platform 层**：用 `GetClipboardSequenceNumber` 过滤同一事件的重复通知
- **core 层**：内容相同则更新时间戳而不是新增（已实现，靠 `ON CONFLICT` + `excluded`）

注意：富文本来源（如 QQ）会分多步写剪贴板，产生多次真实变化。序列号去重挡不住，必须靠内容去重。

### R3. 不做自动粘贴，点击条目只写入剪贴板

用户点条目后把内容写入剪贴板，窗口保持不动、焦点不变，由用户自己切到目标位置粘贴。

理由：自动粘贴要求"先选好目标位置 → 再唤出窗口 → 点条目"，把操作顺序绑死了；而"先复制、什么时候粘随你"更灵活，可控性更强。省掉的还有一整套高风险代码：记录原前台窗口、`SetForegroundWindow` 还原焦点、`SendInput` 模拟按键、管理员权限程序收不到按键的兼容问题，以及"隐藏后等多久才发按键"这种只能靠实测调参的经验值。
