#pragma once

namespace clipvault::platform::win32
{
	// 创建主窗口并进入消息循环。
	// 该函数会阻塞，直到窗口关闭；调用方收到返回即可退出进程。
	void runMainWindow();
}