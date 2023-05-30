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
	explicit scan_err(Str msg): m_msg(std::move(msg)) {}

	const char* what() const noexcept override {
		return m_msg.c_str();
	}
};

// internal error to throw
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
// replace "\n\n" with "\n</p>\n<p>　　\n"
inline Long paragraph_tag1(Str_IO str)
{
	Long ind0 = 0, N = 0;
	trim(str, " \n");
	// delete extra '\n' (more than two continuous)
	while (1) {
		ind0 = find(str, "\n\n\n", ind0);
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
inline Long pentry_cmd(Str_IO str)
{
	if (!gv::is_eng)
		return Command2Tag("pentry",
			u8R"(<div class = "w3-panel w3-round-large w3-light-blue"><b>预备知识</b>　)", "</div>", str);
	return Command2Tag("pentry",
		u8R"(<div class = "w3-panel w3-round-large w3-light-blue"><b>Prerequisite</b>　)", "</div>", str);
}

// mark incomplete
inline Long addTODO(Str_IO str)
{
	return Command2Tag("addTODO",
		u8R"(<div class = "w3-panel w3-round-large w3-sand">未完成：)", "</div>", str);
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
		if (Long(find(link, "wikipedia.org")) > 0) {
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
		str.replace(ind0, 4, R"(<div id="pay" style="display:inline">)");
		ind0 = find_command(str, "paid", ind0);
		if (ind0 < 0)
			throw scan_err(u8R"(\pay 命令没有匹配的 \paid 命令)");
		str.replace(ind0, 5, "</div>");
	}
}

// buttons for last and next entry
inline void last_next_buttons(Str_IO html, Str_I entry, Str_I title, Bool_I in_main,
							Str_I last, Str_I next, SQLite::Database &db_read)
{
	Str last_title, last_url, next_title, next_url;
	SQLite::Statement stmt_select(db_read,
		R"(SELECT "caption" FROM "entries" WHERE "id"=?;)");
	const Str *last_entry, *next_entry;

	// find last
	if (last.empty()) {
		last_entry = &entry;
		last_title = u8"没有上一篇了哟~";
		if (!in_main)
			last_title << u8"（本文不在目录中）";
	}
	else {
		last_entry = &last;
		stmt_select.bind(1, last);
		if (!stmt_select.executeStep())
			throw internal_err(u8"词条未找到： " + last + SLS_WHERE);
		last_title = (const char*)stmt_select.getColumn(0);
		stmt_select.reset();
	}

	// find next
	if (next.empty()) {
		next_entry = &entry;
		next_title = u8"没有下一篇了哟~";
		if (!in_main)
			next_title << u8"（本文不在目录中）";
	}
	else {
		next_entry = &next;
		stmt_select.bind(1, next);
		if (!stmt_select.executeStep())
			throw internal_err(u8"词条未找到： " + next + SLS_WHERE);
		next_title = (const char*)stmt_select.getColumn(0);
		stmt_select.reset();
	}

	// insert into html
	if (replace(html, "PhysWikiLastEntryURL", gv::url + *last_entry + ".html") != 2)
		throw internal_err(u8"\"PhysWikiLastEntry\" 在 entry_template.html 中数量不对");
	if (replace(html, "PhysWikiNextEntryURL", gv::url + *next_entry + ".html") != 2)
		throw internal_err(u8"\"PhysWikiNextEntry\" 在 entry_template.html 中数量不对");
	if (replace(html, "PhysWikiLastTitle", u8"上一篇：" + last_title) != 2)
		throw internal_err(u8"\"PhysWikiLastTitle\" 在 entry_template.html 中数量不对");
	if (replace(html, "PhysWikiNextTitle", u8"下一篇：" + next_title) != 2)
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
	db_update_entries_from_toc(entries, titles, entry_part, part_ids, entry_chap, chap_ids, db_rw);
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
// output title from first line comment
inline void PhysWikiOnline1(Str_O html, Bool_O update_db, unordered_set<Str> &img_to_delete,
	Str_O title, vecStr_O fig_ids, vecLong_O fig_orders, vector<unordered_map<Str, Str>> &fig_ext_hash,
	Bool_O isdraft, vecStr_O keywords, vecStr_O labels, vecLong_O label_orders,
	unordered_map<Str, Bool> &uprefs_change, unordered_map<Str, Bool> &bibs_change,
	Pentry_O pentry_raw, Str_I entry, vecStr_I rules, SQLite::Database &db_read)
{
	Str str;
	read(str, gv::path_in + "contents/" + entry + ".tex"); // read tex file
	if (!is_valid(str))
		throw std::runtime_error(u8"内部错误： 非法的 UTF-8 文档： " + entry + ".tex");
	CRLF_to_LF(str);

	// read title from first comment
	get_title(title, str);
	isdraft = is_draft(str);
	Str db_title, authors, db_keys_str, db_pentry_str, chapter, last_entry, next_entry;
	Long db_draft;
	SQLite::Statement stmt_select
			(db_read,
			 R"(SELECT "caption", "authors", "chapter", "last", "next", "keys", "pentry", "isdraft" FROM "entries" WHERE "id"=?;)");
	stmt_select.bind(1, entry);
	if (!stmt_select.executeStep())
		throw internal_err(u8"数据库中找不到词条（应该由 editor 在创建时添加或 scan 暂时模拟添加）： " + entry);
	db_title = (const char*)stmt_select.getColumn(0);
	authors = (const char*)stmt_select.getColumn(1);
	chapter = (const char*)stmt_select.getColumn(2);
	last_entry = (const char*)stmt_select.getColumn(3);
	next_entry = (const char*)stmt_select.getColumn(4);
	db_keys_str = (const char*)stmt_select.getColumn(3);
	db_pentry_str = (const char*)stmt_select.getColumn(4);
	db_draft = (int)stmt_select.getColumn(5);
	stmt_select.reset();

	bool in_main = !chapter.empty(); // entry in main.tex

	// read html template and \newcommand{}
	html.clear();
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

	if (title != db_title) {
		if (in_main > 0)
			throw scan_err(u8"检测到标题改变（" + db_title + u8" ⇒ " + title + u8"）， 请使用重命名按钮修改标题（如果还不行请尝试重新编译 main.tex， 并确保其中的标题和词条文件第一行注释相同）");
	}

	// insert HTML title
	if (replace(html, "PhysWikiHTMLtitle", db_title) != 1)
		throw internal_err(u8"\"PhysWikiHTMLtitle\" 在 entry_template.html 中数量不对");

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
	EnvLabel(fig_ids, fig_orders,labels, label_orders, entry, str);
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
	figure_env(img_to_delete, fig_ext_hash, str, entry, fig_ids, fig_orders, db_read);
	// get dependent entries from \pentry{}
	get_pentry(pentry_raw, str, db_read);
	// issues environment
	issuesEnv(str);
	addTODO(str);
	// process \pentry{}
	pentry_cmd(str);
	// replace user defined commands
	newcommand(str, rules);
	subsections(str);
	// replace \name{} with html tags
	Command2Tag("subsubsection", "<h3><b>", "</b></h3>", str);
	Command2Tag("bb", "<b>", "</b>", str); Command2Tag("textbf", "<b>", "</b>", str);
	Command2Tag("textsl", "<i>", "</i>", str);
	pay2div(str); // deal with "\pay" "\paid" pseudo command
	// replace \upref{} with link icon
	upref(uprefs_change, str, entry, db_read);
	href(str); // hyperlinks
	cite(bibs_change, str, entry, db_read); // citation
	
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

	last_next_buttons(html, entry, title, in_main, last_entry, next_entry, db_read);

	if (replace(html, "PhysWikiAuthorList", db_get_author_list(entry, db_read)) != 1)
		throw internal_err(u8"\"PhysWikiAuthorList\" 在 entry_template.html 中数量不对");

	// update db "entries" table
	Str key_str; join(key_str, keywords, "|");
	if (title != db_title || key_str != db_keys_str
		|| isdraft != db_draft)
		update_db = true;
}

inline void PhysWikiOnlineN_round1(map<Str, Str> &entry_err, // entry -> err msg
        vecStr_O titles, vecStr_IO entries, SQLite::Database &db_read)
{
	vecStr rules;  // for newcommand()
	define_newcommands(rules);
	titles.clear(); titles.resize(entries.size()); entry_err.clear();

	cout << u8"\n\n======  第 1 轮转换 ======\n" << endl;
	Bool update_db, isdraft;
	Str key_str;
	unordered_set<Str> update_entries;
	vecLong fig_orders, label_orders;
	vecStr keywords, fig_ids, labels;
	Pentry pentry_raw;
	vector<unordered_map<Str, Str>> fig_ext_hash;
	Long N0 = size(entries);
	unordered_set<Str> img_to_delete; // img files that copied and renamed to new format
	vector<DGnode> tree;
	unordered_map<Str, pair<Str, Pentry>> entry_info;
	vector<Node> nodes;
	Str pentry_str, db_pentry_str;
	unordered_map<Str, unordered_map<Str, Bool>> entry_bibs_change; // entry -> (bib -> [1]add/[0]del)
	unordered_map<Str, unordered_map<Str, Bool>> entry_uprefs_change; // entry -> (entry -> [1]add/[0]del)

	for (Long i = 0; i < size(entries); ++i) {
		auto &entry = entries[i];
		if (i == N0)
			cout << u8"\n\n\n 以下词条引用标签序号发生改变也需要更新：\n" << endl;

		cout << std::setw(5) << std::left << i
			 << std::setw(10) << std::left << entry; cout.flush();
		Str html;

		// =================================================================
        try {
            PhysWikiOnline1(html, update_db, img_to_delete, titles[i],
                            fig_ids, fig_orders,
                            fig_ext_hash, isdraft, keywords,
                            labels, label_orders, entry_uprefs_change[entry],
                            entry_bibs_change[entry],
                            pentry_raw, entry, rules, db_read);
        }
        catch (const runtime_error e) {
            entry_err[entry] = e.what();
        }
		// ===================================================================
		// save html file
		// TODO: mark redundant pentry with a slightly different color
		write(html, gv::path_out + entry + ".html.tmp");

		cout << titles[i] << endl; cout.flush();

		// update db "entries"
		SQLite::Database db_rw(gv::path_data + "scan.db", SQLite::OPEN_READWRITE);
		if (update_db) {
			SQLite::Statement stmt_update
					(db_rw,
					 R"(UPDATE "entries" SET "caption"=?, "keys"=?, "draft"=? )"
					 R"(WHERE "id"=?;)");
			join(key_str, keywords, "|");
			stmt_update.bind(1, titles[i]); // titles[i] might differ from db only when entry is not in main.tex
			stmt_update.bind(2, key_str);
			stmt_update.bind(3, (int) isdraft);
			stmt_update.bind(4, entries[i]);
			stmt_update.exec(); stmt_update.reset();
		}

		// update db labels, figures
		db_update_labels(update_entries, {entry}, {labels}, {label_orders}, db_rw);
		db_update_figures(update_entries, {entry}, {fig_ids}, {fig_orders},
						  {fig_ext_hash}, db_rw);
		db_update_images(entry, fig_ids, fig_ext_hash, db_rw);

		// order change means `update_entries` needs to be updated with autoref() as well.
		// but just in case, we also run 1st round again for them
		if (!gv::is_entire && !update_entries.empty()) {
			for (auto &new_entry: update_entries)
				if (search(new_entry, entries) < 0)
					entries.push_back(new_entry);
			update_entries.clear();
			titles.resize(entries.size());
		}

		// check dependency tree and auto mark redundant pentry with ~
		db_get_tree1(tree, nodes, entry_info,
					 pentry_raw, entry, titles[i], db_read);
		// update entries.pentry if changed
		db_pentry_str = get_text("entries", "id", entry, "pentry", db_read);
		join_pentry(pentry_str, pentry_raw);
		if (pentry_str != db_pentry_str) {
			SQLite::Statement stmt_update(db_rw,
				R"(UPDATE "entries" SET "pentry"=? WHERE "id"=?;)");
			stmt_update.bind(1, pentry_str);
			stmt_update.bind(2, entry);
			stmt_update.exec(); stmt_update.reset();
		}
	}

	// update entries.bibs & bibliography.ref_by
	db_update_entry_bibs(entry_bibs_change);
	// update entries.uprefs & entries.ref_by
	db_update_uprefs(entry_uprefs_change);

	for (auto &e : img_to_delete)
		file_remove(e);
}

// will ignore entries in entry_err
inline void PhysWikiOnlineN_round2(const map<Str, Str> &entry_err, // entry -> err msg
        vecStr_I entries, vecStr_I titles, SQLite::Database &db_read)
{
	cout << "\n\n\n\n" << u8"====== 第 2 轮转换 ======\n" << endl;
	Str html, fname;
	unordered_map<Str, unordered_set<Str>> entry_add_refs, entry_del_refs; // entry -> refs to add/delete
	for (Long i = 0; i < size(entries); ++i) {
		auto &entry = entries[i];
        if (entry_err.count(entry)) continue;
		cout << std::setw(5) << std::left << i
			 << std::setw(10) << std::left << entry
			 << titles[i] << endl; cout.flush();
		fname = gv::path_out + entry + ".html";
		read(html, fname + ".tmp"); // read html file
		// process \autoref and \upref
		autoref(entry_add_refs[entry], entry_del_refs[entry], html, entry, db_read);
		write(html, fname); // save html file
		file_remove(fname + ".tmp");
	}
	cout << endl; cout.flush();

	db_update_refs(entry_add_refs, entry_del_refs);
}

// generate json file containing dependency tree
// empty elements of 'titles' will be ignored
inline Long dep_json(SQLite::Database &db_read)
{
	vecStr entries, titles, entry_chap, entry_part, chap_ids, chap_names, chap_parts, part_ids, part_names;
	vector<DGnode> tree;
	db_get_parts(part_ids, part_names, db_read);
	db_get_chapters(chap_ids, chap_names, chap_parts, db_read);
	db_get_tree(tree, entries, titles, entry_part, entry_chap, db_read);

	Str str;
	// write part names
	str += "{\n  \"parts\": [\n";
	for (Long i = 0; i < size(part_names); ++i)
		str += R"(    {"name": ")" + part_names[i] + "\"},\n";
	str.pop_back(); str.pop_back();
	str += "\n  ],\n";
	// write chapter names
	str += "  \"chapters\": [\n";
	for (Long i = 0; i < size(chap_names); ++i)
		str += R"(    {"name": ")" + chap_names[i] + "\"},\n";
	str.pop_back(); str.pop_back();
	str += "\n  ],\n";
	// write entries
	str += "  \"nodes\": [\n";
	for (Long i = 0; i < size(titles); ++i) {
		if (titles[i].empty())
			continue;
		str += R"(    {"id": ")" + entries[i] + "\"" +
			   ", \"part\": " + num2str(search(entry_part[i], part_ids)) +
			   ", \"chap\": " + num2str(search(entry_chap[i], chap_ids)) +
			   R"(, "title": ")" + titles[i] + "\""
											", \"url\": \"../online/" +
			   entries[i] + ".html\"},\n";
	}
	str.pop_back(); str.pop_back();
	str += "\n  ],\n";

	// write links
	str += "  \"links\": [\n";
	Long Nedge = 0;
	for (Long i = 0; i < size(tree); ++i) {
		for (auto &j : tree[i]) {
			str += R"(    {"source": ")" + entries[i] + "\", ";
			str += R"("target": ")" + entries[j] + "\", ";
			str += "\"value\": 1},\n";
			++Nedge;
		}
	}
	if (Nedge > 0) {
		str.pop_back(); str.pop_back();
	}
	str += "\n  ]\n}\n";
	write(str, gv::path_out + "../tree/data/dep.json");
	return 0;
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
    map<Str, Str> entry_err;
	PhysWikiOnlineN_round1(entry_err, titles, entries, db_read);
	PhysWikiOnlineN_round2(entry_err, entries, titles, db_read);

    if (!entry_err.empty()) {
        cout << SLS_RED_BOLD "一些词条编译失败： " SLS_NO_STYLE << endl;
        for (auto &e : entry_err) {
            cout << SLS_RED_BOLD << e.first << SLS_NO_STYLE << endl;
            cout << e.second << endl;
        }
    }
}

// convert PhysWiki/ folder to wuli.wiki/online folder
// goal: should not need to be used!
inline void PhysWikiOnline()
{
	SQLite::Database db_read(gv::path_data + "scan.db", SQLite::OPEN_READONLY);

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
		vecStr titles;
		entries_titles(entries, titles);

		SQLite::Database db_rw(gv::path_data + "scan.db", SQLite::OPEN_READWRITE);
		db_check_add_entry_simulate_editor(entries, db_rw);
	}

	arg_bib();
	arg_toc();
    map<Str, Str> entry_err;

	PhysWikiOnlineN_round1(entry_err, titles, entries, db_read);

	// generate dep.json
	// TODO: change the tree with new rule
//    if (file_exist(gv::path_out + "../tree/data/dep.json"))
//        dep_json(db_read);

	PhysWikiOnlineN_round2(entry_err, entries, titles, db_read);

	// TODO: warn unused figures, based on "ref_by"

	if (!illegal_chars.empty()) {
		SLS_WARN(u8"非法字符的 code point 已经保存到 data/illegal_chars.txt");
		ofstream fout("data/illegal_chars.txt");
		for (auto c: illegal_chars) {
			fout << Long(c) << endl;
		}
	}

    if (!entry_err.empty()) {
        cout << SLS_RED_BOLD "一些词条编译失败： " SLS_NO_STYLE << endl;
        for (auto &e : entry_err) {
            cout << SLS_RED_BOLD << e.first << SLS_NO_STYLE << endl;
            cout << e.second << endl;
        }
    }
}

// recompile every user in user-notes
int main(int argc, const char *argv[]);

inline void all_users(Str_I sub_folder) {
	Str file_old, file;
	vecStr folders;
	folder_list(folders, "../user-notes/");
	const int argc = 4;
	const char *argv[4];
	argv[0] = "PhysWikiScan"; argv[1] = "."; argv[2] = "--path";
	for (auto &folder: folders) {
		if (!file_exist("../user-notes/" + folder + "main.tex"))
			continue;
		folder += sub_folder;
		argv[3] = folder.c_str();
		cout << argv[3] << endl;
		main(argc, argv);
	}
}
