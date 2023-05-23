#pragma once
#include "matlab.h"

namespace slisc {

// find all strings in a matlab code
inline Long Matlab_strings(Intvs_O intv, Str32_I str)
{
    Long ind0 = 0;
    Bool in_string = false;
    while (true) {
        ind0 = find(str, U'\'', ind0);
        if (ind0 < 0) {
            return intv.size();
        }

        if (!in_string) {
            // first quote in comment
            Long ikey;
            rfind(ikey, str, { "\n", "%" }, ind0 - 1);
            if (ikey == 1) {
                ++ind0; continue;
            }
            // transpose, not a single quote
            if (Matlab_is_trans(str, ind0)) {
                ++ind0; continue;
            }
        }
        if (in_string)
            intv.pushR(ind0);
        else
            intv.pushL(ind0);
        in_string = !in_string;
        ++ind0;
    }
}

// highlight Matlab keywords
// str is Matlab code only
// replace keywords with <span class="keyword_class">...</span>
inline Long Matlab_keywords(Str32_IO str, vecStr32_I keywords, Str32_I keyword_class)
{
    // find comments and strings
    Intvs intv_comm, intv_str;
    Matlab_strings(intv_str, str);
    Matlab_comments(intv_comm, str, intv_str);

    Long N = 0;
    Long ind0 = str.size() - 1;
    while (true) {
        // process str backwards so that unprocessed ranges doesn't change

        // find the last keyword
        vecLong ikeys;
        ind0 = rfind(ikeys, str, keywords, ind0);
        if (ind0 < 0)
            break;

        // in case of multiple match, use the longest keyword (e.g. elseif/else)
        Long ikey, max_size = 0;
        for (Long i = 0; i < size(ikeys); ++i) {
            if (max_size < size(keywords[ikeys[i]])) {
                max_size = keywords[ikeys[i]].size();
                ikey = ikeys[i];
            }
        }

        // check whole word
        if (!is_whole_word(str, ind0, keywords[ikey].size())) {
            --ind0;  continue;
        }
        // ignore keyword in comment or in string
        if (is_in(ind0, intv_comm) || is_in(ind0, intv_str)) {
            --ind0; continue;
        }
        // found keyword
        str.replace(ind0, keywords[ikey].size(),
            "<span class = \"" + keyword_class + "\">" + keywords[ikey] + "</span>");
        ++N;
    }
    return N;
}

// highlight comments in Matlab code
// code is Matlab code snippet
// replace keywords with <span class="string_class">...</span>
// return the total number of strings and commens processed
inline Long Matlab_string(Str32_IO code, Str32_I str_class)
{
    Long N = 0;
    // find comments and strings
    Intvs intv_str;
    Matlab_strings(intv_str, code);

    // highlight backwards
    for (Long i = intv_str.size()-1; i >= 0; --i) {
        code.insert(intv_str.R(i) + 1, "</span>");
        code.insert(intv_str.L(i), "<span class = \"" + str_class + "\">");
        ++N;
    }
    return N;
}

// highlight comments in Matlab code
// code is Matlab code snippet
// replace keywords with <span class="string_class">...</span>
// return the total number of strings and commens processed
inline Long Matlab_comment(Str32_IO code, Str32_I comm_class)
{
    Long N = 0;
    // find comments and strings
    Intvs intv_comm, intv_str;
    Matlab_strings(intv_str, code);
    Matlab_comments(intv_comm, code, intv_str);
    
    // highlight backwards
    for (Long i = intv_comm.size() - 1; i >= 0; --i) {
        code.insert(intv_comm.R(i) + 1, "</span>");
        code.insert(intv_comm.L(i), "<span class = \"" + comm_class + "\">");
        ++N;
    }
    return N;
}

// highlight matlab code
// str is Matlab code only
// return the number of highlight
inline Long Matlab_highlight(Str32_IO code)
{
    // ======= settings ========
    vecStr32 keywords = { "break", "case", "catch", "classdef",
        "continue", "else", "elseif", "end", "for", "function",
        "global", "if", "otherwie", "parfor", "persistent",
        "return", "spmd", "switch", "try", "while"};
    
    Str32 keyword_class = "keyword";
    Str32 str_class = "string";
    Str32 comm_class = "comment";
    // ==========================

    Long N = 0;
    // replace "<" and ">"
    replace(code, "<", "&lt;"); replace(code, ">", "&gt;");
    Long ind = find(code, "\n\t");
    if (ind >= 0)
        throw scan_err(u8"Matlab 代码中统一使用四个空格作为缩进： " + code.substr(ind, 20) + "...");
    // highlight keywords
    N += Matlab_keywords(code, keywords, keyword_class);
    // highlight comments
    Matlab_comment(code, comm_class);
    // highlight strings
    Matlab_string(code, str_class);
    return N;
}

} // namespace slisc
