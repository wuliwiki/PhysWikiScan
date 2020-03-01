#pragma once
#include "tex.h"
#include "../SLISC/scalar_arith.h"

namespace slisc {

// ensure space around (CString)name
// return number of spaces added
// will only check equation env. and inline equations
inline Long EnsureSpace(Str32_I name, Str32_IO str, Long start, Long end)
{
    Long N{}, ind0{ start };
    while (true) {
        ind0 = str.find(name, ind0);
        if (ind0 < 0 || ind0 > end) break;
        if (ind0 == 0 || str.at(ind0 - 1) != U' ') {
            str.insert(ind0, U" "); ++ind0; ++N;
        }
        ind0 += name.size();
        if (ind0 == str.size() || str.at(ind0) != U' ') {
            str.insert(ind0, U" "); ++N;
        }
    }
    return N;
}

// check if an index is in an HTML tag "<name>...</name>
inline Bool is_in_tag(Str32_I str, Str32_I name, Long_I ind)
{
    Long ind1 = str.rfind(U"<" + name + U">", ind);
    if (ind1 < 0)
        return false;
    Long ind2 = str.rfind(U"</" + name + U">", ind);
    if (ind2 < ind1)
        return true;
    return false;
}

// ensure space around '<' and '>' in equation env. and $$ env
// return number of spaces added
inline Long EqOmitTag(Str32_IO str)
{
    Long i{}, N{}, Nrange{};
    Intvs intv, indInline;
    find_env(intv, str, U"equation");
    find_inline_eq(indInline, str);
    Nrange = combine(intv, indInline);
    for (i = Nrange - 1; i >= 0; --i) {
        N += EnsureSpace(U"<", str, intv.L(i), intv.R(i));
        N += EnsureSpace(U">", str, intv.L(i), intv.R(i));
    }
    return N;
}

// detect "\name*" command and replace with "\nameStar"
// return number of commmands replaced
// must remove comments first
inline Long StarCommand(Str32_I name, Str32_IO str)
{
    Long N{}, ind0{}, ind1{};
    while (true) {
        ind0 = find_command(str, name, ind0);
        if (ind0 < 0)
            break;
        ind0 += name.size() + 1;
        ind1 = expect(str, U"*", ind0);
        if (ind1 < 0)
            continue;
        str.replace(ind0, ind1 - ind0, U"Star"); // delete * and spaces
        ++N;
    }
    return N;
}

// detect "\name" with variable parameters
// replace "\name{}{}" with "\nameTwo{}{}" and "\name{}{}{}" with "\nameThree{}{}{}"
// replace "\name[]{}{}" with "\nameTwo[]{}{}" and "\name[]{}{}{}" with "\nameThree[]{}{}{}"
// return number of commmands replaced
// maxVars  = 2 or 3
inline Long VarCommand(Str32_I name, Str32_IO str, Int_I maxVars)
{
    Long i{}, N{}, Nvar{}, ind0{}, ind1{};

    while (true) {
        ind0 = str.find(U"\\" + name, ind0 + 1);
        if (ind0 < 0) break;
        ind0 += name.size() + 1;
        ind1 = expect(str, U"[", ind0);
        if (ind1 > 0)
            ind1 = pair_brace(str, ind1 - 1, U']') + 1;
        else
            ind1 = ind0;
        ind1 = expect(str, U"{", ind1);
        if (ind1 < 0) continue;
        ind1 = pair_brace(str, ind1 - 1);
        ind1 = expect(str, U"{", ind1 + 1);
        if (ind1 < 0) continue;
        ind1 = pair_brace(str, ind1 - 1);
        if (maxVars == 2) { str.insert(ind0, U"Two"); ++N; continue; }
        ind1 = expect(str, U"{", ind1 + 1);
        if (ind1 < 0) {
            str.insert(ind0, U"Two"); ++N; continue;
        }
        str.insert(ind0, U"Three"); ++N; continue;
    }
    return N;
}

// replace e.g. `\sin(...)` with `\sin\left(...\right)`
// replace e.g. `\sin[n](...)` with `\sin^n\left(...\right)`
// \exp and \qty are special, also support `\exp[]`, `\exp{}`, `\qty[]`, `qty{}` auto resize
// see physics package for detail
inline Long fun_auto_size(Str32_IO str)
{
    // supports \cmd(), \cmd[]()
    vecStr32 keys = {U"sin", U"cos", U"tan", U"csc", U"sec", U"cot", U"sinh", U"cosh",
        U"tanh", U"arcsin", U"arccos", U"arctan", U"log", U"ln"};
    // supports \cmd(), \cmd[], \cmd{}
    vecStr32 keys1 = { U"exp", U"qty" };

    // TODO
}

// ==== directly implement \newcommand{}{} by replacing ====
// does not support multiple layer!
// braces can not be omitted for now, e.g. frac12
// matching order: from complicated to simple
// does not support more than 9 arguments (including optional arg)
inline Long newcommand(Str32_IO str)
{
    // rules (order matters for the same command name and format, match backward)
    // 1. cmd name | 2. format | 3. number of arguments (including optional) | 4. rule
    vecStr32 rules = {
        // `\cmd`
        U"I", U"", U"0", U"\\mathrm{i}",
        U"E", U"", U"0", U"\\mathrm{e}",
        U"Nabla", U"", U"0", U"\\boldsymbol{\\nabla}",
        U"Tr", U"", U"0", U"^{\\mathrm{T}}",
        U"Cj", U"", U"0", U"^*",
        U"Her", U"", U"0", U"^\\dagger",
        U"sinc", U"", U"0", U"\\operatorname{sinc}",
        U"Arctan", U"", U"0", U"\\operatorname{Arctan}",
        U"erfi", U"", U"0", U"\\operatorname{erfi}",
        U"vdot", U"", U"0", U"\\boldsymbol\\cdot",
        U"cross", U"", U"0", U"\\boldsymbol\\times",
        U"grad", U"", U"0", U"\\boldsymbol\\nabla",
        U"div", U"", U"0", U"\\boldsymbol{\\nabla}\\boldsymbol{\\cdot}"
        U"curl", U"", U"0", U"\\boldsymbol{\\nabla}\\boldsymbol{\\times}"
        U"laplacian", U"", U"0", U"\\boldsymbol{\\nabla}^2"
        U"Re", U"", U"0", U"\\mathrm{Re}",
        U"Im", U"", U"0", U"\\mathrm{Im}",
        U"opn", U"", U"0", U"\\operatorname"
        // `\cmd{}`
        U"qty", U"", U"1", U"\\left\\{{#1}\\right\\}",
        U"bvec", U"", U"1", U"\\boldsymbol{\\mathbf{#1}}",
        U"mat", U"", U"1", U"\\boldsymbol{\\mathbf{#1}}",
        U"ten", U"", U"1", U"\\boldsymbol{\\mathbf{#1}}",
        U"uvec", U"", U"1", U"\\hat{\\boldsymbol{\\mathbf{#1}}}",
        U"pmat", U"", U"1", U"\\begin{pmatrix}#1\\end{pmatrix}",
        U"ali", U"", U"1", U"\\begin{aligned}#1\\end{aligned}",
        U"leftgroup", U"", U"1", U"\\left\\{\\begin{aligned}#1\\end{aligned}\\right.",
        U"vmat", U"", U"1", U"\\begin{vmatrix}#1\\end{vmatrix}",
        U"Q", U"", U"1", U"\\hat{#1}",
        U"Qv", U"", U"1", U"\\hat{\\boldsymbol{\\mathbf{#1}}}",
        U"Si", U"", U"1", U"\\,\\mathrm{#1}",
        U"abs", U"", U"1", U"\\left\\lvert{#1}\\right\\rvert",
        U"eval", U"", U"1", U"\\left.{#1}\\right\\rvert",
        U"dd", U"", U"1", U"\\,\\mathrm{d}^{#1}",
        U"bra", U"", U"1", U"\\left\\langle{#1}\\right\\rvert",
        U"ket", U"", U"1", U"\\left\\lvert{#1}\\right\\rangle",
        U"braket", U"", U"1", U"\\left\\langle{#1}\\middle|{#1}\\right\\rangle",
        U"ev", U"", U"1", U"\\left\\langle{#1}\\right\\rangle",
        U"order", U"", U"1", U"\\mathcal{O}\\left(#1\\right)",
        U"bmat", U"", U"1", U"\\begin{bmatrix}#1\\end{bmatrix}",
        U"Bmat", U"", U"1", U"\\left\\{\\begin{matrix}#1\\end{matrix}\\right\\}",
        U"sumint", U"", U"1", U"\\int\\kern-1.4em\\sum",
        U"Q", U"", U"1", U"\\hat{#1}",
        U"norm", U"", U"1", U"\\left\\lVert{#1}\\right\\rVert",
        U"pdv", U"", U"1", U"\\frac{\\partial}{\\partial{#1}}",
        U"dd", U"", U"1", U"\\,\\mathrm{d}{#1}",
        U"dv", U"", U"1", U"\\frac{\\mathrm{d}}{\\mathrm{d}{#1}}",
        // `\cmd[]{}`
        U"pdv", U"[]", U"2", U"\\frac{\\partial^{#1}}{\\partial{#2}^{#1}}",
        U"dd", U"[]", U"2", U"\\,\\mathrm{d}^{#1}{#2}",
        U"dv", U"[]", U"2", U"\\frac{\\mathrm{d}^{#1}}{\\mathrm{d}{#2}^{#1}}",
        // `\cmd*{}`
        U"ev", U"*", U"1", U"\\langle{#1}\\rangle",
        U"braket", U"*", U"1", U"\\langle{#1}|{#1}\\rangle",
        U"ket", U"*", U"1", U"\\lvert{#1}\\rangle",
        U"bra", U"*", U"1", U"\\langle{#1}\\rvert",
        U"dv", U"*", U"1", U"\\mathrm{d}/\\mathrm{d}{#1}",
        // `\cmd*[]{}`
        U"dv", U"*[]", U"2", U"\\mathrm{d}^{#1}/\\mathrm{d}{#2}^{#1}",
        // non-standard: `\cmd()`
        U"qty", U"()", U"1", U"\\left({#1}\\right)",
        U"sin", U"()", U"1", U"\\sin\\left(#1\\right)",
        U"cos", U"()", U"1", U"\\cos\\left(#1\\right)",
        U"tan", U"()", U"1", U"\\tan\\left(#1\\right)",
        U"csc", U"()", U"1", U"\\csc\\left(#1\\right)",
        U"sec", U"()", U"1", U"\\sec\\left(#1\\right)",
        U"cot", U"()", U"1", U"\\cot\\left(#1\\right)",
        U"sinh", U"()", U"1", U"\\sinh\\left(#1\\right)",
        U"cosh", U"()", U"1", U"\\cosh\\left(#1\\right)",
        U"tanh", U"()", U"1", U"\\tanh\\left(#1\\right)",
        U"arcsin", U"()", U"1", U"\\arcsin\\left(#1\\right)",
        U"arccos", U"()", U"1", U"\\arccos\\left(#1\\right)",
        U"arctan", U"()", U"1", U"\\arctan\\left(#1\\right)",
        U"exp", U"()", U"1", U"\\exp\\left(#1\\right)",
        U"log", U"()", U"1", U"\\log\\left(#1\\right)"
        U"ln", U"()", U"1", U"\\ln\\left(#1\\right)"
        // non-standard: `\cmd[]`
        U"qty", U"[]", U"1", U"\\left[{#1}\\right]"
        // non-standard: `\cmd[]()`
        U"sin", U"[]()", U"2", U"\\sin^{#1}\\left(#2\\right)",
        U"cos", U"[]()", U"2", U"\\cos^{#1}\\left(#2\\right)",
        U"tan", U"[]()", U"2", U"\\tan^{#1}\\left(#2\\right)",
        U"csc", U"[]()", U"2", U"\\csc^{#1}\\left(#2\\right)",
        U"sec", U"[]()", U"2", U"\\sec^{#1}\\left(#2\\right)",
        U"cot", U"[]()", U"2", U"\\cot^{#1}\\left(#2\\right)",
        U"sinh", U"[]()", U"2", U"\\sinh^{#1}\\left(#2\\right)",
        U"cosh", U"[]()", U"2", U"\\cosh^{#1}\\left(#2\\right)",
        U"tanh", U"[]()", U"2", U"\\tanh^{#1}\\left(#2\\right)",
        U"arcsin", U"[]()", U"2", U"\\arcsin^{#1}\\left(#2\\right)",
        U"arccos", U"[]()", U"2", U"\\arccos^{#1}\\left(#2\\right)",
        U"arctan", U"[]()", U"2", U"\\arctan^{#1}\\left(#2\\right)",
        U"log", U"[]()", U"2", U"\\log^{#1}\\left(#2\\right)"
        U"ln", U"[]()", U"2", U"\\ln^{#1}\\left(#2\\right)"
        // `\cmd{}{}`
        U"comm", U"", U"2", U"\\left[{#1},{#2}\\right]",
        U"pb", U"", U"2", U"\\left\\{{#1},{#2}\\right\\}",
        U"braket", U"", U"2", U"\\left\\langle{#1}\\middle|{#2}\\right\\rangle",
        U"ev", U"", U"2", U"\\left\\langle{#2}\\middle|{#1}\\middle|{#2}\\right\\rangle",
        U"dv", U"", U"2", U"\\frac{\\mathrm{d}{#1}}{\\mathrm{d}{#2}}",
        U"pdv", U"", U"2", U"\\frac{\\partial {#1}}{\\partial {#2}}",
        // `\cmd*{}{}`
        U"braket", U"*", U"2", U"\\langle{#1}|{#2}\\rangle",
        U"ev", U"*", U"2", U"\\langle{#2}|{#1}|{#2}\\rangle",
        U"comm", U"*", U"2", U"[{#1},{#2}]",
        U"pb", U"*", U"2", U"\\{{#1},{#2}\\}",
        U"dv", U"*", U"2", U"\\mathrm{d}{#1}/\\mathrm{d}{#2}",
        U"pdv", U"*", U"2", U"\\partial^{#1}/\\partial{#2}^{#1}",
        // `\cmd[]{}{}`
        U"dv", U"[]", U"3", U"\\frac{\\mathrm{d}^{#1}{#2}}{\\mathrm{d}{#3}^{#1}}",
        U"pdv", U"[]", U"3", U"\\frac{\\partial^{#1}}{\\partial{#2}^{#1}}",
        // `\cmd*[]{}{}`
        U"dv", U"*[]", U"3", U"\\mathrm{d}^{#1}{#2}/\\mathrm{d}{#3}^{#1}",
        U"pdv", U"*[]", U"3", U"\\partial^{#1}{#2}/\\partial{#3}^{#1}"
        // `\cmd{}{}{}`
        U"pdv", U"3", U"\\frac{\\partial^2{#1}}{\\partial{#2}\\partial{#3}}",
        U"mel", U"3", U"\\left\\langle{#1}\\middle|{#2}\\middle|{#3}\\right\\rangle",
        // `\cmd*{}{}{}`
        U"pdv", U"*3", U"\\partial^2{#1}/\\partial{#2}\\partial{#3}",
        U"mel", U"*3", U"\\langle{#1}|{#2}|{#3}\\rangle"
    };

    // all commands to find
    if (rules.size() % 4 != 0)
        throw Str32(U"rules.size() illegal!");
    // reverse order
    for (Long i = 0; i < size(rules)/2; i += 4) {
        swap(rules[i], rules[rules.size()-i-4]);
        swap(rules[i+1], rules[rules.size()-i-3]);
        swap(rules[i+2], rules[rules.size()-i-2]);
        swap(rules[i+3], rules[rules.size()-i-1]);
    }
    // all commands to replace
    vecStr32 cmds;
    for (Long i = 0; i < size(rules); i+=4)
        cmds.push_back(rules[i]);

    Long ind0 = 0, ikey;
    Str32 cmd, new_cmd, format;
    vecStr32 args; // cmd arguments, including optional
    while (true) {
        ind0 = find_command(ikey, str, cmds, ind0);
        if (ind0 < 0)
            break;
        cmd = cmds[ikey];
        Bool has_st = command_star(str, ind0);
        Bool has_op = command_has_opt(str, ind0);
        Long Narg = command_Narg(str, ind0);
        format.clear;
        if (has_st)
            format += U"*";
        if (Narg == -1)
            format += U"()";
        else if (Narg == -2)
            format += U"[]()";
        else if (has_op)
            format += U"[]";
        

        args.clear();
        for (Long i = 0; i < Narg; ++i)
            command_arg(args[i], str, ind0, i);
        Long ind1 = skip_command(str, ind0, Narg, false);

        Long ind = -1;
        while (1) {
            ind = search(cmd, rules, ind + 1);
            if (ind < 0)
                throw Str32(U"无法替换命令：" + cmd + U" （格式：" + format + U"）");
            if (ind % 4 != 0 || rules[ind+1] != format || Int(rules[ind+2] - U"0") > Narg)
                continue;
            new_cmd = rules[ind+3];
            for (Long i = 0; i < size(args); ++i)
                replace(new_cmd, U"#" + num2str32(i+1), args[i]);
            break;
        }
        
        
    }
}

// deal with escape simbols in normal text
// str must be normal text
inline Long TextEscape(Str32_IO str)
{
    Long N{};
    N += replace(str, U"\\ ", U" ");
    N += replace(str, U"{}", U"");
    N += replace(str, U"\\^", U"^");
    N += replace(str, U"\\%", U"%");
    N += replace(str, U"\\&", U"&amp");
    N += replace(str, U"<", U"&lt");
    N += replace(str, U">", U"&gt");
    N += replace(str, U"\\{", U"{");
    N += replace(str, U"\\}", U"}");
    N += replace(str, U"\\#", U"#");
    N += replace(str, U"\\~", U"~");
    N += replace(str, U"\\_", U"_");
    N += replace(str, U"\\,", U" ");
    N += replace(str, U"\\;", U" ");
    N += replace(str, U"\\textbackslash", U"&bsol;");
    return N;
}

// deal with escape simbols in normal text, \x{} commands, Command environments
// must be done before \command{} and environments are replaced with html tags
inline Long NormalTextEscape(Str32_IO str)
{
    Long i{}, N1{}, N{}, Nnorm{};
    Str32 temp;
    Intvs intv, intv1;
    FindNormalText(intv, str);
    find_scopes(intv1, U"\\x", str);
    Nnorm = combine(intv, intv1);
    find_env(intv1, str, U"Command");
    Nnorm = combine(intv, intv1);
    for (i = Nnorm - 1; i >= 0; --i) {
        temp = str.substr(intv.L(i), intv.R(i) - intv.L(i) + 1);
        N1 = TextEscape(temp);
        if (N1 < 0)
            continue;
        N += N1;
        str.replace(intv.L(i), intv.R(i) - intv.L(i) + 1, temp);
    }
    return N;
}

// process table
// return number of tables processed
// must be used after EnvLabel()
inline Long Table(Str32_IO str)
{
    Long N{}, ind0{}, ind1{}, ind2{}, ind3{}, Nline;
    Intvs intv;
    vecLong indLine; // stores the position of "\hline"
    Str32 caption;
    Str32 str_beg = U"<div class = \"eq\" align = \"center\">"
        U"<div class = \"w3 - cell\" style = \"width:710px\">\n<table><tr><td>";
    Str32 str_end = U"</td></tr></table>\n</div></div>";
    N = find_env(intv, str, U"table", 'o');
    if (N == 0) return 0;
    for (Long i = N - 1; i >= 0; --i) {
        indLine.clear();
        ind0 = find_command(str, U"caption", intv.L(i));
        if (ind0 < 0 || ind0 > intv.R(i))
            throw Str32(U"table no caption!");
        ind0 += 8; ind0 = expect(str, U"{", ind0);
        ind1 = pair_brace(str, ind0 - 1);
        caption = str.substr(ind0, ind1 - ind0);
        // recognize \hline and replace with tags, also deletes '\\'
        while (true) {
            ind0 = str.find(U"\\hline", ind0);
            if (ind0 < 0 || ind0 > intv.R(i)) break;
            indLine.push_back(ind0);
            ind0 += 5;
        }
        Nline = indLine.size();
        str.replace(indLine[Nline - 1], 6, str_end);
        ind0 = ExpectKeyReverse(str, U"\\\\", indLine[Nline - 1] - 1);
        str.erase(ind0 + 1, 2);
        for (Long j = Nline - 2; j > 0; --j) {
            str.replace(indLine[j], 6, U"</td></tr><tr><td>");
            ind0 = ExpectKeyReverse(str, U"\\\\", indLine[j] - 1);
            str.erase(ind0 + 1, 2);
        }
        str.replace(indLine[0], 6, str_beg);
    }
    // second round, replace '&' with tags
    // delete latex code
    // TODO: add title
    find_env(intv, str, U"table", 'o');
    for (Long i = N - 1; i >= 0; --i) {
        ind0 = intv.L(i) + 12; ind1 = intv.R(i);
        while (true) {
            ind0 = str.find('&', ind0);
            if (ind0 < 0 || ind0 > ind1) break;
            str.erase(ind0, 1);
            str.insert(ind0, U"</td><td>");
            ind1 += 8;
        }
        ind0 = str.rfind(str_end, ind1) + str_end.size();
        str.erase(ind0, ind1 - ind0 + 1);
        ind0 = str.find(str_beg, intv.L(i)) - 1;
        str.replace(intv.L(i), ind0 - intv.L(i) + 1,
            U"<div align = \"center\"> 表" + num2str(i + 1) + U"：" +
            caption + U"</div>");
    }
    return N;
}

// process Itemize environments
// return number processed
inline Long Itemize(Str32_IO str)
{
    Long i{}, j{}, N{}, Nitem{}, ind0{};
    Intvs intvIn, intvOut;
    vecLong indItem; // positions of each "\item"
    N = find_env(intvIn, str, U"itemize");
    find_env(intvOut, str, U"itemize", 'o');
    for (i = N - 1; i >= 0; --i) {
        // delete paragraph tags
        ind0 = intvIn.L(i);
        while (true) {
            ind0 = str.find(U"<p>　　", ind0);
            if (ind0 < 0 || ind0 > intvIn.R(i))
                break;
            str.erase(ind0, 5); intvIn.R(i) -= 5; intvOut.R(i) -= 5;
        }
        ind0 = intvIn.L(i);
        while (true) {
            ind0 = str.find(U"</p>", ind0);
            if (ind0 < 0 || ind0 > intvIn.R(i))
                break;
            str.erase(ind0, 4); intvIn.R(i) -= 4;  intvOut.R(i) -= 4;
        }
        // replace tags
        indItem.resize(0);
        str.erase(intvIn.R(i) + 1, intvOut.R(i) - intvIn.R(i));
        str.insert(intvIn.R(i) + 1, U"</li></ul>");
        ind0 = intvIn.L(i);
        while (true) {
            ind0 = str.find(U"\\item", ind0);
            if (ind0 < 0 || ind0 > intvIn.R(i)) break;
            indItem.push_back(ind0); ind0 += 5;
        }
        Nitem = indItem.size();

        for (j = Nitem - 1; j > 0; --j) {
            str.erase(indItem[j], 5);
            str.insert(indItem[j], U"</li><li>");
        }
        str.erase(indItem[0], 5);
        str.insert(indItem[0], U"<ul><li>");
        str.erase(intvOut.L(i), indItem[0] - intvOut.L(i));
    }
    return N;
}

// process enumerate environments
// similar to Itemize()
inline Long Enumerate(Str32_IO str)
{
    Long i{}, j{}, N{}, Nitem{}, ind0{};
    Intvs intvIn, intvOut;
    vecLong indItem; // positions of each "\item"
    N = find_env(intvIn, str, U"enumerate");
    find_env(intvOut, str, U"enumerate", 'o');
    for (i = N - 1; i >= 0; --i) {
        // delete paragraph tags
        ind0 = intvIn.L(i);
        while (true) {
            ind0 = str.find(U"<p>　　", ind0);
            if (ind0 < 0 || ind0 > intvIn.R(i))
                break;
            str.erase(ind0, 5); intvIn.R(i) -= 5; intvOut.R(i) -= 5;
        }
        ind0 = intvIn.L(i);
        while (true) {
            ind0 = str.find(U"</p>", ind0);
            if (ind0 < 0 || ind0 > intvIn.R(i))
                break;
            str.erase(ind0, 4); intvIn.R(i) -= 4; intvOut.R(i) -= 4;
        }
        // replace tags
        indItem.resize(0);
        str.erase(intvIn.R(i) + 1, intvOut.R(i) - intvIn.R(i));
        str.insert(intvIn.R(i) + 1, U"</li></ol>");
        ind0 = intvIn.L(i);
        while (true) {
            ind0 = str.find(U"\\item", ind0);
            if (ind0 < 0 || ind0 > intvIn.R(i)) break;
            indItem.push_back(ind0); ind0 += 5;
        }
        Nitem = indItem.size();

        for (j = Nitem - 1; j > 0; --j) {
            str.erase(indItem[j], 5);
            str.insert(indItem[j], U"</li><li>");
        }
        str.erase(indItem[0], 5);
        str.insert(indItem[0], U"<ol><li>");
        str.erase(intvOut.L(i), indItem[0] - intvOut.L(i));
    }
    return N;
}

// process \footnote{}, return number of \footnote{} found
inline Long footnote(Str32_IO str)
{
    Long ind0 = 0, N = 0;
    Str32 temp, idNo;
    ind0 = find_command(str, U"footnote", ind0);
    if (ind0 < 0)
        return 0;
    str += U"\n<hr><p>\n";
    while (true) {
        ++N;
        num2str(idNo, N);
        command_arg(temp, str, ind0);
        str += U"<span id = \"footnote" + idNo + U"\"></span>" + idNo + U". " + temp + U"<br>\n";
        ind0 -= eatL(str, ind0 - 1, U" \n");
        str.replace(ind0, skip_command(str, ind0, 1) - ind0,
            U"<sup><a href = \"#footnote" + idNo + U"\">" + idNo + U"</a></sup>");
        ++ind0;
        ind0 = find_command(str, U"footnote", ind0);
        if (ind0 < 0)
            break;
    }
    str += U"</p>";
    return N;
}
} // namespace slisc
