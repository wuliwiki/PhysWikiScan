#pragma once
#include "../SLISC/global.h"
#include "../SLISC/unicode.h"

namespace slisc {

// see if an index i falls into the scopes of ind
Bool IndexInRange(Long i, vector<Long> ind)
{
	if (ind.size() == 0)
		return false;
	for (Long j = 0; j < ind.size() - 1; j += 2) {
		if (ind[j] <= i && i <= ind[j + 1])
			return true;
	}
	return false;
}

// see if an index ind is in any of the evironments \begin{names[j]}...\end{names[j]}
// output iname of name[iname], -1 if return false
// TODO: check if this function works.
Bool IndexInEnv(Long& iname, Long ind, const vector<Str32>& names, Str32_I str)
{
	Long j{}, ikey{}, ind0{ ind + 1 }, ind1{}, Nname{};
	Nname = names.size(); iname = -1;
	vector<Str32> key{U"\\begin", U"\\end"};
	// find next \begin{name} or \end{name}
	while (true) {
		ind0 = FindMultiple(ikey, str, key, ind0);
		if (ind0 < 0) return false;
		ind0 = ExpectKey(str, U"{", ind0 + key[ikey].size());
		for (j = 0; j < Nname; ++j) {
			ind1 = ExpectKey(str, names[j], ind0);
			if (ind1 >= 0) {
				iname = j;  break;
			}
		}
		if (ind1 < 0)
			return false;
		else if (ikey == 0)
			return false;
		else
			return true;
	}
}

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

// combine ranges ind1 and ind2
// a range can contain another range, but not partial overlap
// return total range number, or -1 if failed.
Long CombineRange(vector<Long>& ind, vector<Long> ind1, vector<Long> ind2)
{
	Long i, N1 = ind1.size(), N2 = ind2.size();
	if (ind1.size() % 2 != 0 || ind2.size() % 2 != 0)
		cout << "size error, must be even!" << endl; // break point here
	if (N1 == 0)
	{
		ind = ind2; return N2 / 2;
	}
	else if (N2 == 0)
	{
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
		if (order[i] >= end.size())
			cout << "out of bound!" << endl;
		temp[i] = end[order[i]];
	}
	end = temp;

	// load ind
	ind.resize(0);
	ind.push_back(start[0]);
	i = 0;
	while (i < N - 1)
	{
		if (end[i] > start[i + 1])
		{
			if (end[i] > end[i + 1])
			{
				end[i + 1] = end[i]; ++i;
			}
			else
			{
				cout << "error! range overlap!" << endl;
				return -1;  // break point here
			}
		}
		else if (end[i] == start[i + 1] - 1)
			++i;
		else
		{
			ind.push_back(end[i]);
			ind.push_back(start[i + 1]);
			++i;
		}
	}
	ind.push_back(end.back());
	if (ind.size() % 2 != 0) {
		cout << "size error! must be even!" << endl;
	}
		
	return ind.size() / 2;
}

// invert ranges in ind0, output to ind1
Long InvertRange(vector<Long>& ind, const vector<Long>& ind0, Long Nstr)
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

// find comments
// output: ind is index in pairs
// return number of comments found. return -1 if failed
// range is from '%' to 'CR'
Long FindComment(vector<Long>& ind, Str32_I str)
{
	Long ind0{}, ind1{};
	Long N{}; // number of comments found
	ind.resize(0);
	//TCHAR escape{'\\'};
	while (true) {
		ind1 = str.find(U'%', ind0);
		if (ind1 >= 0)
			if (ind1 == 0 || (ind1 > 0 && str.at(ind1 - 1) != U'\\')) {
				ind.push_back(ind1); ++N;
			}
			else {
				ind0 = ind1 + 1;  continue;
			}
		else
			return N;

		ind1 = str.find(U'\n', ind1 + 1);
		if (ind1 < 0) {// not found
			ind.push_back(str.size() - 1);
			return N;
		}
		else
			ind.push_back(ind1);

		ind0 = ind1;
	}
}

// find a scope in str for environment named env
// output: ind is index in pairs
// return number of environments found. return -1 if failed
// if option = 'i', range starts from the next index of \begin{} and previous index of \end{}
// if option = 'o', range starts from '\' of \begin{} and '}' of \end{}
Long FindEnv(vector<Long>& ind, Str32_I str, Str32_I env, Char option = 'i')
{
	Long ind0{}, ind1{}, ind2{}, ind3{};
	vector<Long> indComm{}; // result from FindComment
	Long N{}; // number of environments found
	ind.resize(0);
	FindComment(indComm, str);
	while (true) {
		// find "\\begin"
		ind3 = str.find(U"\\begin", ind0);
		if (IndexInRange(ind3, indComm)) { ind0 = ind3 + 6; continue; }
		if (ind3 < 0)
			return N;
		ind0 = ind3 + 6;

		// expect '{'
		ind0 = ExpectKey(str, U"{", ind0);
		if (ind0 < 0)
			return -1;
		// expect 'env'
		ind1 = ExpectKey(str, env, ind0);
		if (ind1 < 0)
			continue;
		ind0 = ind1;
		// expect '}'
		ind0 = ExpectKey(str, U"}", ind0);
		if (ind0 < 0)
			return -1;
		ind.push_back(option == 'i' ? ind0 : ind3);

		while (true)
		{
			// find "\\end"
			ind0 = str.find(U"\\end", ind0);
			if (IndexInRange(ind0, indComm)) { ind0 += 4; continue; }
			if (ind0 < 0)
				return -1;
			ind2 = ind0 - 1;
			ind0 += 4;
			// expect '{'
			ind0 = ExpectKey(str, U"{", ind0);
			if (ind0 < 1)
				return -1;
			// expect 'env'
			ind1 = ExpectKey(str, env, ind0);
			if (ind1 < 0)
				continue;
			ind0 = ind1;
			// expect '}'
			ind0 = ExpectKey(str, U"}", ind0);
			if (ind0 < 0)
				return -1;
			ind.push_back(option == 'i' ? ind2 : ind0 - 1);
			++N;
			break;
		}
	}
}

// find the range of inline equations using $$
// if option = 'i', range does not include $, if 'o', it does.
// return the number of $$ environments found.
Long FindInline(vector<Long>& ind, Str32_I str, Char option = 'i')
{
	ind.resize(0);
	Long N{}; // number of $$
	Long ind0{};
	vector<Long> indComm; // result from FindComment
	FindComment(indComm, str);
	while (true) {
		ind0 = str.find(U"$", ind0);
		if (ind0 < 0) {
			break;
		} // did not find
		if (ind0 > 0 && str.at(ind0 - 1) == '\\') { // escaped
			++ind0; continue;
		}
		if (IndexInRange(ind0, indComm)) { // in comment
			++ind0; continue;
		}
		ind.push_back(ind0);
		++ind0; ++N;
	}
	if (N % 2 != 0) {
		SLS_ERR("odd number of $ found!"); // breakpoint here
		return -1;
	}
	N /= 2;
	if (option == 'i' && N > 0)
		for (Long i{}; i < 2 * N; i += 2) {
			++ind[i]; --ind[i + 1];
		}
	return N;
}

// Find all <key>{} environment in str
// find <key>{}{} when option = '2'
// output ranges to ind
// if option = 'i', range does not include {}, if 'o', range from first character of <key> to '}'
// return number of <key>{} found, return -1 if failed.
Long FindComBrace(vector<Long>& ind, Str32_I key, Str32_I str, Char option = 'i')
{
	ind.resize(0);
	Long ind0{}, ind1;
	while (true)
	{
		ind1 = str.find(key, ind0);
		if (ind1 < 0)
			return ind.size() / 2;
		ind0 = ind1 + key.size();
		ind0 = ExpectKey(str, U"{", ind0);
		if (ind0 < 0) {
			ind0 = ind1 + key.size(); continue;
		}
		ind.push_back(option == 'i' ? ind0 : ind1);
		ind0 = PairBraceR(str, ind0 - 1);
		if (option != '2') {
			ind.push_back(option == 'i' ? ind0 - 1 : ind0);
		}
		else {
			ind0 = ExpectKey(str, U"{", ind0 + 1);
			if (ind0 < 0)
				continue;
			ind0 = PairBraceR(str, ind0 - 1);
			ind.push_back(ind0);
		}
	}
}

// Find "\begin{env}" or "\begin{env}{}" (option = '2')
// output ranges to ind, from '\' to '}'
// return number found, return -1 if failed
Long FindBegin(vector<Long>& ind, Str32_I env, Str32_I str, Char option)
{
	ind.resize(0);
	Long N{}, ind0{}, ind1;
	while (true) {
		ind1 = str.find(U"\\begin", ind0);
		if (ind1 < 0)
			return N;
		ind0 = ExpectKey(str, U"{", ind1 + 6);
		if (ExpectKey(str, env, ind0) < 0)
			continue;
		++N; ind.push_back(ind1);
		ind0 = PairBraceR(str, ind0 - 1);
		if (option == '1')
			ind.push_back(ind0);
		ind0 = ExpectKey(str, U"{", ind0 + 1);
		if (ind0 < 0) {
			SLS_ERR("expecting {}{}!"); return -1;  // break point here
		}
		ind0 = PairBraceR(str, ind0 - 1);
		ind.push_back(ind0);
	}
}

// Find "\end{env}"
// output ranges to ind, from '\' to '}'
// return number found, return -1 if failed
Long FindEnd(vector<Long>& ind, Str32_I env, Str32_I str)
{
	ind.resize(0);
	Long N{}, ind0{}, ind1{};
	while (true) {
		ind1 = str.find(U"\\end", ind0);
		if (ind1 < 0)
			return N;
		ind0 = ExpectKey(str, U"{", ind1 + 4);
		if (ExpectKey(str, env, ind0) < 0)
			continue;
		++N; ind.push_back(ind1);
		ind0 = PairBraceR(str, ind0 - 1);
		ind.push_back(ind0);
	}
}

// Find normal text range
// return -1 if failed
Long FindNormalText(vector<Long>& indNorm, Str32_I str)
{
	vector<Long> ind, ind1;
	// comments
	FindComment(ind, str);
	// inline equation environments
	FindInline(ind1, str, 'o');
	if (CombineRange(ind, ind, ind1) < 0) return -1;
	// equation environments
	FindEnv(ind1, str, U"equation", 'o');
	if (CombineRange(ind, ind, ind1) < 0) return -1;
	// command environments
	FindEnv(ind1, str, U"Command", 'o');
	if (CombineRange(ind, ind, ind1) < 0) return -1;
	// gather environments
	FindEnv(ind1, str, U"gather", 'o');
	if (CombineRange(ind, ind, ind1) < 0) return -1;
	// align environments (not "aligned")
	FindEnv(ind1, str, U"align", 'o');
	if (CombineRange(ind, ind, ind1) < 0) return -1;
	// texttt command
	FindComBrace(ind1, U"\\texttt", str, 'o');
	if (CombineRange(ind, ind, ind1) < 0) return -1;
	// input command
	FindComBrace(ind1, U"\\input", str, 'o');
	if (CombineRange(ind, ind, ind1) < 0) return -1;
	// Figure environments
	FindEnv(ind1, str, U"figure", 'o');
	if (CombineRange(ind, ind, ind1) < 0) return -1;
	// Table environments
	FindEnv(ind1, str, U"table", 'o');
	if (CombineRange(ind, ind, ind1) < 0) return -1;
	// subsubsection command
	FindComBrace(ind1, U"\\subsubsection", str, 'o');
	if (CombineRange(ind, ind, ind1) < 0) return -1;
	//  \begin{exam}{} and \end{exam}
	FindBegin(ind1, U"exam", str, '2');
	if (CombineRange(ind, ind, ind1) < 0) return -1;
	FindEnd(ind1, U"exam", str);
	if (CombineRange(ind, ind, ind1) < 0) return -1;
	//  exer\begin{exer}{} and \end{exer}
	FindBegin(ind1, U"exer", str, '2');
	if (CombineRange(ind, ind, ind1) < 0) return -1;
	FindEnd(ind1, U"exer", str);
	if (CombineRange(ind, ind, ind1) < 0) return -1;
	// invert range
	return InvertRange(indNorm, ind, str.size());
}

// detect unnecessary braces and add "删除标记"
// return the number of braces pairs removed
Long RemoveBraces(vector<Long>& ind_left, vector<Long>& ind_right,
	vector<Long>& ind_RmatchL, Str32_IO str)
{
	unsigned i, N{};
	vector<Long> ind; // redundent right brace index
	for (i = 1; i < ind_right.size(); ++i)
		// there must be no space between redundent {} and neiboring braces.
		if (ind_right[i] == ind_right[i - 1] + 1 &&
			ind_left[ind_RmatchL[i]] == ind_left[ind_RmatchL[i - 1]] - 1)
		{
			ind.push_back(ind_right[i]);
			ind.push_back(ind_left[ind_RmatchL[i]]);
			++N;
		}

	if (N > 0) {
		sort(ind.begin(), ind.end());
		for (Long i = ind.size() - 1; i >= 0; --i) {
			str.insert(ind[i], U"删除标记");
		}
	}
	return N;
}

// replace \nameComm{...} with strLeft...strRight
// {} cannot be omitted
// must remove comments first
Long Command2Tag(Str32_I nameComm, Str32_I strLeft, Str32_I strRight, Str32_IO str)
{
	Long N{}, ind0{}, ind1{}, ind2{};
	while (true) {
		ind0 = str.find(U"\\" + nameComm, ind0);
		if (ind0 < 0) break;
		ind1 = ind0 + nameComm.size() + 1;
		ind1 = ExpectKey(str, U"{", ind1); --ind1;
		if (ind1 < 0) {
			++ind0; continue;
		}
		ind2 = PairBraceR(str, ind1);
		str.erase(ind2, 1);
		str.insert(ind2, strRight);
		str.erase(ind0, ind1 - ind0 + 1);
		str.insert(ind0, strLeft);
		++N;
	}
	return N;
}

// replace nameEnv environment with strLeft...strRight
// must remove comments first
Long Env2Tag(Str32_I nameEnv, Str32_I strLeft, Str32_I strRight, Str32_IO str)
{
	Long i{}, N{}, Nenv;
	vector<Long> indEnvOut, indEnvIn;
	Nenv = FindEnv(indEnvIn, str, nameEnv, 'i');
	Nenv = FindEnv(indEnvOut, str, nameEnv, 'o');
	for (i = 2 * Nenv - 2; i >= 0; i -= 2) {
		str.erase(indEnvIn[i + 1] + 1, indEnvOut[i + 1] - indEnvIn[i + 1]);
		str.insert(indEnvIn[i + 1] + 1, strRight);
		str.erase(indEnvOut[i], indEnvIn[i] - indEnvOut[i]);
		str.insert(indEnvOut[i], strLeft);
		++N;
	}
	return N;
}

} // namespace slisc
