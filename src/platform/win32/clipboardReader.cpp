#include "clipvault/platform/win32/clipboardReader.hpp"

#include <cstdio>

#include <windows.h>

namespace clipvault::platform::win32
{
    std::optional<std::string> readClipboardText()
    {
        int tryTime = 0;
        while(!OpenClipboard(nullptr))
        {
            Sleep(10);
            tryTime++;
            if(tryTime >= 5)
                return std::nullopt;
        }

        HANDLE handle = GetClipboardData(CF_UNICODETEXT);
        if(handle == nullptr)
        {
            CloseClipboard();
            return std::nullopt;
        }
            
        const wchar_t* wide = static_cast<const wchar_t*>(GlobalLock(handle));
        if(wide == nullptr)
        {
            CloseClipboard();
            return std::nullopt;
        }

        int byteCount = WideCharToMultiByte(CP_UTF8, 0, wide, -1, nullptr, 0, nullptr, nullptr);
        if(byteCount <= 1)
        {
            GlobalUnlock(handle);
            CloseClipboard();
            return std::nullopt;
        }
        std::string utf8(static_cast<std::size_t>(byteCount), '\0');
        WideCharToMultiByte(CP_UTF8, 0, wide, -1, utf8.data(), byteCount, nullptr, nullptr);
        utf8.resize(static_cast<std::size_t>(byteCount - 1));

        GlobalUnlock(handle);
        CloseClipboard();
        return utf8;
    }

    void handleClipboardUpdate(const std::string& text)
	{
		std::printf("[clip] %zu bytes: %s\n", text.size(), text.c_str());
		std::fflush(stdout);
	}
}