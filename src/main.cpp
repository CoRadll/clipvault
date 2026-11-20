#include "clipvault/platform/win32/mainWindow.hpp"

#include <windows.h>

int main()
{
	SetConsoleOutputCP(CP_UTF8);
	// 用 main 而不是 WinMain：控制台子系统会保留一个黑框，调试时能往里打日志；
	// 窗口本身不关心入口函数叫什么名字。
	clipvault::platform::win32::runMainWindow();
	return 0;
}