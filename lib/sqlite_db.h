#pragma once
#include <regex>
#include "../SLISC/str/str.h"
#include "../SLISC/file/file.h"
#include "../SLISC/util/time.h"
#include "../SLISC/util/sha1sum.h"

// add or delete elements from a set
template <class T>
inline void change_set(set<T> &s, const unordered_map<T, bool> &change)
{
	for (auto &e : change)
		if (e.second)
			s.insert(e.first);
		else
			s.erase(e.first);
}

// get table of content info from db "chapters", in ascending "order"
inline void db_get_chapters(vecStr_O ids, vecStr_O captions, vecStr_O parts,
							SQLite::Database &db)
{
	SQLite::Statement stmt(db,
		R"(SELECT "id", "order", "caption", "part" FROM "chapters" ORDER BY "order" ASC)");
	vecLong orders;
	while (stmt.executeStep()) {
		ids.push_back(stmt.getColumn(0));
		orders.push_back((int)stmt.getColumn(1));
		captions.push_back(stmt.getColumn(2));
		parts.push_back(stmt.getColumn(3));
	}
	for (Long i = 0; i < size(orders); ++i)
		if (orders[i] != i)
			SLS_ERR("something wrong!");
}

// get table of content info from db "parts", in ascending "order"
inline void db_get_parts(vecStr_O ids, vecStr_O captions, SQLite::Database &db)
{
	SQLite::Statement stmt(db,
		R"(SELECT "id", "order", "caption" FROM "parts" ORDER BY "order" ASC)");
	vecLong orders;
	while (stmt.executeStep()) {
		ids.push_back(stmt.getColumn(0));
		orders.push_back((int)stmt.getColumn(1));
		captions.push_back(stmt.getColumn(2));
	}
	for (Long i = 0; i < size(orders); ++i)
		if (orders[i] != i)
			SLS_ERR("something wrong!");
}

inline void db_check_add_entry_simulate_editor(vecStr_I entries, SQLite::Database &db_rw)
{
	SQLite::Statement stmt_select(db_rw,
		R"(SELECT "caption", "deleted" FROM "entries" WHERE "id"=?;)");
	SQLite::Statement stmt_insert(db_rw,
		R"(INSERT INTO "entries" ("id", "caption", "draft", "deleted") VALUES (?, ?, 1, 0);)");
	SQLite::Statement stmt_undelete(db_rw,
		R"(UPDATE "entries" SET "deleted"=0 WHERE "id"=?;)");
	Str str, title;

	for (auto &entry : entries) {
		clear(sb) << gv::path_in; sb << "contents/" << entry << ".tex";
		if (!file_exist(sb))
			SLS_ERR("词条文件不存在：" + entry + ".tex");
		stmt_select.bind(1, entry);
		bool deleted = false;
		if (!stmt_select.executeStep()) {
			db_log(u8"词条不存在数据库中， 将模拟 editor 添加： " + entry);
			// 从 tex 文件获取标题
			read(str, sb); // read tex file
			CRLF_to_LF(str);
			get_title(title, str);
			// 写入数据库
			stmt_insert.bind(1, entry);
			stmt_insert.bind(2, title);
			stmt_insert.exec(); stmt_insert.reset();
		}
		else {
			deleted = (int)stmt_select.getColumn(1);
			if (deleted) {
				title = (const char*)stmt_select.getColumn(0);
				clear(sb) << u8"词条文件存在，但数据库却标记了已删除（将恢复）："
					<< entry << " (" << title << ')';
				db_log(sb);
				stmt_undelete.bind(1, entry);
				stmt_undelete.exec();
				stmt_undelete.reset();
			}
		}
		stmt_select.reset();
	}
}

// delete and rewrite "chapters" and "parts" table of sqlite db
inline void db_update_parts_chapters(
		vecStr_I part_ids, vecStr_I part_name, vecStr_I chap_first, vecStr_I chap_last,
		vecStr_I chap_ids, vecStr_I chap_name, vecLong_I chap_part,
		vecStr_I entry_first, vecStr_I entry_last, SQLite::Database &db_rw)
{
	cout << "updating sqlite database (" << part_name.size() << " parts, "
		 << chap_name.size() << " chapters) ..." << endl;
	cout.flush();
	check_foreign_key(db_rw, false);
	cout << "clear parts and chatpers tables" << endl;
	table_clear("parts", db_rw); table_clear("chapters", db_rw);

	// insert parts
	cout << "inserting parts to db_rw..." << endl;
	SQLite::Statement stmt_insert_part(db_rw,
		R"(INSERT INTO "parts" ("id", "order", "caption", "chap_first", "chap_last") VALUES (?, ?, ?, ?, ?);)");
	for (Long i = 0; i < size(part_name); ++i) {
		// cout << "part " << i << ". " << part_ids[i] << ": " << part_name[i] << " chapters: " << chap_first[i] << " -> " << chap_last[i] << endl;
		stmt_insert_part.bind(1, part_ids[i]);
		stmt_insert_part.bind(2, int(i));
		stmt_insert_part.bind(3, part_name[i]);
		stmt_insert_part.bind(4, chap_first[i]);
		stmt_insert_part.bind(5, chap_last[i]);
		stmt_insert_part.exec(); stmt_insert_part.reset();
	}
	cout << "\n\n\n" << endl;

	// insert chapters
	cout << "inserting chapters to db_rw..." << endl;
	SQLite::Statement stmt_insert_chap(db_rw,
		R"(INSERT INTO "chapters" ("id", "order", "caption", "part", "entry_first", "entry_last") VALUES (?, ?, ?, ?, ?, ?);)");

	for (Long i = 0; i < size(chap_name); ++i) {
		// cout << "chap " << i << ". " << chap_ids[i] << ": " << chap_name[i] << " chapters: " << entry_first[i] << " -> " << entry_last[i] << endl;
		stmt_insert_chap.bind(1, chap_ids[i]);
		stmt_insert_chap.bind(2, int(i));
		stmt_insert_chap.bind(3, chap_name[i]);
		stmt_insert_chap.bind(4, part_ids[chap_part[i]]);
		stmt_insert_chap.bind(5, entry_first[i]);
		stmt_insert_chap.bind(6, entry_last[i]);
		stmt_insert_chap.exec(); stmt_insert_chap.reset();
	}
	check_foreign_key(db_rw, false);
	cout << "done." << endl;
}

// update "entries" table of sqlite db, based on the info from main.tex
// `entries` are a list of entreis from main.tex
inline void db_update_entries_from_toc(
		vecStr_I entries, vecStr_I titles, vecLong_I entry_part, vecStr_I part_ids,
		vecLong_I entry_chap, vecStr_I chap_ids, SQLite::Database &db_rw)
{
	cout << "updating sqlite database (" <<
		entries.size() << " entries) with info from main.tex..." << endl; cout.flush();

	SQLite::Statement stmt_update(db_rw,
		R"(UPDATE "entries" SET "caption"=?, "part"=?, "chapter"=?, "last"=?, "next"=? WHERE "id"=?;)");
	SQLite::Statement stmt_select(db_rw,
		R"(SELECT "caption", "keys", "pentry", "part", "chapter", "last", "next" FROM "entries" WHERE "id"=?;)");
	Str empty;
	Long N = size(entries);

	for (Long i = 0; i < N; i++) {
		auto &entry = entries[i];
		auto &entry_last = (i == 0 ? empty : entries[i-1]);
		auto &entry_next = (i == N-1 ? empty : entries[i+1]);

		// does entry already exist (expected)?
		Bool entry_exist;
		entry_exist = exist("entries", "id", entry, db_rw);
		if (entry_exist) {
			// check if there is any change (unexpected)
			stmt_select.bind(1, entry);
			SLS_ASSERT(stmt_select.executeStep());
			Str db_title = (const char*) stmt_select.getColumn(0);
			Str db_key_str = (const char*) stmt_select.getColumn(1);
			Str db_pentry_str = (const char*) stmt_select.getColumn(2);
			Str db_part = (const char*) stmt_select.getColumn(3);
			Str db_chapter = (const char*) stmt_select.getColumn(4);
			Str db_last = (const char*) stmt_select.getColumn(5);
			Str db_next = (const char*) stmt_select.getColumn(6);
			stmt_select.reset();

			bool changed = false;
			if (titles[i] != db_title) {
				clear(sb) << entry << " 检测到标题改变（将更新） " << db_title << " -> " << titles[i];
				db_log(sb);
				changed = true;
			}
			if (part_ids[entry_part[i]] != db_part) {
				clear(sb) << entry << " 检测到所在部分改变（将更新） " << db_part << " -> " << part_ids[entry_part[i]];
				db_log(sb);
				changed = true;
			}
			if (chap_ids[entry_chap[i]] != db_chapter) {
				clear(sb) << entry << " 检测到所在章节改变（将更新） " << db_chapter << " -> " << chap_ids[entry_chap[i]];
				db_log(sb);
				changed = true;
			}
			if (entry_last != db_last) {
				clear(sb) << entry << " 检测到上一个词条改变（将更新） " << db_last << " -> " << entry_last;
				db_log(sb);
				changed = true;
			}
			if (entry_next != db_next) {
				clear(sb) << entry << " 检测到下一个词条改变（将更新） " << db_next << " -> " << entry_next;
				db_log(sb);
				changed = true;
			}
			if (changed) {
				stmt_update.bind(1, titles[i]);
				stmt_update.bind(2, part_ids[entry_part[i]]);
				stmt_update.bind(3, chap_ids[entry_chap[i]]);
				stmt_update.bind(4, entry_last);
				stmt_update.bind(5, entry_next);
				stmt_update.bind(6, entry);
				stmt_update.exec(); stmt_update.reset();
			}
		}
		else // entry_exist == false
			throw scan_err(u8"main.tex 中的词条在数据库中未找到： " + entry);
	}
	cout << "done." << endl;
}

// sum history.add and history.del for an author for a given time period
// time_start and time_end are in yyyymmddmmss format
inline void author_char_stat(Str_I time_start, Str_I time_end, Str author, SQLite::Database &db_read)
{
	if (time_start.size() != 12 || time_end.size() != 12)
		throw scan_err(u8"日期格式不正确" SLS_WHERE);
	Long authorID = -1;
	SQLite::Statement stmt_author0(db_read,
		R"(SELECT "id", "name" FROM "authors" WHERE "name"=?;)");
	stmt_author0.bind(1, author);
	Long Nfound = 0;
	while (stmt_author0.executeStep()) {
		++Nfound;
		authorID = (int)stmt_author0.getColumn(0);
		cout << "作者： " << stmt_author0.getColumn(1).getString() << endl;
		cout << "id: " << authorID << endl;
	}
	if (Nfound > 1)
		throw scan_err(u8"数据库中找到多于一个作者名（精确匹配）： " + author);
	else if (Nfound == 0) {
		cout << "精确匹配失败， 尝试模糊匹配……" << endl;
		SQLite::Statement stmt_author(db_read,
			R"(SELECT "id", "name" FROM "authors" WHERE "name" LIKE ? ESCAPE '\';)");
		replace(author, "_", "\\_"); replace(author, "%", "\\%");
		stmt_author.bind(1, '%' + author + '%');
		Nfound = 0;
		while (stmt_author.executeStep()) {
			++Nfound;
			authorID = (int)stmt_author.getColumn(0);
			cout << "作者： " << stmt_author.getColumn(1).getString() << endl;
			cout << "id: " << authorID << endl;
		}
		if (Nfound == 0)
			throw scan_err(u8"数据库中找不到作者名（模糊匹配）： " + author);
		if (Nfound > 1)
			throw scan_err(u8"数据库中找到多于一个作者名（模糊匹配）： " + author);
	}

	sb = R"(SELECT SUM("add"), SUM("del") FROM "history" WHERE "author"=)";
	sb << authorID << R"( AND "time" >= ')" << time_start << R"(' AND "time" <= ')" << time_end << "';";
	cout << "SQL 命令:\n" << sb << endl;
	SQLite::Statement stmt_select(db_read, sb);
	if (!stmt_select.executeStep())
		throw internal_err(SLS_WHERE);

	auto data = stmt_select.getColumn(0);
	Long tot_add = data.isNull() ? 0 : data.getInt64();

	data = stmt_select.getColumn(1);
	Long tot_del = data.isNull() ? 0 : data.getInt64();

	cout << "新增字符：" << tot_add << endl;
	cout << "删除字符：" << tot_del << endl;
	cout << "新增减删除：" << tot_add - tot_del << endl;
}

// update entries.bibs and bibliography.ref_by
inline void db_update_entry_bibs(const unordered_map<Str, unordered_map<Str, Bool>> &entry_bibs_change, SQLite::Database &db_rw)
{
	// convert arguments
	unordered_map<Str, unordered_map<Str, Bool>> bib_ref_bys_change; // bib -> (entry -> [1]add/[0]del)
	for (auto &e : entry_bibs_change) {
		auto &entry = e.first;
		for (auto &bib: e.second)
			bib_ref_bys_change[bib.first][entry] = bib.second;
	}

	// =========== change bibliography.ref_by =================
	cout << "add & delete bibliography.ref_by..." << endl;
	SQLite::Statement stmt_update_ref_by(db_rw,
		R"(UPDATE "bibliography" SET "ref_by"=? WHERE "id"=?;)");
	Str ref_by_str;
	set<Str> ref_by;

	for (auto &e : bib_ref_bys_change) {
		auto &bib = e.first;
		auto &by_entries = e.second;
		ref_by.clear();
		ref_by_str = get_text("bibliography", "id", bib, "ref_by", db_rw);
		parse(ref_by, ref_by_str);
		change_set(ref_by, by_entries);
		join(ref_by_str, ref_by);
		stmt_update_ref_by.bind(1, ref_by_str);
		stmt_update_ref_by.bind(2, bib);
		stmt_update_ref_by.exec(); stmt_update_ref_by.reset();
	}
	cout << "done!" << endl;

	// =========== add & delete entry.bibs =================
	cout << "add & delete entry.bibs ..." << endl;
	Str bibs_str;
	set<Str> bibs;
	SQLite::Statement stmt_select_entry_bibs(db_rw,
		R"(SELECT "bibs" FROM "entries" WHERE "id"=?)");
	SQLite::Statement stmt_update_entry_bibs(db_rw,
		R"(UPDATE "entries" SET "bibs"=? WHERE "id"=?)");

	for (auto &e : entry_bibs_change) {
		auto &entry = e.first;
		auto &bibs_changed = e.second;
		stmt_select_entry_bibs.bind(1, entry);
		if (!stmt_select_entry_bibs.executeStep())
			throw internal_err("entry 找不到： " + entry + SLS_WHERE);
		parse(bibs, stmt_select_entry_bibs.getColumn(0));
		stmt_select_entry_bibs.reset();
		for (auto &bib : bibs_changed) {
			if (bib.second)
				bibs.insert(bib.first);
			else
				bibs.erase(bib.first);
		}
		join(bibs_str, bibs);
		stmt_update_entry_bibs.bind(1, bibs_str);
		stmt_update_entry_bibs.bind(2, entry);
		stmt_update_entry_bibs.exec(); stmt_update_entry_bibs.reset();
	}
	cout << "done!" << endl;
}

// update entries.uprefs, entries.ref_by
inline void db_update_uprefs(
		const unordered_map<Str, unordered_map<Str, Bool>> &entry_uprefs_change,
		SQLite::Database &db_read, SQLite::Database &db_rw)
{
	cout << "updating entries.uprefs ..." << endl;
	SQLite::Statement stmt_select(db_rw,
		R"(SELECT "uprefs" FROM "entries" WHERE "id"=?;)");
	SQLite::Statement stmt_update(db_rw,
		R"(UPDATE "entries" SET "uprefs"=? WHERE "id"=?;)");
	Str str;
	set<Str> uprefs, ref_by;
	for (auto &e : entry_uprefs_change) {
		auto &entry = e.first;
		auto &uprefs_change = e.second;
		stmt_select.bind(1, entry);
		if (!stmt_select.executeStep())
			throw internal_err(SLS_WHERE);
		parse(uprefs, stmt_select.getColumn(0));
		stmt_select.reset();
		for (auto &ee : uprefs_change) {
			auto &entry_refed = ee.first;
			auto &is_add = ee.second;
			bool deleted = get_int("entries", "id", entry_refed, "deleted", db_read);
			if (deleted) {
				if (is_add)
					throw scan_err(u8"不允许 \\upref{被删除的词条}：" + entry_refed + SLS_WHERE);
				else
					db_log(u8"检测到删除命令 \\upref{被删除的词条}（删除词条时应该已经确保了没有被 upref 才对）（将视为没有被删除）：" + entry_refed);
			}
		}
		change_set(uprefs, uprefs_change);
		join(str, uprefs);
		stmt_update.bind(1, str);
		stmt_update.bind(2, entry);
		stmt_update.exec(); stmt_update.reset();
	}

	cout << "updating entries.ref_by ..." << endl;
	unordered_map<Str, unordered_map<Str, Bool>> entry_ref_bys_change;
	for (auto &e : entry_uprefs_change)
		for (auto &ref : e.second)
			entry_ref_bys_change[ref.first][e.first] = ref.second;

	SQLite::Statement stmt_select2(db_rw,
		R"(SELECT "ref_by" FROM "entries" WHERE "id"=?;)");
	SQLite::Statement stmt_update2(db_rw,
		R"(UPDATE "entries" SET "ref_by"=? WHERE "id"=?;)");
	for (auto &e : entry_ref_bys_change) {
		stmt_select2.bind(1, e.first);
		if (!stmt_select2.executeStep()) throw internal_err(SLS_WHERE);
		parse(ref_by, stmt_select2.getColumn(0));
		stmt_select2.reset();
		change_set(ref_by, e.second);
		join(str, ref_by);
		stmt_update2.bind(1, str);
		stmt_update2.bind(2, e.first);
		stmt_update2.exec(); stmt_update2.reset();
	}

}

// update entries.refs, labels.ref_by, figures.ref_by
inline void db_update_refs(const unordered_map<Str, unordered_set<Str>> &entry_add_refs,
	unordered_map<Str, unordered_set<Str>> &entry_del_refs,
	SQLite::Database &db_rw)
{
	// transform arguments
	unordered_map<Str, unordered_set<Str>> label_add_ref_bys, fig_add_ref_bys;
	for (auto &e : entry_add_refs) {
		auto &entry = e.first;
		for (auto &label: e.second) {
			if (label_type(label) == "fig")
				fig_add_ref_bys[label_id(label)].insert(entry);
			else
				label_add_ref_bys[label].insert(entry);
		}
	}

	unordered_map<Str, unordered_set<Str>> label_del_ref_bys, fig_del_ref_bys;
	for (auto &e : entry_del_refs) {
		auto &entry = e.first;
		for (auto &label: e.second) {
			if (label_type(label) == "fig")
				fig_del_ref_bys[label_id(label)].insert(entry);
			else
				label_del_ref_bys[label].insert(entry);
		}
	}

	// =========== add & delete labels.ref_by =================
	cout << "add & delete labels ref_by..." << endl;
	SQLite::Statement stmt_update_ref_by(db_rw,
		R"(UPDATE "labels" SET "ref_by"=? WHERE "id"=?;)");
	Str ref_by_str;
	set<Str> ref_by;

	for (auto &e : label_add_ref_bys) {
		auto &label = e.first;
		auto &by_entries = e.second;
		ref_by.clear();
		ref_by_str = get_text("labels", "id", label, "ref_by", db_rw);
		parse(ref_by, ref_by_str);
		for (auto &by_entry : by_entries)
			ref_by.insert(by_entry);
		if (label_del_ref_bys.count(label)) {
			for (auto &by_entry : label_del_ref_bys[label])
				ref_by.erase(by_entry);
			label_del_ref_bys.erase(label);
		}
		join(ref_by_str, ref_by);
		stmt_update_ref_by.bind(1, ref_by_str);
		stmt_update_ref_by.bind(2, label);
		stmt_update_ref_by.exec(); stmt_update_ref_by.reset();
	}
	cout << "done!" << endl;

	// =========== add & delete figures.ref_by =================
	cout << "add & delete figures ref_by..." << endl;
	SQLite::Statement stmt_update_ref_by_fig(db_rw,
		R"(UPDATE "figures" SET "ref_by"=? WHERE "id"=?;)");
	unordered_map<Str, set<Str>> new_entry_figs;
	// add to ref_by
	for (auto &e : fig_add_ref_bys) {
		auto &fig_id = e.first;
		auto &by_entries = e.second;
		ref_by.clear();
		ref_by_str = get_text("figures", "id", fig_id, "ref_by", db_rw);
		parse(ref_by, ref_by_str);
		for (auto &by_entry : by_entries)
			ref_by.insert(by_entry);
		if (fig_del_ref_bys.count(fig_id)) {
			for (auto &by_entry : fig_del_ref_bys[fig_id])
				ref_by.erase(by_entry);
			fig_del_ref_bys.erase(fig_id);
		}
		join(ref_by_str, ref_by);
		stmt_update_ref_by_fig.bind(1, ref_by_str);
		stmt_update_ref_by_fig.bind(2, fig_id);
		stmt_update_ref_by_fig.exec(); stmt_update_ref_by_fig.reset();
	}
	cout << "done!" << endl;

	// ========== delete from labels.ref_by ===========
	cout << "delete from labels.ref_by" << endl;
	Str type;
	for (auto &e : label_del_ref_bys) {
		auto &label = e.first;
		auto &by_entries = e.second;
		try {
			ref_by_str = get_text("labels", "id", label, "ref_by", db_rw);
		}
		catch (const std::exception &e) {
			if (find(e.what(), "row not found") >= 0)
				continue; // label deleted
		}
		parse(ref_by, ref_by_str);
		for (auto &by_entry: by_entries)
			ref_by.erase(by_entry);
		join(ref_by_str, ref_by);
		stmt_update_ref_by.bind(1, ref_by_str);
		stmt_update_ref_by.bind(2, label);
		stmt_update_ref_by.exec(); stmt_update_ref_by.reset();
	}

	// ========== delete from figures.ref_by ===========
	cout << "delete from figures.ref_by" << endl;
	for (auto &e : fig_del_ref_bys) {
		auto &fig_id = e.first;
		auto &by_entries = e.second;
		try {
			ref_by_str = get_text("figures", "id", fig_id, "ref_by", db_rw);
		}
		catch (const std::exception &e) {
			if (find(e.what(), "row not found") >= 0)
				continue; // label deleted
		}
		parse(ref_by, ref_by_str);
		for (auto &by_entry : by_entries)
			ref_by.erase(by_entry);
		join(ref_by_str, ref_by);
		stmt_update_ref_by_fig.bind(1, ref_by_str);
		stmt_update_ref_by_fig.bind(2, fig_id);
		stmt_update_ref_by_fig.exec(); stmt_update_ref_by_fig.reset();
	}
	cout << "done!" << endl;

	// =========== add & delete entries.refs =================
	cout << "add & delete entries.refs ..." << endl;
	Str ref_str;
	set<Str> refs;
	SQLite::Statement stmt_select_entry_refs(db_rw,
		R"(SELECT "refs" FROM "entries" WHERE "id"=?)");
	SQLite::Statement stmt_update_entry_refs(db_rw,
		R"(UPDATE "entries" SET "refs"=? WHERE "id"=?)");
	for (auto &e : entry_add_refs) {
		auto &entry = e.first;
		auto &new_refs = e.second;
		stmt_select_entry_refs.bind(1, entry);
		if (!stmt_select_entry_refs.executeStep())
			throw internal_err("entry 找不到： " + entry + SLS_WHERE);
		parse(refs, stmt_select_entry_refs.getColumn(0));
		stmt_select_entry_refs.reset();
		refs.insert(new_refs.begin(), new_refs.end());
		if (entry_del_refs.count(entry)) {
			for (auto &label: entry_del_refs[entry])
				refs.erase(label);
			entry_del_refs.erase(entry);
		}
		join(ref_str, refs);
		stmt_update_entry_refs.bind(1, ref_str);
		stmt_update_entry_refs.bind(2, entry);
		stmt_update_entry_refs.exec(); stmt_update_entry_refs.reset();
	}
	for (auto &e : entry_del_refs) {
		auto &entry = e.first;
		auto &del_refs = e.second;
		stmt_select_entry_refs.bind(1, entry);
		if (!stmt_select_entry_refs.executeStep())
			throw internal_err("entry 找不到： " + entry + SLS_WHERE);
		parse(refs, stmt_select_entry_refs.getColumn(0));
		stmt_select_entry_refs.reset();
		for (auto &label : del_refs)
			refs.erase(label);
		join(ref_str, refs);
		stmt_update_entry_refs.bind(1, ref_str);
		stmt_update_entry_refs.bind(2, entry);
		stmt_update_entry_refs.exec(); stmt_update_entry_refs.reset();
	}
	cout << "done!" << endl;
}

// make db consistent
// (regenerate derived fields)
// TODO: fix other generated field, i.e. entries.ref_by
inline void arg_fix_db(SQLite::Database &db_read, SQLite::Database &db_rw)
{
	Str refs_str, entry;
	set<Str> refs;
	unordered_map<Str, set<Str>> figs_ref_by, labels_ref_by, bib_ref_by;

	// get all labels.id and figures.id from database, to detect unused ones
	unordered_set<Str> db_figs_unused, db_labels_unused;
	{
		vecStr db_figs0, db_labels0;
		get_column(db_figs0, "figures", "id", db_rw);
		get_column(db_labels0, "labels", "id", db_rw);
		db_figs_unused.insert(db_figs0.begin(), db_figs0.end());
		db_labels_unused.insert(db_labels0.begin(), db_labels0.end());
	}

	// === regenerate "figures.entry", "labels.entry" from "entries.figures", "entries.labels" ======
	cout << R"(regenerate "figures.entry", "labels.entry" from "entries.figures", "entries.labels"...)" << endl;

	SQLite::Statement stmt_select_labels(db_rw, R"(SELECT "id", "labels" FROM "entries" WHERE "labels" != '';)");
	SQLite::Statement stmt_update_labels_entry(db_rw, R"(UPDATE "labels" SET "entry"=? WHERE "id"=?;)");
	unordered_set<Str> labels;
	while (stmt_select_labels.executeStep()) {
		entry = (const char*)stmt_select_labels.getColumn(0);
		parse(labels, stmt_select_labels.getColumn(1));
		for (auto &label : labels) {
			db_labels_unused.erase(label);
			stmt_update_labels_entry.bind(1, entry);
			stmt_update_labels_entry.bind(2, label);
			stmt_update_labels_entry.exec();
			if (!stmt_update_labels_entry.getChanges())
				throw internal_err("数据库 labels 表格中未找到： " + label + SLS_WHERE);
			stmt_update_labels_entry.reset();
		}
	}

	SQLite::Statement stmt_select_figs(db_rw, R"(SELECT "id", "figures" FROM "entries" WHERE "figures" != '';)");
	SQLite::Statement stmt_update_figs_entry(db_rw, R"(UPDATE "figures" SET "entry"=? WHERE "id"=?;)");
	unordered_set<Str> figs;
	while (stmt_select_figs.executeStep()) {
		entry = (const char*)stmt_select_figs.getColumn(0);
		parse(figs, stmt_select_figs.getColumn(1));
		for (auto &fig_id : figs) {
			db_figs_unused.erase(fig_id);
			stmt_update_figs_entry.bind(1, entry);
			stmt_update_figs_entry.bind(2, fig_id);
			stmt_update_figs_entry.exec();
			if (!stmt_update_figs_entry.getChanges())
				throw internal_err("数据库 figures 表格中未找到 entries.figures 中的： " + fig_id + SLS_WHERE);
			stmt_update_figs_entry.reset();
		}
	}
	cout << "done!" << endl;

	// === regenerate "figures.ref_by", "labels.ref_by" from "entries.refs" ===========
	exec(R"(UPDATE "figures" SET "ref_by"='';)", db_rw);
	exec(R"(UPDATE "labels" SET "ref_by"='';)", db_rw);

	cout << R"(regenerate "figures.ref_by", "labels.ref_by" from "entries.refs...")" << endl;
	SQLite::Statement stmt_select_refs(db_rw, R"(SELECT "id", "refs" FROM "entries" WHERE "refs" != '';)");
	while (stmt_select_refs.executeStep()) {
		entry = (const char*)stmt_select_refs.getColumn(0);
		parse(refs, stmt_select_refs.getColumn(1));
		for (auto &ref : refs) {
			if (label_type(ref) == "fig")
				figs_ref_by[label_id(ref)].insert(entry);
			else
				labels_ref_by[ref].insert(entry);
		}
	}

	SQLite::Statement stmt_update_fig_ref_by(db_rw, R"(UPDATE "figures" SET "ref_by"=? WHERE "id"=?;)");
	Str ref_by_str;
	for (auto &e : figs_ref_by) {
		join(ref_by_str, e.second);
		stmt_update_fig_ref_by.bind(1, ref_by_str);
		stmt_update_fig_ref_by.bind(2, e.first);
		stmt_update_fig_ref_by.exec();
		if (!stmt_update_fig_ref_by.getChanges())
			throw internal_err("数据库 figures 表格中未找到： " + e.first + SLS_WHERE);
		stmt_update_fig_ref_by.reset();
	}

	SQLite::Statement stmt_update_labels_ref_by(db_rw, R"(UPDATE "labels" SET "ref_by"=? WHERE "id"=?;)");
	for (auto &e : labels_ref_by) {
		join(ref_by_str, e.second);
		stmt_update_labels_ref_by.bind(1, ref_by_str);
		stmt_update_labels_ref_by.bind(2, e.first);
		stmt_update_labels_ref_by.exec();
		if (!stmt_update_labels_ref_by.getChanges())
			throw internal_err("数据库 labels 表格中未找到： " + e.first + SLS_WHERE);
		stmt_update_labels_ref_by.reset();
	}
	cout << "done!" << endl;

	// === regenerate "bibliography.ref_by" from "entries.bibs" =====
	set<Str> bibs;
	exec(R"(UPDATE "bibliography" SET "ref_by"='';)", db_rw);
	cout << R"(regenerate "bibliography.ref_by"  from "entries.bibs ...")" << endl;
	SQLite::Statement stmt_select_bibs(db_rw, R"(SELECT "id", "bibs" FROM "entries" WHERE "bibs" != '';)");
	while (stmt_select_bibs.executeStep()) {
		entry = (const char*)stmt_select_bibs.getColumn(0);
		parse(bibs, stmt_select_bibs.getColumn(1));
		for (auto &bib : bibs) {
			bib_ref_by[bib].insert(entry);
		}
	}

	SQLite::Statement stmt_update_bib_ref_by(db_rw, R"(UPDATE "bibliography" SET "ref_by"=? WHERE "id"=?;)");
	for (auto &e : bib_ref_by) {
		join(ref_by_str, e.second);
		stmt_update_bib_ref_by.bind(1, ref_by_str);
		stmt_update_bib_ref_by.bind(2, e.first); // bib_id
		stmt_update_bib_ref_by.exec();
		if (!stmt_update_bib_ref_by.getChanges())
			throw internal_err("数据库 figures 表格中未找到： " + e.first + SLS_WHERE);
		stmt_update_bib_ref_by.reset();
	}

	// === regenerate images.figures from figures.image ========
	unordered_map<Str, set<Str>> images_figures, images_figures_old; // images.figures* generated from figures.image
	SQLite::Statement stmt_select3(db_read, R"(SELECT "id", "image", "image_alt", "image_old" FROM "figures" WHERE "id" != '';)");
	SQLite::Statement stmt_select4(db_read, R"(SELECT "figures" FROM "images" WHERE "hash"=?;)");
	vecStr image_alt, image_old;
	while (stmt_select3.executeStep()) {
		const char *fig_id = stmt_select3.getColumn(0);
		const char *image_hash = stmt_select3.getColumn(1);
		parse(image_alt, stmt_select3.getColumn(2));
		if (size(image_alt) > 1)
			throw internal_err(u8"暂时不允许多于一个 image_alt！");
		parse(image_old, stmt_select3.getColumn(3));
		if (size(image_old) > 1)
			throw internal_err(u8"暂时不允许多于一个 image_old！");
		stmt_select4.bind(1, image_hash);
		if (!stmt_select4.executeStep()) {
			clear(sb) << u8"图片 " << fig_id << " 的 image " << image_hash << u8" 不存在！";
			throw internal_err(sb);
		}
		stmt_select4.reset();

		images_figures[image_hash].insert(fig_id);
		if (!image_alt.empty())
			images_figures[image_alt[0]].insert(fig_id);
		if (!image_old.empty())
			images_figures_old[image_old[0]].insert(fig_id);
	}

	vecStr db_images;
	get_column(db_images, "images", "hash", db_read);
	SQLite::Statement stmt_update4(db_rw, R"(UPDATE "images" SET "figures"=?, "figures_old"=? WHERE "hash"=?;)");
	SQLite::Statement stmt_delete4(db_rw, R"(DELETE FROM "images" WHERE "hash"=?;)");
	for (auto &image_hash : db_images) {
		if (!images_figures.count(image_hash) && !images_figures_old.count(image_hash)) {
			clear(sb) << u8"图片 " << image_hash << u8" 没有被 figures 表中任何环境引用（将删除）。";
			db_log(sb);
			stmt_delete4.bind(1, image_hash);
			stmt_delete4.exec(); stmt_delete4.reset();
		}
		join(sb, images_figures[image_hash]);
		stmt_update4.bind(1, sb);
		join(sb, images_figures_old[image_hash]);
		stmt_update4.bind(2, sb);
		stmt_update4.bind(3, image_hash);
		stmt_update4.exec(); stmt_update4.reset();
	}

	cout << "done!" << endl;
}

// upgrade `user-notes/*/cmd_data/scan.db` with the schema of `note-template/cmd_data/scan.db`
// data of `note-template/cmd_data/scan.db` has no effect
inline void migrate_user_db() {
	Str file_old, file;
	vecStr folders;
	folder_list_full(folders, "../user-notes/");
	for (auto &folder: folders) {
		if (!file_exist(folder + "main.tex") || folder == "../user-notes/note-template/")
			continue;
		file_old = folder + "cmd_data/scan-old.db";
		file = folder + "cmd_data/scan.db";
		if (!file_exist(file))
			SLS_ERR("file not found:" + file);
		cout << "migrating " << file << endl;
		file_move(file_old, file, true);
		file_copy(file, "../user-notes/note-template/cmd_data/scan.db", true);
		migrate_db(file, file_old);
		file_remove(file_old);
	}
}

// `git diff --no-index --word-diff-regex=. file1.tex file2.tex`
// then search for each `{+..+}` and `{-..-}`
inline void str_add_del(Long_O add, Long_O del, Str str_old, Str str_new)
{
	std::regex rm_s(u8"\\s");
	add = del = 0;

	// remove all space, tab, return etc
	str_old = std::regex_replace(str_old, rm_s, "");
	str_new = std::regex_replace(str_new, rm_s, "");
	SLS_ASSERT(utf8::is_valid(str_old));
	SLS_ASSERT(utf8::is_valid(str_new));

	if (str_old == str_new) {
		add = del = 0; return;
	}
	Str str;
	// check if git is installed (only once)
	static bool checked = false;
	if (!checked) {
		exec_str(str, "which git");
		trim(str, " \n");
		if (str.empty())
			throw std::runtime_error("git binary not found!" SLS_WHERE);
	}

	// replace "{+", "+}", "[-", "-]" to avoid conflict
	static const Str file1 = "file_add_del_tmp_file1.txt",
		file2 = "file_add_del_tmp_file2.txt",
		file_diff = "file_add_del_git_diff_output.txt";
	replace(str_old, "{+", u8"{➕"); replace(str_old, "+}", u8"➕}");
	replace(str_old, "[-", u8"[➖"); replace(str_old, "-]", u8"➖]");
	write(str_old, file1);

	replace(str_new, "{+", u8"{➕"); replace(str_new, "+}", u8"➕}");
	replace(str_new, "[-", u8"[➖"); replace(str_new, "-]", u8"➖]");
	write(str_new, file2);

	clear(sb) << "git diff --no-index --word-diff-regex=. "
		<< '"' << file1 << "\" \"" << file2 << '"';
	// git diff will return 0 iff there is no difference
	if (exec_str(str, sb) == 0)
		return;
#ifndef NDEBUG
	write(str, file_diff);
#endif
	if (str.empty()) return;
	// parse git output, find addition
	Long ind = -1;
	while (1) {
		ind = find(str, "{+", ind+1);
		if (ind < 0) break;
		ind += 2;
		Long ind1 = find(str, "+}", ind);
		if (ind1 < 0) {
			file_rm(file1); file_rm(file2);
#ifndef NDEBUG
			file_rm(file_diff);
#endif
			throw std::runtime_error("matching +} not found!" SLS_WHERE);
		}
		add += u8count(str, ind, ind1);
		ind = ind1 + 1;
	}
	// parse git output, find deletion
	ind = -1;
	while (1) {
		ind = find(str, "[-", ind+1);
		if (ind < 0) break;
		ind += 2;
		Long ind1 = find(str, "-]", ind);
		if (ind1 < 0) {
			file_rm(file1); file_rm(file2);
#ifndef NDEBUG
			file_rm(file_diff);
#endif
			throw std::runtime_error("matching -] not found!" SLS_WHERE);
		}
		del += u8count(str, ind, ind1);
		ind = ind1 + 1;
	}

	// check
	Long Nchar1 = u8count(str_old), Nchar2 = u8count(str_new);
	Long net_add = Nchar2 - Nchar1;
	if (net_add != add - del) {
		sb << "something wrong" << to_string(net_add) <<
			", add = " << to_string(add) << ", del = " << to_string(del) <<
			", add-del = " << to_string(add - del) << SLS_WHERE;
		throw std::runtime_error(sb);
	}

	file_rm(file1); file_rm(file2);
#ifndef NDEBUG
	file_rm(file_diff);
#endif
}
