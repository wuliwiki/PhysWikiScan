// tex parser utilities
// always remove comments first
#pragma once
#include "../SLISC/str/unicode.h"
#include "../SLISC/str/parser.h"
#include "../SLISC/algo/search.h"

namespace slisc {

// find text command '\name', return the index of '\'
// return the index of "name.back()", return -1 if not found
inline Long find_command(Str_I str, Str_I name, Long_I start = 0)
{
	Long ind0 = start;
	while (true) {
	    ind0 = find(str, "\\" + name, ind0);
	    if (ind0 < 0)
	        return -1;

	    // check right
	    if (!is_letter(str[ind0 + name.size() + 1]))
	        return ind0;
	    ++ind0;
	}
}

// remove comments
// if a comment is preceeded by '\n' and spaces, then they are deleted too
// will not remove '\n' after the comment
inline Long rm_comments(Str_IO str)
{
	Intvs intvComm;
	find_comments(intvComm, str, "%");
	for (Long i = intvComm.size() - 1; i >= 0; --i) {
	    Long ind = ExpectKeyReverse(str, "\n", intvComm.L(i) - 1) + 1;
	    if (ind < 0) {
	        if (intvComm.R(i) < size(str) && str[intvComm.R(i)] == U'\n' &&
	            str[intvComm.R(i) + 1] != U'\n')
	            str.erase(intvComm.R(i), 1);
	        str.erase(intvComm.L(i), intvComm.R(i) - intvComm.L(i));
	    }
	    else
	        str.erase(ind, intvComm.R(i) - ind);
	}
	return intvComm.size();
}

// find one of multiple commands
// return -1 if not found
inline Long find_command(Long_O ikey, Str_I str, vecStr_I names, Long_I start)
{
	Long i_min = 100000000;
	ikey = -1;
	for (Long i = 0; i < size(names); ++i) {
	    Long ind = find_command(str, names[i], start);
	    if (ind >= 0 && ind < i_min) {
	        i_min = ind; ikey = i;
	    }
	}
	if (i_min < 100000000)
	    return i_min;
	return -1;
}

// skipt command with 'Narg' arguments (command name can only have letters)
// arguments must be in braces '{}' for now
// input the index of '\'
// return one index after command name if 'Narg = 0'
// return one index after '}' if 'Narg > 0'
// might return str.size()
// '*' after command will be omitted and skipped
// 'omit_skip_opt' will omit and skip optional argument in [], else [] will be counted into 'Narg'
// use `arg_no_brace` to skip args without `{}`, e.g. `b` in `\frac{a} b` (only a single char)
inline Long skip_command(Str_I str, Long_I ind, Long_I Narg = 0, Bool_I omit_skip_opt = true, Bool_I arg_no_brace = false, Bool_I skip_star = true)
{
	Long i, Narg1 = Narg;
	// skip the command name itself
	Bool found = false;
	for (i = ind + 1; i < size(str); ++i) {
	    if (!is_letter(str[i])) {
	        found = true; break;
	    }
	}
	if (!found) {
	    if (Narg1 == 0)
	        return str.size();
	    else
	        throw scan_err("skip_command() failed!");
	}
	// skip *
	Long ind0 = -1;
	if (skip_star)
	    ind0 = expect(str, "*", i);
	if (ind0 < 0)
	    ind0 = i;
	// skip optional argument
	Long ind1 = expect(str, "[", ind0), ind2;
	if (ind1 >= 0) {
	    ind2 = pair_brace(str, ind1-1);
	    if (omit_skip_opt) {
	        ind0 = ind2 + 1;
	    }
	    else if (Narg1 > 0) {
	        ind0 = ind2 + 1;
	        Narg1 -= 1;
	    }
	}
	// skip arguments
	for (Long i = 0; i < Narg1; ++i) {
	    ind0 = expect(str, "{", ind0);
	    if (ind0 > 0) {
	        ind0 = pair_brace(str, ind0-1) + 1;
	        continue;
	    }
	    if (!arg_no_brace)
	        throw scan_err("skip_command(): '{' not found!");
	    ind0 = str.find_first_not_of(' ', ind0);
	    if (ind0 < 0)
	        throw scan_err("skip_command(): end of file!");
	    ++ind0;
	}
	return ind0;
}

// get the name of the command starting at str[ind]
// does not include '\\'
inline void command_name(Str_O name, Str_I str, Long_I ind)
{
	if (str[ind] != '\\')
	    throw scan_err("not a command!");
	// skip the command name
	Bool found = false;
	Long i;
	for (i = ind + 1; i < size(str); ++i) {
	    if (!is_letter(str[i])) {
	        found = true; break;
	    }
	}
	if (!found)
	    i = str.size();
	name = str.substr(ind+1, i - (ind+1));
}

// determin if a command has a following star
inline Bool command_star(Str_I str, Long_I ind)
{
	if (str[ind] != '\\')
	    throw scan_err("command_star(): not a command!");
	Long ind0 = skip_command(str, ind, 0, false);
	if (str[ind0 - 1] == '*')
	    return true;
	return false;
}

// check if an index is in a command \name{...}
inline Bool is_in_cmd(Str_I str, Str_I name, Long_I ind)
{
	if (ind < 0 || ind >= size(str))
	    throw scan_err(u8"内部错误： is_in_cmd() index out of bound");
	Long ind0 = str.rfind("\\" + name, ind);
	if (ind0 < 0)
	    return false;
	ind0 = expect(str, "{", ind0 + name.size() + 1);
	if (ind0 < 0)
	    return false;
	Long ind1 = pair_brace(str, ind0 - 1);
	if (ind1 <= ind)
	    return false;
	return true;
}

// check if command has optional argument in []
inline Bool command_has_opt(Str_I str, Long_I ind, Bool_I trim = true)
{
	Str arg;
	if (str[ind] != '\\')
	    throw scan_err("has_opt(): not a command!");
	Long ind0 = skip_command(str, ind, 0, false);
	ind0 = expect(str, "[", ind0);
	if (ind0 < 0)
	    return false;
	return true;
}

// get optional argument of command in [], trim result by default
// return empty string if not found
inline Str command_opt(Str_I str, Long_I ind, Bool_I trim = true)
{
	Str arg;
	if (str[ind] != '\\')
	    throw scan_err("has_opt(): not a command!");
	Long ind0 = skip_command(str, ind, 0, false);
	ind0 = expect(str, "[", ind0);
	if (ind0 < 0)
	    return arg;
	Long ind1 = pair_brace(str, ind0-1);
	arg = str.substr(ind0, ind1-ind0);
	if (trim)
	    slisc::trim(arg);
	return arg;
}

// get number of command arguments
// return # of [] or {}
// or return -1: \cmd() or \cmd*()
// or return -2: \cmd[]() or \cmd*[]()
inline Long command_Narg(Str_I str, Long_I ind)
{
	if (str[ind] != '\\')
	    throw scan_err("not a command!");
	Long Narg = 0, ind0 = skip_command(str, ind, 0, false);
	// skip optional argument
	Long ind1 = expect(str, "[", ind0) - 1;
	if (ind1 > 0) {
	    ind0 = pair_brace(str, ind1) + 1;
	    ++Narg;
	}
	if (expect(str, "(", ind0) > 0) {
	    Narg = -Narg - 1;
	}
	else {
	    while (true) {
	        ind0 = expect(str, "{", ind0) - 1;
	        if (ind0 > 0) {
	            ind0 = pair_brace(str, ind0) + 1;
	            ++Narg;
	        }
	        else
	            break;
	    }
	}
	return Narg;
}

// get the i-th command argument in {}
// when "trim" is 't', trim white spaces on both sides of "arg"
// return the index after '}' of the argument if successful
// return -1 if requested argument does not exist
// use `arg_no_brace` to get args without `{}`, e.g. `b` in `\frac{a} b` (single char/command), will return one index after arg
inline Long command_arg(Str_O arg, Str_I str, Long_I ind, Long_I i = 0, Bool_I trim = true, Bool_I ignore_opt = false, Bool_I arg_no_brace = false)
{
	Long ind0 = ind, ind1;
	if (ignore_opt)
	    ind0 = skip_command(str, ind0, i, true, arg_no_brace);
	else if (i == 0) {
	    arg = command_opt(str, ind, trim);
	    if (!arg.empty())
	        return skip_command(str, ind0);
	    ind0 = skip_command(str, ind0, 0);
	}
	else if (i > 0) {
	    ind0 = skip_command(str, ind0, i, false, arg_no_brace);
	}
	else {
	    throw scan_err("command_arg(): i < 0 not allowed!");
	}
	
	ind1 = expect(str, "{", ind0);
	if (ind1 < 0) {
	    if (!arg_no_brace)
	        return -1;
	    ind0 = str.find_first_not_of(' ', ind0);
	    if (ind0 < 0)
	        throw scan_err("command_arg(): end of file!");
	    if (str[ind0] == '\\') {
	        command_name(arg, str, ind0);
	        arg = '\\' + arg;
	        ind0 += arg.size();
	    }
	    else {
	        arg = str[ind0]; ++ind0;
	    }
	    return ind0;
	}
	ind0 = ind1;
	ind1 = pair_brace(str, ind0 - 1);
	arg = str.substr(ind0, ind1 - ind0);
	if (trim)
	    slisc::trim(arg);
	return ind1 + 1;
}

// find command with a specific 1st arguments
// i.e. \begin{equation}
// return the index of '\', return -1 if not found
inline Long find_command_spec(Str_I str, Str_I name, Str_I arg1, Long_I start)
{
	Long ind0 = start;
	Str arg1_;
	while (true) {
	    ind0 = find_command(str, name, ind0);
	    if (ind0 < 0)
	        return -1;
	    if (command_arg(arg1_, str, ind0, 0) == -1) {
	        ++ind0; continue;
	    }
	    if (arg1_ == arg1)
	        return ind0;
	    ++ind0;
	}
}

// skip an environment
// might return str.size()
inline Long skip_env(Str_I str, Long_I ind)
{
	Str name;
	command_arg(name, str, ind);
	Long ind0 = find_command_spec(str, "end", name, ind);
	if (ind0 < 0)
	    throw scan_err(u8"环境 " + name + u8" 没有 end");
	return skip_command(str, ind0, 1);
}

// find the intervals of all commands with 1 argument
// intervals are from '\' to '}'
// return the number of commands found
inline Long find_all_command_intv(Intvs_O intv, Str_I name, Str_I str)
{
	Long ind0 = 0;
	intv.clear();
	while (true) {
	    ind0 = find_command(str, name, ind0);
	    if (ind0 < 0)
	        return intv.size();
	    intv.pushL(ind0);
	    ind0 = skip_command(str, ind0, 1);
	    intv.pushR(ind0-1);
	}
}

// find a scope in str for environment named env
// return number of environments found
// if option = 'i', range starts from the next index of \begin{} and previous index of \end{}
// if option = 'o', range starts from '\' of \begin{} and '}' of \end{}
inline Long find_env(Intvs_O intv, Str_I str, Str_I env, char option = 'i')
{
	Long ind0{};
	if (option != 'i' && option != 'o')
	    throw scan_err(u8"内部错误： illegal option in find_env()!");
	intv.clear();
	// find comments
	while (true) {
	    ind0 = find_command_spec(str, "begin", env, ind0);
	    if (ind0 < 0)
	        return intv.size();
	    if (option == 'i') {
	        if (env == "example" || env == "exercise" || env == "definition" || env == "lemma" || env == "theorem" || env == "corollary")
	            ind0 = skip_command(str, ind0, 2);
	        else
	            ind0 = skip_command(str, ind0, 1);
	    }
	    intv.pushL(ind0);

	    ind0 = find_command_spec(str, "end", env, ind0);
	    if (ind0 < 0)
	        throw scan_err("find_env() failed!");
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
inline Long inside_env(Long_O right, Str_I str, Long_I ind, Long_I Narg = 1)
{
	if (expect(str, "\\begin", ind) < 0)
	    throw scan_err("inside_env() failed (1)!");
	Str env;
	command_arg(env, str, ind, 0);
	Long left = skip_command(str, ind, Narg);
	right = find_command_spec(str, "end", env, left);
	if (right < 0)
	    throw scan_err("inside_env() failed (2)!");
	return left;
}

// check if an index ind is in any of the evironments \begin{names[j]}...\end{names[j]}
// output iname of names[iname], -1 if return false
inline Bool index_in_env(Long& iname, Long ind, vecStr_I names, Str_I str)
{
	Intvs intv;
	for (Long i = 0; i < size(names); ++i) {
	    if (find_env(intv, str, names[i]) < 0) {
	        continue;
	    }
	    if (is_in(ind, intv)) {
	        iname = i;
	        return true;
	    }
	}
	iname = -1;
	return false;
}

// find the current environment an index is in
// commands that is an alias of environment will also count
inline Str current_env(Long_I ind, Str_I str)
{
	// check if in a qualified command
	vecStr cmd_env = {
	    "ali", "aligned",
	    "leftgroup", "aligned",
	    "pmat", "pmatrix",
	    "vmat", "vmatrix",
	    "bmat", "bmatrix",
	    "Bmat", "matrix",
	    "pentry", "mdframed",
	    "issues", "mdframed",
	    "addTODO", "mdframed"
	};
	for (Long i = 0; i < size(cmd_env); i += 2)
	    if (is_in_cmd(str, cmd_env[i], ind))
	        return cmd_env[i + 1];

	// check environment
	Long ind0 = find_command(str, "end");
	if (ind < 0)
	    return "document";
	Str arg;
	command_arg(arg, str, ind0);
	return arg;
}

inline Bool index_in_env(Long_I ind, Str_I name, Str_I str)
{
	Long iname;
	return index_in_env(iname, ind, { name.c_str() }, str);
}

// find interval of all "\lstinline *...*"
inline Long lstinline_intv(Intvs_O intv, Str_I str)
{
	Long N{}, ind0{}, ind1{}, ind2{};
	char dlm;
	intv.clear();
	while (true) {
	    ind0 = find_command(str, "lstinline", ind0);
	    if (ind0 < 0)
	        break;
	    ind1 = str.find_first_not_of(' ', ind0 + 10);
	    if (ind1 < 0)
	        throw scan_err("lstinline_intv() failed (1)!");
	    dlm = str[ind1];
	    ind2 = find(str, dlm, ind1 + 1);
	    if (ind2 < 0)
	        throw scan_err("lstinline_intv() failed (2)!");
	    intv.pushL(ind0); intv.pushR(ind2);
	    ind0 = ind2 + 1;
	    ++N;
	}
	return N;
}

// find interval of all "\verb *...*"
inline Long verb_intv(Intvs_O intv, Str_I str)
{
	Long N{}, ind0{}, ind1{}, ind2{};
	char dlm;
	intv.clear();
	while (true) {
	    ind0 = find_command(str, "verb", ind0);
	    if (ind0 < 0)
	        break;
	    ind1 = str.find_first_not_of(' ', ind0 + 5);
	    if (ind1 < 0)
	        throw scan_err("verb_intv() failed (1)!");
	    dlm = str[ind1];
	    ind2 = find(str, dlm, ind1 + 1);
	    if (ind2 < 0)
	        throw scan_err("verb_intv() failed (2)!");
	    intv.pushL(ind0); intv.pushR(ind2);
	    ind0 = ind2 + 1;
	    ++N;
	}
	return N;
}

// find the range of inline equations using $...$
// if option = 'i', intervals does not include $, if 'o', it does.
// return the number of $...$ environments found.
// "$$" with nothing inside will be ignored
inline Long find_single_dollar_eq(Intvs_O intv, Str_I str, char option = 'i')
{
	intv.clear();
	Long N{}; // number of $$
	Long ind0{};
	while (true) {
	    ind0 = find(str, "$", ind0);
	    if (ind0 < 0)
	        break;
	    if (ind0 > 0 && str[ind0 - 1] == '\\') { // escaped
	        ++ind0; continue;
	    }
	    if (ind0 < size(str) - 1 && str[ind0 + 1] == '$') { // ignore $$
	        ind0 += 2; continue;
	    }
	    intv.push_back(ind0);
	    ++ind0; ++N;
	}
	if (N % 2 != 0) {
	    throw scan_err(u8"行内公式 $ 符号不匹配");
	}
	N /= 2;
	if (option == 'i' && N > 0) {
	    for (Long i = 0; i < N; ++i) {
	        ++intv.L(i); --intv.R(i);
	    }
	}
	return N;
}

// find the range of inline equations using \(...\)
// if option = 'i', intervals does not include \( and \), if 'o', it does.
// return the number of \(...\) environments found.
inline Long find_paren_eq(Intvs_O intv, Str_I str, char option = 'i')
{
	intv.clear();
	Long N = 0; // number of \(...\)
	Long ind0 = -1;
	while (true) {
	    // find \(
	    ind0 = find(str, "\\(", ++ind0);
	    if (ind0 < 0) break;
	    // `\\(` means a line break and (, 
	    //     whether inside or outside equation env
	    if (ind0 > 0 && str[ind0-1] == '\\') continue;
	    intv.pushL(ind0);
	    // find `\)`, ignore `\\)`
	    while (true) {
	        ind0 = find(str, "\\)", ++ind0);
	        if (ind0 < 0)
	            throw scan_err(u8"\\( 没有找到匹配的 \\)");
	        if (ind0 > 0 && str[ind0-1] == '\\') continue;
	        intv.pushR(ind0+1); break;
	    }
	}
	N /= 2;
	if (option == 'i' && N > 0)
	    for (Long i = 0; i < N; ++i)
	        intv.L(i)+=2, intv.R(i)-=2;
	return N;
}

// combine find_single_dollar_eq() and find_paren_eq()
inline void find_inline_eq(Intvs_O intv, Str_I str, char option = 'i')
{
	Intvs intv1;
	find_single_dollar_eq(intv, str, option);
	find_paren_eq(intv1, str, option);
	combine(intv, intv1);
}

// TODO: find one instead of all
// TODO: find one using FindComBrace
// Long FindAllBegin

// TODO: delete this
// Find "\begin{env}" or "\begin{env}{}" (option = '2')
// output interval from '\' to '}'
// return number found
// use FindAllBegin
inline Long FindAllBegin(Intvs_O intv, Str_I env, Str_I str, char option)
{
	intv.clear();
	Long N{}, ind0{}, ind1;
	while (true) {
	    ind1 = find(str, "\\begin", ind0);
	    if (ind1 < 0)
	        return N;
	    ind0 = expect(str, "{", ind1 + 6);
	    if (expect(str, env, ind0) < 0)
	        continue;
	    ++N; intv.pushL(ind1);
	    ind0 = pair_brace(str, ind0 - 1);
	    if (option == '1')
	        intv.pushR(ind0);
	    ind0 = expect(str, "{", ind0 + 1);
	    if (ind0 < 0) {
	        throw scan_err(u8"内部错误： FindAllBegin(): expecting {}{}!");
	    }
	    ind0 = pair_brace(str, ind0 - 1);
	    intv.pushR(ind0);
	}
}

// TODO: delete this
// Find "\end{env}"
// output ranges to intv, from '\' to '}'
// return number found
inline Long FindEnd(Intvs_O intv, Str_I env, Str_I str)
{
	intv.clear();
	Long N{}, ind0{};
	Str env1;
	while (true) {
	    ind0 = find_command(str, "end", ind0+1);
	    if (ind0 < 0)
	        return N;
	    command_arg(env1, str, ind0);
	    if (env != env1)
	        continue;
	    ++N;
	    intv.pushL(ind0);
	    ind0 = skip_command(str, ind0, 1) - 1;
	    intv.pushR(ind0);
	}
}

// find $$...$$ equation environments
// assuming comments and verbatims are removed
// if option == 'o' interval includes all dollar signs, if option == 'i', include only what's inside
inline void find_double_dollar_eq(Intvs_O intv, Str_I str, char option = 'i')
{
	intv.clear();
	Long ind0 = 0;
	while (true) {
	    ind0 = find(str, "$$", ind0);
	    if (ind0 < 0)
	        return;
	    if (option == 'i')
	        intv.pushL(ind0+2);
	    else
	        intv.pushL(ind0);
	    ind0 = find(str, "$$", ind0+2);
	    if (ind0 < 0)
	        throw scan_err(u8"$$...$$ 公式环境不闭合");
	    if (option == 'i')
	        intv.pushR(ind0 - 1);
	    else
	        intv.pushR(ind0 + 1);
	    ++ind0;
	}
}

// find the range of display equations using \[...\]
// if option = 'i', intervals does not include \[ and \], if 'o', it does.
// return the number of \[...\] environments found.
inline Long find_sqr_bracket_eq(Intvs_O intv, Str_I str, char option = 'i')
{
	intv.clear();
	Long N; // number of \[...\]
	Long ind0 = -1;
	while (1) {
	    // find \[
	    ind0 = find(str, "\\[", ++ind0);
	    if (ind0 < 0) break;
	    if (ind0 > 0 && str[ind0-1] == '\\')
	        continue; // `\\[3pt]` might be used in split environment
	    intv.pushL(ind0);
	    // find \]
	    ind0 = find(str, "\\]", ++ind0);
	    if (ind0 < 0)
	        throw scan_err(u8"\\[ 没有找到匹配的 \\]");
	    if (ind0 > 0 && str[ind0-1] == '\\')
	        throw scan_err(u8"非法命令： \\\\]");
	    intv.pushR(ind0+1);
	}
	N = intv.size();
	if (option == 'i' && N > 0)
	    for (Long i = 0; i < N; ++i)
	        intv.L(i)+=2, intv.R(i)-=2;
	return N;
}

// find display equations
inline void find_display_eq(Intvs_O intv, Str_I str, char option = 'i')
{
	Intvs intv1;
	intv.clear();
	find_double_dollar_eq(intv, str, option);
	find_sqr_bracket_eq(intv1, str, option);
	combine(intv, intv1);
	find_env(intv1, str, "equation", option);
	combine(intv, intv1);
	find_env(intv1, str, "align", option);
	combine(intv, intv1);
	find_env(intv1, str, "gather", option);
	combine(intv, intv1);
}

// in display equation, check if `~,`, `~.` exist,
// or if a single `~` exist in the end (except spaces and returns)
inline void check_display_eq_punc(Str_I str)
{
	Intvs intv;
	find_display_eq(intv, str);
	Str tmp;
	for (Long i = 0; i < intv.size(); ++i) {
		Long start = intv.L(i), end = intv.R(i);
		Long j;
		for (j = start; j < end; ++j) {
			char c = str[j], c1 = str[j+1];
			if (c == '~' && (c1 == '.' || c1 == ','))
				break;
		}
		if (j == end) {
			for (j = end; j >= start; --j) {
				char c = str[j];
				if (c == ' ' || c == '\n') continue;
				if (c == '~') break;
				tmp = u8R"(根据编写规范，请在行间公式最后添加标点（如 "~," 或 "~."）， 详见 Sample.tex。
如果的确没必要，请在最后添加 "~"。 当前公式为：
)";
				tmp << str.substr(start, end+1-start);
				SLS_WARN(tmp);
				throw scan_err(tmp);
			}
		}
	}
}

// 在 latex 中， 空行代表分段
// 有些人不熟悉 latex 为了让代码看起来舒服，在代码前后都加空行
// 在 pdf 中，公式前面分段会让公式上方的间距变大
// 所以这个函数要求如果你真的想在公式前后分段， 就空两行
inline void check_display_eq_paragraph(Str_I str)
{
	Str tmp;
	u8_iter it0(str);
	Intvs intv_o;
	find_display_eq(intv_o, str, 'o');
	for (Long i = 0; i < intv_o.size(); ++i) {
		try {
			// 公式前面是否有空行
			it0 = intv_o.L(i);
			do { --it0; } while (*it0 == " ");
			if (*it0 == "\n") { // 第 1 个换行符
				do { --it0; } while (*it0 == " ");
				if (*it0 == "\n") { // 第 2 个换行符（至少空一行）
					do { --it0; } while (*it0 == " ");
					if (*it0 != "\n") { // 只空一行
						throw scan_err(u8"在 latex 中，空行表示分段，而在公式前分段并不常见。如果你确实要这么做，请在公式前空两行以确认。 当前公式： \n"
									   + str.substr(intv_o.L(i), intv_o.R(i) + 1 - intv_o.L(i)));
					}
				}
			} else {
				throw scan_err(u8"根据小时百科格式规范，行间公式代码必须另起一行。 当前公式：\n"
							   + str.substr(intv_o.L(i), intv_o.R(i) + 1 - intv_o.L(i)));
			}

			// 公式后面是否有空行
			vecStr excepts = {"\\subs", "\\item", "\\end{", "\\begin{ex", "\\begin{f",
							  "\\begin{t"}; // 若公式后面是这些字符串， 则不警告
			it0 = intv_o.R(i);
			do { ++it0; } while (*it0 == " ");
			if (*it0 == "\n") { // 第 1 个换行符
				do { ++it0; } while (*it0 == " ");
				if (*it0 == "\n") { // 第 2 个换行符（至少空一行）
					do { ++it0; } while (*it0 == " ");
					if (*it0 != "\n") { // 只空一行
						bool excepted = false;
						for (auto &e: excepts)
							if (str.substr(it0, size(e)) == e)
								excepted = true;
						if (!excepted)
							throw scan_err(u8"在 latex 中，空行表示分段。有些作者在每个公式后面都随手空行，这是错的。如果你的确想要在公式后面分段，请空两行以确认。 当前公式： \n"
										   + str.substr(intv_o.L(i), intv_o.R(i) + 1 - intv_o.L(i)));
					}
				}
			} else {
				throw scan_err(u8"根据小时百科格式规范，行间公式代码结束后必须另起一行。 当前公式：\n"
							   + str.substr(intv_o.L(i), intv_o.R(i) + 1 - intv_o.L(i)));
			}
		}
		catch (const std::out_of_range& e) {
			continue;
		}
	}
}

// forbid empty line in equations
inline void check_eq_empty_line(Str_I str)
{
	Intvs intv, intv1;
	find_inline_eq(intv, str);
	find_display_eq(intv1, str);
	combine(intv, intv1);
	Str tmp;
	for (Long i = intv.size() - 1; i >= 0; --i) {
	    for (Long j = intv.L(i); j < intv.R(i); ++j) {
	        if (str[j] == '\n' && str[j+1] == '\n')
	            throw scan_err(u8"公式中禁止空行：" + str.substr(j, 40));
	    }
	}
}

// forbid non-ascii char in equations (except in \text)
inline void check_eq_ascii(Str_I str)
{
	Intvs intv, intv1;
	find_inline_eq(intv, str);
	find_display_eq(intv1, str);
	combine(intv, intv1);
	Str tmp;
	for (Long i = intv.size() - 1; i >= 0; --i) {
	    for (Long j = intv.L(i); j <= intv.R(i); ++j) {
	        if (!is_ascii(str[j]) && !is_in_cmd(str, "text", j))
	            throw scan_err(u8"公式中只能包含 ascii 字符： 不能有全角标点，汉字，希腊字母等， \\text{...} 命令内除外：" + str.substr(j, 40));
	    }
	}
}

// Find normal text range
inline Long FindNormalText(Intvs_O intvNorm, Str_I str)
{
	Intvs intv, intv1;
	// verbatim
	lstinline_intv(intv, str);
	verb_intv(intv1, str);
	combine(intv, intv1);
	find_env(intv1, str, "lstlisting", 'o');
	combine(intv, intv1);
	// comments
	find_comments(intv1, str, "%");
	combine(intv, intv1);
	// inline equations
	find_inline_eq(intv1, str, 'o');
	combine(intv, intv1);
	// display equation environments
	find_display_eq(intv1, str, 'o');
	combine(intv, intv1);
	// command environments
	find_env(intv1, str, "Command", 'o');
	combine(intv, intv1);
	// texttt command
	find_all_command_intv(intv1, "texttt", str);
	combine(intv, intv1);
	// input command
	find_all_command_intv(intv1, "input", str);
	combine(intv, intv1);
	// Figure environments
	find_env(intv1, str, "figure", 'o');
	combine(intv, intv1);
	// Table environments
	find_env(intv1, str, "table", 'o');
	combine(intv, intv1);
	// subsubsection command
	// find_all_command_intv(intv1, "subsubsection", str);
	// combine(intv, intv1);

	//  \begin{example}{} and \end{example}
	FindAllBegin(intv1, "example", str, '2');
	combine(intv, intv1);
	FindEnd(intv1, "example", str);
	combine(intv, intv1);
	//  exer\begin{exercise}{} and \end{exercise}
	FindAllBegin(intv1, "exercise", str, '2');
	combine(intv, intv1);
	FindEnd(intv1, "exercise", str);
	combine(intv, intv1);
	//  exer\begin{theorem}{} and \end{theorem}
	FindAllBegin(intv1, "theorem", str, '2');
	combine(intv, intv1);
	FindEnd(intv1, "theorem", str);
	combine(intv, intv1);
	//  exer\begin{definition}{} and \end{definition}
	FindAllBegin(intv1, "definition", str, '2');
	combine(intv, intv1);
	FindEnd(intv1, "definition", str);
	combine(intv, intv1);
	//  exer\begin{lemma}{} and \end{lemma}
	FindAllBegin(intv1, "lemma", str, '2');
	combine(intv, intv1);
	FindEnd(intv1, "lemma", str);
	combine(intv, intv1);
	//  exer\begin{corollary}{} and \end{corollary}
	FindAllBegin(intv1, "corollary", str, '2');
	combine(intv, intv1);
	FindEnd(intv1, "corollary", str);
	combine(intv, intv1);
	// invert range
	return invert(intvNorm, intv, str.size());
}

// detect unnecessary braces and add "删除标记"
// return the number of braces pairs removed
inline Long RemoveBraces(vecLong_I ind_left, vecLong_I ind_right,
	vecLong_I ind_RmatchL, Str_IO str)
{
	unsigned i, N{};
	vecLong ind; // redundent right brace index
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
	        str.insert(ind[i], u8"删除标记");
	    }
	}
	return N;
}

// replace all \nameComm{...} with strLeft...strRight
// {} cannot be omitted
// must remove comments first
// return the number replaced
inline Long Command2Tag(Str_I nameComm, Str_I strLeft, Str_I strRight, Str_IO str)
{
	Long N{}, ind0{}, ind1{}, ind2{};
	while (true) {
	    ind0 = find(str, "\\" + nameComm, ind0);
	    if (ind0 < 0) break;
	    ind1 = ind0 + nameComm.size() + 1;
	    ind1 = expect(str, "{", ind1); --ind1;
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

// replace verbatim environments (\verb, \lstinline, lstlisting) with index number `ind`, to escape normal processing
// will ignore \lstinline and \verb in lstlisting environment
// doesn't matter if \lstinline is in latex comment
// TODO: it does matter if \begin{lstlisting} is in comment!
inline Long verbatim(vecStr_O str_verb, Str_IO str)
{
	Long ind0 = 0, ind1, ind2;
	char dlm;
	Str tmp;

	// verb
	while (true) {
	    ind0 = find_command(str, "verb", ind0);
	    if (ind0 < 0)
	        break;
	    ind1 = str.find_first_not_of(' ', ind0 + 5);
	    if (ind1 < 0)
	        throw scan_err(u8"\\verb 没有开始");
	    dlm = str[ind1];
	    if (dlm == '{')
	        throw scan_err(u8"\\verb 不支持 {...}， 请使用任何其他符号如 \\verb|...|， \\verb@...@");
	    ind2 = find(str, dlm, ind1 + 1);
	    if (ind2 < 0)
	        throw scan_err(u8"\\verb 没有闭合");
	    if (ind2 - ind1 == 1)
	        throw scan_err(u8"\\verb 不能为空");

	    str_verb.push_back(str.substr(ind1 + 1, ind2 - ind1 - 1));
	    Long ind10 = skip_char8(str, ind0, -1);
	    if (is_chinese(str, ind10))
	        tmp = ' ';
	    else
	        tmp.clear();
	    tmp += "\\verb|" + num2str(size(str_verb) - 1) + "|";
	    ind10 = skip_char8(str, ind2, 1);
	    if (is_chinese(str, ind10))
	        tmp += ' ';
	    str.replace(ind0, ind2 + 1 - ind0, tmp);
	    ind0 += 3;
	}

	// lstinline
	ind0 = 0;
	while (true) {
	    ind0 = find_command(str, "lstinline", ind0);
	    if (ind0 < 0)
	        break;
	    ind1 = str.find_first_not_of(' ', ind0 + 10);
	    if (ind1 < 0)
	        throw scan_err(u8"\\lstinline 没有开始");
	    dlm = str[ind1];
	    if (dlm == '{')
	        throw scan_err(u8"lstinline 不支持 {...}， 请使用任何其他符号如 \\lstinline|...|， \\lstinline@...@");
	    ind2 = find(str, dlm, ind1 + 1);
	    if (ind2 < 0)
	        throw scan_err(u8"\\lstinline 没有闭合");
	    if (ind2 - ind1 == 1)
	        throw scan_err(u8"\\lstinline 不能为空");

	    str_verb.push_back(str.substr(ind1 + 1, ind2 - ind1 - 1));

	    Long ind10 = skip_char8(str, ind0, -1);
	    if (is_chinese(str, ind10))
	        tmp = ' ';
	    else
	        tmp.clear();
	    tmp += "\\lstinline|" + num2str(size(str_verb) - 1) + "|";
	    ind10 = skip_char8(str, ind2, 1);
	    if (is_chinese(str, ind10))
	        tmp += ' ';
	    str.replace(ind0, ind2 + 1 - ind0, tmp);
	    ind0 += 3;
	}
	
	// lstlisting
	ind0 = 0;
	Intvs intvIn, intvOut;
	Str code;
	find_env(intvIn, str, "lstlisting", 'i');
	Long N = find_env(intvOut, str, "lstlisting", 'o');
	Str lang; // language
	for (Long i = N - 1; i >= 0; --i) {
	    // get language
	    ind0 = expect(str, "[", intvIn.L(i));
	    if (ind0 > 0) {
	        ind0 = pair_brace(str, ind0-1) + 1;
	    }
	    else {
	        ind0 = intvIn.L(i);
	    }
	    str_verb.push_back(str.substr(ind0, intvIn.R(i) + 1 - ind0));
	    trim(str_verb.back(), "\n ");
	    str.replace(ind0, intvIn.R(i) - ind0 + 1, "\n" + num2str(size(str_verb)-1) + "\n");
	}

	return str_verb.size();
}

// reverse process of verbatim()
inline Long verb_recover(Str_IO str, vecStr_IO str_verb)
{
	SLS_ERR("unfinished!");
	Long N = 0, ind0 = 0;
	Str ind_str, tmp;

	// verb
	while (1) {
	    ind0 = find(str, "\\verb|", ind0);
	    if (ind0 < 0)
	        break;
	    ind0 += 6; // one char after |
	    Long ind1 = find(str, '|', ind0 + 1);
	    if (ind1 < 0)
	        throw scan_err(u8"内部错误： verb_recover: \\verb|数字| 格式错误");

	    Long verb_ind = str2Llong(str.substr(ind0, ind1 - ind0));

	    // determin delimiter
	    char dlm;
	    if (find(str_verb[verb_ind], '|') >= 0) {
	        if (find(str_verb[verb_ind], '+') >= 0) {
	            if (find(str_verb[verb_ind], '-') >= 0) {
	                if (find(str_verb[verb_ind], '*') >= 0) {
	                    dlm = '^';
	                }
	                else
	                    dlm = '*';
	            }
	            else
	                dlm = '-';
	        }
	        else
	            dlm = '+';
	    }
	    else
	        dlm = '|';

	    str.replace(ind0, ind1 - ind0, dlm + str_verb[verb_ind] + dlm);
	}
	return N;
}

// replace `\lstinline|ind|` with `<code>str_verb[ind]</code>`
// return the number replaced
inline Long lstinline(Str_IO str, vecStr_IO str_verb)
{
	Long N = 0, ind0 = 0, ind1 = 0, ind2 = 0;
	Str ind_str, tmp;
	while (true) {
	    ind0 = find_command(str, "lstinline", ind0);
	    if (ind0 < 0)
	        break;
	    if (index_in_env(ind0, "lstlisting", str)) {
	        ++ind0; continue;
	    }
	    ind1 = expect(str, "|", ind0 + 10); --ind1;
	    if (ind1 < 0)
	        throw scan_err(u8"内部错误： expect `|index|` after `lstinline`");
	    ind2 = find(str, "|", ind1 + 1);
	    ind_str = str.substr(ind1 + 1, ind2 - ind1 - 1); trim(ind_str);
	    Long ind = str2Llong(ind_str);
	replace(str_verb[ind], "&", "&amp;");
	    replace(str_verb[ind], "<", "&lt;");
	    replace(str_verb[ind], ">", "&gt;");
	    tmp = "<code>" + str_verb[ind] + "</code>";
	    str.replace(ind0, ind2 - ind0 + 1, tmp);
	    ind0 += tmp.size();
	    ++N;
	}
	return N;
}

// replace `\verb|ind|` with `<code>str_verb[ind]</code>`
// return the number replaced
inline Long verb(Str_IO str, vecStr_IO str_verb)
{
	Long N = 0, ind0 = 0, ind1 = 0, ind2 = 0;
	Str ind_str, tmp;
	while (true) {
	    ind0 = find_command(str, "verb", ind0);
	    if (ind0 < 0)
	        break;
	    if (index_in_env(ind0, "lstlisting", str)) {
	        ++ind0; continue;
	    }
	    ind1 = expect(str, "|", ind0 + 5); --ind1;
	    if (ind1 < 0)
	        throw scan_err(u8"内部错误： expect `|index|` after `verb`");
	    ind2 = find(str, "|", ind1 + 1);
	    ind_str = str.substr(ind1 + 1, ind2 - ind1 - 1); trim(ind_str);
	    Long ind = str2Llong(ind_str);
	replace(str_verb[ind], "&", "&amp;");
	    replace(str_verb[ind], "<", "&lt;");
	    replace(str_verb[ind], ">", "&gt;");
	    tmp = "<code>" + str_verb[ind] + "</code>";
	    str.replace(ind0, ind2 - ind0 + 1, tmp);
	    ind0 += tmp.size();
	    ++N;
	}
	return N;
}

// replace one nameEnv environment with strLeft...strRight
// must remove comments first
// return the increase in str.size()
inline Long Env2Tag(Str_IO str, Long_I ind, Str_I strLeft, Str_I strRight, Long_I NargBegin = 1)
{
	Long ind1 = skip_command(str, ind, NargBegin);
	Str envName;
	command_arg(envName, str, ind);

	Long ind2 = find_command_spec(str, "end", envName, ind);
	Long ind3 = skip_command(str, ind2, 1);
	Long Nbegin = ind1 - ind;
	Long Nend = ind3 - ind2;

	str.replace(ind2, Nend, strRight);
	str.replace(ind, Nbegin, strLeft);
	return strRight.size() + strLeft.size() - Nend - Nbegin;
}

// replace all nameEnv environments with strLeft...strRight
// must remove comments first
inline Long Env2Tag(Str_I nameEnv, Str_I strLeft, Str_I strRight, Str_IO str)
{
	Long i{}, N{}, Nenv;
	Intvs intvEnvOut, intvEnvIn;
	Nenv = find_env(intvEnvIn, str, nameEnv, 'i');
	find_env(intvEnvOut, str, nameEnv, 'o');
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
