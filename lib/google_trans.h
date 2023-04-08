#pragma once

// for google translation: hide code to protect them from translation
// replace with `\verb|编号|`
inline Long hide_verbatim(vecStr32_O str_verb, Str32_IO str)
{
    Long ind0 = 0, ind1, ind2;
    Char32 dlm;
    Str32 tmp;

    // verb
    while (1) {
        ind0 = find_command(str, U"verb", ind0);
        if (ind0 < 0)
            break;
        ind1 = str.find_first_not_of(U' ', ind0 + 5);
        if (ind1 < 0)
            throw scan_err(u8"\\verb 没有开始");
        dlm = str[ind1];
        if (dlm == U'{')
            throw scan_err(u8"\\verb 不支持 {...}， 请使用任何其他符号如 \\verb|...|， \\verb@...@");
        ind2 = str.find(dlm, ind1 + 1);
        if (ind2 < 0)
            throw scan_err(u8"\\verb 没有闭合");
        if (ind2 - ind1 == 1)
            throw scan_err(u8"\\verb 不能为空");
        tmp = str.substr(ind0, ind2 + 1 - ind0);
        replace(tmp, U"\n", U"PhysWikiScanLF");
        str_verb.push_back(tmp);
        str.replace(ind0, ind2 + 1 - ind0, U"\\verb|" + num2str32(size(str_verb)-1, 4) + U"|");
        ind0 += 3;
    }

    // lstinline
    ind0 = 0;
    while (1) {
        ind0 = find_command(str, U"lstinline", ind0);
        if (ind0 < 0)
            break;
        ind1 = str.find_first_not_of(U' ', ind0 + 10);
        if (ind1 < 0)
            throw scan_err(u8"\\lstinline 没有开始");
        dlm = str[ind1];
        if (dlm == U'{')
            throw scan_err(u8"lstinline 不支持 {...}， 请使用任何其他符号如 \\lstinline|...|， \\lstinline@...@");
        ind2 = str.find(dlm, ind1 + 1);
        if (ind2 < 0)
            throw scan_err(u8"\\lstinline 没有闭合");
        if (ind2 - ind1 == 1)
            throw scan_err(u8"\\lstinline 不能为空");
        tmp = str.substr(ind0, ind2 + 1 - ind0);
        replace(tmp, U"\n", U"PhysWikiScanLF");
        str_verb.push_back(tmp);

        str.replace(ind0, ind2 + 1 - ind0, U"\\verb|" + num2str32(size(str_verb)-1, 4) + U"|");
        ind0 += 3;
    }

    // lstlisting
    vecStr32 str_verb1;
    ind0 = 0;
    Intvs intv;
    Str32 code;
    Long N = find_env(intv, str, U"lstlisting", 'o');
    Str32 lang = U""; // language
    for (Long i = N - 1; i >= 0; --i) {
        tmp = str.substr(intv.L(i), intv.R(i) + 1 - intv.L(i));
        replace(tmp, U"\n", U"PhysWikiScanLF");
        str_verb1.push_back(tmp);
        str.replace(intv.L(i), intv.R(i) + 1 - intv.L(i), U"\\verb|" + num2str32(size(str_verb)+i, 4) + U"|");
    }
    for (Long i = 0; i < N; ++i) {
        str_verb.push_back(str_verb1.back());
        str_verb1.pop_back();
    }
    return str_verb.size();
}

// for google translation: hide equations and verbatims
inline void hide_eq_verb(Str32_IO str)
{
    Str32 tmp;
    vecStr32 eq_list, verb_list;
    Intvs intv, intv1;
    find_inline_eq(intv, str, 'o');
    find_display_eq(intv1, str, 'o');
    combine(intv, intv1);

    for (Long i = intv.size() - 1; i >= 0; --i) {
        tmp = str.substr(intv.L(i), intv.R(i) - intv.L(i) + 1);
        replace(tmp, U"\n", U"PhysWikiScanLF");
        eq_list.push_back(tmp);
        str.replace(intv.L(i), intv.R(i) - intv.L(i) + 1, U"$" + num2str32(size(eq_list)-1, 4) + U"$");
    }
    hide_verbatim(verb_list, str);
    // save to files
    write_vec_str(eq_list, gv::path_data + U"eq_list.txt");
    write_vec_str(verb_list, gv::path_data + U"verb_list.txt");
}

inline void unhide_eq_verb(Str32_IO str)
{
    Str32 tmp, label;
    vecStr32 eq_list, verb_list;
    read_vec_str(eq_list, gv::path_data + U"eq_list.txt");
    read_vec_str(verb_list, gv::path_data + U"verb_list.txt");
    for (Long i = 0; i < size(eq_list); ++i) {
        label = U"$" + num2str32(i, 4) + U"$";
        Long ind = str.find(label);
        tmp = eq_list[i];
        replace(tmp, U"PhysWikiScanLF", U"\n");
        if (ind < 0)
            SLS_WARN(label + U" 没有找到，替换： \n" + tmp + U"\n");
        else
            str.replace(ind, label.size(), tmp);
    }

    for (Long i = 0; i < size(verb_list); ++i) {
        label = U"\\verb|" + num2str32(i, 4) + U"|";
        Long ind = str.find(label);
        tmp = verb_list[i];
        replace(tmp, U"PhysWikiScanLF", U"\n");
        if (ind < 0)
            SLS_WARN(label + U" 没有找到，替换： \n" + tmp + U"\n");
        else
            str.replace(ind, label.size(), tmp);
    }
}
