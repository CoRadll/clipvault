#pragma once

#include <string>
#include <optional>

namespace clipvault::platform::win32
{
	std::optional<std::string> readClipboardText();
	void handleClipboardUpdate(const std::string& text);
}