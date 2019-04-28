// interval algorithms
// an interval is represented by two numbers e.g. Long interv[2]
// interv[0] is the left bound and interv[1] is the right bound
// an interval can represent, e.g. a sub vector of a vector,
// elements at the bounds are included.

#pragma once
#include "sort.h"

namespace slisc {

typedef vector<Long> Intvs;
typedef const Intvs &Intvs_I;
typedef Intvs &Intvs_O, &Intvs_IO;

// see if an index i falls into the scopes of ind
inline Bool is_in_ptr(Long_I i, const Long *intvs, Long_I N)
{
	if (N == 0)
		return false;
	for (Long j = 0; j < N - 1; j += 2) {
		if (intvs[j] <= i && i <= intvs[j + 1])
			return true;
	}
	return false;
}

inline Bool is_in(Long_I i, Intvs_I intvs)
{ return is_in_ptr(i, &intvs[0], intvs.size()); }

// invert ranges in ind0, output to ind1
Long invert(Intvs_O ind, Intvs_I ind0, Long_I Nstr)
{
	ind.resize(0);
	if (ind0.size() == 0) {
		ind.push_back(0); ind.push_back(Nstr - 1); return 1;
	}

	Long N{}; // total num of ranges output
	if (ind0[0] > 0) {
		ind.push_back(0); ind.push_back(ind0[0] - 1); ++N;
	}
	for (Long i = 1; i < ind0.size() - 1; i += 2) {
		ind.push_back(ind0[i] + 1);
		ind.push_back(ind0[i + 1] - 1);
		++N;
	}
	if (ind0.back() < Nstr - 1) {
		ind.push_back(ind0.back() + 1); ind.push_back(Nstr - 1); ++N;
	}
	return N;
}

// combine ranges ind1 and ind2
// a range can contain another range, but not partial overlap
// return total range number, or -1 if failed.
Long combine(Intvs_O ind, Intvs_I ind1, Intvs_I ind2)
{
	Long i, N1 = ind1.size(), N2 = ind2.size();
	if (isodd(ind1.size()) || isodd(ind2.size()) ) {
		SLS_WARN("size error, must be even!"); // break point here
		return -1;
	}
	if (&ind == &ind1 || &ind == &ind2) {
		SLS_ERR("aliasing is not allowed!");
	}
	if (N1 == 0) {
		ind = ind2; return N2 / 2;
	}
	else if (N2 == 0) {
		ind = ind1; return N1 / 2;
	}

	// load start and end, and sort
	Intvs start, end;
	for (i = 0; i < N1; i += 2)
		start.push_back(ind1[i]);
	for (i = 1; i < N1; i += 2)
		end.push_back(ind1[i]);
	for (i = 0; i < N2; i += 2)
		start.push_back(ind2[i]);
	for (i = 1; i < N2; i += 2)
		end.push_back(ind2[i]);
	sort2(start, end);

	// load ind
	ind.resize(0);
	ind.push_back(start[0]);
	i = 0;
	while (i < start.size() - 1) {
		if (end[i] > start[i + 1]) {
			if (end[i] > end[i + 1]) {
				end[i + 1] = end[i]; ++i;
			}
			else {
				cout << "error! range overlap!" << endl;
				return -1;  // break point here
			}
		}
		else if (end[i] == start[i + 1] - 1)
			++i;
		else {
			ind.push_back(end[i]);
			ind.push_back(start[i + 1]);
			++i;
		}
	}
	ind.push_back(end.back());
	if (isodd(ind.size())) {
		cout << "must be even!" << endl;
		return -1;
	}
	return ind.size() / 2;
}

// combine intervals ind and ind1, and assign the result to ind
Long combine(Intvs_O ind, Intvs_I ind1)
{
	Intvs temp;
	Long ret = combine(temp, ind, ind1);
	ind = temp;
	return ret;
}

} // namespace slisc
