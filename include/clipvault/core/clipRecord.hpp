#pragma once

#include <string>
#include <cstdint>

namespace clipvault::core
{
    struct ClipRecord
    {
        std::int64_t id = 0;
        std::string content;
        std::int64_t byteSize = 0;
        std::int64_t createdAt = 0;
        std::int64_t lastUsedAt = 0;
        bool isFavorite = false;
    };
}