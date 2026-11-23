#include "clipvault/core/clipStore.hpp"

#include <stdexcept>
#include <chrono>
#include <vector>

#include <sqlite3.h>

namespace clipvault::core
{
	ClipStore::ClipStore(const std::string& dbPath)
	{
		if(sqlite3_open(dbPath.c_str(), &db_) != SQLITE_OK)
		{
			std::string message = "open database failed: ";
			message += sqlite3_errmsg(db_);
			sqlite3_close(db_);
			db_ = nullptr;
			throw std::runtime_error(message);
		}

		const char* kCreateSql =
		"CREATE TABLE IF NOT EXISTS clips ("
		"    id           INTEGER PRIMARY KEY AUTOINCREMENT,"
		"    content      TEXT    NOT NULL UNIQUE,"
		"    byte_size    INTEGER NOT NULL,"
		"    created_at   INTEGER NOT NULL,"
		"    last_used_at INTEGER NOT NULL,"
		"    is_favorite  INTEGER NOT NULL DEFAULT 0"
		");"
		"CREATE INDEX IF NOT EXISTS idx_clips_last_used ON clips(last_used_at DESC);";

		char* errMsg = nullptr;
		if (sqlite3_exec(db_, kCreateSql, nullptr, nullptr, &errMsg) != SQLITE_OK)
		{
			std::string message = "create table failed: ";
			message += (errMsg != nullptr) ? errMsg : sqlite3_errmsg(db_);
			sqlite3_free(errMsg);
			sqlite3_close(db_);
			db_ = nullptr;
			throw std::runtime_error(message);
		}

	}

	ClipStore::~ClipStore()
	{
		if (db_ != nullptr)
		{
			sqlite3_close(db_);
			db_ = nullptr;
		}
	}

	void ClipStore::insert(const std::string& text)
	{
		const auto now = std::chrono::system_clock::now();
		const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
    		now.time_since_epoch()).count();

		const char* kInsertSql = 
			"INSERT INTO clips (content, byte_size, created_at, last_used_at, is_favorite)"
			"VALUES (?1, ?2, ?3, ?4, 0)"
			"ON CONFLICT(content) DO UPDATE SET last_used_at = excluded.last_used_at";

		
		sqlite3_stmt* stmt = nullptr;
		if (sqlite3_prepare_v2(db_, kInsertSql, -1, &stmt, nullptr) != SQLITE_OK)
			throw std::runtime_error(std::string("insert prepare failed: ") + sqlite3_errmsg(db_));
		
		sqlite3_bind_text(stmt, 1, text.c_str(), -1, SQLITE_TRANSIENT);
		sqlite3_bind_int64(stmt, 2, static_cast<sqlite3_int64>(text.size()));
		sqlite3_bind_int64(stmt, 3, ms);
		sqlite3_bind_int64(stmt, 4, ms);

		const int rc = sqlite3_step(stmt);
		sqlite3_finalize(stmt);

		if (rc != SQLITE_DONE)
			throw std::runtime_error(std::string("insert failed: ") + sqlite3_errmsg(db_));

	}

	void ClipStore::trimTo(std::size_t limit)
	{
		if(limit == 0)
			return;
		// 保留最近使用的 limit 条，其余删掉。
		// 子查询先选出"要保留的 id"，外层把不在这个集合里的行全删。
		const char* kTrimSql =
			"DELETE FROM clips WHERE id NOT IN ("
			"    SELECT id FROM clips ORDER BY last_used_at DESC LIMIT ?1"
			");";

		// 把 SQL 编译成可执行形式。?1 是占位符，后面单独绑值，
		// 不把数字拼进字符串，避免拼接注入和转义问题。
		sqlite3_stmt* stmt = nullptr;
		if (sqlite3_prepare_v2(db_, kTrimSql, -1, &stmt, nullptr) != SQLITE_OK)
		{
			throw std::runtime_error(std::string("trim prepare failed: ") + sqlite3_errmsg(db_));
		}

		// 绑定参数。SQLite 只接受 64 位有符号整数，
		// 而 limit 是 64 位无符号，所以显式转换。
		sqlite3_bind_int64(stmt, 1, static_cast<sqlite3_int64>(limit));

		// 执行。这条语句不返回结果行，正常结束时返回 SQLITE_DONE。
		const int rc = sqlite3_step(stmt);

		// 无论成败都要销毁语句对象，否则语句和它占的锁会一起泄漏。
		sqlite3_finalize(stmt);

		if (rc != SQLITE_DONE)
		{
			throw std::runtime_error(std::string("trim failed: ") + sqlite3_errmsg(db_));
		}
	}

	std::vector<ClipRecord> ClipStore::recent(std::size_t limit)
	{
		std::vector<ClipRecord> records;
		int rc = SQLITE_OK;
		const char* kRecentSql =
			"SELECT id, content, byte_size, created_at, last_used_at, is_favorite "
			"FROM clips "
			"ORDER BY last_used_at DESC "
			"LIMIT ?1";
		sqlite3_stmt* stmt = nullptr;
		if (sqlite3_prepare_v2(db_, kRecentSql, -1, &stmt, nullptr) != SQLITE_OK)
		{
			sqlite3_finalize(stmt);
			throw std::runtime_error(std::string("recent prepare failed: ") + sqlite3_errmsg(db_));
		}
		sqlite3_bind_int64(stmt, 1, static_cast<sqlite3_int64>(limit));
		while((rc = sqlite3_step(stmt)) == SQLITE_ROW)
		{
			ClipRecord record;
			record.id = sqlite3_column_int64(stmt, 0);
			const auto* rawText = sqlite3_column_text(stmt, 1);
			if(rawText != nullptr)
				record.content = reinterpret_cast<const char*>(rawText);
			record.byteSize = sqlite3_column_int64(stmt, 2);
			record.createdAt = sqlite3_column_int64(stmt, 3);
			record.lastUsedAt = sqlite3_column_int64(stmt, 4);
			record.isFavorite = sqlite3_column_int64(stmt, 5) != 0;
			records.push_back(std::move(record));
		}
		sqlite3_finalize(stmt);
		if (rc != SQLITE_DONE)
			throw std::runtime_error(std::string("recent failed: ") + sqlite3_errmsg(db_));
		return records;
	}
}