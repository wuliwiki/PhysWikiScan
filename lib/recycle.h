#pragma once

// add line numbers to the code (using table)
inline void code_table(Str_O table_str, Str_I code)
{
    Str line_nums;
    Long ind1 = 0, N = 0;
    while (1) {
        ++N;
        line_nums += num2str(N) + "<br>";
        ind1 = code.find(U'\n', ind1);
        if (ind1 < 0)
            break;
        ++ind1;
    }
    line_nums.erase(line_nums.size() - 4, 4);
    table_str =
        "<table class=\"code\"><tr class=\"code\"><td class=\"linenum\">\n"
        + line_nums +
        "\n</td><td class=\"code\">\n"
        "<div class = \"w3-code notranslate w3-pale-yellow\">\n"
        "<div class = \"nospace\"><pre class = \"mcode\">\n"
        + code +
        "\n</pre></div></div>\n"
        "</td></tr></table>";
}

// [recycle] this problem is already fixed
// prevent line wraping before specific chars
// by using <span style="white-space:nowrap">...</space>
// consecutive chars can be grouped together
inline void puc_no_wrap(Str_IO str)
{
    Str keys = u8"，、．。！？）：”】", tmp;
    Long ind0 = 0, ind1;
    while (1) {
        ind0 = str.find_first_of(keys, ind0);
        if (ind0 < 0)
            break;
        if (ind0 == 0 || str[ind0 - 1] == U'\n') {
            ++ind0; continue;
        }
        ind1 = ind0 + 1; --ind0;
        while (search(str[ind1], keys) >= 0 && ind1 < size(str))
            ++ind1;
        tmp = "<span style=\"white-space:nowrap\">" + str.substr(ind0, ind1 - ind0) + "</span>";
        str.replace(ind0, ind1 - ind0, tmp);
        ind0 += tmp.size();
    }
}