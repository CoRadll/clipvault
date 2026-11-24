#pragma once

#include <string>
#include <optional>

namespace clipvault::core
{
	class ClipStore;
}

namespace clipvault::platform::win32
{
	std::optional<std::string> readClipboardText();
	void handleClipboardUpdate(const std::string& text, clipvault::core::ClipStore& store);
}