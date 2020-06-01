#pragma once
#include "../SLISC/unicode.h"

namespace slisc {

    // highlight cpp code using gnu source-highlight
    // return -1 if failed, return 0 if successful
    Long cpp_highlight(Str32_IO code)
    {
#ifdef _MSC_VER
        return 0;
#endif
        Long ind = code.find(U"\n\t");
        if (ind >= 0)
            throw Str32(U"cpp 代码中统一使用四个空格作为缩进： " + code.substr(ind, 20) + U"...");
        write(code, U"tmp.cpp");
        remove("tmp.cpp.html");
        int ret = system("source-highlight --src-lang cpp --out-format html "
            "--input tmp.cpp --output tmp.cpp.html");
        if (!file_exist("tmp.cpp.html")) {
            return -1;
        }
        read(code, U"tmp.cpp.html");
        Long ind0 = code.find(U"<pre>", 0) + 5;
        code = code.substr(ind0, code.size() - 7 - ind0);
        remove("tmp.cpp"); remove("tmp.cpp.html");
        return 0;
    }
} // namespace slisc
