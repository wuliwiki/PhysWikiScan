#pragma once
#include "scan_global.h"
#include "tex2html.h"
#include "check_entry.h"
#include "labels.h"
#include "sqlite_db.h"
#include "fix_db.h"
#include "tree.h"
#include "history_author.h"
#include "figures.h"
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
	Str str1, env, end, begin = u8"<p>　　\n";

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
		str1 = str.substr(left, length);
		N += paragraph_tag1(str1);
		if (str1.empty()) {
			if (next == 'e' && last != 'e') {
				str1 = "\n<p>\n";
			}
			else if (last == 'e' && next != 'e') {
				str1 = "\n</p>\n";
			}
			else {
				str1 = "\n";
			}
		}
		else {
			str1 = begin + str1 + end;
		}
		str.replace(left, length, str1);
		find_inline_eq(intvInline, str);
		if (next == 'f')
			return N;
		ind0 += size(str1) - length;
		
		// skip
		if (ikey == 0) {
			if (next == 'p') {
				Long ind1 = skip_env(str, ind0);
				Long right;
				left = inside_env(right, str, ind0, 2);
				length = right - left;
				str1 = str.substr(left, length);
				paragraph_tag(str1);
				str.replace(left, length, str1);
				find_inline_eq(intvInline, str);
				ind0 = ind1 + size(str1) - length;
			}
			else
				ind0 = skip_env(str, ind0);
		}
		else {
			if (str.substr(ind0,7) == "\\pentry")
				ind0 = skip_command(str, ind0, 2);
			else
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

// batch modification of all `.tex` files as needed
inline void arg_batch_mod()
{
	vecStr entries;
	Str root = gv::path_in + "contents/";
	file_list_ext(entries, root, "tex");
	Str str, pentry_arg;
	for (auto &entry : entries) {
		read(str, root + entry);
		Long ind0 = 0;
		while (1) {
			Long ind = find_command(str, "pentry", ind0);
			if (ind < 0)
				break;
			command_arg(pentry_arg, str, ind, 0, false);
			Long len = size(pentry_arg);
			ind = skip_command(str, ind);
			Long ind1 = expect(str, "{", ind);
			if (ind1 < 0) {
				ind0++; continue;
			}
			replace(pentry_arg, "\\upref{", "\\nref{nod_");
			str.replace(ind1, len, pentry_arg);
			ind0++;
		}
		write(str, root + entry);
	}
}

// replace \pentry command with html round panel
// skip pentry without \\upref*
inline void pentry_cmd(Str_IO str, Pentry_I pentry, bool is_eng, SQLite::Database &db_read)
{
	SQLite::Statement stmt_select(db_read, R"(SELECT "entry", "order" FROM "nodes" WHERE "id"=?;)");
	Long ind = 0;
	Str pentry_arg, entry, node_id, icon_html;
	Str span_beg = R"(<span style="color:gray;">)", span_end = "</span>";
	Long i = -1; // index to pentry
	while (1) {  // loop through \pentry command (not pentry var)
		++i;
		ind = find_command(str, "pentry", ind);
		if (ind < 0) {
			if (i < size(pentry))
				throw internal_err(SLS_WHERE);
			break;
		}
		auto &pentry1 = pentry[i];
		Long Nref = pentry.empty() ? 0 : size(pentry1.second);
		Long ind1 = skip_command(str, ind, 2);
		command_arg(pentry_arg, str, ind);
		Long ind2 = 0, j = -1;
		while (1) { // loop through \nref command
			++j;
			Long ind3 = find(pentry_arg, "\\nref", ind2);
			if (ind3 < 0) {
				if (j == 0) { // \pentry without \upref*
					--i; break;
				}
				if (j != Nref)
					throw internal_err(SLS_WHERE);
				break;
			}
			if (j >= Nref)
				throw internal_err(SLS_WHERE);
			if (pentry1.second[j].hide) {
				pentry_arg.insert(ind3, span_end);
				pentry_arg.insert(ind2, span_beg);
				ind3 += size(span_beg) + size(span_end);
			}
			Long len1 = size(pentry_arg);
			ind2 = skip_command(pentry_arg, ind3, 1);
			command_arg(node_id, pentry_arg, ind3);
			stmt_select.bind(1, node_id.substr(4));
			if (!stmt_select.executeStep())
				throw scan_err(u8"pentry_cmd(), node_id 不存在：" + node_id);
			const Str &node_entry = stmt_select.getColumn(0);
			// Long order = stmt_select.getColumn(1).getInt();
			stmt_select.reset();
			clear(icon_html) << R"(<sup><a href = ")"
				<< gv::url << node_entry << ".html";
			if (node_id.substr(4) != node_entry)
				icon_html << '#' << node_id;
			icon_html << R"(" target="_blank"><span class="icon"><i class="fa fa-bookmark-o"></i></span></a></sup>)";
			pentry_arg.replace(ind3, ind2-ind3, icon_html);
			Long added_chars = size(icon_html) - (ind2-ind3);
			ind2 += added_chars; len1 += added_chars;
			if (ind2 == len1) {
				if (j != Nref-1)
					throw internal_err(SLS_WHERE);
				break;
			}
			u8_iter it(pentry_arg, ind2);
			while (*it == " ") ++it;
			if (ind2 == len1) {
				if (j != Nref-1)
					throw internal_err(SLS_WHERE);
				break;
			}
			if (j < len1-1 && *it != "，")
				throw scan_err(u8"预备知识之间必须用中文逗号隔开，不要有空格");
			++it; ind2 = it;
		}
		clear(sb) << R"(<div id = "nod_)" << pentry1.first
			<< R"(" class = "w3-panel w3-round-large w3-light-blue"><b>)"
			<< (is_eng ? "Prerequisite " : u8"预备知识");
		if (pentry.size() > 1)
		       sb << ' ' << i+1;
	        sb << "</b>　" << pentry_arg << "</div>";
		str.replace(ind, ind1-ind, sb);
		ind += size(sb);
	}
}

// mark incomplete
inline Long addTODO(Str_IO str)
{
	return Command2Tag("addTODO",
		u8R"(<div class = "w3-panel w3-round-large w3-sand">未完成：)", "</div>", str);
}

// replace "<" and ">" in equations
// ref: https://docs.mathjax.org/en/latest/input/tex/html.html
inline Long rep_eq_lt_gt(Str_IO str)
{
	Long N = 0;
	Intvs intv, intv1;
	find_inline_eq(intv, str);
	find_display_eq(intv1, str); combine(intv, intv1);
	Str buff;
	for (Long i = intv.size() - 1; i >= 0; --i) {
		Long ind0 = intv.L(i), Nstr = intv.R(i) - intv.L(i) + 1;
		buff = str.substr(ind0, Nstr);
		Long ind = 0;
		while (true) {
			ind = find(buff, "<", ind);
			if (ind < 0) break;
			Long ind1 = ++ind;
			if (ind >= size(buff)) break;
			if (is_letter(buff[ind]))
				buff.insert(ind1, " ");
		}
		str.replace(ind0, Nstr, buff);
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
inline bool ind_in_pay(Str_I str, Long_I ind)
{
	Long ikey;
	Long ind0 = rfind(ikey, str, { "\\pay", "\\paid" }, ind);
	if (ind0 < 0 || ikey == 1)
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
		str.replace(ind0, 4, R"(<div class="pay" style="display:inline">)");
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
			throw internal_err(u8"文章未找到： " + last + SLS_WHERE);
		last_title = stmt_select.getColumn(0).getString();
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
			throw internal_err(u8"文章未找到： " + next + SLS_WHERE);
		next_title = stmt_select.getColumn(0).getString();
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
inline void arg_toc(SQLite::Database &db_rw)
{
	cout << u8"\n\n\n\n\n正在从 main.tex 生成目录 index.html ...\n" << endl;
	vecStr titles, entries;
	vecBool is_draft;
	vecStr part_ids, part_name, chap_first, chap_last, chap_ids, chap_name, entry_first, entry_last;
	vecLong entry_part, chap_part, entry_chap;
	table_of_contents(part_ids, part_name, chap_first, chap_last,
					  chap_ids, chap_name, chap_part, entry_first, entry_last,
					  entries, titles, is_draft, entry_part, entry_chap, db_rw);

	db_update_parts_chapters(part_ids, part_name, chap_first, chap_last, chap_ids, chap_name, chap_part,
							 entry_first, entry_last, db_rw);
	db_update_entries_from_toc(entries, titles, entry_part, part_ids, entry_chap, chap_ids, db_rw);
}

// --bib
// parse bibliography.tex and update db
inline void arg_bib(SQLite::Database &db_rw)
{
	vecStr bib_labels, bib_details;
	bibliography(bib_labels, bib_details);
	db_update_bib(bib_labels, bib_details, db_rw);
}

// --history
// update db "history" table from backup files
inline void arg_history(Str_I path, SQLite::Database &db_rw)
{
	db_update_author_history(path, db_rw);
	db_update_authors(db_rw);
}

// generate html from a single tex
// output title from first line comment
// use `clear=true` to only keep the title line and key-word line of the tex file
inline void PhysWikiOnline1(Str_O html, Bool_O update_db, unordered_set<Str> &img_to_delete,
	Str_O title, Char &is_eng, vecStr_O fig_ids, vecStr_O fig_captions,
	unordered_map<Str, unordered_map<Str, Str>> &fig_ext_hash,
	Bool_O isdraft, vecStr_O keywords, Str_O license, Str_O type, vecStr_O labels, vecLong_O label_orders,
	vecStr &str_verb, // [out] temp storage of verbatim strings
	unordered_map<Str, bool> &bibs_change, // entry -> [1]add/[0]delete
	Pentry_O pentry, set<Char32> &illegal_chars,
	Str_I entry, Bool_I clear, vecStr_I rules, SQLite::Database &db_read)
{
	Str str;
	read(str, gv::path_in + "contents/" + entry + ".tex"); // read tex file
	if (!is_valid(str))
		throw std::runtime_error(u8"内部错误： 非法的 UTF-8 文档： " + entry + ".tex");
	CRLF_to_LF(str);
	if (clear) {
		// this option is used when deleting the entry, it will help to remove all labels, figures, etc from the db
		// and get error if they are still being referenced
		Long ind0 = find(str, '\n', 0);
		if (ind0 > 0) {
			ind0 = find(str, '\n', ind0);
			if (ind0 > 0)
				str.resize(ind0+1);
		}
	}

	// read title from first comment
	get_title(title, str);
	isdraft = is_draft(str);
	Str db_title, authors, db_keys_str, db_license, db_type, chapter, last_entry, next_entry;
	bool db_isdraft;
	SQLite::Statement stmt_select(db_read,
		R"(SELECT "caption", "chapter", "last", "next", "keys", "isdraft", "license", "type" )"
		R"(FROM "entries" WHERE "id"=?;)");
	stmt_select.bind(1, entry);
	if (!stmt_select.executeStep())
		throw internal_err(u8"数据库中找不到文章（应该由 editor 在创建时添加或 scan 暂时模拟添加）： " + entry);
	db_title = stmt_select.getColumn(0).getString();
	chapter = stmt_select.getColumn(1).getString();
	last_entry = stmt_select.getColumn(2).getString();
	next_entry = stmt_select.getColumn(3).getString();
	db_keys_str = stmt_select.getColumn(4).getString();
	db_isdraft = stmt_select.getColumn(5).getInt();
	db_license = stmt_select.getColumn(6).getString();
	db_type = stmt_select.getColumn(7).getString();
	stmt_select.reset();

	bool in_main = !chapter.empty(); // entry in main.tex

	// read html template and \newcommand{}
	html.clear();
	read(html, gv::path_out + "templates/entry_template.html");
	CRLF_to_LF(html);

	// check language: "\n%%eng\n" at the end of file means english, otherwise chinese
	if ((size(str) > 7 && str.substr(size(str) - 7) == "\n%%eng\n") ||
			gv::path_in.substr(gv::path_in.size()-4) == "/en/")
		is_eng = 1;
	else
		is_eng = 0;

	// add keyword meta to html
	get_keywords(keywords, str);
	if (size(keywords) > 0) {
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
			throw scan_err(u8"检测到标题改变（" + db_title + u8" ⇒ " + title + u8"）， 请使用重命名按钮修改标题（如果还不行请尝试重新编译 main.tex， 并确保其中的标题和文章文件第一行注释相同）");
	}

	// insert HTML title
	if (replace(html, "PhysWikiHTMLtitle", db_title) != 1)
		throw internal_err(u8"\"PhysWikiHTMLtitle\" 在 entry_template.html 中数量不对");
	// get license
	get_entry_license(license, str, db_read);
	// get entry type
	get_entry_type(type, str, db_read);
	// check globally forbidden char
	global_forbid_char(illegal_chars, str);
	// save and replace verbatim code with an index
	verbatim(str_verb, str);
	// wikipedia_link(str);
	rm_comments(str); // remove comments
	limit_env_cmd(str);
	if (!is_eng)
		autoref_space(str, true); // set true to error instead of warning
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
	// ensure equation punctuation
	if (gv::is_wiki) {
		check_display_eq_punc(str);
		// check_display_eq_paragraph(str);
	}
	// add spaces around inline equation
	inline_eq_space(str);
	// replace "<" and ">" in equations
	rep_eq_lt_gt(str);
	// escape characters
	NormalTextEscape(str);
	// get dependent entries from \pentry{}
	get_pentry(pentry, str, db_read);
	// add paragraph tags
	paragraph_tag(str);
	// add html id for links
	env_labels(fig_ids, labels, label_orders, entry, str);
	// replace environments with html tags
	equation_tag(str, "equation"); equation_tag(str, "align"); equation_tag(str, "gather");
	// itemize and enumerate
	itemize(str); enumerate(str);
	// process table environments
	table(str, is_eng);
	// process example and exercise environments
	theorem_like_env(str, is_eng);
	// process figure environments
	figure_env(img_to_delete, fig_ext_hash, str, fig_captions,
		entry, fig_ids, is_eng, db_read);
	// issues environment
	issuesEnv(str);
	addTODO(str);
	// replace user defined commands
	newcommand(str, rules);
	subsections(str);
	// replace \name{} with html tags
	Command2Tag("subsubsection", "<h3><b>", "</b></h3>", str);
	Command2Tag("bb", "<b>", "</b>", str); Command2Tag("textbf", "<b>", "</b>", str);
	Command2Tag("textsl", "<i>", "</i>", str);
	pay2div(str); // deal with "\pay" "\paid" pseudo command
	href(str); // hyperlinks
	cite(bibs_change, str, entry, db_read); // citation
	
	// footnote
	footnote(str, entry, gv::url);
	// delete redundent commands
	replace(str, "\\dfracH", "");
	// remove spaces around chinese punctuations
	rm_chinese_punc_space(str);

	Command2Tag("x", "<code>", "</code>", str);
	// insert body Title
	if (replace(html, "PhysWikiTitle", title) != 1)
		throw internal_err(u8"\"PhysWikiTitle\" 在 entry_template.html 中数量不对");
	// insert HTML body
	if (replace(html, "PhysWikiHTMLbody", str) != 1)
		throw internal_err(u8"\"PhysWikiHTMLbody\" 在 entry_template.html 中数量不对");
	if (replace(html, "PhysWikiEntry", entry) != (gv::is_wiki? 6:2))
		throw internal_err(u8"\"PhysWikiEntry\" 在 entry_template.html 中数量不对");

	last_next_buttons(html, entry, title, in_main, last_entry, next_entry, db_read);

	if (replace(html, "PhysWikiAuthorList", db_get_author_list(entry, db_read)) != 1)
		throw internal_err(u8"\"PhysWikiAuthorList\" 在 entry_template.html 中数量不对");

	// update db "entries" table
	Str key_str; join(key_str, keywords, "|");
	if (title != db_title || key_str != db_keys_str
		|| isdraft != db_isdraft || db_license != license || db_type != type)
		update_db = true;
}

// might append to `entries` if `order` of eq, fig, etc are updated
inline void PhysWikiOnlineN_round1(
		map<Str, Str> &entry_err, // entry -> err msg
		set<Char32> &illegal_chars,
		vecStr_O titles, vecStr_IO entries,
        vector<Pentry> &pentries,
		vector<vecStr> &str_verbs, // temp storage for verbatim strings
        bool clear,
		vector<Char> &is_eng,
		SQLite::Database &db_rw
) {
	vecStr rules;  // for newcommand()
	define_newcommands(rules);
	titles.clear(); entry_err.clear(); pentries.clear();
    titles.resize(entries.size()); pentries.resize(entries.size());
	str_verbs.clear(); str_verbs.resize(entries.size());

	cout << u8"\n\n======  第 1 轮转换 ======\n" << endl;
	bool update_db, isdraft;
	Str key_str, license, type;
	unordered_set<Str> update_entries;
	vecLong label_orders;
	vecStr keywords, fig_ids, labels, fig_captions;
	unordered_map<Str, unordered_map<Str, Str>> fig_ext_hash; // figures.id -> (ext -> hash)
	Long N0 = size(entries);
	unordered_set<Str> img_to_delete; // img files that copied and renamed to new format
	Str html;
	unordered_map<Str, unordered_map<Str, bool>> entry_bibs_change; // entry -> (bib -> [1]add/[0]del)

	for (Long i = 0; i < size(entries); ++i) {
		auto &entry = entries[i];
		try {
			if (i == N0)
				cout << u8"\n\n\n 以下文章引用标签序号发生改变也需要更新：\n" << endl;

			cout << std::setw(5) << std::left << i
				 << std::setw(10) << std::left << entry; cout.flush();

			// =================================================================
			PhysWikiOnline1(html, update_db, img_to_delete, titles[i], is_eng[i],
				fig_ids, fig_captions, fig_ext_hash, isdraft, keywords,
				license, type, labels, label_orders, str_verbs[i],
				entry_bibs_change[entry], pentries[i], illegal_chars, entry, clear, rules,
				db_rw); // db_rw is only for fixing db error
			// ===================================================================
			// save html file
			write(html, gv::path_out + entry + ".html.tmp");

			cout << titles[i] << endl; cout.flush();

			// update db "entries"
			if (update_db) {
				SQLite::Statement stmt_update(db_rw,
					R"(UPDATE "entries" SET "caption"=?, "keys"=?, "draft"=?, "license"=?, "type"=? WHERE "id"=?;)");
				join(key_str, keywords, "|");
				stmt_update.bind(1, titles[i]); // titles[i] might differ from db only when entry is not in main.tex
				stmt_update.bind(2, key_str);
				stmt_update.bind(3, (int64_t)isdraft);
				stmt_update.bind(4, license);
				stmt_update.bind(5, type);
				stmt_update.bind(6, entries[i]);
				stmt_update.exec(); stmt_update.reset();
			}

			// update db labels, figures
			db_update_labels(update_entries, {entry}, {labels}, {label_orders}, db_rw);
			db_update_figures(update_entries, {entry}, {fig_ids}, {fig_captions}, fig_ext_hash, db_rw);
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
			// update db table nodes for 1 entry (might also delete "edges" if nodes are deleted)
            db_update_nodes(pentries[i], entry, db_rw);
		}
		catch (const std::exception &e) {
			cout << SLS_RED_BOLD << u8"\n错误：" << e.what() << SLS_NO_STYLE << endl;
			entry_err[entry] = e.what();
		}
	}

	try {
		// update entries.bibs & bibliography.ref_by
		db_update_entry_bibs(entry_bibs_change, db_rw);
		// update edges
		db_update_edges(pentries, entries, db_rw);
	}
	catch (const std::exception &e) {
		entry_err["entry_unknown"] = e.what();
	}

	for (auto &e : img_to_delete)
		file_remove(e);
}

inline void get_tree_last_node(Str_O last_node_id, Str_I str, Str_I entry, SQLite::Database &db_read)
{
	SQLite::Statement stmt_select(db_read,
		R"(SELECT "id", "order" FROM "nodes" WHERE "entry"=? ORDER BY "order" DESC LIMIT 2;)");
	stmt_select.bind(1, entry);
	if (!stmt_select.executeStep())
		throw scan_err(u8"文章不存在 node：" + entry);
	int order = stmt_select.getColumn(1);
	if (order > 1) {
		if (!stmt_select.executeStep())
			throw scan_err(SLS_WHERE);
		last_node_id = stmt_select.getColumn(0).getString();
	}
	else
		last_node_id = entry;
	stmt_select.reset();
}

// convert \autoref{} in *.tmp to html, write *.html
// will ignore entries in entry_err
inline void PhysWikiOnlineN_round2(const map<Str, Str> &entry_err, // entry -> err msg
		vecStr_I entries, vecStr_I titles, vector<Pentry> &pentries,
		vector<vecStr> &str_verbs, // temporary verbatim storage
		vector<Char> &is_eng, // use english
        SQLite::Database &db_rw, bool write_html = true)
{
	cout << "\n\n\n\n" << u8"====== 第 2 轮转换 ======\n" << endl;
	Str html, fname;
	unordered_map<Str, pair<Str, Pentry>> entry_info;
	unordered_map<Str, unordered_map<Str, bool>> entry_uprefs_change; // entry -> (entry -> [1]add/[0]del)
	vecStr autoref_labels;

	for (Long i = 0; i < size(entries); ++i) {
		auto &entry = entries[i];
		if (entry_err.count(entry)) continue;
		cout << std::setw(5) << std::left << i
			 << std::setw(10) << std::left << entry
			 << titles[i] << endl; cout.flush();
		fname = gv::path_out + entry + ".html";
		read(html, fname + ".tmp"); // read html file
        // process \pentry{} and tree
        {
            // check dependency tree and auto mark redundant pentry with ~
            vector<DGnode> tree; vector<Node> nodes; unordered_map<Str, Long> node_id2ind;
            db_get_tree1(tree, nodes, node_id2ind, entry_info, pentries[i], entry, db_rw);
            // convert \pentry{} to html
            pentry_cmd(html, pentries[i], is_eng[i], db_rw); // use after db_get_tree1()
			// update db `edges.hide` only
            db_update_edges_hide(pentries[i], entry, db_rw);
        }
		// process \autoref and \upref
		autoref_tilde_upref(html, entry, db_rw);
		// replace \upref{} with link icon
		upref(entry_uprefs_change[entry], html, entry, db_rw);
		autoref(autoref_labels, html, entry, is_eng[i], db_rw);
		db_update_autorefs(entry, autoref_labels, db_rw);

		// verbatim recover (in inverse order of `verbatim()`)
		lstlisting(html, str_verbs[i]);
		lstinline(html, str_verbs[i]);
		verb(html, str_verbs[i]);
		// tree button link
		if (gv::is_wiki) {
			static Str last_node_id;
			get_tree_last_node(last_node_id, html, entry, db_rw);
			if (replace(html, "PhysWikiLastNnodeId", last_node_id) != 2)
				throw internal_err(u8"\"PhysWikiLastNnodeId\" 在 entry_template.html 中数量不对");
		}
		// write html file
		if (write_html)
			write(html, fname); // save html file
		file_remove(fname + ".tmp");
	}
	cout << endl; cout.flush();
	db_update_entry_uprefs(entry_uprefs_change, db_rw);
}

// generate json file containing dependency tree
// empty elements of 'titles' will be ignored
// nodes with `node_id == entry` will only be kept if it's the only node in entry
inline void dep_json(SQLite::Database &db_read)
{
	vecStr chap_ids, chap_names, chap_parts, part_ids, part_names;
	vector<DGnode> tree;
	db_get_parts(part_ids, part_names, db_read);
	db_get_chapters(chap_ids, chap_names, chap_parts, db_read);

	vector<Node> nodes;
	// entry -> (title,part,chapter,Pentry)
	unordered_map<Str, tuple<Str, Str, Str, Pentry>> entry_info;
	unordered_map<Str, Long> node_id2ind;
	db_get_tree(tree, nodes, node_id2ind, entry_info, db_read);

	Str str, node_title;

	// resolve a node with `node.id == node.entry` to the last actual node
	auto resolve_node_id = [&](Str_I node_id) {
		auto &nod = nodes[node_id2ind[node_id]];
		if (nod.id == nod.entry) {
			auto &pentry = get<3>(entry_info[nod.entry]);
			if (!pentry.empty())
				return Node(pentry.back().first, nod.entry, pentry.size());
		}
		return nod;
	};

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

	// write nodes (entry + node#)
	str += "  \"nodes\": [\n";
	for (auto &node : nodes) {
		if (node.order <= 0)
			throw internal_err(SLS_WHERE);
		if (resolve_node_id(node.id).id != node.id)
			continue;
		auto &info = entry_info[node.entry];
		auto &title = get<0>(info), &part = get<1>(info), &chap = get<2>(info);
		auto &pentry = get<3>(info);
		node_title = title;
		if (size(pentry) > 1)
			node_title << ':' << node.order;
		str << R"(    {"id": ")" << node.id << '"'
			<< R"(, "part": )" << search(part, part_ids)
			<< R"(, "chap": )" << search(chap, chap_ids)
			<< R"(, "title": ")" << node_title << '"'
			<< R"(, "url": "../online/)"
			<< node.entry << ".html\"},\n";
	}
	str.pop_back(); str.pop_back();
	str += "\n  ],\n";

	// write links
	str += "  \"links\": [\n";
	Long Nedge = 0;
	for (Long i = 0; i < size(tree); ++i) {
		for (auto &j : tree[i]) {
			auto &node_i = nodes[i], &node_j = nodes[j];
			int strength = (node_i.entry == node_j.entry ? 3 : 1); // I thought this is strength, but it has no effect
			str << R"(    {"source": ")" << node_j.id << "\", ";
			str << R"("target": ")" << node_i.id << "\", ";
			str << "\"value\": " << strength << "},\n";
			++Nedge;
		}
	}
	if (Nedge > 0) {
		str.pop_back(); str.pop_back();
	}
	str += "\n  ]\n}\n";
	write(str, gv::path_out + "../tree/data/dep.json");
}

// like PhysWikiOnline, but convert only specified files
inline void PhysWikiOnlineN(vecStr_IO entries, bool clear, SQLite::Database &db_rw)
{
	db_check_add_entry_simulate_editor(entries, db_rw);
	vecStr titles(entries.size());
	vector<Char> is_eng(entries.size(), -1);
	map<Str, Str> entry_err;
	set<Char32> illegal_chars;
	vector<Pentry> pentries;
	vector<vecStr> str_verbs;

	PhysWikiOnlineN_round1(entry_err, illegal_chars, titles, entries, pentries, str_verbs, clear, is_eng, db_rw);

	// generate dep.json
//	if (file_exist(gv::path_out + "../tree/data/dep.json"))
//		dep_json(db_read);

	PhysWikiOnlineN_round2(entry_err, entries, titles, pentries, str_verbs, is_eng, db_rw, !clear);

	if (!entry_err.empty()) {
		if (size(entry_err) > 1) {
			sb << SLS_RED_BOLD "一些文章编译失败： " SLS_NO_STYLE << '\n';
			for (auto &e: entry_err) {
				sb << SLS_RED_BOLD << e.first << SLS_NO_STYLE << '\n' << e.second << '\n';
			}
			throw scan_err(sb);
		}
		else
			throw scan_err((*entry_err.begin()).second);
	}
}

// delete entries
// if failed, db will not change
inline void arg_delete(vecStr_I entries, SQLite::Database &db_rw, Bool_I no_throw = false)
{
	vecStr ref_by, vtmp(1);
	Str stmp, err_msg;
	SQLite::Statement stmt_select(db_rw,
		R"(SELECT "deleted", "caption", "keys", "type", "license", "draft" FROM "entries" WHERE "id"=?;)");
	SQLite::Statement stmt_select2(db_rw,
		R"(SELECT "entry" FROM "entry_uprefs" WHERE "upref"=?;)");
	SQLite::Statement stmt_update(db_rw,
		R"(UPDATE "entries" SET "deleted"=1, "caption"=?, "keys"=?, "type"=?, "license"=?, "draft"=? WHERE "id"=?;)");

	for (auto &entry : entries) {
		// check entries.ref_by
		stmt_select.bind(1,entry);
		if (!stmt_select.executeStep())
			throw scan_err(u8"arg_delete(): 数据库中找不到要删除的文章： " + entry);
		bool db_deleted = stmt_select.getColumn(0).getInt();
		const Str &db_title = stmt_select.getColumn(1);
		const Str &db_keys_str = stmt_select.getColumn(2);
		const Str &db_type_str = stmt_select.getColumn(3);
		const Str &db_license_str = stmt_select.getColumn(4);
		bool db_draft = stmt_select.getColumn(5).getInt();
		stmt_select.reset();

		clear(sb) << gv::path_in << "contents/" << entry << ".tex";
		if (db_deleted) {
			if (file_exist(sb))
				db_deleted = false;
			else {
				if (no_throw)
					continue;
				throw internal_err(u8"arg_delete(): 要删除的文章已经被标记删除：" + entry);
			}
		}
		else { // !db_deleted
			if (!file_exist(sb)) {
				scan_warn(u8"要删除的文章未被标记删除，但文章文件不存在（将创建 dummy 文件并尝试删除）：" + entry);
				clear(sb1) << "% " << db_title << "\n\n";
				write(sb1, sb);
			}
		}
		ref_by.clear();
		stmt_select2.bind(1, entry);
		while (stmt_select2.executeStep())
			ref_by.push_back(stmt_select2.getColumn(0));
        stmt_select2.reset();
		if (!ref_by.empty()) {
			join(sb, ref_by);
			err_msg << u8"无法删除文章，因为被其他文章引用：" << sb << "\n\n";
			continue;
		}

		// make sure no one is referencing anything else
		vtmp[0] = entry;
		try { PhysWikiOnlineN(vtmp, true, db_rw); }
		catch (const std::exception &e) {
			err_msg << "\n\n" << e.what() << "\n\n";
		}
		catch (...) {
			throw internal_err(SLS_WHERE);
		}

		// TODO: check if hash is the same with latest backup

		if (!err_msg.empty())
			throw scan_err(err_msg);

		// update entries.deleted
		stmt_update.bind(1, db_title);
		stmt_update.bind(2, db_keys_str);
		stmt_update.bind(3, db_type_str);
		stmt_update.bind(4, db_license_str);
		stmt_update.bind(5, db_draft);
		stmt_update.bind(6, entry);
		stmt_update.exec(); stmt_update.reset();

		// delete file
		stmp.clear(); stmp << gv::path_in << "contents/" << entry << ".tex";
		file_remove(stmp);
		cout << "成功(浅)删除： " << stmp << endl;
	}
	if (!err_msg.empty())
		throw scan_err(err_msg);
}

// delete an entry if the tex file no longer exist but entries.deleted==0
// use --delete, skip if failed, throw when finished
inline void auto_delete_entries(SQLite::Database &db_rw)
{
	SQLite::Statement stmt_select(db_rw,
		R"(SELECT "id" FROM "entries" WHERE "deleted"=0;)");
	vecStr entries_to_delete;
	Str err_msg;
	while(stmt_select.executeStep()) {
		const Str &entry = stmt_select.getColumn(0);
		clear(sb) << gv::path_in << "contents/" << entry << ".tex";
		if (!file_exist(sb)) {
			scan_warn(u8"发现 entries.deleted=0 但是文章文件不存在（尝试用 --delete 删除）：" + entry);
			try {
				SQLite::Transaction transaction(db_rw);
				arg_delete({entry}, db_rw);
				transaction.commit();
			}
			catch (const std::exception &e) {
				err_msg << u8"发现 entries.deleted=0 但是文章文件不存在， 尝试删除失败（" << e.what() << u8"）：" << entry << '\n';
			}
			catch (...) {
				err_msg << u8"发现 entries.deleted=0 但是文章文件不存在， 尝试删除失败：" << entry << '\n';
			}
		}
	}
	if (!err_msg.empty())
		throw scan_err(err_msg);
}

// arg_delete(), plus everything associated with this entry
// except stuff shared between multiple entries
inline void arg_delete_hard(vecStr_IO entries, SQLite::Database &db_rw)
{
	vecStr history_hash, figures, figs_dangling;
	SQLite::Statement stmt_select0(db_rw, R"(SELECT "deleted" FROM "entries" WHERE "id"=?;)");
	SQLite::Statement stmt_select1(db_rw, R"(SELECT "id" FROM "figures" WHERE "entry"=?;)");
	SQLite::Statement stmt_update(db_rw, R"(UPDATE "entries" SET "last_backup"='' WHERE "id"=?;)");
	SQLite::Statement stmt_select2(db_rw, R"(SELECT "time", "author", "entry" FROM "history" WHERE "hash"=?;)");
	SQLite::Statement stmt_delete(db_rw, R"(DELETE FROM "history" WHERE "hash"=?;)");
	SQLite::Statement stmt_delete0(db_rw, R"(DELETE FROM "entries" WHERE "id"=?;)");
	SQLite::Statement stmt_select3(db_rw, R"(SELECT "id" FROM "figures" WHERE "entry"=?;)");
	SQLite::Statement stmt_delete1(db_rw, R"(DELETE FROM "figures" WHERE "id"=?;)");
	SQLite::Statement stmt_update2(db_rw, R"(UPDATE "figures" SET "deleted"='1' WHERE "id"=?;)");

	for (auto &entry : entries) {
		cout << "--- deleting everything about entry: " << entry << " ---" << endl;
		// delete entries.figures and all associated images
		stmt_select0.bind(1, entry);
		if (!stmt_select0.executeStep()) {
			db_log(u8"arg_delete(): 数据库中找不到要删除的文章（将忽略）： " + entry);
			stmt_select0.reset();
			continue;
		}
		bool deleted = stmt_select0.getColumn(0).getInt();
		stmt_select1.bind(1, entry);
		while (stmt_select1.executeStep())
			figures.push_back(stmt_select1.getColumn(0));
		stmt_select1.reset();
		if (!deleted) // do normal delete first
			arg_delete({entry}, db_rw, true);
		arg_delete_figs_hard(figures, db_rw);

		// delete all history records and files
		db_get_history(history_hash, entry, db_rw);
		stmt_update.bind(1, entry);
		stmt_update.exec(); stmt_update.reset();
		if (!history_hash.empty()) {
			cout << "deleting " << history_hash.size() << " history (files and db)." << endl;
			for (auto &hash: history_hash) {
				stmt_select2.bind(1, hash);
				if (!stmt_select2.executeStep())
					throw internal_err(SLS_WHERE);
				if (entry != stmt_select2.getColumn(2).getString())
					throw internal_err(SLS_WHERE);
				clear(sb) << "../PhysWiki-backup/" << stmt_select2.getColumn(0).getString() << '_'
					<< stmt_select2.getColumn(1).getInt64() << '_' << entry << ".tex";
				stmt_select2.reset();
				stmt_delete.bind(1, hash);
				stmt_delete.exec(); stmt_delete.reset();
				file_remove(sb);
			}
		}

		// delete from entries
		try {
			stmt_delete0.bind(1, entry);
			stmt_delete0.exec();
			stmt_delete0.reset();
		}
		catch (const std::exception &e) {
			if (find(e.what(), "FOREIGN KEY") >= 0) {
				db_log("db foreign key err, trying to resolve...");
				// delete from figures using `figures.entry`
				stmt_select3.bind(1, entry);
				while (stmt_select3.executeStep())
					figs_dangling.push_back(stmt_select3.getColumn(0));
				stmt_select3.reset();
				for (auto &fig_id : figs_dangling) {
					stmt_update2.bind(1, fig_id);
					stmt_update2.exec(); stmt_update2.reset();
					arg_delete_figs_hard({Str(fig_id)}, db_rw);
				}
			}
		}
	}
}

// convert PhysWiki/ folder to wuli.wiki/online folder
// goal: should not need to be used!
inline void PhysWikiOnline(SQLite::Database &db_rw)
{
	gv::is_entire = true; // compiling the whole wiki

	// remove matlab files
	// TODO: check deleted lstlisting using db
//	vecStr matlab_files;
//	ensure_dir(gv::path_out + "code/matlab/");
//	file_list_full(matlab_files, gv::path_out + "code/matlab/");


	vecStr entries, titles;

	// get entries from folder
	vecStr titles_tmp;
	entries_titles(entries, titles_tmp);
	db_check_add_entry_simulate_editor(entries, db_rw);
	vector<Pentry> pentries;
	vector<Char> is_eng(entries.size(), -1);

	arg_bib(db_rw);
	arg_toc(db_rw);
	map<Str, Str> entry_err;
	set<Char32> illegal_chars;
	vector<vecStr> verb_strs;

	PhysWikiOnlineN_round1(entry_err, illegal_chars, titles, entries, pentries, verb_strs, false, is_eng, db_rw);

	PhysWikiOnlineN_round2(entry_err, entries, titles, pentries, verb_strs, is_eng, db_rw);

	// generate dep.json
	if (file_exist(gv::path_out + "../tree/data/dep.json"))
		dep_json(db_rw);

	// TODO: warn unused figures, based on "ref_by"

	if (!illegal_chars.empty()) {
		scan_warn(u8"非法字符的 code point 已经保存到 data/illegal_chars.txt");
		ofstream fout("data/illegal_chars.txt");
		for (auto c: illegal_chars)
			fout << u8(c) << endl;
	}

	if (!entry_err.empty()) {
		cout << SLS_RED_BOLD << entry_err.size() << u8"个文章编译失败： " SLS_NO_STYLE << endl;
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
