// cpp parser utils
#pragma once
#include "../SLISC/global.h"

namespace slisc {

// find intervals of all comments using double slash
inline Long cpp_comments_slash(Intvs_O intv, Str32_I str, Intvs_I intv_str)
{
	Long ind0 = 0;
	Long N = 0;
	while (true) {
	    ind0 = str.find(U"//", ind0);
	    if (ind0 < 0) {
	        return intv.size();
	    }
	    if (is_in(ind0, intv_str)) {
	        ++ind0;    continue;
	    }
	    intv.pushL(ind0);
	    ind0 = str.find(U'\n', ind0);
	    // last line, line ending
	    if (ind0 < 0) {
	        intv.pushR(str.size()-1);
	        return N;
	    }
	    intv.pushR(ind0-1);
	}
}

} // namespace slisc
