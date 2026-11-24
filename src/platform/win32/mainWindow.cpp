#include "clipvault/platform/win32/mainWindow.hpp"
#include "clipvault/platform/win32/clipboardReader.hpp"

#include <cstdio>

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

		// 内层圆角：比外层小一圈，视觉上才像同一个形状缩小
		constexpr int kInnerCornerRadius = 8;

		// 顶部这一段可以按住拖动窗口。
		// 它只存在于命中测试里，外观上看不出任何"标题栏"的分界。
		constexpr int kDragBarHeight = 32;

		// 外圈边框：淡蓝
		constexpr COLORREF kOuterColor = RGB(208, 228, 246);
		// 内层主体：带一点蓝调的柔和白，比纯白不刺眼，和淡蓝同色系
		constexpr COLORREF kContentColor = RGB(247, 249, 251);

		LRESULT CALLBACK windowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
		{
			switch (msg)
			{
			case WM_DESTROY:
				RemoveClipboardFormatListener(hwnd);
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
				HDC hdc = BeginPaint(hwnd, &ps);

				RECT clientRect{};
				GetClientRect(hwnd, &clientRect);

				// 整片先铺淡蓝，露出来的那一圈就是外框
				HBRUSH outerBrush = CreateSolidBrush(kOuterColor);
				FillRect(hdc, &clientRect, outerBrush);
				DeleteObject(outerBrush);

				// 内层主体：四周等距内缩的圆角矩形
				RECT innerRect{};
				innerRect.left = kMargin;
				innerRect.top = kMargin;
				innerRect.right = clientRect.right - kMargin;
				innerRect.bottom = clientRect.bottom - kMargin;

				HBRUSH contentBrush = CreateSolidBrush(kContentColor);
				HBRUSH oldBrush = static_cast<HBRUSH>(SelectObject(hdc, contentBrush));

				// 必须选一支空画笔进去，否则 RoundRect 会顺手描一圈黑色边框
				HPEN nullPen = CreatePen(PS_NULL, 0, RGB(0, 0, 0));
				HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, nullPen));

				RoundRect(hdc,
					innerRect.left, innerRect.top, innerRect.right, innerRect.bottom,
					kInnerCornerRadius, kInnerCornerRadius);

				SelectObject(hdc, oldBrush);
				SelectObject(hdc, oldPen);
				DeleteObject(contentBrush);
				DeleteObject(nullPen);

				EndPaint(hwnd, &ps);
				return 0;
			}

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

		// ---------- 第三步：把窗口裁成圆角矩形 ----------
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

		// ---------- 第四步：消息循环 ----------
		MSG msg{};
		while (GetMessageW(&msg, nullptr, 0, 0) > 0)
		{
			TranslateMessage(&msg);     // 把按键消息翻译成字符消息
			DispatchMessageW(&msg);     // 把消息分发给对应窗口的 windowProc
		}
	}
}
