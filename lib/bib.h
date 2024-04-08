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
		info.order = stmt_select.getColumn(1).getInt64();
		info.detail = stmt_select.getColumn(2).getString();
		const Str &ref_by_str = stmt_select.getColumn(3);
		parse(info.ref_by, ref_by_str);
		if (search(id, bib_labels) < 0) {
			if (!info.ref_by.empty()) {
				clear(sb) << u8"检测到删除的文献： " << id << u8"， 但被以下文章引用： " << ref_by_str;
				throw scan_err(sb);
			}
			clear(sb) << u8"检测到删除文献（将删除）： " << to_string(info.order) << ". " << id;
			db_log(sb);
			stmt_delete.bind(1, id);
			if (stmt_delete.exec() != 1) throw internal_err(SLS_WHERE);
			stmt_delete.reset();
		}
	}
	stmt_select.reset();

	SQLite::Statement stmt_update(db,
		R"(UPDATE "bibliography" SET "order"=?, "details"=? WHERE "id"=?;)");
	SQLite::Statement stmt_insert(db,
		R"(INSERT OR REPLACE INTO "bibliography" ("id", "order", "details") VALUES (?, ?, ?);)");
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
				stmt_update.bind(1, -(int64_t)order); // to avoid unique constraint
			}
			if (bib_detail != info.detail) {
				clear(sb) << u8"数据库中文献 " << id << " 详情改变（将更新）： " << info.detail << " -> " << bib_detail;
				db_log(sb);
				changed = true;
				stmt_update.bind(1, (int64_t)order); // to avoid unique constraint
			}
			if (changed) {
				stmt_update.bind(2, bib_detail);
				stmt_update.bind(3, id);
				if (stmt_update.exec() != 1) throw internal_err(SLS_WHERE);
				stmt_update.reset();
			}
		}
		else {
			db_log(u8"数据库中不存在文献（将添加）： " + num2str(order) + ". " + id);
			stmt_insert.bind(1, id);
			stmt_insert.bind(2, -(int64_t)order);
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
		if (stmt_update2.exec() != 1) throw internal_err(SLS_WHERE);
		stmt_update2.reset();
	}
}

// replace "\cite{}" with `[?]` citation linke
// generate local bib list
inline void cite(
	unordered_map<Str,Long> &bib_order, // bibID -> order of first appearance
	Str_IO str, Str_I entry, SQLite::Database &db_read)
{
	bib_order.clear();
	SQLite::Statement stmt_select(db_read,
		R"(SELECT "details" FROM "bibliography" WHERE "id"=?;)");

	Long ind0 = 0, order = 0;
	Str bib_id;
	// replace \cite{}
	while (1) {
		ind0 = find_command(str, "cite", ind0);
		if (ind0 < 0)
			break;
		command_arg(bib_id, str, ind0);
		bool is_new = false;
		if (!bib_order.count(bib_id)) {
			bib_order[bib_id] = (order = bib_order.size() + 1);
			is_new = true;
		}
		else
			order = bib_order[bib_id];
		Long ind1 = skip_command(str, ind0, 1);
		clear(sb) << " <a href=\"" << gv::url << entry << ".html#bib" << order << '"';
		if (is_new) sb << " id=\"bret" << order << '"';
		sb << ">[" << order << "]</a> ";
		str.replace(ind0, ind1 - ind0, sb);
	}
	// generate local bib list
	if (!bib_order.empty()) {
		Str detail;
		vector<const Str *> bibId_sorted(bib_order.size());
		str << "\n<hr><p>\n";
		for (auto &e : bib_order)
			bibId_sorted[e.second-1] = &e.first;
		for (Long i = 0; i < size(bibId_sorted); ++i) {
			Long order = i+1;
			stmt_select.bind(1, *bibId_sorted[i]);
			if (!stmt_select.executeStep())
				throw scan_err(u8"文献 label 未找到（请检查并编译 bibliography.tex）：" + bib_id);
			detail = stmt_select.getColumn(0).getString();
			stmt_select.reset();
			href(detail); Command2Tag("textsl", "<i>", "</i>", detail);
			str << "<a href = \"" << gv::url << entry << ".html#bret" << order << "\" id=\"bib"
				<< order << "\">[" << order << "] <b>^</b></a> " << detail << "<br>\n";
		}
	}
}

// convert bibliography.tex to html page
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

// update "entry_bibs" table
inline void db_update_entry_bibs(
		//                 entry ->           (bib -> \cite{} order)
		const unordered_map<Str, unordered_map<Str,   Long>> &entry_bib_order,
		SQLite::Database &db_rw)
{
	unordered_map<vecSQLval, vecSQLval> records; // (entry,bib) -> order
	for (auto &e : entry_bib_order) {
		auto &entry = e.first;
		records.clear();
		for (auto &bib_order : e.second) {
			vecSQLval key(2), val(1);
			key[0] = entry; key[1] = bib_order.first; val[0] = bib_order.second;
			records[move(key)] = move(val);
		}
		update_sqlite_table(records, "entry_bibs", R"("entry"=')" + entry + '\'',
			{"entry", "bib", "order"}, 2, db_rw, &sqlite_callback);
	}
}
