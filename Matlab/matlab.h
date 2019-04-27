#pragma once
#include "../SLISC/global.h"

namespace slisc {

// determin if a apostrophe is a transpose operator
inline Long Matlab_is_trans(Str32_I str, Long_I ind)
{
	if (str.at(ind) != U'\'')
		SLS_ERR("not an apostrophe!");
	Long ind0 = ind - 1;
	if (ind0 >= 0 && (is_alphanum_(str[ind0]) || str[ind0] == U'.'))
		return true;
	return false;
}

// find ranges of all comments in a matlab code (including '%', not including '\n')
// "strings" are the ranges of all
inline Long Matlab_comments(vector<Long> &ind, Str32_I str, const vector<Long> &ind_str)
{
	Long ind0 = 0;
	Long N = 0;
	while (true) {
		ind0 = str.find(U'%', ind0);
		if (ind0 < 0) {
			if (isodd(ind.size()))
				SLS_ERR("range pairs must be even!");
			return ind.size()/2;
		}
		if (IndexInRange(ind0, ind_str))
			continue;
		ind.push_back(ind0);
		ind0 = str.find(U'\n', ind0);
		// last line, line ending
		if (ind0 < 0) {
			ind.push_back(str.size()-1);
			return N;
		}
		ind.push_back(ind0-1);
	}
}

} // namespace slisc
