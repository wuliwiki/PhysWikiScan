#pragma once
#include "../SLISC/unicode.h"

namespace slisc {

	// highlight cpp code using gnu source-highlight
	// return -1 if failed, return 0 if successful
	Long cpp_highlight(Str32_IO str)
	{
		write_file(str, U"tmp.cpp");
		remove("tmp.cpp.html");
		int ret = system("source-highlight --src-lang cpp --out-format html < tmp.cpp > tmp.cpp.html");
		if (!file_exist("tmp.cpp.html")) {
			return -1;
		}
		read_file(str, U"tmp.cpp.html");
		str = str.substr(140);
		cout << str;
		SLS_ERR("TODO");
		remove("tmp.cpp"); remove("tmp.cpp.html");
	}

} // namespace slisc
