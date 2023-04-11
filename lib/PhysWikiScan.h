#pragma once
#include "../SLISC/file/sqlite_ext.h"
#include "../SLISC/str/str.h"
#include "../SLISC/str/disp.h"
#include "../SLISC/algo/sort.h"
#include "../SLISC/algo/graph.h"
#include "../SLISC/util/sha1sum.h"


using namespace slisc;

// global variables, must be set only once
namespace gv {
    Str path_in; // e.g. ../PhysWiki/
    Str path_out; // e.g. ../littleshi.cn/online/
    Str path_data; // e.g. ../littleshi.cn/data/
    Str url; // e.g. https://wuli.wiki/online/
    Bool is_wiki; // editing wiki or note
    Bool is_eng = false; // use english for auto-generated text (Eq. Fig. etc.)
    Bool is_entire = false; // running one tex or the entire wiki
}

class scan_err : public std::exception
{
private:
    Str m_msg;
public:
    explicit scan_err(Str_I msg): m_msg(msg) {}

    const char* what() const noexcept {
        return m_msg.c_str();
    }
};

// 内部错误
class internal_err : public scan_err
{
public:
    explicit internal_err(Str_I msg): scan_err(u8"内部错误（请联系管理员）： " + msg) {}
};

#include "../TeX/tex2html.h"
#include "check_entry.h"
#include "labels.h"
#include "sqlite_db.h"
#include "code.h"
#include "bib.h"
#include "toc.h"
#include "tex_envs.h"
#include "tex_tidy.h"
#include "statistics.h"
#include "google_trans.h"

// trim "\n" and " " on both sides
// remove unnecessary "\n"
// replace “\n\n" with "\n</p>\n<p>　　\n"
inline Long paragraph_tag1(Str_IO str)
{
    Long ind0 = 0, N = 0;
    trim(str, " \n");
    // delete extra '\n' (more than two continuous)
    while (1) {
        ind0 = str.find("\n\n\n", ind0);
        if (ind0 < 0)
            break;
        eatR(str, ind0 + 2, "\n");
    }

    // replace "\n\n" with "\n</p>\n<p>　　\n"
    N = replace(str, "\n\n", u8"\n</p>\n<p>　　\n");
    return N;
}

inline Long paragraph_tag(Str_IO str)
{
    Long N = 0, ind0 = 0, left = 0, length = -500, ikey = -500;
    Str temp, env, end, begin = u8"<p>　　\n";

    // "begin", and commands that cannot be in a paragraph
    vecStr commands = {"begin", "subsection", "subsubsection", "pentry"};

    // environments that must be in a paragraph (use "<p>" instead of "<p>　　" when at the start of the paragraph)
    vecStr envs_eq = {"equation", "align", "gather", "lstlisting"};

    // environments that needs paragraph tags inside
    vecStr envs_p = { "example", "exercise", "definition", "theorem", "lemma", "corollary"};

    // 'n' (for normal); 'e' (for env_eq); 'p' (for env_p); 'f' (end of file)
    char next, last = 'n';

    Intvs intv, intvInline, intv_tmp;

    // handle normal text intervals separated by
    // commands in "commands" and environments
    while (1) {
        // decide mode
        if (ind0 == size(str)) {
            next = 'f';
        }
        else {
            ind0 = find_command(ikey, str, commands, ind0);
            find_double_dollar_eq(intv, str, 'o');
            find_sqr_bracket_eq(intv_tmp, str, 'o');
            combine(intv, intv_tmp);
            Long itmp = is_in_no(ind0, intv);
            if (itmp >= 0) {
                ind0 = intv.R(itmp) + 1; continue;
            }
            if (ind0 < 0)
                next = 'f';
            else {
                next = 'n';
                if (ikey == 0) { // found environment
                    find_inline_eq(intvInline, str);
                    if (is_in(ind0, intvInline)) {
                        ++ind0; continue; // ignore in inline equation
                    }
                    command_arg(env, str, ind0);
                    if (search(env, envs_eq) >= 0) {
                        next = 'e';
                    }
                    else if (search(env, envs_p) >= 0) {
                        next = 'p';
                    }
                }
            }
        }

        // decide ending tag
        if (next == 'n' || next == 'p') {
            end = "\n</p>\n";
        }
        else if (next == 'e') {
            // equations can be inside paragraph
            if (ExpectKeyReverse(str, "\n\n", ind0 - 1) >= 0) {
                end = "\n</p>\n<p>\n";
            }
            else
                end = "\n";
        }
        else if (next == 'f') {
            end = "\n</p>\n";
        }

        // add tags
        length = ind0 - left;
        temp = str.substr(left, length);
        N += paragraph_tag1(temp);
        if (temp.empty()) {
            if (next == 'e' && last != 'e') {
                temp = "\n<p>\n";
            }
            else if (last == 'e' && next != 'e') {
                temp = "\n</p>\n";
            }
            else {
                temp = "\n";
            }
        }
        else {
            temp = begin + temp + end;
        }
        str.replace(left, length, temp);
        find_inline_eq(intvInline, str);
        if (next == 'f')
            return N;
        ind0 += temp.size() - length;
        
        // skip
        if (ikey == 0) {
            if (next == 'p') {
                Long ind1 = skip_env(str, ind0);
                Long left, right;
                left = inside_env(right, str, ind0, 2);
                length = right - left;
                temp = str.substr(left, length);
                paragraph_tag(temp);
                str.replace(left, length, temp);
                find_inline_eq(intvInline, str);
                ind0 = ind1 + temp.size() - length;
            }
            else
                ind0 = skip_env(str, ind0);
        }
        else {
            ind0 = skip_command(str, ind0, 1);
            if (ind0 < size(str)) {
                Long ind1 = expect(str, "\\label", ind0);
                if (ind1 > 0)
                    ind0 = skip_command(str, ind1 - 6, 1);
            }
        }

        left = ind0;
        last = next;
        if (ind0 == size(str)) {
            continue;
        }
        
        // beginning tag for next interval
        
        if (last == 'n' || last == 'p')
            begin = u8"\n<p>　　\n";
        else if (last == 'e') {
            if (expect(str, "\n\n", ind0) >= 0) {
                begin = u8"\n</p>\n<p>　　\n";
            }
            else
                begin = "\n";
        }    
    }
}

// replace \pentry comman with html round panel
inline Long pentry(Str_IO str)
{
    if (!gv::is_eng)
        return Command2Tag("pentry", u8"<div class = \"w3-panel w3-round-large w3-light-blue\"><b>预备知识</b>　", "</div>", str);
    return Command2Tag("pentry", "<div class = \"w3-panel w3-round-large w3-light-blue\"><b>Prerequisite</b>　", "</div>", str);
}

// mark incomplete
inline Long addTODO(Str_IO str)
{
    return Command2Tag("addTODO", u8"<div class = \"w3-panel w3-round-large w3-sand\">未完成：", "</div>", str);
}

// replace "<" and ">" in equations
inline Long rep_eq_lt_gt(Str_IO str)
{
    Long N = 0;
    Intvs intv, intv1;
    Str tmp;
    find_inline_eq(intv, str);
    find_display_eq(intv1, str); combine(intv, intv1);
    for (Long i = intv.size() - 1; i >= 0; --i) {
        Long ind0 = intv.L(i), Nstr = intv.R(i) - intv.L(i) + 1;
        tmp = str.substr(ind0, Nstr);
        N += ensure_space_around(tmp, "<") + ensure_space_around(tmp, ">");
        str.replace(ind0, Nstr, tmp);
    }
    return N;
}

// replace all wikipedia domain to another mirror domain
// return the number replaced
inline Long wikipedia_link(Str_IO str)
{
    Str alter_domain = "jinzhao.wiki";
    Long ind0 = 0, N = 0;
    Str link;
    while (1) {
        ind0 = find_command(str, "href", ind0);
        if (ind0 < 0)
            return N;
        command_arg(link, str, ind0);
        ind0 = skip_command(str, ind0);
        ind0 = expect(str, "{", ind0);
        if (ind0 < 0)
            throw scan_err(u8"\\href{网址}{文字} 命令格式错误！");
        Long ind1 = pair_brace(str, ind0 - 1);
        if (Long(link.find("wikipedia.org")) > 0) {
            replace(link, "wikipedia.org", alter_domain);
            str.replace(ind0, ind1 - ind0, link);
        }
        ++N;
    }
}

// if a position is in \pay...\paid
inline Bool ind_in_pay(Str_I str, Long_I ind)
{
    Long ikey;
    Long ind0 = rfind(ikey, str, { "\\pay", "\\paid" }, ind);
    if (ind0 < 0)
        return false;
    else if (ikey == 1)
        return false;
    else if (ikey == 0)
        return true;
    else
        throw scan_err("ind_in_pay(): unknown!");
}

// deal with "\pay"..."\paid"
inline Long pay2div(Str_IO str)
{
    Long ind0 = 0, N = 0;
    while (1) {
        ind0 = find_command(str, "pay", ind0);
        if (ind0 < 0)
            return N;
        ++N;
        str.replace(ind0, 4, "<div id=\"pay\" style=\"display:inline\">");
        ind0 = find_command(str, "paid", ind0);
        if (ind0 < 0)
            throw scan_err(u8"\\pay 命令没有匹配的 \\paid 命令");
        str.replace(ind0, 5, "</div>");
    }
}

inline void last_next_buttons(Str_IO html, SQLite::Database &db_read, Long_I order, Str_I entry, Str_I title)
{
    Str last_entry, last_title, last_url, next_entry, next_title, next_url;
    SQLite::Statement stmt_select(db_read,
        R"(SELECT "id", "caption" FROM "entries" WHERE "order"=?;)");

    if (order < 0)
        throw internal_err("last_next_buttons()");

    // find last
    if (order == 0) {
        last_entry = entry;
        last_title = u8"没有上一篇了哟~（本文不在目录中）";
    }
    else if (order == 1) {
        last_entry = entry;
        last_title = u8"没有上一篇了哟~";
    }
    else {
        stmt_select.bind(1, (int)order-1);
        if (!stmt_select.executeStep())
            throw internal_err(u8"找不到具有以下 order 的词条： " + num2str(order-1));
        else {
            last_entry = (const char*)stmt_select.getColumn(0);
            last_title = (const char*)stmt_select.getColumn(1);
        }
        stmt_select.reset();
    }

    // find next
    if (order == 0) {
        next_entry = entry;
        next_title = u8"没有下一篇了哟~（本文不在目录中）";
    }
    else {
        stmt_select.bind(1, (int) order + 1);
        if (!stmt_select.executeStep()) {
            next_entry = entry;
            next_title = u8"没有下一篇了哟~";
        } else {
            next_entry = (const char*)stmt_select.getColumn(0);
            next_title = (const char*)stmt_select.getColumn(1);
        }
        stmt_select.reset();
    }

    // insert into html
    if (replace(html, "PhysWikiLastEntryURL", gv::url+last_entry+".html") != 2)
        throw internal_err(u8"\"PhysWikiLastEntry\" 在 entry_template.html 中数量不对");
    if (replace(html, "PhysWikiNextEntryURL", gv::url+next_entry+".html") != 2)
        throw internal_err(u8"\"PhysWikiNextEntry\" 在 entry_template.html 中数量不对");
    if (replace(html, "PhysWikiLastTitle", last_title) != 2)
        throw internal_err(u8"\"PhysWikiLastTitle\" 在 entry_template.html 中数量不对");
    if (replace(html, "PhysWikiNextTitle", next_title) != 2)
        throw internal_err(u8"\"PhysWikiNextTitle\" 在 entry_template.html 中数量不对");
}


// --toc
// table of contents
// generate index.html from main.tex
inline void arg_toc()
{
    cout << u8"\n\n\n\n\n正在从 main.tex 生成目录 index.html ...\n" << endl;
    vecStr titles, entries;
    vecBool is_draft;
    vecStr part_ids, part_name, chap_first, chap_last, chap_ids, chap_name, entry_first, entry_last;
    vecLong entry_part, chap_part, entry_chap;

    SQLite::Database db_read(gv::path_data + "scan.db", SQLite::OPEN_READONLY);
    table_of_contents(part_ids, part_name, chap_first, chap_last,
                      chap_ids, chap_name, chap_part, entry_first, entry_last,
                      entries, titles, is_draft, entry_part, entry_chap, db_read);

    SQLite::Database db_rw(gv::path_data + "scan.db", SQLite::OPEN_READWRITE);
    db_update_parts_chapters(part_ids, part_name, chap_first, chap_last, chap_ids, chap_name, chap_part,
                             entry_first, entry_last);
    db_update_entries_from_toc(entries, entry_part, part_ids, entry_chap, chap_ids, db_rw);
}

// --bib
// parse bibliography.tex and update db
inline void arg_bib()
{
    vecStr bib_labels, bib_details;
    SQLite::Database db_rw(gv::path_data + "scan.db", SQLite::OPEN_READWRITE);
    bibliography(bib_labels, bib_details);
    db_update_bib(bib_labels, bib_details, db_rw);
}

// --history
// update db "history" table from backup files
inline void arg_history(Str_I path)
{
    SQLite::Database db(gv::path_data + "scan.db", SQLite::OPEN_READWRITE);
    db_update_author_history(path, db);
    db_update_authors(db);
}

// generate html from a single tex
inline void PhysWikiOnline1(Bool_O update_db, Str_O title, vecStr_O img_ids, vecLong_O img_orders, vecStr_O img_hashes,
                            Bool_O isdraft, vecStr_O keywords, vecStr_O labels, vecLong_O label_orders,
                            vecStr_O pentries, Str_I entry, vecStr_I rules, SQLite::Database &db_read)
{
    Str str;
    read(str, gv::path_in + "contents/" + entry + ".tex"); // read tex file
    if (!is_valid(str))
        throw std::runtime_error("内部错误： 非法的 UTF-8 文档： " + entry + ".tex");
    CRLF_to_LF(str);

    // read title from first comment
    get_title(title, str);
    isdraft = is_draft(str);
    Str db_title, authors, db_keys_str, db_pentry_str;
    Long order, db_draft;
    SQLite::Statement stmt_select
            (db_read,
             R"(SELECT "caption", "authors", "order", "keys", "pentry", "isdraft" FROM "entries" WHERE "id"=?;)");
    stmt_select.bind(1, entry);
    if (!stmt_select.executeStep())
        throw internal_err(u8"数据库中找不到词条（应该由 editor 在创建时添加或 scan 暂时模拟添加）： " + entry);
    db_title = (const char*)stmt_select.getColumn(0);
    authors = (const char*)stmt_select.getColumn(1);
    order = (int)stmt_select.getColumn(2);
    db_keys_str = (const char*)stmt_select.getColumn(3);
    db_pentry_str = (const char*)stmt_select.getColumn(4);
    db_draft = (int)stmt_select.getColumn(5);

    // read html template and \newcommand{}
    Str html;
    read(html, gv::path_out + "templates/entry_template.html");
    CRLF_to_LF(html);


    // check language: "\n%%eng\n" at the end of file means english, otherwise chinese
    if ((size(str) > 7 && str.substr(size(str) - 7) == "\n%%eng\n") ||
            gv::path_in.substr(gv::path_in.size()-4) == "/en/")
        gv::is_eng = true;
    else
        gv::is_eng = false;

    // add keyword meta to html
    if (get_keywords(keywords, str) > 0) {
        Str keywords_str = keywords[0];
        for (Long i = 1; i < size(keywords); ++i) {
            keywords_str += "," + keywords[i];
        }
        if (replace(html, "PhysWikiHTMLKeywords", keywords_str) != 1)
            throw internal_err(u8"\"PhysWikiHTMLKeywords\" 在 entry_template.html 中数量不对");
    }
    else {
        if (replace(html, "PhysWikiHTMLKeywords", u8"高中物理, 物理竞赛, 大学物理, 高等数学") != 1)
            throw internal_err(u8"\"PhysWikiHTMLKeywords\" 在 entry_template.html 中数量不对");
    }

    // TODO: enable this when editor can do the rename in db
//    if (!db_title.empty() && title != db_title)
//        throw scan_err(u8"检测到标题改变（" + db_title + u8" ⇒ " + title + u8"）， 请使用重命名按钮修改标题");

    // insert HTML title
    if (replace(html, "PhysWikiHTMLtitle", title) != 1)
        throw internal_err("\"PhysWikiHTMLtitle\" 在 entry_template.html 中数量不对");

    // check globally forbidden char
    global_forbid_char(str);
    // save and replace verbatim code with an index
    vecStr str_verb;
    verbatim(str_verb, str);
    // wikipedia_link(str);
    rm_comments(str); // remove comments
    limit_env_cmd(str);
    if (!gv::is_eng)
        autoref_space(str, true); // set true to error instead of warning
    autoref_tilde_upref(str, entry);
    if (str.empty()) str = " ";
    // ensure spaces between chinese char and alphanumeric char
    chinese_alpha_num_space(str);
    // ensure spaces outside of chinese double quotes
    chinese_double_quote_space(str);
    // check escape characters in normal text i.e. `& # _ ^`
    check_normal_text_escape(str);
    // check non ascii char in equations (except in \text)
    check_eq_ascii(str);
    // forbid empty lines in equations
    check_eq_empty_line(str);
    // add spaces around inline equation
    inline_eq_space(str);
    // replace "<" and ">" in equations
    rep_eq_lt_gt(str);
    // escape characters
    NormalTextEscape(str);
    // add paragraph tags
    paragraph_tag(str);
    // add html id for links
    EnvLabel(labels, label_orders, entry, str);
    // replace environments with html tags
    equation_tag(str, "equation"); equation_tag(str, "align"); equation_tag(str, "gather");
    // itemize and enumerate
    Itemize(str); Enumerate(str);
    // process table environments
    Table(str);
    // ensure '<' and '>' has spaces around them
    EqOmitTag(str);
    // process example and exercise environments
    theorem_like_env(str);
    // process figure environments
    FigureEnvironment(img_ids, img_orders, img_hashes, str, entry);
    // get dependent entries from \pentry{}
    get_pentry(pentries, str, db_read);
    // issues environment
    issuesEnv(str);
    addTODO(str);
    // process \pentry{}
    pentry(str);
    // replace user defined commands
    newcommand(str, rules);
    subsections(str);
    // replace \name{} with html tags
    Command2Tag("subsubsection", "<h3><b>", "</b></h3>", str);
    Command2Tag("bb", "<b>", "</b>", str); Command2Tag("textbf", "<b>", "</b>", str);
    Command2Tag("textsl", "<i>", "</i>", str);
    pay2div(str); // deal with "\pay" "\paid" pseudo command
    // replace \upref{} with link icon
    upref(str, entry);
    href(str); // hyperlinks
    cite(str, db_read); // citation
    
    // footnote
    footnote(str, entry, gv::url);
    // delete redundent commands
    replace(str, "\\dfracH", "");
    // remove spaces around chinese punctuations
    rm_chinese_punc_space(str);
    // verbatim recover (in inverse order of `verbatim()`)
    lstlisting(str, str_verb);
    lstinline(str, str_verb);
    verb(str, str_verb);

    Command2Tag("x", "<code>", "</code>", str);
    // insert body Title
    if (replace(html, "PhysWikiTitle", title) != 1)
        throw internal_err(u8"\"PhysWikiTitle\" 在 entry_template.html 中数量不对");
    // insert HTML body
    if (replace(html, "PhysWikiHTMLbody", str) != 1)
        throw internal_err(u8"\"PhysWikiHTMLbody\" 在 entry_template.html 中数量不对");
    if (replace(html, "PhysWikiEntry", entry) != (gv::is_wiki? 8:2))
        throw internal_err(u8"\"PhysWikiEntry\" 在 entry_template.html 中数量不对");

    last_next_buttons(html, db_read, order, entry, title);

    if (replace(html, "PhysWikiAuthorList", db_get_author_list(entry, db_read)) != 1)
        throw internal_err(u8"\"PhysWikiAuthorList\" 在 entry_template.html 中数量不对");

    // save html file
    write(html, gv::path_out + entry + ".html.tmp");

    // update db "entries" table
    Str key_str; join(key_str, keywords, "|");
    Str pentry_str; join(pentry_str, pentries);
    if (title != db_title || key_str != db_keys_str
        || isdraft != db_draft || pentry_str != db_pentry_str)
        update_db = true;
}

inline void PhysWikiOnlineN_round1(vecStr_O titles, vecStr_IO entries, SQLite::Database &db_read)
{
    vecStr rules;  // for newcommand()
    define_newcommands(rules);
    titles.clear(); titles.resize(entries.size());

    cout << u8"\n\n======  第 1 轮转换 ======\n" << endl;
    Bool update_db, isdraft;
    Str key_str, pentry_str;
    unordered_set<Str> update_entries;
    vecLong img_orders, label_orders;
    vecStr keywords, img_ids, img_hashes, labels, pentries;
    Long N0 = entries.size();

    for (Long i = 0; i < size(entries); ++i) {
        auto &entry = entries[i];
        if (i == N0)
            cout << "\n\n\n 以下词条引用标签序号发生改变也需要更新：\n" << endl;

        cout << std::setw(5) << std::left << i
             << std::setw(10) << std::left << entry; cout.flush();

        PhysWikiOnline1(update_db, titles[i], img_ids, img_orders, img_hashes, isdraft,
            keywords, labels, label_orders, pentries, entry, rules, db_read);

        cout << titles[i] << endl; cout.flush();

        {
            // update db "entries"
            SQLite::Database db_rw(gv::path_data + "scan.db", SQLite::OPEN_READWRITE);
            if (update_db) {
                SQLite::Statement stmt_update
                        (db_rw,
                         R"(UPDATE "entries" SET "caption"=?, "keys"=?, "draft"=?, "pentry"=? )"
                         R"(WHERE "id"=?;)");
                join(key_str, keywords, "|");
                join(pentry_str, pentries);
                stmt_update.bind(1, titles[i]);
                stmt_update.bind(2, key_str);
                stmt_update.bind(3, (int) isdraft);
                stmt_update.bind(4, pentry_str);
                stmt_update.bind(5, entries[i]);
                stmt_update.exec(); stmt_update.reset();
            }

            // update db labels, figures
            db_update_figures(update_entries, {entry}, {img_ids}, {img_orders},
                              {img_hashes}, db_rw);
            db_update_labels(update_entries, {entry}, {labels}, {label_orders}, db_rw);

            // order change means `update_entries` needs to be updated with autoref() as well.
            // but just in case, we also run 1st round again for them
            if (!gv::is_entire && !update_entries.empty()) {
                for (auto &new_entry: update_entries)
                    if (search(new_entry, entries) < 0)
                        entries.push_back(new_entry);
                update_entries.clear();
                titles.resize(entries.size());
            }
        }

        // check dependency tree
        {
            vector<DGnode> tree;
            vecStr _entries, _titles, parts, chapters;
            db_get_tree1(tree, _entries, _titles, parts, chapters, entry, db_read);
        }
    }
}

inline void PhysWikiOnlineN_round2(vecStr_I entries, vecStr_I titles, SQLite::Database &db_read)
{
    cout << "\n\n\n\n" << u8"====== 第 2 轮转换 ======\n" << endl;
    Str html, fname;
    unordered_map<Str, set<Str>> label_ref_by, fig_ref_by;
    for (Long i = 0; i < size(entries); ++i) {
        auto &entry = entries[i];
        cout << std::setw(5) << std::left << i
             << std::setw(10) << std::left << entry
             << titles[i] << endl; cout.flush();
        fname = gv::path_out + entry + ".html";
        read(html, fname + ".tmp"); // read html file
        // process \autoref and \upref
        autoref(label_ref_by, fig_ref_by, html, entry, db_read);
        write(html, fname); // save html file
        file_remove(fname + ".tmp");
    }
    cout << endl; cout.flush();

    // updating labels and figures ref_by
    cout << "updating labels ref_by..." << endl;
    SQLite::Database db_rw(gv::path_data + "scan.db", SQLite::OPEN_READWRITE);
    SQLite::Statement stmt_update_ref_by(db_rw,
        R"(UPDATE "labels" SET "ref_by"=? WHERE "id"=?;)");
    Str ref_by_str;
    set<Str> ref_by;
    for (auto &label_id : label_ref_by) {
        ref_by.clear();
        ref_by_str = get_text("labels", "id", label_id.first, "ref_by", db_rw);
        parse(ref_by, ref_by_str);
        for (auto &by_entry : label_id.second)
            ref_by.insert(by_entry);
        join(ref_by_str, ref_by);
        stmt_update_ref_by.bind(1, ref_by_str);
        stmt_update_ref_by.bind(2, label_id.first);
        stmt_update_ref_by.exec(); stmt_update_ref_by.reset();
    }

    cout << "updating figures ref_by..." << endl;
    SQLite::Statement stmt_select_ref_by_fig(db_rw,
        R"(SELECT "ref_by" FROM "figures" WHERE "id"=?;)");
    SQLite::Statement stmt_update_ref_by_fig(db_rw,
        R"(UPDATE "figures" SET "ref_by"=? WHERE "id"=?;)");
    for (auto &fig_id : fig_ref_by) {
        ref_by.clear();
        stmt_select_ref_by_fig.bind(1, fig_id.first);
        if (!stmt_select_ref_by_fig.executeStep())
            throw internal_err("找不到 figures.id： " + fig_id.first);
        ref_by_str = (const char*)stmt_select_ref_by_fig.getColumn(0);
        stmt_select_ref_by_fig.reset();
        parse(ref_by, ref_by_str);
        for (auto &by_entry : fig_id.second)
            ref_by.insert(by_entry);
        join(ref_by_str, ref_by);
        stmt_update_ref_by_fig.bind(1, ref_by_str);
        stmt_update_ref_by_fig.bind(2, fig_id.first);
        stmt_update_ref_by_fig.exec(); stmt_update_ref_by_fig.reset();
    }
    cout << "done!" << endl;
}

// like PhysWikiOnline, but convert only specified files
// requires ids.txt and labels.txt output from `PhysWikiOnline()`
inline void PhysWikiOnlineN(vecStr_IO entries)
{
    {
        SQLite::Database db_rw(gv::path_data + "scan.db", SQLite::OPEN_READWRITE);
        db_check_add_entry_simulate_editor(entries, db_rw);
    }
    SQLite::Database db_read(gv::path_data + "scan.db", SQLite::OPEN_READONLY);
    vecStr titles(entries.size());
    PhysWikiOnlineN_round1(titles, entries, db_read);
    PhysWikiOnlineN_round2(entries, titles, db_read);
}

// convert PhysWiki/ folder to wuli.wiki/online folder
// goal: should not need to be used!
inline void PhysWikiOnline()
{
    SQLite::Database db_read(gv::path_data + "scan.db", SQLite::OPEN_READONLY);
    SQLite::Database db(gv::path_data + "scan.db", SQLite::OPEN_READWRITE);

    gv::is_entire = true; // compiling the whole wiki

    // remove matlab files
    vecStr fnames;
    ensure_dir(gv::path_out + "code/matlab/");
    file_list_full(fnames, gv::path_out + "code/matlab/");
    for (Long i = 0; i < size(fnames); ++i)
        file_remove(fnames[i]);

    vecStr entries, titles;

    // get entries from folder
    {
        vecStr not_used;
        entries_titles(entries, not_used);

        SQLite::Database db_rw(gv::path_data + "scan.db", SQLite::OPEN_READWRITE);
        db_check_add_entry_simulate_editor(entries, db_rw);
    }

    arg_bib();
    arg_toc();

    PhysWikiOnlineN_round1(titles, entries, db_read);

    // generate dep.json
    if (file_exist(gv::path_out + "../tree/data/dep.json"))
        dep_json(db);

    PhysWikiOnlineN_round2(entries, titles, db_read);

    // TODO: warn unused figures, based on "ref_by"

    if (!illegal_chars.empty()) {
        SLS_WARN("非法字符的 code point 已经保存到 data/illegal_chars.txt");
        ofstream fout("data/illegal_chars.txt");
        for (auto c: illegal_chars) {
            fout << Long(c) << endl;
        }
    }
}
