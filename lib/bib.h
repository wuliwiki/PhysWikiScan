#pragma once

// replace "\cite{}" with `[?]` cytation linke
inline Long cite(unordered_map<Str, Bool> &bibs_change,
	Str_IO str, Str_I entry, SQLite::Database &db_read)
{
	// get "entries.bibs" for entry to detect deletion
	unordered_set<Str> db_bibs;
	SQLite::Statement stmt_select0(db_read,
		R"(SELECT "bibs" FROM "entries" WHERE "id"=?;)");
	stmt_select0.bind(1, entry);
	if (!stmt_select0.executeStep())
		throw internal_err(SLS_WHERE);
	parse(db_bibs, stmt_select0.getColumn(0));

	SQLite::Statement stmt_select(db_read,
		R"(SELECT "order", "details" FROM "bibliography" WHERE "id"=?;)");
	Long ind0 = 0, N = 0;
	Str bib_id;
	while (1) {
		ind0 = find_command(str, "cite", ind0);
		if (ind0 < 0)
			break;
		++N;
		command_arg(bib_id, str, ind0);
		stmt_select.bind(1, bib_id);
		if (!stmt_select.executeStep())
			throw scan_err(u8"文献 label 未找到（请检查并编译 bibliography.tex）：" + bib_id);
		if (!db_bibs.count(bib_id)) { // new \cite{}
			SLS_WARN(u8"发现新的文献引用（将添加）： " + bib_id);
			bibs_change[bib_id] = true;
		}
		else
			db_bibs.erase(bib_id);
		Long ibib = (int)stmt_select.getColumn(0);
		// bib_detail = (const char*)stmt_select.getColumn(1);
		stmt_select.reset();
		Long ind1 = skip_command(str, ind0, 1);
		str.replace(ind0, ind1 - ind0, " <a href=\"" + gv::url + "bibliography.html#"
			+ num2str(ibib+1) + "\">[" + num2str(ibib+1) + "]</a> ");
	}

	// deleted \cite{}
	for (auto &bib_del : db_bibs) {
		SLS_WARN(u8"发现文献引用被删除（将移除）： " + bib_del);
		bibs_change[bib_del] = false;
	}
	return N;
}

inline Long bibliography(vecStr_O bib_labels, vecStr_O bib_details)
{
	Long N = 0;
	Str str, bib_list, bib_label;
	bib_labels.clear(); bib_details.clear();
	read(str, gv::path_in + "bibliography.tex");
	CRLF_to_LF(str);
	Long ind0 = 0;
	while (1) {
		ind0 = find_command(str, "bibitem", ind0);
		if (ind0 < 0) break;
		command_arg(bib_label, str, ind0);
		bib_labels.push_back(bib_label);
		ind0 = skip_command(str, ind0, 1);
		ind0 = expect(str, "\n", ind0);
		bib_details.push_back(getline(str, ind0));
		href(bib_details.back()); Command2Tag("textsl", "<i>", "</i>", bib_details.back());
		++N;
		bib_list += "<span id = \"" + num2str(N) + "\"></span>[" + num2str(N) + "] " + bib_details.back() + "<br>\n";
	}
	Long ret = find_repeat(bib_labels);
	if (ret > 0)
		throw scan_err(u8"文献 label 重复：" + bib_labels[ret]);
	Str html;
	read(html, gv::path_out + "templates/bib_template.html");
	replace(html, "PhysWikiBibList", bib_list);
	write(html, gv::path_out + "bibliography.html");
	return N;
}
