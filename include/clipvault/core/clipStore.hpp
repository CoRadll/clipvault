#pragma once

#include "clipvault/core/clipRecord.hpp"

#include <string>
#include <vector>
#include <cstddef>

struct sqlite3;

namespace clipvault::core
{
    inline constexpr std::size_t kClipLimit = 100;
    class ClipStore
    {
    private:
        sqlite3* db_ = nullptr;

    public:
        explicit ClipStore(const std::string& dbPath);
        ~ClipStore();
        ClipStore(const ClipStore&) = delete;
		ClipStore& operator=(const ClipStore&) = delete;

        void insert(const std::string& text);
        void trimTo(std::size_t limit);
        std::vector<ClipRecord> recent(std::size_t limit);
    };
}