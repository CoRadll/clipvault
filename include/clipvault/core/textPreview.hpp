#pragma once

#include <cstddef>
#include <string>

namespace clipvault::core
{
    // 把一条已入库的记录准备成能直接画在列表里的预览文字：
    // 去掉首部空白，再按 UTF-8 字符边界截到 maxBytes 字节。
    //
    // 不做压平：换行和缩进都按原文保留，列表靠原文的硬换行分行显示。
    // 首部空白要去掉，因为 normalize() 只去尾部，库里存的字符串开头
    // 可能还带着缩进或空行。
    //
    // maxBytes 由调用方给（窗口层传 300）。core 不认识"窗口"这个概念，
    // 所以这里不收常量，只收参数。
    std::string preparePreview(const std::string& text, std::size_t maxBytes);
}
