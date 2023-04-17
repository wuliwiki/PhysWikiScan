#pragma once

// an object representing all \pentry{} info of an entry
// v_pentries[i] is a single \pentry{} command
// Str is "entry" or "entry:num", name of a tree node,
// and Bool is whether the dependency is "weak" (will be first deleted in case of circle)
typedef vector<vector<pair<Str,Bool>>> Pentry;
typedef Pentry &Pentry_O, &Pentry_IO;
typedef const Pentry &Pentry_I;

// get the title (defined in the first comment, can have space after %)
// limited to 20 characters
inline void get_title(Str_O title, Str_I str)
{
    if (size(str) == 0 || str.at(0) != U'%')
        throw scan_err(u8"请在第一行注释标题！");
    Long ind1 = str.find(U'\n');
    if (ind1 < 0)
        ind1 = size(str) - 1;
    title = str.substr(1, ind1 - 1);
    trim(title);
    if (title.empty())
        throw scan_err(u8"请在第一行注释标题（不能为空）！");
    Long ind0 = title.find("\\");
    if (ind0 >= 0)
        throw scan_err(u8"第一行注释的标题不能含有 “\\” ");
}

// check if an entry is labeled "\issueDraft"
inline Bool is_draft(Str_I str)
{
    Intvs intv;
    find_env(intv, str, "issues");
    if (intv.size() > 1)
        throw scan_err(u8"每个词条最多支持一个 issues 环境!");
    else if (intv.empty())
        return false;
    Long ind = str.find("\\issueDraft", intv.L(0));
    if (ind < 0)
        return false;
    if (ind < intv.R(0))
        return true;
    else
        throw scan_err(u8"\\issueDraft 命令不在 issues 环境中!");
}

// get dependent entries (id) from \pentry{}
inline void get_pentry(Pentry_O v_pentries, Str_I str, SQLite::Database &db_read)
{
    Bool star;
    Long ind0 = -1, ikey;
    Str temp, depEntry;
    v_pentries.clear();
    while (1) {
        ind0 = find_command(str, "pentry", ind0+1);
        if (ind0 < 0)
            return;
        v_pentries.emplace_back();
        auto &pentries = v_pentries.back();
        command_arg(temp, str, ind0, 0, 't');
        Long ind1 = 0, ind2 = 0;
        Bool first_upref = true;
        while (1) {
            ind1 = find(ikey, temp, {"\\upref", "\\upref2"}, ind2);
            if (ikey == 1) star = true;
            if (ind1 < 0)
                break;
            if (!first_upref)
                if (expect(temp, u8"，", ind2) < 0)
                    throw scan_err(u8"\\pentry{} 中预备知识格式不对， 应该用中文逗号隔开， 如： \\pentry{词条1\\upref{文件名1}， 词条2\\upref{文件名2}}。");
            command_arg(depEntry, temp, ind1, 0, 't');
            if (!exist("entries", "id", depEntry, db_read))
                throw scan_err(u8"\\pentry{} 中 \\upref 引用的词条未找到: " + depEntry + ".tex");
            if (search(depEntry, pentries) >= 0)
                throw scan_err(u8"\\pentry{} 中预备知识重复： " + depEntry + ".tex");
            pentries.push_back(make_pair(depEntry, star));
            ind2 = skip_command(temp, ind1, 1);
            first_upref = false;
        }
    }
}

// get keywords from the comment in the second line
// return numbers of keywords found
// e.g. "关键词1|关键词2|关键词3"
inline Long get_keywords(vecStr_O keywords, Str_I str)
{
    keywords.clear();
    Str word;
    Long ind0 = str.find("\n", 0);
    if (ind0 < 0 || ind0 == size(str)-1)
        return 0;
    ind0 = expect(str, "%", ind0+1);
    if (ind0 < 0) {
        // SLS_WARN(u8"请在第二行注释关键词： 例如 \"% 关键词1|关键词2|关键词3\"！");
        return 0;
    }
    Str line; get_line(line, str, ind0);
    Long tmp = line.find("|", 0);
    if (tmp < 0) {
        // SLS_WARN(u8"请在第二行注释关键词： 例如 \"% 关键词1|关键词2|关键词3\"！");
        return 0;
    }

    ind0 = 0;
    while (1) {
        Long ind1 = line.find("|", ind0);
        if (ind1 < 0)
            break;
        word = line.substr(ind0, ind1 - ind0);
        trim(word);
        if (word.empty())
            throw scan_err(u8"第二行关键词格式错误！ 正确格式为 “% 关键词1|关键词2|关键词3”");
        keywords.push_back(word);
        ind0 = ind1 + 1;
    }
    word = line.substr(ind0);
    trim(word);
    if (word.empty())
        throw scan_err(u8"第二行关键词格式错误！ 正确格式为 “% 关键词1|关键词2|关键词3”");
    keywords.push_back(word);
    return keywords.size();
}
