#include <cstdio>
#include <concepts>
#include <sqlite3.h>

template <std::integral T>
T twice(T v)
{
    return v*2;
}

int main()
{
    #if defined(_MSC_VER)
	std::printf("compiler    : MSVC %d\n", _MSC_VER);
    #else
	std::printf("compiler    : NOT MSVC\n");
    #endif
	std::printf("__cplusplus : %ld\n", static_cast<long>(__cplusplus));
	std::printf("twice(21)   : %d\n", twice(21));
    sqlite3* db = nullptr;
	if (sqlite3_open(":memory:", &db) != SQLITE_OK)
	{
		std::printf("sqlite open failed\n");
		return 1;
	}

	sqlite3_exec(db, "CREATE TABLE t(v INTEGER);", nullptr, nullptr, nullptr);
	sqlite3_exec(db, "INSERT INTO t VALUES(42);", nullptr, nullptr, nullptr);

	sqlite3_stmt* stmt = nullptr;
	sqlite3_prepare_v2(db, "SELECT v FROM t;", -1, &stmt, nullptr);
	if (sqlite3_step(stmt) == SQLITE_ROW)
	{
		std::printf("sqlite       : %d, %s\n", sqlite3_column_int(stmt, 0), sqlite3_libversion());
	}
	sqlite3_finalize(stmt);
	sqlite3_close(db);
    return 0;
}
