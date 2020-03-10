// tex parser utilities
// always remove comments first
#pragma once
#include "../SLISC/parser.h"
#include <cassert>
#include "../SLISC/search.h"

namespace slisc {

// find text command '\name', return the index of '\'
// return the index of "name.back()", return -1 if not found
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

// find one of multiple commands
// return -1 if not found
inline Long find_command(Long_O ikey, Str32_I str, vecStr32_I names, Long_I start)
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
inline Long skip_command(Str32_I str, Long_I ind, Long_I Narg = 0, Bool_I omit_skip_opt = true, Bool_I arg_no_brace = false, Bool_I skip_star = true)
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
            throw Str32(U"skip_command() failed!");
    }
    // skip *
    Long ind0 = -1;
    if (skip_star)
        ind0 = expect(str, U"*", i);
    if (ind0 < 0)
        ind0 = i;
    // skip optional argument
    Long ind1 = expect(str, U"[", ind0), ind2;
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
        ind0 = expect(str, U"{", ind0);
        if (ind0 > 0) {
            ind0 = pair_brace(str, ind0-1) + 1;
            continue;
        }
        if (!arg_no_brace)
            throw Str32(U"skip_command(): '{' not found!");
        ind0 = str.find_first_not_of(U' ', ind0);
        if (ind0 < 0)
            throw Str32(U"skip_command(): end of file!");
        ++ind0;
    }
    return ind0;
}

// get the name of the command starting at str[ind]
// does not include '\\'
inline void command_name(Str32_O name, Str32_I str, Long_I ind)
{
    if (str[ind] != U'\\')
        throw Str32(U"not a command!");
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
inline Bool command_star(Str32_I str, Long_I ind)
{
    if (str[ind] != U'\\')
        throw Str32(U"command_star(): not a command!");
    Long ind0 = skip_command(str, ind, 0, false);
    if (str[ind0 - 1] == U'*')
        return true;
    return false;
}

// check if command has optional argument in []
inline Bool command_has_opt(Str32_I str, Long_I ind, Bool_I trim = true)
{
    Str32 arg;
    if (str[ind] != U'\\')
        throw Str32(U"has_opt(): not a command!");
    Long ind0 = skip_command(str, ind, 0, false);
    ind0 = expect(str, U"[", ind0);
    if (ind0 < 0)
        return false;
    return true;
}

// get optional argument of command in [], trim result by default
// return empty string if not found
inline Str32 command_opt(Str32_I str, Long_I ind, Bool_I trim = true)
{
    Str32 arg;
    if (str[ind] != U'\\')
        throw Str32(U"has_opt(): not a command!");
    Long ind0 = skip_command(str, ind, 0, false);
    ind0 = expect(str, U"[", ind0);
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
inline Long command_Narg(Str32_I str, Long_I ind)
{
    if (str[ind] != U'\\')
        throw Str32(U"not a command!");
    Long Narg = 0, ind0 = skip_command(str, ind, 0, false);
    // skip optional argument
    Long ind1 = expect(str, U"[", ind0) - 1;
    if (ind1 > 0) {
        ind0 = pair_brace(str, ind1) + 1;
        ++Narg;
    }
    if (expect(str, U"(", ind0) > 0) {
        Narg = -Narg - 1;
    }
    else {
        while (true) {
            ind0 = expect(str, U"{", ind0) - 1;
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
inline Long command_arg(Str32_O arg, Str32_I str, Long_I ind, Long_I i = 0, Bool_I trim = true, Bool_I ignore_opt = false, Bool_I arg_no_brace = false)
{
    Long ind0 = ind, ind1, i1 = i;
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
        throw Str32(U"command_arg(): i < 0 not allowed!");
    }
    
    ind1 = expect(str, U"{", ind0);
    if (ind1 < 0) {
        if (!arg_no_brace)
            return -1;
        ind0 = str.find_first_not_of(U' ', ind0);
        if (ind0 < 0)
            throw Str32(U"command_arg(): end of file!");
        if (str[ind0] == U'\\') {
            command_name(arg, str, ind0);
            arg = U'\\' + arg;
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
inline Long find_command_spec(Str32_I str, Str32_I name, Str32_I arg1, Long_I start)
{
    Long ind0 = start;
    Str32 arg1_;
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
inline Long skip_env(Str32_I str, Long_I ind)
{
    Str32 name;
    command_arg(name, str, ind);
    Long ind0 = find_command_spec(str, U"end", name, ind);
    return skip_command(str, ind0, 1);
}

// find the intervals of all commands with 1 argument
// intervals are from '\' to '}'
// return the number of commands found
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
        intv.pushR(ind0-1);
    }
}

// find a scope in str for environment named env
// return number of environments found
// if option = 'i', range starts from the next index of \begin{} and previous index of \end{}
// if option = 'o', range starts from '\' of \begin{} and '}' of \end{}
inline Long find_env(Intvs_O intv, Str32_I str, Str32_I env, Char option = 'i')
{
    Long ind0{}, ind1{}, ind2{}, ind3{};
    Long N{}; // number of environments found
    if (option != 'i' && option != 'o')
        throw Str32(U"内部错误： illegal option in find_env()!");
    intv.clear();
    // find comments
    while (true) {
        ind0 = find_command_spec(str, U"begin", env, ind0);
        if (ind0 < 0)
            return intv.size();
        if (option == 'i') {
            if (env == U"example" || env == U"exercise" || env == U"definition" || env == U"lemma" || env == U"theorem" || env == U"corollary")
                ind0 = skip_command(str, ind0, 2);
            else
                ind0 = skip_command(str, ind0, 1);
        }
        intv.pushL(ind0);

        ind0 = find_command_spec(str, U"end", env, ind0);
        if (ind0 < 0)
            throw Str32(U"find_env() failed!");
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
inline Long inside_env(Long_O right, Str32_I str, Long_I ind, Long_I Narg = 1)
{
    if (expect(str, U"\\begin", ind) < 0)
        throw Str32(U"inside_env() failed (1)!");
    Str32 env;
    command_arg(env, str, ind, 0);
    Long left = skip_command(str, ind, Narg);
    right = find_command_spec(str, U"end", env, left);
    if (right < 0)
        throw Str32(U"inside_env() failed (2)!");
    return left;
}

// see if an index ind is in any of the evironments \begin{names[j]}...\end{names[j]}
// output iname of names[iname], -1 if return false
inline Bool index_in_env(Long& iname, Long ind, vecStr32_I names, Str32_I str)
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

inline Bool index_in_env(Long_I ind, Str32_I name, Str32_I str)
{
    Long iname;
    return index_in_env(iname, ind, { name.c_str() }, str);
}

// find interval of all "\lstinline *...*"
inline Long lstinline_intv(Intvs_O intv, Str32_I str)
{
    Long N{}, ind0{}, ind1{}, ind2{};
    Char32 dlm;
    intv.clear();
    while (true) {
        ind0 = find_command(str, U"lstinline", ind0);
        if (ind0 < 0)
            break;
        ind1 = str.find_first_not_of(U' ', ind0 + 10);
        if (ind1 < 0)
            throw Str32(U"lstinline_intv() failed (1)!");
        dlm = str[ind1];
        ind2 = str.find(dlm, ind1 + 1);
        if (ind2 < 0)
            throw Str32(U"lstinline_intv() failed (2)!");
        intv.pushL(ind0); intv.pushR(ind2);
        ind0 = ind2 + 1;
        ++N;
    }
    return N;
}

// find the range of inline equations using $$
// if option = 'i', intervals does not include $, if 'o', it does.
// return the number of $$ environments found.
inline Long find_inline_eq(Intvs_O intv, Str32_I str, Char option = 'i')
{
    intv.clear();
    Long N{}; // number of $$
    Long ind0{};
    while (true) {
        ind0 = str.find(U"$", ind0);
        if (ind0 < 0)
            break;
        if (ind0 > 0 && str.at(ind0 - 1) == '\\') { // escaped
            ++ind0; continue;
        }
        intv.push_back(ind0);
        ++ind0; ++N;
    }
    if (N % 2 != 0) {
        throw Str32(U"行内公式 $ 符号不匹配！");
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
// return number found
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
            throw Str32(U"内部错误： FindAllBegin(): expecting {}{}!");
        }
        ind0 = pair_brace(str, ind0 - 1);
        intv.pushR(ind0);
    }
}

// TODO: delete this
// Find "\end{env}"
// output ranges to intv, from '\' to '}'
// return number found
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
inline Long FindNormalText(Intvs_O indNorm, Str32_I str)
{
    Intvs intv, intv1;
    lstinline_intv(intv, str);
    find_env(intv1, str, U"lstlisting", 'o');
    combine(intv, intv1);
    // comments
    find_comments(intv1, str, U"%");
    combine(intv, intv1);
    // inline equation environments
    find_inline_eq(intv1, str, 'o');
    combine(intv, intv1);
    // equation environments
    find_env(intv1, str, U"equation", 'o');
    combine(intv, intv1);
    // command environments
    find_env(intv1, str, U"Command", 'o');
    combine(intv, intv1);
    // gather environments
    find_env(intv1, str, U"gather", 'o');
    combine(intv, intv1);
    // align environments (not "aligned")
    find_env(intv1, str, U"align", 'o');
    combine(intv, intv1);
    // texttt command
    find_all_command_intv(intv1, U"texttt", str);
    combine(intv, intv1);
    // input command
    find_all_command_intv(intv1, U"input", str);
    combine(intv, intv1);
    // Figure environments
    find_env(intv1, str, U"figure", 'o');
    combine(intv, intv1);
    // Table environments
    find_env(intv1, str, U"table", 'o');
    combine(intv, intv1);
    // subsubsection command
    find_all_command_intv(intv1, U"subsubsection", str);
    combine(intv, intv1);

    //  \begin{example}{} and \end{example}
    FindAllBegin(intv1, U"example", str, '2');
    combine(intv, intv1);
    FindEnd(intv1, U"example", str);
    combine(intv, intv1);
    //  exer\begin{exercise}{} and \end{exercise}
    FindAllBegin(intv1, U"exercise", str, '2');
    combine(intv, intv1);
    FindEnd(intv1, U"exercise", str);
    combine(intv, intv1);
    //  exer\begin{theorem}{} and \end{theorem}
    FindAllBegin(intv1, U"theorem", str, '2');
    combine(intv, intv1);
    FindEnd(intv1, U"theorem", str);
    combine(intv, intv1);
    //  exer\begin{definition}{} and \end{definition}
    FindAllBegin(intv1, U"definition", str, '2');
    combine(intv, intv1);
    FindEnd(intv1, U"definition", str);
    combine(intv, intv1);
    //  exer\begin{lemma}{} and \end{lemma}
    FindAllBegin(intv1, U"lemma", str, '2');
    combine(intv, intv1);
    FindEnd(intv1, U"lemma", str);
    combine(intv, intv1);
    //  exer\begin{corollary}{} and \end{corollary}
    FindAllBegin(intv1, U"corollary", str, '2');
    combine(intv, intv1);
    FindEnd(intv1, U"corollary", str);
    combine(intv, intv1);
    // invert range
    return invert(indNorm, intv, str.size());
}

// detect unnecessary braces and add "删除标记"
// return the number of braces pairs removed
inline Long RemoveBraces(vecLong_I ind_left, vecLong_I ind_right,
    vecLong_I ind_RmatchL, Str32_IO str)
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

// replace verbatim environments with index number `ind`, to escape normal processing
// will ignore \lstinline in lstlisting environment
// doesn't matter if \lstinline is in latex comment
// TODO: it does matter if \begin{lstlisting} is in comment!
inline Long verbatim(vecStr32_O str_verb, Str32_IO str)
{
    Long ind0 = 0, ind1, ind2;
    Char32 dlm;
    // lstinline
    while (true) {
        ind0 = find_command(str, U"lstinline", ind0);
        if (ind0 < 0)
            break;
        if (index_in_env(ind0, U"lstlisting", str)) {
            ++ind0; continue;
        }
        ind1 = str.find_first_not_of(U' ', ind0 + 10);
        if (ind1 < 0)
            throw Str32(U"\\lstinline 没有开始！");
        dlm = str[ind1];
        if (dlm == U'{')
            throw Str32(U"lstinline 不支持 {...}， 请使用任何其他符号如 \\lstinline|...|， \\lstinline@...@");
        ind2 = str.find(dlm, ind1 + 1);
        if (ind2 < 0)
            throw Str32(U"\\lstinline 没有闭合！");
        if (ind2 - ind1 == 1)
            throw Str32(U"\\lstinline 不能为空");

        str_verb.push_back(str.substr(ind1 + 1, ind2 - ind1 - 1));
        str.replace(ind0 + 10, ind2 - (ind0+10) + 1, U"|" + num2str32(size(str_verb)-1) + U"|");
        ++ind0;
    }
    
    // process lstlisting
    ind0 = 0;
    Intvs intvIn, intvOut;
    Str32 code;
    find_env(intvIn, str, U"lstlisting", 'i');
    Long N = find_env(intvOut, str, U"lstlisting", 'o');
    Str32 lang = U""; // language
    for (Long i = N - 1; i >= 0; --i) {
        // get language
        ind0 = expect(str, U"[", intvIn.L(i));
        if (ind0 > 0) {
            ind0 = pair_brace(str, ind0-1) + 1;
        }
        else {
            ind0 = intvIn.L(i);
        }
        str_verb.push_back(str.substr(ind0, intvIn.R(i) + 1 - ind0));
        trim(str_verb.back(), U"\n ");
        str.replace(ind0, intvIn.R(i) - ind0 + 1, U"\n" + num2str32(size(str_verb)-1) + U"\n");
    }

    return str_verb.size();
}

// replace `\lstinline{ind}` with `<code>str_verb[ind]</code>`
// return the number replaced
// output the interval of replaced code
inline Long lstinline(Intvs_O intv, Str32_IO str, vecStr32_IO str_verb)
{
    Long N = 0, ind0 = 0, ind1 = 0, ind2 = 0;
    Str32 ind_str, tmp;
    intv.clear();
    while (true) {
        ind0 = find_command(str, U"lstinline", ind0);
        if (ind0 < 0)
            break;
        if (index_in_env(ind0, U"lstlisting", str)) {
            ++ind0; continue;
        }
        ind1 = expect(str, U"|", ind0 + 10); --ind1;
        if (ind1 < 0)
            throw Str32(U"内部错误： expect `|index|` after `lstinline`");
        ind2 = str.find(U"|", ind1 + 1);
        ind_str = str.substr(ind1 + 1, ind2 - ind1 - 1); trim(ind_str);
        Long ind = str2int(ind_str);
        replace(str_verb[ind], U"<", U"&lt");
        replace(str_verb[ind], U">", U"&gt");
        tmp = U"<code>" + str_verb[ind] + U"</code>";
        str.replace(ind0, ind2 - ind0 + 1, tmp);
        intv.pushL(ind0); intv.pushR(ind0 + size(tmp) - 1);
        ind0 += tmp.size();
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
