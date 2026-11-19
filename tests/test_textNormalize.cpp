#include <catch_amalgamated.hpp>

#include "clipvault/core/textNormalize.hpp"

TEST_CASE("CRLF 统一成 LF")
{
	REQUIRE(clipvault::core::normalize("a\r\nb") == "a\nb");
}

TEST_CASE("尾部空白去掉")
{
	REQUIRE(clipvault::core::normalize("abc   ") == "abc");
}

TEST_CASE("首部空白保留")
{
	REQUIRE(clipvault::core::normalize("  abc") == "  abc");
}

TEST_CASE("全空白")
{
	REQUIRE(clipvault::core::normalize("") == "");
}

TEST_CASE("全换行符")
{
	REQUIRE(clipvault::core::normalize("\r\n") == "");
}

TEST_CASE("其他")
{
	REQUIRE(clipvault::core::normalize("a\r\n\r\nb") == "a\n\nb");
}

TEST_CASE("剥掉 BOM")
{
	REQUIRE(clipvault::core::normalize("\xEF\xBB\xBF" "abc") == "abc");
}

TEST_CASE("末尾孤立回车")
{
	REQUIRE(clipvault::core::normalize("abc\r") == "abc");
}

TEST_CASE("中间空格保留")
{
	REQUIRE(clipvault::core::normalize("a b") == "a b");
}

TEST_CASE("纯空格")
{
	REQUIRE(clipvault::core::normalize("   ") == "");
}

TEST_CASE("大小边界")
{
	REQUIRE(clipvault::core::normalize(std::string(2u * 1024u * 1024u, 'a')).empty());
	REQUIRE_FALSE(clipvault::core::normalize(std::string(2u * 1024u * 1024u - 1u, 'a')).empty());
}