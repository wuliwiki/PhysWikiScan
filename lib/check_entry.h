#pragma once
#include "../SLISC/file/file.h"

// a single \nref{node_id} or \nreff{node_id} inside a \pentry{}
struct PentryRef {
	Str node_id; // \nref{node_label}
	bool weak; // \reqq{} (prefer to be ignored), previously marked *
	Char hide; // omitted in the tree, previously marked ~
	PentryRef(Str node_id, bool weak, Char hide = -1):
		node_id(std::move(node_id)), weak(weak), hide(hide) {};
};

// all \pentry{}{} info of an entry
// pentry[i] is a single \pentry{}{} command
//                node_id        \upref
typedef vector<pair<Str, vector<PentryRef>>> Pentry;
typedef Pentry &Pentry_O, &Pentry_IO;
typedef const Pentry &Pentry_I;

// get the title (defined in the first comment, can have space after %)
// limited to 20 characters
inline void get_title(Str_O title, Str_I str)
{
	if (size(str) == 0 || str.at(0) != U'%')
		throw scan_err(u8"请在第一行注释标题！");
	Long ind1 = find(str, '\n');
	if (ind1 < 0)
		ind1 = size(str) - 1;
	title = str.substr(1, ind1 - 1);
	trim(title);
	if (title.empty())
		throw scan_err(u8"请在第一行注释标题（不能为空）！");
	Long ind0 = find(title, '\\');
	if (ind0 >= 0)
		throw scan_err(u8"第一行注释的标题不能含有 “\\” ");
}

// get the license (defined in comment as "% license <license>")
// check the lines starting with '%' at the beginning of `str`
// if a license is defined and in db, then return `licenses.id`
// if is defined but not in db, then throw an error
// if is not defined, then output empty string
inline void get_entry_license(Str_O license, Str_I str, SQLite::Database &db_read,
	unique_ptr<SQLite::Database> &db_read_wiki)
{
	Long ind = 0; license.clear();
	while (str[ind] == '%') {
		ind = (Long)str.find_first_not_of(' ', ind+1);
		if (ind > 0 && str.substr(ind, 8) == "license ") {
			ind = (Long)str.find_first_not_of(' ', ind + 8);
			if (ind < 0) return;
			Long ind1 = (Long)str.find_first_of(" \n", ind);
			if (ind1 < 0) ind1 = size(str);
			license = str.substr(ind, ind1-ind); break;
		}
		ind = find(str, '\n', ind);
		if (ind < 0) return;
		++ind;
	}
	if (license.empty())
		return;

	// check db
	if (gv::is_wiki) {
		if (!exist("licenses", "id", license, db_read))
			throw internal_err(u8"license 不存在于 wiki 数据库：" + license);
	}
	else {
		if (!exist("licenses", "id", license, *db_read_wiki))
			throw internal_err(u8"用户笔记的 license 不存在于 wiki 数据库：" + license);
	}
}

// get the type of entry (entries.type)
// similar with `get_entry_license()`
inline void get_entry_type(Str_O type, Str_I str, SQLite::Database &db_read)
{
	Long ind = 0; type.clear();
	while (str[ind] == '%') {
		ind = (Long)str.find_first_not_of(' ', ind+1);
		if (ind > 0 && str.substr(ind, 5) == "type ") {
			ind = (Long)str.find_first_not_of(' ', ind + 5);
			if (ind < 0) return;
			Long ind1 = (Long)str.find_first_of(" \n", ind);
			if (ind1 < 0) ind1 = size(str);
			type = str.substr(ind, ind1-ind); break;
		}
		ind = find(str, '\n', ind);
		if (ind < 0) return;
		++ind;
	}
	if (type.empty())
		return;

	// check db
	if (!exist("types", "id", type, db_read))
		throw internal_err(u8"type 不存在于数据库：" + type);
}

// check if an entry is labeled "\issueDraft"
inline bool is_draft(Str_I str)
{
	Intvs intv;
	find_env(intv, str, "issues");
	if (intv.size() > 1)
		throw scan_err(u8"每篇文章最多支持一个 issues 环境!");
	else if (intv.empty())
		return false;
	Long ind = find(str, "\\issueDraft", intv.L(0));
	if (ind < 0)
		return false;
	if (ind < intv.R(0))
		return true;
	else
		throw scan_err(u8"\\issueDraft 命令不在 issues 环境中!");
}

// get dependent entries (id) from \pentry{}
// will ignore \pentry{} with no \nref{} or \wnreff{}
// all PentryRef::hide will set to -1
inline void get_pentry(Pentry_O pentry_raw, Str_I entry, Str_I str, SQLite::Database &db_read)
{
	bool weak;
	Long ind0 = -1, ikey;
	Str temp, node_id, label;
	SQLite::Statement stmt_select1(db_read, R"(SELECT "entry" FROM "nodes" WHERE "id"=? AND "entry" != ')" + entry + "';");
	pentry_raw.clear();
	while (1) {
		ind0 = find_command(str, "pentry", ind0+1);
		if (ind0 < 0)
			break;
		if (pentry_raw.empty() || !pentry_raw.back().second.empty())
			pentry_raw.emplace_back();
		auto &pentry1 = pentry_raw.back();
		command_arg(temp, str, ind0, 0);
		command_arg(pentry1.first, str, ind0, 1); // get node_id
		if (pentry1.first.substr(0, 4) != "nod_")
			throw scan_err(u8"\\pentry{...}{nod_xxx} 格式错误，第二个参数不可省略");
		pentry1.first = pentry1.first.substr(4);
		// check repeated node_id
		stmt_select1.bind(1, pentry1.first);
		if (stmt_select1.executeStep()) {
			const Str &entry = stmt_select1.getColumn(0);
			throw scan_err(u8"\\pentry{...}{标签} 中标签 nod_" + pentry1.first + u8" 已存在于文章 " + entry + u8" 请使用预备知识按钮自动生成新的标签。");
		}
		stmt_select1.reset();

		Long ind1 = 0, ind2 = 0;
		bool first_ref = true;
		while (1) {
			ind1 = find(ikey, temp, {"\\nref", "\\wnref", "\\upref"}, ind2);
			if (ikey == 2)
				throw scan_err(u8"预备知识中不允许使用 \\upref{xxx}， 你可能想用 \\nref{nod_xxx}");
			weak = (ikey == 1);
			if (ind1 < 0)
				break;
			if (!first_ref)
				if (expect(temp, u8"，", ind2) < 0)
					throw scan_err(u8R"(\pentry{} 中预备知识格式不对， 应该用中文逗号隔开， 如： \pentry{文章1\nref{文件名1}， 文章2\nref{文件名2}}{nod_标签}。)");
			command_arg(node_id, temp, ind1);
			if (node_id.substr(0, 4) != "nod_")
				throw scan_err(u8"\\nref{nod_xxx} 格式错误");
			node_id = node_id.substr(4);
			for (auto &e : pentry1.second)
				if (node_id == e.node_id)
					throw scan_err(u8R"(\pentry{} 中预备知识重复： )" + node_id + ".tex");
			pentry1.second.emplace_back(node_id, weak, -1); // `edges.hide` unknown
			ind2 = skip_command(temp, ind1, 1);
			first_ref = false;
		}
	}
	if (!pentry_raw.empty() && pentry_raw.back().second.empty())
		pentry_raw.pop_back();
}

// get keywords from the comment in the second line
// return numbers of keywords found
// e.g. "关键词1|关键词2|关键词3"
inline void get_keywords(vecStr_O keywords, Str_I str)
{
	Long ind = 0; keywords.clear();
	Str keys_str;
	while (str[ind] == '%') {
		ind = (Long)str.find_first_not_of(' ', ind+1);
		if (ind > 0 && str.substr(ind, 5) == "keys ") {
			ind = (Long)str.find_first_not_of(' ', ind + 5);
			if (ind < 0) return;
			Long ind1 = (Long)str.find_first_of('\n', ind);
			if (ind1 < 0) ind1 = size(str);
			keys_str = str.substr(ind, ind1-ind); break;
		}
		ind = find(str, '\n', ind);
		if (ind < 0) return;
		++ind;
	}
	if (keys_str.empty()) return;
	split(keywords, keys_str, "|");
	for (auto key : keywords)
		trim(key);
}

// check dead url for one or all entries
// if `entries` is empty, then check all non-deleted entries
// TODO: not tested!
inline void check_url(vecStr &entries, Str_I entry_start = "")
{
	if (entries.empty()) {
		entries.clear();
		file_list_ext(entries, gv::path_out, "html", false);
	}
	Str html, fpath, url, res, cmd;
	unordered_set<Str> checked_links;
	map<Str, unordered_map<Str,Str>> bad_links; // entry -> vector of (ulr, stdout)
	Long start = 0;
	if (!entry_start.empty()) {
		start = search(entry_start, entries);
		if (start < 0)
			throw scan_err(entries[start] + ".html 未找到");
	}
	for (Long i = start; i < size(entries); ++i) {
		auto &entry = entries[i];
		fpath = gv::path_out; fpath << entry << ".html";
		cout << "\n\n" << fpath << "\n----------------" << endl;
		read(html, fpath);
		if (!is_valid(html))
			throw std::runtime_error(u8"内部错误： 非法的 UTF-8 文档： " + entry + ".tex");
		CRLF_to_LF(html);

		Long ind = -1, ikey;
		while (1) {
			ind = find(ikey, html, {"http://", "https://"}, ++ind);
			if (ind < 0) break;
			Long ind1 = (Long)html.find_first_of("\" >", ind);
			if (ind1 < 0) continue;
			url = html.substr(ind, ind1-ind);
			Long ind = rfind(url, '#');
			if (ind > 0) // remove html tag
				url.resize(ind);
			if (checked_links.count(url))
				continue;
			checked_links.insert(url);
			bool check_url = true;
			if (find(ikey, url, {"wikipedia.org", "github.com", "chaoli.club"}) >= 0) {
				// TODO: 替换成 wanwei 百科 url 并检查。 wanwei 自带审核，可以放心。
				check_url = false;
			}
			if (check_url) {
				int ret = 0;
				bool exec_err = false;
				cmd = "curl --max-time 5 -ILs \""; cmd << url <<
					R"(" | awk 'BEGIN{RS="\r\n\r\n"}{header=$0}END{print header}' | head -n 1)";
				try {
					ret = exec_str(res, cmd);
				}
				catch (...) {
					exec_err = true; res = u8"exec_str() error!";
				}
				bool wrong_http_code = true;
				Long ind = find(res, ' ');
				if (ind >= 0 && search(res.substr(ind+1, 3), {"200"}) >= 0)
					wrong_http_code = false;
				if (exec_err || ret || wrong_http_code) {
					bad_links[entry][url] = res;
					cout << SLS_RED_BOLD << u8"无法正常访问： " << url << SLS_NO_STYLE << endl;
					cout << SLS_YELLOW_BOLD << cmd << SLS_NO_STYLE << endl;
					cout << SLS_YELLOW_BOLD << res << SLS_NO_STYLE << endl;
					checked_links.erase(url);
				}
				else
					cout << u8"可以正常访问： " << url << endl;
			}
		}
	}
	for (auto &e : bad_links) {
		for (auto &e1 : e.second) {
			cout << "\n---------------------" << endl;
			cout << SLS_RED_BOLD << e.first << ".html" << SLS_NO_STYLE << endl;
			cout << e1.first << endl;
			cout << e1.second << endl;
		}
	}
}
