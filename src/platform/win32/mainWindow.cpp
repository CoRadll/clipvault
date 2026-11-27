#include "clipvault/platform/win32/mainWindow.hpp"
#include "clipvault/platform/win32/clipboardReader.hpp"
#include "clipvault/core/clipStore.hpp"
#include "clipvault/core/textPreview.hpp"   // preparePreview：显示前的去首部空白 + 截断

#include <algorithm>
#include <cstdio>
#include <exception>
#include <string>
#include <vector>

#include <windows.h>
#include <windowsx.h>   // GET_X_LPARAM / GET_Y_LPARAM

namespace
{
	DWORD g_lastSequence = 0;
}
namespace clipvault::platform::win32
{
	namespace
	{
		constexpr wchar_t kClassName[] = L"ClipVaultMainWindow";
		constexpr int kWindowWidth = 256;
		constexpr int kWindowHeight = 320;

		// 外层圆角：CreateRoundRectRgn 要的是圆角椭圆的宽高（直径），16 等于半径 8
		constexpr int kCornerRadius = 16;

		// 内外两层之间的间距，四周相同
		constexpr int kMargin = 6;

		// 顶部这一段可以按住拖动窗口。
		// 它只存在于命中测试里，外观上看不出任何"标题栏"的分界。
		constexpr int kDragBarHeight = 32;

		// 窗口底色：淡蓝。整个窗口就铺这一层，没有内层主框了
		constexpr COLORREF kOuterColor = RGB(208, 228, 246);
		// 列表文字：比纯黑柔和的深灰，在白卡片上对比度足够
		constexpr COLORREF kTextColor = RGB(48, 52, 60);

		// 空状态提示文字：比正文浅，不抢眼
		constexpr COLORREF kHintColor = RGB(150, 158, 170);

		// 内容区相对窗口边缘内缩的量
		constexpr int kContentPadding = 8;

		// 相邻两条记录之间留的空隙。
		// 卡片会吃掉一部分，所以留 10，扣掉卡片上下外扩 3+3 之后还剩 4px 视觉间隙
		constexpr int kItemGap = 10;

		// 每条记录的白色圆角卡片，浮在淡蓝底上。
		//
		// GDI 的刷子只能实色填充，画不出真正的 alpha。
		// 但底色是纯色，所以"白色 90% 不透明"可以直接算成等效色：
		//   R = 0.9*255 + 0.1*208 = 250
		//   G = 0.9*255 + 0.1*228 = 252
		//   B = 0.9*255 + 0.1*246 = 254
		// 视觉上和真半透明一致。以后底色若变成渐变或图片，就得改用 AlphaBlend。
		constexpr COLORREF kCardColor = RGB(250, 252, 254);
		constexpr int kCardRadius = 8;

		// 鼠标悬停的那条卡片：比普通卡片深一档、偏蓝，和淡蓝底同色系。
		// 这是"这条可以点"的唯一提示
		constexpr COLORREF kCardHoverColor = RGB(234, 241, 251);

		// 鼠标按下的那条卡片：比悬停再深一档，给"已经按住了"的手感。
		// 注意不能比窗口底色更深，否则看起来像凹陷进了卡片背后的蓝底
		constexpr COLORREF kCardPressedColor = RGB(216, 229, 246);

		// 背景块相对文字矩形每边外扩的量
		constexpr int kCardPadding = 3;

		// 没有悬停时的下标
		constexpr std::size_t kNoHover = static_cast<std::size_t>(-1);

		// 每条记录最多保留的字节数，传给 core::preparePreview 由它截断。
		// 单条内容允许到 2 MB，原样留着的话每次重绘都要扫一遍全串，
		// 拖动窗口会明显掉帧；而两行的宽度最多也就几十个汉字，300 字节够用。
		// 截多少属于显示策略，所以常量留在窗口层，core 只收参数。
		constexpr std::size_t kPreviewBytes = 300;

		// 列表内容。只存处理好的宽字符，
		// 于是 WM_PAINT 里剩下的唯一一件事就是把它们画出来。
		std::vector<std::wstring> g_rows;

		// 鼠标当前悬停在第几条，kNoHover 表示没有。
		// 纯视觉状态，不参与任何逻辑 —— 点击是直接执行的，不需要"选中"这层
		std::size_t g_hoverIndex = kNoHover;

		// 鼠标按住在第几条。也是纯视觉，动作（写剪贴板）还没接
		std::size_t g_pressedIndex = kNoHover;

		// 是否已向系统登记"鼠标离开"通知。不登记的话收不到 WM_MOUSELEAVE，
		// 鼠标移出窗口后高亮会一直挂着
		bool g_mouseTracking = false;

		// 字体，以及从字体度量算出来的尺寸，创建窗口时算一次。
		HFONT g_font = nullptr;
		bool g_fontOwned = false;   // GetStockObject 拿到的系统字体不能删
		int g_lineHeight = 0;       // 一行文字占的高度，含字体自带行距
		int g_itemHeight = 0;       // 一条记录占的高度，固定两行

		// UTF-8 → UTF-16。
		// 显式传长度而不是 -1，免得函数把结尾的 '\0' 也算进去还要额外处理。
		std::wstring toWide(const std::string& utf8)
		{
			if (utf8.empty())
				return {};

			const int needed = MultiByteToWideChar(CP_UTF8, 0, utf8.data(),
				static_cast<int>(utf8.size()), nullptr, 0);
			if (needed <= 0)
				return {};

			std::wstring wide(static_cast<std::size_t>(needed), L'\0');
			MultiByteToWideChar(CP_UTF8, 0, utf8.data(),
				static_cast<int>(utf8.size()), wide.data(), needed);
			return wide;
		}

		// 把客户区纵坐标换算成"第几条记录"；落在列表外面返回 kNoHover。
		// 悬停和按下都要做同一件事，所以抽出来，避免两处算法不一致。
		std::size_t hitTestRow(int y)
		{
			const int contentTop = kMargin + kDragBarHeight;
			const int stride = g_itemHeight + kItemGap;
			if (stride <= 0 || y < contentTop)
				return kNoHover;

			// 卡片之间的间隙不会单独区分，落在间隙里算作上下相邻的那一条
			const std::size_t hit = static_cast<std::size_t>((y - contentTop) / stride);
			return hit < g_rows.size() ? hit : kNoHover;
		}

		// 重建列表缓存。只在两处调用：窗口起来时一次、每次剪贴板入库后一次。
		void rebuildRows(clipvault::core::ClipStore& store)
		{
			g_rows.clear();
			// 列表换了就不再有"悬停的那条"，否则下标会指向一条已经不存在的内容
			g_hoverIndex = kNoHover;
			g_pressedIndex = kNoHover;
			try
			{
				const auto records = store.recent(clipvault::core::kClipLimit);
				g_rows.reserve(records.size());
				for (const auto& record : records)
					g_rows.push_back(toWide(
						clipvault::core::preparePreview(record.content, kPreviewBytes)));
			}
			catch (const std::exception& e)
			{
				// 消息处理函数里不能让异常跑出去，穿过消息循环会直接终止进程
				std::printf("[list] rebuild failed: %s\n", e.what());
			}
		}

		LRESULT CALLBACK windowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
		{
			switch (msg)
			{
			case WM_DESTROY:
				RemoveClipboardFormatListener(hwnd);
				// 兜底：万一是在按住鼠标的状态下窗口被关掉，
				// 这里把捕获解掉，否则这份捕获会一直留着，别的窗口收不到鼠标
				ReleaseCapture();
				// 字体此时没被任何 DC 选着（绘制时换回过旧字体），可以安全删除。
				// GetStockObject 拿到的系统字体不归我们管，g_fontOwned 用来区分。
				if (g_fontOwned && g_font != nullptr)
				{
					DeleteObject(g_font);
				}
				g_font = nullptr;
				g_fontOwned = false;
				PostQuitMessage(0);
				return 0;

			case WM_KEYDOWN:
				if (wParam == VK_ESCAPE)
				{
					DestroyWindow(hwnd);
					return 0;
				}
				break;

			case WM_ERASEBKGND:
				// 背景在 WM_PAINT 里一次性画完，这里返回 1 告诉系统"别再擦一遍"，
				// 否则系统先刷一遍再画，中间会闪
				return 1;

			case WM_PAINT:
			{
				PAINTSTRUCT ps{};
				HDC screenDc = BeginPaint(hwnd, &ps);

				RECT clientRect{};
				GetClientRect(hwnd, &clientRect);

				// 双缓冲：所有绘制先画到内存位图，最后一次贴到屏幕上。
				// 单缓冲时每条卡片都是"先被淡蓝盖住、再重新画上去"，
				// 点击触发重绘的那一瞬间能看见卡片消失又出现，也就是"闪一下"
				HDC hdc = CreateCompatibleDC(screenDc);
				HBITMAP backBuffer = CreateCompatibleBitmap(screenDc,
					clientRect.right, clientRect.bottom);
				HBITMAP oldBackBuffer = static_cast<HBITMAP>(SelectObject(hdc, backBuffer));

				// 整片铺淡蓝，这就是窗口底色。
				// 不再画内层柔白主框 —— 白色交给每条记录的卡片，
				// 露出来的蓝就是卡片之间的间隙和四周留边
				HBRUSH outerBrush = CreateSolidBrush(kOuterColor);
				FillRect(hdc, &clientRect, outerBrush);
				DeleteObject(outerBrush);

				// ---------- 列表 ----------
				// 文字背景设为透明，否则每行会带一块矩形背景色，
				// 把卡片和圆角处露出的淡蓝盖成方块
				SetBkMode(hdc, TRANSPARENT);
				SetTextColor(hdc, kTextColor);
				HFONT previousFont = static_cast<HFONT>(SelectObject(hdc, g_font));

				const int contentLeft = kMargin + kContentPadding;
				const int contentRight = clientRect.right - kMargin - kContentPadding;
				const int contentTop = kMargin + kDragBarHeight;
				const int contentBottom = clientRect.bottom - kMargin - kContentPadding;

				// 只画能完整放下的条数，放不下的留给以后的滚动条
				std::size_t visible = 0;
				const int available = contentBottom - contentTop;
				const int stride = g_itemHeight + kItemGap;
				if (available > 0 && stride > 0)
					visible = static_cast<std::size_t>(available / stride);

				const std::size_t count = std::min(g_rows.size(), visible);

				// 一条都放不下（库是空的，或窗口小到连一条都容不下）：
				// 在内容区正中画一行灰字，否则整片淡蓝分不清"没数据"和"程序坏了"
				if (count == 0)
				{
					SetTextColor(hdc, kHintColor);

					RECT area{};
					area.left = contentLeft;
					area.top = contentTop;
					area.right = contentRight;
					area.bottom = contentBottom;
					DrawTextW(hdc, L"暂无记录", -1, &area,
						DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

					SelectObject(hdc, previousFont);

					// 一次性贴到屏幕，然后释放后备位图
					BitBlt(screenDc, 0, 0, clientRect.right, clientRect.bottom, hdc, 0, 0, SRCCOPY);
					SelectObject(hdc, oldBackBuffer);
					DeleteObject(backBuffer);
					DeleteDC(hdc);
					EndPaint(hwnd, &ps);
					return 0;
				}

				// 卡片用的三档刷子和空画笔：整轮复用，循环外各建一次。
				// 必须选一支 PS_NULL 画笔，否则每张卡片会被描一圈黑边。
				HBRUSH cardBrush = CreateSolidBrush(kCardColor);
				HBRUSH hoverBrush = CreateSolidBrush(kCardHoverColor);
				HBRUSH pressedBrush = CreateSolidBrush(kCardPressedColor);
				HPEN cardPen = CreatePen(PS_NULL, 0, RGB(0, 0, 0));
				HBRUSH previousBrush = static_cast<HBRUSH>(SelectObject(hdc, cardBrush));
				HPEN previousPen = static_cast<HPEN>(SelectObject(hdc, cardPen));

				for (std::size_t i = 0; i < count; ++i)
				{
					// DrawTextW 第 4 个参数是非 const 的 LPRECT，
					// 必须传一个能取地址的局部变量（对临时对象取地址不合法）
					RECT row{};
					row.left = contentLeft;
					row.top = contentTop + static_cast<int>(i) * stride;
					row.right = contentRight;
					row.bottom = row.top + g_itemHeight;

					// 先铺卡片，再画文字。顺序反了卡片会盖住刚画好的字。
					// 卡片比文字矩形四周各外扩 kCardPadding，文字才有内边距感。
					// 按下优先于悬停：按住时鼠标一定停在这一条上，该显示更深的那档
					HBRUSH brush = cardBrush;
					if (i == g_pressedIndex)
						brush = pressedBrush;
					else if (i == g_hoverIndex)
						brush = hoverBrush;
					SelectObject(hdc, brush);

					RECT block{};
					block.left = row.left - kCardPadding;
					block.top = row.top - kCardPadding;
					block.right = row.right + kCardPadding;
					block.bottom = row.bottom + kCardPadding;
					RoundRect(hdc,
						block.left, block.top, block.right, block.bottom,
						kCardRadius, kCardRadius);

					// DT_WORDBREAK 在给定高度内自动折行，超出的行直接被裁掉；
					// DT_NOPREFIX 让 '&' 原样显示，否则会被当成加速键前缀吃掉
					DrawTextW(hdc, g_rows[i].c_str(), -1, &row,
						DT_LEFT | DT_TOP | DT_WORDBREAK | DT_NOPREFIX);
				}

				SelectObject(hdc, previousBrush);
				SelectObject(hdc, previousPen);
				DeleteObject(cardBrush);
				DeleteObject(hoverBrush);
				DeleteObject(pressedBrush);
				DeleteObject(cardPen);

				SelectObject(hdc, previousFont);

				// 一次性贴到屏幕，然后释放后备位图。
				// 必须先换回原来的位图再 DeleteObject，否则位图还选在 DC 里删不掉
				BitBlt(screenDc, 0, 0, clientRect.right, clientRect.bottom, hdc, 0, 0, SRCCOPY);
				SelectObject(hdc, oldBackBuffer);
				DeleteObject(backBuffer);
				DeleteDC(hdc);

				EndPaint(hwnd, &ps);
				return 0;
			}

			case WM_MOUSEMOVE:
			{
				// 鼠标按着时不更新悬停。这种状态有两种来源：
				//   1. 正在拖窗口 —— 按住顶部 HTCAPTION 区，系统进入自己的移动循环，
				//      期间照样往窗口送 WM_MOUSEMOVE。窗口在跟着鼠标跑，
				//      鼠标相对窗口的位置一直在变，不挡住的话高亮会跟着乱跳
				//   2. 在卡片上按住拖动 —— 那时该显示的是按下态，不需要悬停
				if (wParam & MK_LBUTTON)
					return 0;

				// 第一次移动时向系统登记"鼠标离开"通知。
				// 不登记就收不到 WM_MOUSELEAVE，鼠标移出窗口后高亮会一直挂着。
				// 登记是一次性的，触发 WM_MOUSELEAVE 之后要重新登记
				if (!g_mouseTracking)
				{
					TRACKMOUSEEVENT track{};
					track.cbSize = sizeof(track);
					track.dwFlags = TME_LEAVE;
					track.hwndTrack = hwnd;
					TrackMouseEvent(&track);
					g_mouseTracking = true;
				}

				// 把鼠标纵坐标换算成第几条。
				// 卡片之间的 4px 间隙也算进上下相邻的那条，肉眼分辨不出来
				const std::size_t index = hitTestRow(GET_Y_LPARAM(lParam));

				// 只在真的换了条目时重绘，鼠标每动一像素都刷屏没必要
				if (index != g_hoverIndex)
				{
					g_hoverIndex = index;
					InvalidateRect(hwnd, nullptr, FALSE);
				}
				return 0;
			}

			case WM_MOUSELEAVE:
				g_mouseTracking = false;
				if (g_hoverIndex != kNoHover)
				{
					g_hoverIndex = kNoHover;
					InvalidateRect(hwnd, nullptr, FALSE);
				}
				return 0;

			case WM_LBUTTONDOWN:
			{
				// 目前只做视觉：按下时卡片变色。
				// 真正"把内容写回剪贴板"要等后续步骤，这里刻意不做
				const std::size_t index = hitTestRow(GET_Y_LPARAM(lParam));
				if (index != g_pressedIndex)
				{
					g_pressedIndex = index;
					InvalidateRect(hwnd, nullptr, FALSE);
				}

				// 跟系统说"我要按住东西了"：从此不管鼠标跑到哪，
				// 消息都送回这个窗口。不这么做的话，按下后把鼠标拖出窗口再松开，
				// 窗口收不到"松开"，那条卡片会一直显示成按下的样子
				SetCapture(hwnd);
				return 0;
			}

			case WM_LBUTTONUP:
				if (g_pressedIndex != kNoHover)
				{
					g_pressedIndex = kNoHover;
					InvalidateRect(hwnd, nullptr, FALSE);
				}
				// 和上面的 SetCapture 配对。没有捕获时调用也无害
				ReleaseCapture();
				return 0;

			case WM_NCHITTEST:
			{
				// 无边框窗口没有边框可以拖，所以不需要问 DefWindowProc 的默认答案。
				// 而且它对设置了 region 的窗口有可能返回 HTNOWHERE，
				// 那样整个窗口都收不到鼠标消息，表现就是"点不动"。
				POINT pt{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
				ScreenToClient(hwnd, &pt);

				// 顶部一段返回 HTCAPTION，等于告诉系统"这里是标题栏"，
				// 按住拖动、光标变化全部由系统接管。这段在外观上没有痕迹。
				if (pt.y >= 0 && pt.y < kMargin + kDragBarHeight)
				{
					return HTCAPTION;
				}
				return HTCLIENT;
			}

			case WM_NCLBUTTONDBLCLK:
				// 双击标题栏默认会最大化，浮层不需要，直接吞掉
				if (wParam == HTCAPTION)
				{
					return 0;
				}
				break;

			case WM_CLIPBOARDUPDATE:
			{
				auto* store = reinterpret_cast<clipvault::core::ClipStore*>(
    				GetWindowLongPtrW(hwnd, GWLP_USERDATA));
				if(store == nullptr)
					return 0;
				const DWORD seq = GetClipboardSequenceNumber();
				if (seq == g_lastSequence)
					return 0;
				g_lastSequence = seq;
				if (const auto text = readClipboardText())
				{
					handleClipboardUpdate(*text, *store);
					// 入库后重建缓存再请求重绘。
					// InvalidateRect 第三参传 FALSE：背景自己画，
					// 让系统先擦一遍的话中间会闪一下
					rebuildRows(*store);
					InvalidateRect(hwnd, nullptr, FALSE);
				}
				else
				{
					std::printf("[notify] no text\n");
				}
				return 0; //剪切板变化时的动作
			}
				
			default:
				break;
			}

			return DefWindowProcW(hwnd, msg, wParam, lParam);
		}
	}

	void runMainWindow(clipvault::core::ClipStore& store)
	{
		// 取当前进程实例句柄，注册窗口类和创建窗口都要它
		HINSTANCE instance = GetModuleHandleW(nullptr);

		// ---------- 第一步：注册窗口类 ----------
		// 这一步不创建任何窗口，只是向系统声明"我要一种叫这个名字、长这样的窗口"
		WNDCLASSW wc{};
		wc.lpfnWndProc = windowProc;                      // 消息送到哪个函数
		wc.hInstance = instance;
		wc.lpszClassName = kClassName;                    // 类名，创建窗口时按它查找
		wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
		// 留空：背景由 WM_PAINT 里的 FillRect 自己画，不用系统的刷子
		wc.hbrBackground = nullptr;

		if (RegisterClassW(&wc) == 0)
		{
			// 注册失败没有异常可捕，只能查返回值；最常见原因是类名已经被注册过
			return;
		}

		// ---------- 第二步：创建窗口 ----------
		const int x = (GetSystemMetrics(SM_CXSCREEN) - kWindowWidth) / 2;
		const int y = (GetSystemMetrics(SM_CYSCREEN) - kWindowHeight) / 2;

		HWND hwnd = CreateWindowExW(
			WS_EX_TOPMOST,      // 扩展样式：总在最前
			kClassName,         // 按上面注册的类名创建
			L"clipvault",       // 标题（外观上看不见，任务栏和调试工具里能读到）
			WS_POPUP,           // 基础样式：不要标题栏和边框，这就是"无边框"的来源
			x, y, kWindowWidth, kWindowHeight,
			nullptr, nullptr, instance, nullptr);

		if (hwnd == nullptr)
		{
			UnregisterClassW(kClassName, instance);
			return;
		}
		SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&store));
		AddClipboardFormatListener(hwnd); //登记，剪贴板有变化则进行通知

		// ---------- 第三步：建字体，量行高 ----------
		// 整个进程建一次，绝不放在 WM_PAINT 里：
		// 拖动窗口时每秒几十次重绘，GDI 句柄会迅速耗尽。
		g_font = CreateFontW(
			-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
			DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
			CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
			L"Microsoft YaHei UI");
		if (g_font == nullptr)
		{
			// 字体名找不到时 CreateFontW 一般会给替代字体，真失败了就退回系统字体
			g_font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
			g_fontOwned = false;
		}
		else
		{
			g_fontOwned = true;
		}

		// 行高不能拍脑袋：DrawTextW 多行模式的行距由字体自身的度量决定，
		// 外部改不了。所以先量出单行高度，再按"一条记录两行"算出条目高度。
		{
			HDC screen = GetDC(nullptr);
			HFONT oldFont = static_cast<HFONT>(SelectObject(screen, g_font));
			TEXTMETRICW metrics{};
			GetTextMetricsW(screen, &metrics);
			SelectObject(screen, oldFont);
			ReleaseDC(nullptr, screen);

			g_lineHeight = metrics.tmHeight + metrics.tmExternalLeading;
			g_itemHeight = g_lineHeight * 2;
		}

		// 先把库里已有的记录读进来。必须在 UpdateWindow 之前：
		// UpdateWindow 会立刻发一次 WM_PAINT，缓存没准备好首帧就是空的
		rebuildRows(store);

		// ---------- 第四步：把窗口裁成圆角矩形 ----------
		// 前四个参数是外接矩形；右边界和下边界是"排他"的，
		// 传 kWindowWidth 只能得到 kWindowWidth-1 宽，所以都要 +1 才是完整尺寸。
		// 后两个是圆角椭圆的宽高（直径），不是半径。
		HRGN region = CreateRoundRectRgn(
			0, 0, kWindowWidth + 1, kWindowHeight + 1, kCornerRadius, kCornerRadius);
		if (region != nullptr)
		{
			// 成功后这个 HRGN 归窗口系统管理，绝对不能再 DeleteObject，
			// 否则系统之后用到已释放的句柄会出错。只有失败时才自己释放。
			if (SetWindowRgn(hwnd, region, TRUE) == 0)
			{
				DeleteObject(region);
			}
		}

		ShowWindow(hwnd, SW_SHOW);
		UpdateWindow(hwnd);         // 立刻触发一次 WM_PAINT，避免首帧空白

		// ---------- 第五步：消息循环 ----------
		MSG msg{};
		while (GetMessageW(&msg, nullptr, 0, 0) > 0)
		{
			TranslateMessage(&msg);     // 把按键消息翻译成字符消息
			DispatchMessageW(&msg);     // 把消息分发给对应窗口的 windowProc
		}
	}
}
