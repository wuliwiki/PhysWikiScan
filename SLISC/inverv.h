// interval algorithms
// an interval is represented by two numbers e.g. Long interv[2]
// interv[0] is the left bound and interv[1] is the right bound
// an interval can represent, e.g. a sub vector of a vector,
// elements at the bounds are included.

#pragma once
#include "global.h"

namespace slisc {

// TODO: replace this with the SLISC version
// sort x in descending order while tracking the original index
void stupid_sort(vector<Long>& x, vector<Long>& ind)
{
	Long i, temp;
	Long N = x.size();
	// initialize ind
	ind.resize(0);
	for (i = 0; i < N; ++i)
		ind.push_back(i);

	Bool changed{ true };
	while (changed == true)
	{
		changed = false;
		for (i = 0; i < N - 1; i++)
		{
			if (x[i] > x[i + 1])
			{
				temp = x[i];
				x[i] = x[i + 1];
				x[i + 1] = temp;
				temp = ind[i];
				ind[i] = ind[i + 1];
				ind[i + 1] = temp;
				changed = true;
			}
		}
	}
}

// see if an index i falls into the scopes of ind
inline Bool is_in(Long_I i, const Long *intvs, Long_I N)
{
	if (N == 0)
		return false;
	for (Long j = 0; j < N - 1; j += 2) {
		if (intvs[j] <= i && i <= intvs[j + 1])
			return true;
	}
	return false;
}

inline Bool is_in(Long_I i, vector<Long> intvs)
{ return is_in(i, &intvs[0], intvs.size()); }

// invert ranges in ind0, output to ind1
Long invert(vector<Long>& ind, const vector<Long>& ind0, Long Nstr)
{
	ind.resize(0);
	if (ind0.size() == 0)
	{
		ind.push_back(0); ind.push_back(Nstr - 1); return 1;
	}

	Long N{}; // total num of ranges output
	if (ind0[0] > 0)
	{
		ind.push_back(0); ind.push_back(ind0[0] - 1); ++N;
	}
	for (unsigned i{ 1 }; i < ind0.size() - 1; i += 2)
	{
		ind.push_back(ind0[i] + 1);
		ind.push_back(ind0[i + 1] - 1);
		++N;
	}
	if (ind0.back() < Nstr - 1)
	{
		ind.push_back(ind0.back() + 1); ind.push_back(Nstr - 1); ++N;
	}
	return N;
}

// combine ranges ind1 and ind2
// a range can contain another range, but not partial overlap
// return total range number, or -1 if failed.
// TODO: input by reference
Long combine(vector<Long>& ind, vector<Long> ind1, vector<Long> ind2)
{
	Long i, N1 = ind1.size(), N2 = ind2.size();
	if (ind1.size() % 2 != 0 || ind2.size() % 2 != 0) {
		cout << "size error, must be even!" << endl; // break point here
		return -1;
	}	
	if (N1 == 0) {
		ind = ind2; return N2 / 2;
	}
	else if (N2 == 0) {
		ind = ind1; return N1 / 2;
	}

	// load start and end, and sort
	vector<Long> start, end, order;
	for (i = 0; i < N1; i += 2)
		start.push_back(ind1[i]);
	for (i = 1; i < N1; i += 2)
		end.push_back(ind1[i]);
	for (i = 0; i < N2; i += 2)
		start.push_back(ind2[i]);
	for (i = 1; i < N2; i += 2)
		end.push_back(ind2[i]);
	Long N = start.size();
	stupid_sort(start, order);
	vector<Long> temp;
	temp.resize(N);
	for (i = 0; i < N; ++i) {
		if (order[i] >= end.size()) {
			cout << "out of bound!" << endl;
			return -1;
		}
		temp[i] = end[order[i]];
	}
	end = temp;

	// load ind
	ind.resize(0);
	ind.push_back(start[0]);
	i = 0;
	while (i < N - 1) {
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
	if (ind.size() % 2 != 0) {
		cout << "size error! must be even!" << endl;
		return -1;
	}
	return ind.size() / 2;
}

}
