#pragma once
#include "../SLISC/parser.h"

namespace slisc {

// find comments
// result will include the comments in lstlisting!
// return number of comments found. return -1 if failed
// range is from '%' to '\n' (including)
Long FindComment0(Intvs_O intv, Str32_I str)
{
	Long ind0{}, ind1{};
	Long N{}; // number of comments found
	intv.clear();
	while (true) {
		ind1 = str.find(U'%', ind0);
		if (ind1 >= 0)
			if (ind1 == 0 || (ind1 > 0 && str.at(ind1 - 1) != U'\\')) {
				intv.pushL(ind1); ++N;
			}
			else {
				ind0 = ind1 + 1;  continue;
			}
		else
			return N;

		ind1 = str.find(U'\n', ind1 + 1);
		if (ind1 < 0) {// not found
			intv.pushR(str.size() - 1);
			return N;
		}
		else
			intv.pushR(ind1);
		ind0 = ind1;
	}
}

// find a scope in str for environment named env
// return number of environments found. return -1 if failed
// if option = 'i', range starts from the next index of \begin{} and previous index of \end{}
// if option = 'o', range starts from '\' of \begin{} and '}' of \end{}
Long FindEnv(Intvs_O intv, Str32_I str, Str32_I env, Char option = 'i')
{
	Long ind0{}, ind1{}, ind2{}, ind3{};
	Intvs intvComm; // result from FindComment
	Long N{}; // number of environments found
	intv.clear();
	FindComment0(intvComm, str);
	while (true) {
		// find "\\begin"
		ind3 = str.find(U"\\begin", ind0);
		if (is_in(ind3, intvComm)) { ind0 = ind3 + 6; continue; }
		if (ind3 < 0)
			return N;
		ind0 = ind3 + 6;

		// expect '{'
		ind0 = ExpectKey(str, U"{", ind0);
		if (ind0 < 0)
			return -1;
		// expect 'env'
		ind1 = ExpectKey(str, env, ind0);
		if (ind1 < 0 || !is_whole_word(str, ind1-env.size(), env.size()))
			continue;
		ind0 = ind1;
		// expect '}'
		ind0 = ExpectKey(str, U"}", ind0);
		if (ind0 < 0)
			return -1;
		intv.pushL(option == 'i' ? ind0 : ind3);

		while (true)
		{
			// find "\\end"
			ind0 = str.find(U"\\end", ind0);
			if (is_in(ind0, intvComm)) { ind0 += 4; continue; }
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
			intv.pushR(option == 'i' ? ind2 : ind0 - 1);
			++N;
			break;
		}
	}
}

// see if an index ind is in any of the evironments \begin{names[j]}...\end{names[j]}
// output iname of name[iname], -1 if return false
// TODO: check if this function works.
Bool IndexInEnv(Long& iname, Long ind, const vector<Str32>& names, Str32_I str)
{
	Intvs intv;
	for (Long i = 0; i < names.size(); ++i) {
		while (FindEnv(intv, str, names[i]) < 0) {
			Input().Bool("failed! retry?");
		}
		if (is_in(ind, intv)) {
			iname = i;
			return true;
		}
	}
	iname = -1;
	return false;
}

// find latex comments
// similar to FindComment0
// does not include the ones in lstlisting environment
Long FindComment(Intvs_O intv, Str32_I str)
{
	FindComment0(intv, str);
	Intvs intvLst;
	FindEnv(intvLst, str, U"lstlisting", 'o');
	for (Long i = intv.size() - 1; i >= 0; --i) {
		if (is_in(intv.L(i), intvLst))
			intv.erase(i, 1);
	}
	return intv.size();
}

// find the range of inline equations using $$
// if option = 'i', intervals does not include $, if 'o', it does.
// return the number of $$ environments found.
Long FindInline(Intvs_O intv, Str32_I str, Char option = 'i')
{
	intv.clear();
	Long N{}; // number of $$
	Long ind0{};
	Intvs intvComm; // result from FindComment
	FindComment(intvComm, str);
	while (true) {
		ind0 = str.find(U"$", ind0);
		if (ind0 < 0) {
			break;
		} // did not find
		if (ind0 > 0 && str.at(ind0 - 1) == '\\') { // escaped
			++ind0; continue;
		}
		if (is_in(ind0, intvComm)) { // in comment
			++ind0; continue;
		}
		intv.push_back(ind0);
		++ind0; ++N;
	}
	if (N % 2 != 0) {
		SLS_ERR("odd number of $ found!"); // breakpoint here
		return -1;
	}
	N /= 2;
	if (option == 'i' && N > 0)
		for (Long i = 0; i < N; ++i) {
			++intv.L(i); --intv.R(i);
		}
	return N;
}

Long FindComBrace(Intvs_O intv, Str32_I key, Str32_I str, Char option = 'i')
{
	return find_scopes(intv, U"\\"+key, str, option);
}

// Find "\begin{env}" or "\begin{env}{}" (option = '2')
// output ranges to intv, from '\' to '}'
// return number found, return -1 if failed
Long FindBegin(Intvs_O intv, Str32_I env, Str32_I str, Char option)
{
	intv.clear();
	Long N{}, ind0{}, ind1;
	while (true) {
		ind1 = str.find(U"\\begin", ind0);
		if (ind1 < 0)
			return N;
		ind0 = ExpectKey(str, U"{", ind1 + 6);
		if (ExpectKey(str, env, ind0) < 0)
			continue;
		++N; intv.pushL(ind1);
		ind0 = PairBraceR(str, ind0 - 1);
		if (option == '1')
			intv.pushR(ind0);
		ind0 = ExpectKey(str, U"{", ind0 + 1);
		if (ind0 < 0) {
			SLS_ERR("expecting {}{}!"); return -1;  // break point here
		}
		ind0 = PairBraceR(str, ind0 - 1);
		intv.pushR(ind0);
	}
}

// Find "\end{env}"
// output ranges to intv, from '\' to '}'
// return number found, return -1 if failed
Long FindEnd(Intvs_O intv, Str32_I env, Str32_I str)
{
	intv.clear();
	Long N{}, ind0{}, ind1{};
	while (true) {
		ind1 = str.find(U"\\end", ind0);
		if (ind1 < 0)
			return N;
		ind0 = ExpectKey(str, U"{", ind1 + 4);
		if (ExpectKey(str, env, ind0) < 0)
			continue;
		++N; intv.pushL(ind1);
		ind0 = PairBraceR(str, ind0 - 1);
		intv.pushR(ind0);
	}
}

// Find normal text range
// return -1 if failed
Long FindNormalText(Intvs_O indNorm, Str32_I str)
{
	Intvs intv, intv1;
	// comments
	FindComment(intv, str);
	// inline equation environments
	FindInline(intv1, str, 'o');
	if (combine(intv, intv1) < 0) return -1;
	// equation environments
	FindEnv(intv1, str, U"equation", 'o');
	if (combine(intv, intv1) < 0) return -1;
	// command environments
	FindEnv(intv1, str, U"Command", 'o');
	if (combine(intv, intv1) < 0) return -1;
	// gather environments
	FindEnv(intv1, str, U"gather", 'o');
	if (combine(intv, intv1) < 0) return -1;
	// align environments (not "aligned")
	FindEnv(intv1, str, U"align", 'o');
	if (combine(intv, intv1) < 0) return -1;
	// texttt command
	FindComBrace(intv1, U"texttt", str, 'o');
	if (combine(intv, intv1) < 0) return -1;
	// input command
	FindComBrace(intv1, U"input", str, 'o');
	if (combine(intv, intv1) < 0) return -1;
	// Figure environments
	FindEnv(intv1, str, U"figure", 'o');
	if (combine(intv, intv1) < 0) return -1;
	// Table environments
	FindEnv(intv1, str, U"table", 'o');
	if (combine(intv, intv1) < 0) return -1;
	// subsubsection command
	FindComBrace(intv1, U"subsubsection", str, 'o');
	if (combine(intv, intv1) < 0) return -1;
	//  \begin{exam}{} and \end{exam}
	FindBegin(intv1, U"exam", str, '2');
	if (combine(intv, intv1) < 0) return -1;
	FindEnd(intv1, U"exam", str);
	if (combine(intv, intv1) < 0) return -1;
	//  exer\begin{exer}{} and \end{exer}
	FindBegin(intv1, U"exer", str, '2');
	if (combine(intv, intv1) < 0) return -1;
	FindEnd(intv1, U"exer", str);
	if (combine(intv, intv1) < 0) return -1;
	// invert range
	return invert(indNorm, intv, str.size());
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
	Intvs intvEnvOut, intvEnvIn;
	Nenv = FindEnv(intvEnvIn, str, nameEnv, 'i');
	Nenv = FindEnv(intvEnvOut, str, nameEnv, 'o');
	for (i = Nenv - 1; i >= 0; --i) {
		str.erase(intvEnvIn.R(i) + 1, intvEnvOut.R(i) - intvEnvIn.R(i));
		str.insert(intvEnvIn.R(i) + 1, strRight);
		str.erase(intvEnvOut.L(i), intvEnvIn.L(i) - intvEnvOut.L(i));
		str.insert(intvEnvOut.L(i), strLeft);
		++N;
	}
	return N;
}

} // namespace slisc
