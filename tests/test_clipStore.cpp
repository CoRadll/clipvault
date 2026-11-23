#include <catch_amalgamated.hpp>

#include <chrono>
#include <string>
#include <thread>

#include "clipvault/core/clipStore.hpp"

namespace
{
	// 每个用例各开一个内存库，互相隔离，也不在磁盘上留文件。
	constexpr const char* kMemoryDb = ":memory:";

	// 两次插入之间隔一小会儿，避免落在同一毫秒里，
	// 否则时间戳相同，"时间戳变大"这类断言就不可靠了。
	void tick()
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(3));
	}
}

TEST_CASE("空库查询返回空列表")
{
	clipvault::core::ClipStore store(kMemoryDb);

	const auto records = store.recent(10);
	REQUIRE(records.empty());
}

TEST_CASE("插入一条后能原样查回来")
{
	clipvault::core::ClipStore store(kMemoryDb);
	store.insert("hello world");

	const auto records = store.recent(10);
	REQUIRE(records.size() == 1);

	const auto& rec = records[0];
	REQUIRE(rec.content == "hello world");
	REQUIRE(rec.byteSize == 11);
	REQUIRE(rec.createdAt > 0);
	REQUIRE(rec.lastUsedAt == rec.createdAt);   // 刚插入时两个时间相同
	REQUIRE(rec.isFavorite == false);
}

TEST_CASE("重复内容不新增记录，只更新时间戳")
{
	clipvault::core::ClipStore store(kMemoryDb);

	store.insert("same text");
	const auto first = store.recent(10);
	REQUIRE(first.size() == 1);

	tick();
	store.insert("same text");
	const auto second = store.recent(10);

	REQUIRE(second.size() == 1);                            // 没有变成两条
	REQUIRE(second[0].id == first[0].id);                   // 还是同一条记录
	REQUIRE(second[0].lastUsedAt > first[0].lastUsedAt);    // 最近使用时间变新了
	REQUIRE(second[0].createdAt == first[0].createdAt);     // 创建时间保持不变
}

TEST_CASE("recent 的 limit 生效")
{
	clipvault::core::ClipStore store(kMemoryDb);
	for (int i = 0; i < 5; ++i)
	{
		store.insert("item " + std::to_string(i));
	}

	REQUIRE(store.recent(3).size() == 3);
	REQUIRE(store.recent(10).size() == 5);
	REQUIRE(store.recent(0).empty());
}

TEST_CASE("recent 按最近使用时间倒序返回")
{
	clipvault::core::ClipStore store(kMemoryDb);
	for (int i = 0; i < 3; ++i)
	{
		store.insert("item " + std::to_string(i));
		tick();
	}

	const auto records = store.recent(10);
	REQUIRE(records.size() == 3);
	REQUIRE(records[0].content == "item 2");   // 最后插入的在最前
	REQUIRE(records[1].content == "item 1");
	REQUIRE(records[2].content == "item 0");
}

TEST_CASE("trimTo 只保留最近使用的若干条")
{
	clipvault::core::ClipStore store(kMemoryDb);
	for (int i = 0; i < 5; ++i)
	{
		store.insert("item " + std::to_string(i));
		tick();
	}

	store.trimTo(3);
	const auto records = store.recent(10);

	REQUIRE(records.size() == 3);
	REQUIRE(records[0].content == "item 4");
	REQUIRE(records[1].content == "item 3");
	REQUIRE(records[2].content == "item 2");
}

TEST_CASE("trimTo 的条数大于实际数量时不删除任何数据")
{
	clipvault::core::ClipStore store(kMemoryDb);
	store.insert("only one");

	store.trimTo(100);
	REQUIRE(store.recent(10).size() == 1);
}

TEST_CASE("trimTo 传 0 时直接返回，不清空数据")
{
	clipvault::core::ClipStore store(kMemoryDb);
	store.insert("keep me");

	store.trimTo(0);
	REQUIRE(store.recent(10).size() == 1);
}

TEST_CASE("中文内容的字节数按 UTF-8 计算")
{
	clipvault::core::ClipStore store(kMemoryDb);
	store.insert("你好");   // 两个汉字，UTF-8 下一个占 3 字节

	const auto records = store.recent(10);
	REQUIRE(records.size() == 1);
	REQUIRE(records[0].content == "你好");
	REQUIRE(records[0].byteSize == 6);
}
