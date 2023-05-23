#pragma once
#include "tex.h"
#include "../SLISC/arith/scalar_arith.h"

namespace slisc {

// ensure space around (CString)name
// return number of spaces added
// will only check equation env. and inline equations
inline Long EnsureSpace(Str_I name, Str_IO str, Long start, Long end)
{
    Long N{}, ind0{ start };
    while (true) {
        ind0 = find(str, name, ind0);
        if (ind0 < 0 || ind0 > end) break;
        if (ind0 == 0 || str.at(ind0 - 1) != ' ') {
            str.insert(ind0, " "); ++ind0; ++N;
        }
        ind0 += name.size();
        if (ind0 == size(str) || str.at(ind0) != ' ') {
            str.insert(ind0, " "); ++N;
        }
    }
    return N;
}

// check if an index is in an HTML tag "<name>...</name>
inline Bool is_in_tag(Str_I str, Str_I name, Long_I ind)
{
    Long ind1 = str.rfind("<" + name + ">", ind);
    if (ind1 < 0)
        return false;
    Long ind2 = str.rfind("</" + name + ">", ind);
    if (ind2 < ind1)
        return true;
    return false;
}

// ensure space around '<' and '>' in equation env. and $$ env
// return number of spaces added
inline Long EqOmitTag(Str_IO str)
{
    Long i{}, N{}, Nrange{};
    Intvs intv, indInline;
    find_display_eq(intv, str);
    find_inline_eq(indInline, str);
    Nrange = combine(intv, indInline);
    for (i = Nrange - 1; i >= 0; --i) {
        N += EnsureSpace("<", str, intv.L(i), intv.R(i));
        N += EnsureSpace(">", str, intv.L(i), intv.R(i));
    }
    return N;
}

inline Long define_newcommands(vecStr_O rules)
{
    // rules (order matters for the same command name)
    // suggested order: from complex to simple
    // an extra space will be inserted at both ends of each rule
    // 1. cmd name | 2. format | 3. number of arguments (including optional) | 4. rule
    rules.clear();
    ifstream fin("new_commands.txt");
    string line;
    while (getline(fin, line)) {
        Long ind0 = find(line, "\""), ind1;
        if (ind0 < 0) continue;
        ind0 = -1;
        for (Long i = 0; i < 4; ++i) {
            if ((ind0 = find(line, "\"", ind0+1)) < 0)
                throw internal_err(u8"new_commands.txt 格式错误！");
            if ((ind1 = find(line, "\"", ind0+1)) < 0)
                throw internal_err(u8"new_commands.txt 格式错误！");
            rules.push_back(line.substr(ind0+1, ind1-ind0-1));
            ind0 = ind1;
        }
    }
    for (Long i = 4; i < size(rules); i+=4) {
        if (rules[i] < rules[i - 4])
            throw internal_err(u8"define_newcommands(): rules not sorted: " + rules[i]);
    }
    return rules.size();
}

// ==== directly implement \newcommand{}{} by replacing ====
// does not support multiple layer! (call multiple times for that)
// braces can be omitted e.g. \cmd12 (will match in the end)
// matching order: from complicated to simple
// does not support more than 9 arguments (including optional arg)
// command will be ignored if there is no match
// return the number of commands replaced
inline Long newcommand(Str_IO str, vecStr_I rules)
{
    Long N = 0;

    // all commands to find
    if (rules.size() % 4 != 0)
        throw scan_err("rules.size() illegal!");
    // == print newcommands ===
    // for (Long i = 0; i < size(rules); i += 4)
    //     cout << rules[i] << " || " << rules[i+1] << " || " << rules[i+2] << " || " << rules[i+3] << endl;

    // all commands to replace
    vecStr cmds;
    for (Long i = 0; i < size(rules); i+=4)
        cmds.push_back(rules[i]);

    Long ind0 = 0, ikey;
    Str cmd, new_cmd, format;
    vecStr args; // cmd arguments, including optional
    while (true) {
        ind0 = find_command(ikey, str, cmds, ind0);
        if (ind0 < 0)
            break;
        cmd = cmds[ikey];
        Bool has_st = command_star(str, ind0);
        Bool has_op = command_has_opt(str, ind0);
        Long Narg = command_Narg(str, ind0); // actual args used in [] or {}
        format.clear();
        if (has_st)
            format += "*";
        if (Narg == -1) {
            Narg = 1;
            format += "()";
        }
        else if (Narg == -2) {
            Narg = 2;
            format += "[]()";
        }
        else {
            if (has_op)
                format += "[]";
        }

        // decide which rule to use (result: rules[ind])
        Long ind = -1, Narg_rule = -1;
        Long candi = -1; // `rules[candi]` is the last compatible rule with omitted `{}`
        Str rule_format;
        while (1) {
            Long ind1;
            if (ind == 0) {
                lookup(ind1, cmds, cmd, ind+1, cmds.size() - 1);
                while (ind1 > 0 && cmds[ind1 - 1] == cmd)
                    --ind1;
            }
            else
                ind1 = search(cmd, cmds, ind+1);
            if (ind1 < 0) {
                if (candi < 0) {
                    ind = -1; break;
                    // throw scan_err(u8"内部错误：命令替换规则不存在：" + cmd + u8" （格式：" + format + u8"）");
                }
                ind = candi;
                break;
            }
            else
                ind = ind1;
            
            rule_format = rules[ind*4 + 1];
            // match format
            if (format.substr(0, rule_format.size()) == rule_format) {
                Narg_rule = Long(rules[ind*4 + 2][0]) - Long('0');
                if (Narg_rule > Narg) { // has omitted `{}`
                    candi = ind;
                    continue;
                }
                break;
            }
        }
        if (ind < 0) {
            ++ind0; continue;
        }
        // get arguments
        Long end = -1; // replace str[ind0] to str[end-1]
        if (rule_format == "[]()" || rule_format == "*[]()") {
            args.resize(2);
            args[0] = command_opt(str, ind0);
            Long indL = find(str, '(', ind0);
            Long indR = pair_brace(str, indL);
            args[1] = str.substr(indL + 1, indR - indL - 1);
            trim(args[1]);
            end = indR + 1;
        }
        else if (rule_format == "()" || rule_format == "*()") {
            Long indL = find(str, '(', ind0);
            Long indR = pair_brace(str, indL);
            args.resize(1);
            args[0] = str.substr(indL + 1, indR - indL - 1);
            trim(args[0]);
            end = indR + 1;
        }
        else {
            args.resize(Narg_rule);
            for (Long i = 0; i < Narg_rule; ++i)
                end = command_arg(args[i], str, ind0, i, true, false, true);
            if (Narg_rule == 0) {
                if (rule_format[0] == '*')
                    end = skip_command(str, ind0, 0, false, false, true);
                else
                    end = skip_command(str, ind0, 0, false, false, false);
            }
        }
        // apply rule
        new_cmd = rules[ind*4 + 3];
        for (Long i = 0; i < Narg_rule; ++i)
            replace(new_cmd, "#" + num2str32(i + 1), args[i]);
        str.replace(ind0, end - ind0, ' ' + new_cmd + ' '); ++N;
    }
    return N;
}

// deal with escape simbols in normal text
// str must be normal text
inline Long TextEscape(Str_IO str)
{
    Long N{};
    N += replace(str, "<", "&lt;");
    N += replace(str, ">", "&gt;");
    Long tmp = replace(str, "\\\\", "<br>");
    if (tmp > 0)
        throw scan_err(u8"正文中发现 '\\\\' 强制换行！");
    N += tmp;
    N += replace(str, "\\ ", " ");
    N += replace(str, "{}", "");
    N += replace(str, "\\^", "^");
    N += replace(str, "\\%", "%");
    N += replace(str, "\\&", "&amp");
    N += replace(str, "\\{", "{");
    N += replace(str, "\\}", "}");
    N += replace(str, "\\#", "#");
    N += replace(str, "\\~", "~");
    N += replace(str, "\\_", "_");
    N += replace(str, "\\,", " ");
    N += replace(str, "\\;", " ");
    N += replace(str, "\\textbackslash", "&bsol;");
    return N;
}

// deal with escape simbols in normal text, Command environments
// must be done before \command{} and environments are replaced with html tags
inline Long NormalTextEscape(Str_IO str)
{
    Long N1{}, N{}, Nnorm{};
    Str temp;
    Intvs intv;
    Bool warned = false;
    Nnorm = FindNormalText(intv, str);
    for (Long i = Nnorm - 1; i >= 0; --i) {
        temp = str.substr(intv.L(i), intv.R(i) - intv.L(i) + 1);
        try {N1 = TextEscape(temp);}
        catch(const std::exception &e) {
            if (!warned) {
                SLS_WARN(e.what()); warned = true;
            }
        }
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
inline Long Table(Str_IO str)
{
    Long N{}, ind0{}, ind1{}, Nline;
    Intvs intv;
    vecLong indLine; // stores the position of "\hline"
    vecStr captions;
    Str str_beg = "<div class = \"eq\" align = \"center\">"
        "<div class = \"w3 - cell\" style = \"width:710px\">\n<table><tr><td>";
    Str str_end = "</td></tr></table>\n</div></div>";
    N = find_env(intv, str, "table", 'o');
    if (N == 0) return 0;
    captions.resize(N);
    for (Long i = N - 1; i >= 0; --i) {
        indLine.clear();
        ind0 = find_command(str, "caption", intv.L(i));
        if (ind0 < 0 || ind0 > intv.R(i))
            throw scan_err(u8"表格没有标题");
        command_arg(captions[i], str, ind0);
        if (find(captions[i], "\\footnote") >= 0)
            throw scan_err(u8"表格标题中不能添加 \\footnote");
        // recognize \hline and replace with tags, also deletes '\\'
        while (true) {
            ind0 = find(str, "\\hline", ind0);
            if (ind0 < 0 || ind0 > intv.R(i)) break;
            indLine.push_back(ind0);
            ind0 += 5;
        }
        Nline = indLine.size();
        str.replace(indLine[Nline - 1], 6, str_end);
        ind0 = ExpectKeyReverse(str, "\\\\", indLine[Nline - 1] - 1);
        str.erase(ind0 + 1, 2);
        for (Long j = Nline - 2; j > 0; --j) {
            str.replace(indLine[j], 6, "</td></tr><tr><td>");
            ind0 = ExpectKeyReverse(str, "\\\\", indLine[j] - 1);
            str.erase(ind0 + 1, 2);
        }
        str.replace(indLine[0], 6, str_beg);
    }
    // second round, replace '&' with tags
    // delete latex code
    // TODO: add title
    find_env(intv, str, "table", 'o');
    for (Long i = N - 1; i >= 0; --i) {
        ind0 = intv.L(i) + 12; ind1 = intv.R(i);
        while (true) {
            ind0 = find(str, '&', ind0);
            if (ind0 < 0 || ind0 > ind1) break;
            str.erase(ind0, 1);
            str.insert(ind0, "</td><td>");
            ind1 += 8;
        }
        ind0 = str.rfind(str_end, ind1) + str_end.size();
        str.erase(ind0, ind1 - ind0 + 1);
        ind0 = find(str, str_beg, intv.L(i)) - 1;
        str.replace(intv.L(i), ind0 - intv.L(i) + 1,
            "<div align = \"center\"> " + Str(gv::is_eng?"Tab. ":u8"表") + num2str(i + 1) + u8"：" +
            captions[i] + "</div>");
    }
    return N;
}

// process Itemize environments
// return number processed
inline Long Itemize(Str_IO str)
{
    Long i{}, j{}, N{}, Nitem{}, ind0{};
    Intvs intvIn, intvOut;
    vecLong indItem; // positions of each "\item"
    N = find_env(intvIn, str, "itemize");
    find_env(intvOut, str, "itemize", 'o');
    for (i = N - 1; i >= 0; --i) {
        // delete paragraph tags
        ind0 = intvIn.L(i);
        while (true) {
            ind0 = find(str, u8"<p>　　", ind0);
            if (ind0 < 0 || ind0 > intvIn.R(i))
                break;
            str.erase(ind0, 5); intvIn.R(i) -= 5; intvOut.R(i) -= 5;
        }
        ind0 = intvIn.L(i);
        while (true) {
            ind0 = find(str, "</p>", ind0);
            if (ind0 < 0 || ind0 > intvIn.R(i))
                break;
            str.erase(ind0, 4); intvIn.R(i) -= 4;  intvOut.R(i) -= 4;
        }
        // replace tags
        indItem.resize(0);
        str.erase(intvIn.R(i) + 1, intvOut.R(i) - intvIn.R(i));
        str.insert(intvIn.R(i) + 1, "</li></ul>");
        ind0 = intvIn.L(i);
        while (true) {
            ind0 = find(str, "\\item", ind0);
            if (ind0 < 0 || ind0 > intvIn.R(i)) break;
            if (str[ind0 + 5] != ' ' && str[ind0 + 5] != '\n')
                throw scan_err(u8"\\item 命令后面必须有空格或回车！");
            indItem.push_back(ind0); ind0 += 5;
        }
        Nitem = indItem.size();
        if (Nitem == 0)
            continue;

        for (j = Nitem - 1; j > 0; --j) {
            str.erase(indItem[j], 5);
            str.insert(indItem[j], "</li><li>");
        }
        str.erase(indItem[0], 5);
        str.insert(indItem[0], "<ul><li>");
        str.erase(intvOut.L(i), indItem[0] - intvOut.L(i));
    }
    return N;
}

// process enumerate environments
// similar to Itemize()
inline Long Enumerate(Str_IO str)
{
    Long i{}, j{}, N{}, Nitem{}, ind0{};
    Intvs intvIn, intvOut;
    vecLong indItem; // positions of each "\item"
    N = find_env(intvIn, str, "enumerate");
    find_env(intvOut, str, "enumerate", 'o');
    for (i = N - 1; i >= 0; --i) {
        // delete paragraph tags
        ind0 = intvIn.L(i);
        while (true) {
            ind0 = find(str, u8"<p>　　", ind0);
            if (ind0 < 0 || ind0 > intvIn.R(i))
                break;
            str.erase(ind0, 5); intvIn.R(i) -= 5; intvOut.R(i) -= 5;
        }
        ind0 = intvIn.L(i);
        while (true) {
            ind0 = find(str, "</p>", ind0);
            if (ind0 < 0 || ind0 > intvIn.R(i))
                break;
            str.erase(ind0, 4); intvIn.R(i) -= 4; intvOut.R(i) -= 4;
        }
        // replace tags
        indItem.resize(0);
        str.erase(intvIn.R(i) + 1, intvOut.R(i) - intvIn.R(i));
        str.insert(intvIn.R(i) + 1, "</li></ol>");
        ind0 = intvIn.L(i);
        while (true) {
            ind0 = find(str, "\\item", ind0);
            if (ind0 < 0 || ind0 > intvIn.R(i)) break;
            if (str[ind0 + 5] != ' ' && str[ind0 + 5] != '\n')
                throw scan_err(u8"\\item 命令后面必须有空格或回车！");
            indItem.push_back(ind0); ind0 += 5;
        }
        Nitem = indItem.size();
        if (Nitem == 0)
            continue;

        for (j = Nitem - 1; j > 0; --j) {
            str.erase(indItem[j], 5);
            str.insert(indItem[j], "</li><li>");
        }
        str.erase(indItem[0], 5);
        str.insert(indItem[0], "<ol><li>");
        str.erase(intvOut.L(i), indItem[0] - intvOut.L(i));
    }
    return N;
}

// process \footnote{}, return number of \footnote{} found
inline Long footnote(Str_IO str, Str_I entry, Str_I url)
{
    Long ind0 = 0, N = 0;
    Str temp, idNo;
    ind0 = find_command(str, "footnote", ind0);
    if (ind0 < 0)
        return 0;
    str += "\n<hr><p>\n";
    while (true) {
        ++N;
        num2str(idNo, N);
        command_arg(temp, str, ind0);
        str += "<a href = \"" + url + entry + ".html#ret" + idNo + "\" id = \"note" + idNo + "\">" + idNo + ". <b>^</b></a> " + temp + "<br>\n";
        ind0 -= eatL(str, ind0 - 1, " \n");
        str.replace(ind0, skip_command(str, ind0, 1) - ind0,
            "<sup><a href = \"" + url + entry + ".html#note" + idNo + "\" id = \"ret" + idNo + "\"><b>" + idNo + "</b></a></sup>");
        ++ind0;
        ind0 = find_command(str, "footnote", ind0);
        if (ind0 < 0)
            break;
    }
    str += "</p>";
    return N;
}

inline Long subsections(Str_IO str)
{
    Long ind0 = 0, N = 0;
    Str subtitle;
    while (1) {
        ind0 = find_command(str, "subsection", ind0);
        if (ind0 < 0)
            return N;
        ++N;
        command_arg(subtitle, str, ind0);
        Long ind1 = skip_command(str, ind0, 1);
        str.replace(ind0, ind1 - ind0, "<h2 class = \"w3-text-indigo\"><b>" + num2str32(N) + ". " + subtitle + "</b></h2>");
    }
}

// replace "\href{http://example.com}{name}"
// with <a href="http://example.com">name</a>
inline Long href(Str_IO str)
{
    Long ind0 = 0, N = 0, tmp;
    Str name, url;
    while (1) {
        ind0 = find_command(str, "href", ind0);
        if (ind0 < 0)
            return N;
        if (index_in_env(tmp, ind0, { "equation", "align", "gather" }, str)) {
            ++ind0; continue;
        }
        command_arg(url, str, ind0, 0);
        command_arg(name, str, ind0, 1);
        if (url.substr(0, 7) != "http://" &&
            url.substr(0, 8) != "https://") {
            throw scan_err(u8"链接格式错误: " + url);
        }

        Long ind1 = skip_command(str, ind0, 2);
        str.replace(ind0, ind1 - ind0,
            "<a href=\"" + url + "\" target = \"_blank\">" + name + "</a>");
        ++N; ++ind0;
    }
}

} // namespace slisc
