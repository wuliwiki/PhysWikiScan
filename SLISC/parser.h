// general parser utilities
#pragma once
#include "../SLISC/scalar_arith.h"
#include "../SLISC/unicode.h"
#include "../SLISC/inverv.h"

namespace slisc {

// Find the next "key{...}" in "str"
// find "key{...}{...}" when option = '2'
// output ranges
// if option = 'i', range does not include {}, if 'o', range from first character of <key> to '}'
// return the left interval boundary, output the size of the range
// return -1 if not found.
Long find_scope(Long_O right, Str32_I key, Str32_I str, Long_I start, Char option = 'i')
{
	Long ind0 = start, ind1;
	Long left;
	while (true) {
		ind1 = str.find(/*U"\\" +*/ key, ind0);
		if (ind1 < 0) {
			right = -1; return -1;
		}
		ind0 = ind1 + key.size();
		ind0 = ExpectKey(str, U"{", ind0);
		if (ind0 < 0) {
			ind0 = ind1 + key.size(); continue;
		}
		left = (option == 'i' ? ind0 : ind1);
		ind0 = PairBraceR(str, ind0 - 1);
		if (option != '2') {
			right = option == 'i' ? ind0 - 1 : ind0;
			break;
		}
		else {
			ind0 = ExpectKey(str, U"{", ind0 + 1);
			if (ind0 < 0)
				continue;
			ind0 = PairBraceR(str, ind0 - 1);
			right = ind0;
			break;
		}
	}
	return left;
}

// Find all "\key{...}" in "str"
// find "<key>{}{}" when option = '2'
// if option = 'i', range does not include {}, if 'o', range from first character of <key> to '}'
// return number of "\key{}" found, return -1 if failed.
Long find_scopes(Intvs_O intv, Str32_I key, Str32_I str, Char option = 'i')
{
	intv.clear();
	Long ind0 = 0, right;
	while (true)
	{
		ind0 = find_scope(right, key, str, ind0, option);
		if (ind0 < 0)
			return intv.size();
		intv.push(ind0, right);
		++ind0;
	}
}

} // namespace slisc
