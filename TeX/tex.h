// tex parser utilities
// always remove comments first
#pragma once
#include "../SLISC/parser.h"
#include <cassert>

namespace slisc {

// find text command '\name', return the index of '\'
// output the index of "name.back()"
inline Long find_command(Str32_I str, Str32_I name, Long_I start)
{
	Long ind0 = start;
	while (true) {
		ind0 = str.find(U"\\" + name, ind0);
		if (ind0 < 0)
			return -1;
		// check right
		if (!is_letter(str[ind0 + name.size() + 1]))
			return ind0;
		++ind0;
	}
}

// skipt command with 'Narg' arguments (command name can only have letters)
// arguments must be in braces '{}' for now
// input the index of '\'
// return one index after command name if 'Narg = 0'
// return one index after '}' if 'Narg > 0'
// might return str.size()
// return -1 if failed
inline Long skip_command(Str32_I str, Long_I ind, Long_I Narg = 0)
{
	Long i;
	Bool found = false;
	for (i = ind + 1; i < Size(str); ++i) {
		if (!is_letter(str[i])) {
			found = true; break;
		}
			
	}
	if (!found) {
		if (Narg == 0)
			return str.size();
		else
			return -1;
	}
	Long ind0 = i;
	if (Narg > 0)
		return skip_scope(str, ind0, Narg);
	return ind0;
}

// get the i-th command argument
// return the next index of the i-th '}'
// when "option" is 't', trim white spaces on both sides of "arg"
inline Long command_arg(Str32_O arg, Str32_I str, Long_I ind, Long_I i = 0, Char_I option = 't')
{
	Long ind0 = ind, ind1;
	ind0 = skip_command(str, ind0);
	if (ind0 < 0) return -1;
	ind0 = skip_scope(str, ind0, i);
	if (ind0 < 0) return -1;
	ind0 = expect(str, U"{", ind0);
	if (ind0 < 0) return -1;
	ind1 = pair_brace(str, ind0 - 1);
	if (ind1 < 0) return -1;
	arg = str.substr(ind0, ind1 - ind0);
	if (option == 't')
		trim(arg);
	return ind1;
}

// find command with a specific 1st arguments
// i.e. \begin{equation}
// return the index of '\', return -1 if not found
inline Long find_command_spec(Str32_I str, Str32_I name, Str32_I arg1, Long_I start)
{
	Long ind0 = start;
	Str32 arg1_;
	while (true) {
		ind0 = find_command(str, name, ind0);
		if (ind0 < 0)
			return -1;
		command_arg(arg1_, str, ind0, 0);
		if (arg1_ == arg1)
			return ind0;
		++ind0;
	}
}

// skip an environment
// might return str.size()
inline Long skip_env(Str32_I str, Long_I ind)
{
	Str32 name;
	command_arg(name, str, ind);
	Long ind0 = find_command_spec(str, U"end", name, ind);
	return skip_command(str, ind0, 1);
}

// find the intervals of all commands with 1 argument
// intervals are from '\' to '}'
// return the number of commands found, return -1 if failed
inline Long find_all_command_intv(Intvs_O intv, Str32_I name, Str32_I str)
{
	Long ind0 = 0, N = 0;
	intv.clear();
	while (true) {
		ind0 = find_command(str, name, ind0);
		if (ind0 < 0)
			return intv.size();
		intv.pushL(ind0);
		ind0 = skip_command(str, ind0, 1);
		if (ind0 < 0)
			return -1;
		intv.pushR(ind0-1);
	}
}

// find a scope in str for environment named env
// return number of environments found. return -1 if failed
// if option = 'i', range starts from the next index of \begin{} and previous index of \end{}
// if option = 'o', range starts from '\' of \begin{} and '}' of \end{}
inline Long find_env(Intvs_O intv, Str32_I str, Str32_I env, Char option = 'i')
{
	Long ind0{}, ind1{}, ind2{}, ind3{};
	Intvs intvComm; // result from FindComment
	Long N{}; // number of environments found
	if (option != 'i' && option != 'o')
		SLS_ERR("illegal option!");
	intv.clear();
	// find comments including the ones in lstlisting (doesn't matter)
	find_comments(intvComm, str, U"%");
	while (true) {
		// find "\begin{env}"
		ind0 = find_command_spec(str, U"begin", env, ind0);
		if (ind0 < 0)
			return intv.size();
		if (option == 'i')
			ind0 = skip_command(str, ind0, 1);
		intv.pushL(ind0);

		// find "\end{env}"
		ind0 = find_command_spec(str, U"end", env, ind0);
		if (ind0 < 0)
			return -1;
		if (option == 'o')
			ind0 = skip_command(str, ind0, 1);
		if (ind0 < 0) {
			intv.pushR(str.size() - 1);
			return intv.size();
		}
		else
			intv.pushR(ind0 - 1);
	}
}

// get to the inside of the environment
// return the first index inside environment
// output first index of "\end"
// return -1 if failed
inline Long inside_env(Long_O right, Str32_I str, Long_I ind, Long_I Narg = 1)
{
	if (expect(str, U"\\begin", ind) < 0)
		return -1;
	Str32 env;
	command_arg(env, str, ind, 0);
	Long left = skip_command(str, ind, Narg);
	if (left < 0)
		return -1;
	right = find_command_spec(str, U"end", env, left);
	if (right < 0)
		return -1;
	return left;
}

// see if an index ind is in any of the evironments \begin{names[j]}...\end{names[j]}
// output iname of name[iname], -1 if return false
// TODO: check if this function works.
inline Bool index_in_env(Long& iname, Long ind, vector_I<Str32> names, Str32_I str)
{
	Intvs intv;
	for (Long i = 0; i < Size(names); ++i) {
		while (find_env(intv, str, names[i]) < 0) {
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
inline Long find_comment(Intvs_O intv, Str32_I str)
{
	find_comments(intv, str, U"%");
	Intvs intvLst;
	find_env(intvLst, str, U"lstlisting", 'o');
	for (Long i = intv.size() - 1; i >= 0; --i) {
		if (is_in(intv.L(i), intvLst))
			intv.erase(i, 1);
	}
	return intv.size();
}

// find the range of inline equations using $$
// if option = 'i', intervals does not include $, if 'o', it does.
// return the number of $$ environments found.
inline Long find_inline_eq(Intvs_O intv, Str32_I str, Char option = 'i')
{
	intv.clear();
	Long N{}; // number of $$
	Long ind0{};
	Intvs intvComm; // result from FindComment
	find_comment(intvComm, str);
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

// TODO: find one instead of all
// TODO: find one using FindComBrace
// Long FindAllBegin

// TODO: delete this
// Find "\begin{env}" or "\begin{env}{}" (option = '2')
// output interval from '\' to '}'
// return number found, return -1 if failed
// use FindAllBegin
inline Long FindAllBegin(Intvs_O intv, Str32_I env, Str32_I str, Char option)
{
	intv.clear();
	Long N{}, ind0{}, ind1;
	while (true) {
		ind1 = str.find(U"\\begin", ind0);
		if (ind1 < 0)
			return N;
		ind0 = expect(str, U"{", ind1 + 6);
		if (expect(str, env, ind0) < 0)
			continue;
		++N; intv.pushL(ind1);
		ind0 = pair_brace(str, ind0 - 1);
		if (option == '1')
			intv.pushR(ind0);
		ind0 = expect(str, U"{", ind0 + 1);
		if (ind0 < 0) {
			SLS_ERR("expecting {}{}!"); return -1;  // break point here
		}
		ind0 = pair_brace(str, ind0 - 1);
		intv.pushR(ind0);
	}
}

// TODO: delete this
// Find "\end{env}"
// output ranges to intv, from '\' to '}'
// return number found, return -1 if failed
inline Long FindEnd(Intvs_O intv, Str32_I env, Str32_I str)
{
	intv.clear();
	Long N{}, ind0{}, ind1{};
	while (true) {
		ind1 = str.find(U"\\end", ind0);
		if (ind1 < 0)
			return N;
		ind0 = expect(str, U"{", ind1 + 4);
		if (expect(str, env, ind0) < 0)
			continue;
		++N; intv.pushL(ind1);
		ind0 = pair_brace(str, ind0 - 1);
		intv.pushR(ind0);
	}
}

// Find normal text range
// return -1 if failed
inline Long FindNormalText(Intvs_O indNorm, Str32_I str)
{
	Intvs intv, intv1;
	// comments
	find_comment(intv, str);
	// inline equation environments
	find_inline_eq(intv1, str, 'o');
	if (combine(intv, intv1) < 0) return -1;
	// equation environments
	find_env(intv1, str, U"equation", 'o');
	if (combine(intv, intv1) < 0) return -1;
	// command environments
	find_env(intv1, str, U"Command", 'o');
	if (combine(intv, intv1) < 0) return -1;
	// gather environments
	find_env(intv1, str, U"gather", 'o');
	if (combine(intv, intv1) < 0) return -1;
	// align environments (not "aligned")
	find_env(intv1, str, U"align", 'o');
	if (combine(intv, intv1) < 0) return -1;
	// texttt command
	find_all_command_intv(intv1, U"texttt", str);
	if (combine(intv, intv1) < 0) return -1;
	// input command
	find_all_command_intv(intv1, U"input", str);
	if (combine(intv, intv1) < 0) return -1;
	// Figure environments
	find_env(intv1, str, U"figure", 'o');
	if (combine(intv, intv1) < 0) return -1;
	// Table environments
	find_env(intv1, str, U"table", 'o');
	if (combine(intv, intv1) < 0) return -1;
	// subsubsection command
	find_all_command_intv(intv1, U"subsubsection", str);
	if (combine(intv, intv1) < 0) return -1;
	//  \begin{exam}{} and \end{exam}
	FindAllBegin(intv1, U"exam", str, '2');
	if (combine(intv, intv1) < 0) return -1;
	FindEnd(intv1, U"exam", str);
	if (combine(intv, intv1) < 0) return -1;
	//  exer\begin{exer}{} and \end{exer}
	FindAllBegin(intv1, U"exer", str, '2');
	if (combine(intv, intv1) < 0) return -1;
	FindEnd(intv1, U"exer", str);
	if (combine(intv, intv1) < 0) return -1;
	// invert range
	return invert(indNorm, intv, str.size());
}

// detect unnecessary braces and add "删除标记"
// return the number of braces pairs removed
inline Long RemoveBraces(vector_I<Long> ind_left, vector_I<Long> ind_right,
	vector_I<Long> ind_RmatchL, Str32_IO str)
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
inline Long Command2Tag(Str32_I nameComm, Str32_I strLeft, Str32_I strRight, Str32_IO str)
{
	Long N{}, ind0{}, ind1{}, ind2{};
	while (true) {
		ind0 = str.find(U"\\" + nameComm, ind0);
		if (ind0 < 0) break;
		ind1 = ind0 + nameComm.size() + 1;
		ind1 = expect(str, U"{", ind1); --ind1;
		if (ind1 < 0) {
			++ind0; continue;
		}
		ind2 = pair_brace(str, ind1);
		str.erase(ind2, 1);
		str.insert(ind2, strRight);
		str.erase(ind0, ind1 - ind0 + 1);
		str.insert(ind0, strLeft);
		++N;
	}
	return N;
}

// replace one nameEnv environment with strLeft...strRight
// must remove comments first
// return the increase in str.size()
inline Long Env2Tag(Str32_IO str, Long_I ind, Str32_I strLeft, Str32_I strRight, Long_I NargBegin = 1)
{
	Long ind1 = skip_command(str, ind, NargBegin);
	Str32 envName;
	command_arg(envName, str, ind);

	Long ind2 = find_command_spec(str, U"end", envName, ind);
	Long ind3 = skip_command(str, ind2, 1);
	Long Nbegin = ind1 - ind;
	Long Nend = ind3 - ind2;

	str.replace(ind2, Nend, strRight);
	str.replace(ind, Nbegin, strLeft);
	return strRight.size() + strLeft.size() - Nend - Nbegin;
}

// replace all nameEnv environments with strLeft...strRight
// must remove comments first
inline Long Env2Tag(Str32_I nameEnv, Str32_I strLeft, Str32_I strRight, Str32_IO str)
{
	Long i{}, N{}, Nenv;
	Intvs intvEnvOut, intvEnvIn;
	Nenv = find_env(intvEnvIn, str, nameEnv, 'i');
	Nenv = find_env(intvEnvOut, str, nameEnv, 'o');
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
