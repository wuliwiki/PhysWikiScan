#pragma once

// a single \upref{} or \upref0{} inside a \pentry{}
struct PentryRef {
	Str entry;
	Long i_node; // "entry:i_node" starting from 1, 0 if there is none
	Bool star; // \upref0{}, i.e. marked * (prefer to be ignored)
	Bool tilde; // omitted in the tree, i.e. marked ~
	PentryRef(Str_I entry, Long_I i_node, Bool_I star, Bool_I tilde):
		entry(entry), i_node(i_node), star(star), tilde(tilde) {};
};

// all \pentry info of an entry
// pentry[i] is a single \pentry{} command
typedef vector<vector<PentryRef>> Pentry;
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

// check if an entry is labeled "\issueDraft"
inline Bool is_draft(Str_I str)
{
	Intvs intv;
	find_env(intv, str, "issues");
	if (intv.size() > 1)
		throw scan_err(u8"每个词条最多支持一个 issues 环境!");
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
// will ignore \pentry{} with no \upref{} or \upref0{}
inline void get_pentry(Pentry_O pentry_raw, Str_I str, SQLite::Database &db_read)
{
	Bool star;
	Long ind0 = -1, ikey, order;
	Str temp, depEntry;
	pentry_raw.clear();
	while (1) {
		ind0 = find_command(str, "pentry", ind0+1);
		if (ind0 < 0)
			break;
		if (pentry_raw.empty() || !pentry_raw.back().empty())
			pentry_raw.emplace_back();
		auto &pentry1 = pentry_raw.back();
		command_arg(temp, str, ind0, 0, 't');
		Long ind1 = 0, ind2 = 0;
		Bool first_upref = true;
		while (1) {
			ind1 = find(ikey, temp, {"\\upref", "\\upref0"}, ind2);
			star = (ikey == 1);
			if (ind1 < 0)
				break;
			if (!first_upref)
				if (expect(temp, u8"，", ind2) < 0)
					throw scan_err(u8R"(\pentry{} 中预备知识格式不对， 应该用中文逗号隔开， 如： \pentry{词条1\upref{文件名1}， 词条2\upref{文件名2}}。)");
			command_arg(depEntry, temp, ind1);
			if (command_has_opt(temp, ind1)) {
				try { order = str2Llong(command_opt(temp, ind1)); }
				catch (...) { throw scan_err(u8R"(\upref[]{} 或 \upref2[]{} 方括号中只能是正整数，用于引用某词条的第 n 个节点。)"); }
				if (order <= 0)
					throw scan_err(u8R"(\upref[]{} 或 \upref2[]{} 方括号中只能是正整数，用于引用某词条的第 n 个节点。)");
			}
			else
				order = 0;
			if (!exist("entries", "id", depEntry, db_read))
				throw scan_err(u8R"(\pentry{} 中 \upref 引用的词条未找到: )" + depEntry + ".tex");
			for (auto &e : pentry1)
				if (depEntry == e.entry)
					throw scan_err(u8R"(\pentry{} 中预备知识重复： )" + depEntry + ".tex");
			pentry1.emplace_back(depEntry, order, star, false); // tilde always false
			ind2 = skip_command(temp, ind1, 1);
			first_upref = false;
		}
	}
	if (!pentry_raw.empty() && pentry_raw.back().empty())
		pentry_raw.pop_back();
}

// get keywords from the comment in the second line
// return numbers of keywords found
// e.g. "关键词1|关键词2|关键词3"
inline Long get_keywords(vecStr_O keywords, Str_I str)
{
	keywords.clear();
	Str word;
	Long ind0 = find(str, '\n', 0);
	if (ind0 < 0 || ind0 == size(str)-1)
		return 0;
	ind0 = expect(str, "%", ind0+1);
	if (ind0 < 0) {
		// SLS_WARN(u8"请在第二行注释关键词： 例如 \"% 关键词1|关键词2|关键词3\"！");
		return 0;
	}
	Str line; get_line(line, str, ind0);
	Long tmp = find(line, '|', 0);
	if (tmp < 0) {
		// SLS_WARN(u8"请在第二行注释关键词： 例如 \"% 关键词1|关键词2|关键词3\"！");
		return 0;
	}

	ind0 = 0;
	while (1) {
		Long ind1 = find(line, '|', ind0);
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

// check dead url for one or all entries
// if `entries` is empty, then check all non-deleted entries
// TODO: not tested!
inline void check_url(vecStr &entries)
{
	if (entries.empty()) {
		entries.clear();
		file_list_ext(entries, gv::path_out, "html", false);
	}
	Str html, fpath, url, res;
	unordered_set<Str> checked_links;
	map<Str, unordered_map<Str,Str>> bad_links; // entry -> vector of (ulr, stdout)
	for (auto &entry : entries) {
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
			Long ind1 = find(html, '"', ind);
			url = html.substr(ind, ind1-ind);
			if (checked_links.count(url))
				continue;
			checked_links.insert(url);
			bool check_url = true;
			if (find(ikey, url, {"wikipedia.org", "github.com", "chaoli.club"}) >= 0) {
				// TODO: 替换成 wanwei 百科 url 并检查。 wanwei 自带审核，可以放心。
				check_url = false;
			}
			if (check_url) {
				if (exec_str(res, "curl --max-time 5 -Is \"" + url + "\" | head -n 1") != 0 ||
					size(res) < 12 || (
					res.substr(9, 3) != "200" && res.substr(7, 3) != "200" &&
					res.substr(9, 3) != "301" && res.substr(7, 3) != "301" // &&
					// res.substr(9, 3) != "403" && res.substr(7, 3) != "403"
				)) {
					bad_links[entry][url] = res;
					cout << u8"无法正常访问： " << url << endl;
				}
				else
					cout << u8"可以正常访问： " << url << endl;
			}
		}
	}
	for (auto &e : bad_links) {
		for (auto &e1 : e.second) {
			cout << "---------------------" << endl;
			cout << e.first << ".html" << endl;
			cout << e1.first << endl;
			cout << e1.second << endl;
		}
	}
}
