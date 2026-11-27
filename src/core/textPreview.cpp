#include "clipvault/core/textPreview.hpp"

namespace clipvault::core
{
    // 规则（实现必须满足）：
    //   1. 去掉首部空白（'\r'、'\n'、'\t'、' ' 都算）；
    //      尾部不动 —— normalize() 已经去过尾部了，这里不必重复
    //   2. 输出不超过 maxBytes 字节，超出的部分丢掉
    //   3. 截断点必须落在完整的 UTF-8 字符边界上 ——
    //      切在字符中间会让后续转宽字符得到替换符，显示成乱码方块
    //
    // 顺序不能换：先去掉首部空白，再截断。反过来的话，一段开头全是空白的
    // 内容会把字节预算吃光，trim 完可能什么都不剩。
    //
    // 输入 text 是库里存的 UTF-8 原文。入库时 '\r\n' 已经被 normalize()
    // 归一成 '\n'，所以实际只会遇到 '\n'（顺手一起处理 '\r' 更稳）。
    //
    // 边界用例写在 tests/test_textPreview.cpp 里。实现完成后把那边每个
    // TEST_CASE 的 "[.]" 标签删掉，这些用例就会进入 build.bat test。
    std::string preparePreview(const std::string& text, std::size_t maxBytes)
    {
        if(text.empty())
            return "";
        std::size_t start = 0;
        while(text[start] == ' ' || text[start] == '\t' || text[start] == '\n' || text[start] == '\r')
        {
            if(start + 1 >= text.size())
                return "";
            start++;
        }

        if(text.size() - start <= maxBytes)
            return text.substr(start);
        
        maxBytes = maxBytes + start;
        std::size_t end = start;
        std::size_t len = 0;
        do
        {
            end = end + len;
            if((static_cast<unsigned char>(text[end]) & 0x80) == 0)
                len = 1;
            else if((static_cast<unsigned char>(text[end]) & 0xE0) == 0xC0)
                len = 2;
            else if((static_cast<unsigned char>(text[end]) & 0xF0) == 0xE0)
                len = 3;
            else if((static_cast<unsigned char>(text[end]) & 0xF8) == 0xF0)
                len = 4;
            else
                len = 1;
        }while(end + len <= maxBytes);

        return text.substr(start, end - start);
    }
}
