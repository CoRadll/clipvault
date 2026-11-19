#include "clipvault/core/textNormalize.hpp"

namespace clipvault::core
{
    std::string normalize(std::string raw)
    {
        if(raw.size() >= 2u * 1024u * 1024u || raw.empty())
            return {};
        std::size_t r = raw.starts_with("\xEF\xBB\xBF") ? 3 : 0;
        std::size_t w = 0;
        std::size_t goodLen = 0;
        while(r < raw.size())
        {
            std::size_t step = 0;
            char c = '\0';
            if(raw[r] == '\r')
            {
                if(r + 1 < raw.size() && raw[r + 1] == '\n')
                    step = 2;
                else
                    step = 1;
                c = '\n';
            }
            else
            {
                step = 1;
                c = raw[r];
            }
            raw[w] = c;
            if(c != '\r' && c != '\n' && c != '\t' && c != ' ')
                goodLen = w + 1;
            w++;
            r = r + step;
        }
        raw.resize(goodLen);
        return raw;
    }
}
