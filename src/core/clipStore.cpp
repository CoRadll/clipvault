#include "clipvault/core/clipStore.hpp"

#include <sqlite3.h>

namespace clipvault::core
{
	ClipStore::ClipStore(const std::string& dbPath)
	{
		sqlite3_open(dbPath.c_str(), &db_);
	}

	ClipStore::~ClipStore()
	{
	}

	void ClipStore::insert(const std::string& text)
	{
		(void)text;
	}

	void ClipStore::trimTo(std::size_t limit)
	{
		(void)limit;
	}

	std::vector<ClipRecord> ClipStore::recent(std::size_t limit)
	{
		(void)limit;
		return {};
	}
}