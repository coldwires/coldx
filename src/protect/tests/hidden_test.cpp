// COLDX_HIDDEN: the text comes back exactly, and the test's own executable, read from
// disk, does not contain it. The expected strings are built at run time from reversed
// literals, so the only place the forward text could come from is the hidden one.

#include <coldx/protect/hidden.h>

#include <algorithm>
#include <fstream>
#include <iterator>
#include <string>

#include "check.h"

using coldx_test::CheckResult;

namespace {

std::string Reversed(std::string s) {
    std::reverse(s.begin(), s.end());
    return s;
}

} // namespace

int main(int argc, char** argv) {
    const std::string secret = COLDX_HIDDEN("WATERFALL-SECRET-LEVEL-COLDX");
    const std::string expected = Reversed("XDLOC-LEVEL-TERCES-LLAFRETAW");
    CHECK(secret == expected);

    const std::string empty = COLDX_HIDDEN("");
    CHECK(empty.empty());
    const std::string utf8 = COLDX_HIDDEN("caf\xC3\xA9 \xE4\xB8\x96");
    CHECK(utf8.size() == 9 && static_cast<unsigned char>(utf8[3]) == 0xC3);

    // The same text twice is scrambled two ways (a different counter), and both reveal.
    const std::string a = COLDX_HIDDEN("twice"), b = COLDX_HIDDEN("twice");
    CHECK(a == b && a.size() == 5);

    // The executable: the forward text is nowhere in it.
    if (argc > 1) {
        std::ifstream file(argv[1], std::ios::binary);
        const std::string binary((std::istreambuf_iterator<char>(file)), {});
        CHECK(binary.size() > 1000);
        CHECK(binary.find(expected) == std::string::npos);
        CHECK(binary.find(expected.substr(0, 9)) == std::string::npos);
        std::printf("searched %zu bytes of %s\n", binary.size(), argv[1]);
    }
    return CheckResult();
}
