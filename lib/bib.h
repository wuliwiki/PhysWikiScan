#pragma once

struct BibInfo {
	Long order{}; // starts from 1
	Str detail;
	set<Str> ref_by; // {entry1, entry2, ...}
};

// update "bibliography" table of sqlite db
inline void db_update_bib(vecStr_I bib_labels, vecStr_I bib_details, SQLite::Database &db) {
	// read the whole db bibliography table
	SQLite::Statement stmt_select(db,
								  R"(SELECT "id", "order", "details", "ref_by" FROM "bibliography" WHERE "id" != '';)");
	SQLite::Statement stmt_delete(db,
								  R"(DELETE FROM "bibliography" WHERE "id"=?;)");
	unordered_map<Str, BibInfo> db_bib_info; // id -> (order, details, ref_by)

	while (stmt_select.executeStep()) {
		const Str &id = stmt_select.getColumn(0);
		auto &info = db_bib_info[id];
		info.order = (int)stmt_select.getColumn(1);
		info.detail = stmt_select.getColumn(2).getString();
		const Str &ref_by_str = stmt_select.getColumn(3);
		parse(info.ref_by, ref_by_str);
		if (search(id, bib_labels) < 0) {
			if (!info.ref_by.empty()) {
				clear(sb) << u8"检测到删除的文献： " << id << u8"， 但被以下词条引用： " << ref_by_str;
				throw scan_err(sb);
			}
			clear(sb) << u8"检测到删除文献（将删除）： " << to_string(info.order) << ". " << id;
			db_log(sb);
			stmt_delete.bind(1, id);
			stmt_delete.exec(); stmt_delete.reset();
		}
	}
	stmt_select.reset();

	SQLite::Statement stmt_update(db,
								  R"(UPDATE "bibliography" SET "order"=?, "details"=? WHERE "id"=?;)");
	SQLite::Statement stmt_insert(db,
								  R"(INSERT INTO "bibliography" ("id", "order", "details") VALUES (?, ?, ?);)");
	unordered_set<Str> id_flip_sign;

	for (Long i = 0; i < size(bib_labels); i++) {
		Long order = i+1;
		const Str &id = bib_labels[i], &bib_detail = bib_details[i];
		if (db_bib_info.count(id)) {
			auto &info = db_bib_info[id];
			bool changed = false;
			if (order != info.order) {
				clear(sb) << u8"数据库中文献 "; sb << id << " 编号改变（将更新）： " << to_string(info.order)
												   << " -> " << to_string(order);
				db_log(sb);
				changed = true;
				id_flip_sign.insert(id);
				stmt_update.bind(1, -(int)order); // to avoid unique constraint
			}
			if (bib_detail != info.detail) {
				clear(sb) << u8"数据库中文献 " << id << " 详情改变（将更新）： " << info.detail << " -> " << bib_detail;
				db_log(sb);
				changed = true;
				stmt_update.bind(1, (int)order); // to avoid unique constraint
			}
			if (changed) {
				stmt_update.bind(2, bib_detail);
				stmt_update.bind(3, id);
				stmt_update.exec(); stmt_update.reset();
			}
		}
		else {
			db_log(u8"数据库中不存在文献（将添加）： " + num2str(order) + ". " + id);
			stmt_insert.bind(1, id);
			stmt_insert.bind(2, -(int)order);
			stmt_insert.bind(3, bib_detail);
			stmt_insert.exec(); stmt_insert.reset();
			id_flip_sign.insert(id);
		}
	}

	// set the orders back
	SQLite::Statement stmt_update2(db,
								   R"(UPDATE "bibliography" SET "order" = "order" * -1 WHERE "id"=?;)");
	for (auto &id : id_flip_sign) {
		stmt_update2.bind(1, id);
		stmt_update2.exec(); stmt_update2.reset();
	}
}

// replace "\cite{}" with `[?]` cytation linke
inline Long cite(unordered_map<Str, Bool> &bibs_change,
	Str_IO str, Str_I entry, SQLite::Database &db_read)
{
	// get "entries.bibs" for entry to detect deletion
	vecStr db_bibs; vecBool db_bibs_cited; // entries.bibs -> cited
	SQLite::Statement stmt_select0(db_read,
		R"(SELECT "bibs" FROM "entries" WHERE "id"=?;)");
	stmt_select0.bind(1, entry);
	if (!stmt_select0.executeStep())
		throw internal_err(SLS_WHERE);
	parse(db_bibs, stmt_select0.getColumn(0));
	db_bibs_cited.resize(db_bibs.size(), false);

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
	    Long bib_ind = search(bib_id, db_bibs);
		if (bib_ind < 0) { // new \cite{}
			db_log(u8"发现新的文献引用（将添加）： " + bib_id);
			bibs_change[bib_id] = true; // true means add bib_id to entries.bib
		}
		else
			db_bibs_cited[bib_ind] = true;
		Long ibib = (int)stmt_select.getColumn(0);
		stmt_select.reset();
		Long ind1 = skip_command(str, ind0, 1);
		str.replace(ind0, ind1 - ind0, " <a href=\"" + gv::url + "bibliography.html#"
			+ num2str(ibib+1) + "\">[" + num2str(ibib+1) + "]</a> ");
	}

	// deleted \cite{}
	for (Long i = 0; i < size(db_bibs); ++i) {
	    if (!db_bibs_cited[i]) {
	        db_log(u8"发现文献引用被删除（将移除）： " + db_bibs[i]);
	        bibs_change[db_bibs[i]] = false; // false means remove db_bibs[i] from entries.bib
	    }
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
