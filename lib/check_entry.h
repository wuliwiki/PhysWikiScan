#pragma once

// get the title (defined in the first comment, can have space after %)
// limited to 20 characters
inline void get_title(Str32_O title, Str32_I str)
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
    Long ind0 = title.find(U"\\");
    if (ind0 >= 0)
        throw scan_err(u8"第一行注释的标题不能含有 “\\” ");
}

// check if an entry is labeled "\issueDraft"
inline Bool is_draft(Str32_I str)
{
    Intvs intv;
    find_env(intv, str, U"issues");
    if (intv.size() > 1)
        throw scan_err(u8"每个词条最多支持一个 issues 环境!");
    else if (intv.empty())
        return false;
    Long ind = str.find(U"\\issueDraft", intv.L(0));
    if (ind < 0)
        return false;
    if (ind < intv.R(0))
        return true;
    else
        throw scan_err(u8"\\issueDraft 命令不在 issues 环境中!");
}

// get dependent entries (id) from \pentry{}
inline void get_pentry(vecStr32_O pentries, Str32_I str, SQLite::Database &db)
{
    Long ind0 = 0;
    Str32 temp, depEntry;
    pentries.clear();
    while (1) {
        ind0 = find_command(str, U"pentry", ind0);
        if (ind0 < 0)
            return;
        command_arg(temp, str, ind0, 0, 't');
        Long ind1 = 0;
        while (1) {
            ind1 = find_command(temp, U"upref", ind1);
            if (ind1 < 0)
                return;
            command_arg(depEntry, temp, ind1, 0, 't');
            if (!exist(db.getHandle(), "entries", "id", u8(depEntry)))
                throw scan_err("\\pentry{} 中 \\upref 引用的词条未找到: " + depEntry + ".tex");
            if (search(depEntry, pentries) >= 0)
                throw scan_err("\\pentry{} 中预备知识重复： " + depEntry + ".tex");
            pentries.push_back(depEntry);
            ++ind1;
        }
    }
}

// get keywords from the comment in the second line
// return numbers of keywords found
// e.g. "关键词1|关键词2|关键词3"
inline Long get_keywords(vecStr32_O keywords, Str32_I str)
{
    keywords.clear();
    Str32 word;
    Long ind0 = str.find(U"\n", 0);
    if (ind0 < 0 || ind0 == size(str)-1)
        return 0;
    ind0 = expect(str, U"%", ind0+1);
    if (ind0 < 0) {
        // SLS_WARN(u8"请在第二行注释关键词： 例如 \"% 关键词1|关键词2|关键词3\"！");
        return 0;
    }
    Str32 line; get_line(line, str, ind0);
    Long tmp = line.find(U"|", 0);
    if (tmp < 0) {
        // SLS_WARN(u8"请在第二行注释关键词： 例如 \"% 关键词1|关键词2|关键词3\"！");
        return 0;
    }

    ind0 = 0;
    while (1) {
        Long ind1 = line.find(U"|", ind0);
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
