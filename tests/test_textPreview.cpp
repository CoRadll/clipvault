#include <catch_amalgamated.hpp>

#include "clipvault/core/textPreview.hpp"

// 这些用例默认不跑：[.] 是 Catch2 的隐藏标签，因为 preparePreview 现在还是空骨架。
// 实现完成后把每个 TEST_CASE 里的 "[.]" 删掉，它们就会进入 build.bat test。
//
// 用例本身就是规格 —— 全部通过，说明去首部空白和截断的规则都满足了。

TEST_CASE("预览：空输入", "[.]")
{
	REQUIRE(clipvault::core::preparePreview("", 300).empty());
}

TEST_CASE("预览：首部空白去掉", "[.]")
{
	REQUIRE(clipvault::core::preparePreview("  \r\n\t abc", 300) == "abc");
}

TEST_CASE("预览：全是空白时结果是空串", "[.]")
{
	REQUIRE(clipvault::core::preparePreview(" \r\n\t  ", 300).empty());
}

TEST_CASE("预览：内部空白原样保留", "[.]")
{
	// 不压平：换行、制表符、连续空格都按原文留着
	REQUIRE(clipvault::core::preparePreview("a\n\nb\tc  d", 300) == "a\n\nb\tc  d");
}

TEST_CASE("预览：短于上限原样返回", "[.]")
{
	REQUIRE(clipvault::core::preparePreview("hello world", 300) == "hello world");
}

TEST_CASE("预览：正好等于上限原样返回", "[.]")
{
	const std::string input(300, 'a');
	REQUIRE(clipvault::core::preparePreview(input, 300) == input);
}

TEST_CASE("预览：超长 ASCII 截到 maxBytes", "[.]")
{
	const std::string input(500, 'a');
	REQUIRE(clipvault::core::preparePreview(input, 300).size() == 300);
}

TEST_CASE("预览：截断点落在汉字中间时丢掉残缺字符", "[.]")
{
	// 299 个 'a' 跟着一个三字节汉字，上限 300 正好切在这个汉字的首字节之后
	const std::string input = std::string(299, 'a') + "中";
	REQUIRE(clipvault::core::preparePreview(input, 300) == std::string(299, 'a'));
}

TEST_CASE("预览：汉字完整时不丢掉", "[.]")
{
	// 299 个 'a' + 一个完整汉字共 302 字节，上限 302 时应当原样保留
	const std::string input = std::string(299, 'a') + "中";
	REQUIRE(clipvault::core::preparePreview(input, 302) == input);
}

TEST_CASE("预览：先去首部空白再截断", "[.]")
{
	// 100 个空格 + 300 个 'a'，上限 300。
	// 顺序反了（先截断后 trim）结果是空串；正确顺序应当得到 300 个 'a'
	const std::string input = std::string(100, ' ') + std::string(300, 'a');
	REQUIRE(clipvault::core::preparePreview(input, 300) == std::string(300, 'a'));
}

TEST_CASE("预览：maxBytes 为 0", "[.]")
{
	REQUIRE(clipvault::core::preparePreview("abc", 0).empty());
}
