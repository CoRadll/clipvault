#include <cstdio>
#include <concepts>

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
}
