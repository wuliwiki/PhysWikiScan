#pragma once
#include "sqlite_db.h"
#include "../SLISC/str/str_diff_patch.h"

// calculate author list of an entry, based on "entry_authors" table (already using aka)
inline Str db_get_author_list(Str_I entry, SQLite::Database &db_read)
{
	// 作者贡献 15 分钟以上才会显示
	SQLite::Statement stmt_select(db_read,
		R"(SELECT "author", "contrib" FROM "entry_authors" WHERE "entry"=?;)");
	SQLite::Statement stmt_select2(db_read, R"(SELECT "name" FROM "authors" WHERE "id"=?;)");
	SQLite::Statement stmt_select3(db_read, R"(SELECT 1 FROM "author_rights" WHERE "author"=? AND "right"='hide';)");

	// entry_authors.contrib (minutes), authors.id, authors.name
	vector<tuple<int64_t,int64_t,Str>> contrib_id_name; // only store non-hidden

	// get author info from "entry_authors" table
	stmt_select.bind(1, entry);
	while (stmt_select.executeStep()) {
		int64_t id = stmt_select.getColumn(0).getInt64();
		stmt_select3.bind(1, id);
		bool hide = stmt_select3.executeStep();
		stmt_select3.reset();
		if (hide) continue;

		stmt_select2.bind(1, (int64_t)id);
		if (!stmt_select2.executeStep())
			throw internal_err(u8"文章： " + entry + u8" 作者 id 不存在： " + num2str((Long)id));

		contrib_id_name.emplace_back(
			stmt_select.getColumn(1).getInt64(), // contrib
			id,
			stmt_select2.getColumn(0).getString() // name
		);
		stmt_select2.reset();
	}
	stmt_select.reset();

	// sort by contrib (descend), then by id (ascend)
	std::sort(contrib_id_name.begin(), contrib_id_name.end(),
		[](tuple<int64_t,int64_t,Str> &a, tuple<int64_t,int64_t,Str> &b){
		auto diff = get<0>(b) - get<0>(a);
		if (diff == 0)
			return get<1>(a) < get<1>(b);
		else
			return diff < 0;
	});

	Str authors_str;
	vecStr authors;
	for (auto &e : contrib_id_name) {
		if (get<0>(e) < 15) break;
		authors.push_back(get<2>(e));
	}
	if (authors.empty())
		return u8"待更新";
	join(authors_str, authors, "; ");
	return authors_str;
}

// if an author has "authors.aka", return aka; otherwise return the same `author_id`
inline Long real_author(Long_I author_id, SQLite::Database &db_read)
{
	SLS_ERR("this should not be used now, editor logs in the author as aka already!");
	SQLite::Statement stmt_select(db_read,
		R"(SELECT "aka" FROM "authors" WHERE "id"=?;)");
	stmt_select.bind(1, (int64_t)author_id);
	if (!stmt_select.executeStep())
		throw internal_err("real_author() " SLS_WHERE);
	Long aka = stmt_select.getColumn(0).getInt64();
	return (aka < 0 ? author_id : aka);
}

// update db table "entry_authors" for 1 entry, based on backup count in "history" and "contrib_adjust"
inline void db_update_authors1(Str_I entry, SQLite::Database &db_rw)
{
	//            author_id -> (minutes, last_backup)
	unordered_map<Long,    pair<Long,    Str>> author_minutes_last;
	author_minutes_last.clear();
	SQLite::Statement stmt_count(db_rw,
		R"(SELECT "author", COUNT(*) as record_count FROM "history" WHERE "entry"=? GROUP BY "author";)");
	SQLite::Statement stmt_select(db_rw,
		R"(SELECT "author", "minutes" FROM "contrib_adjust" WHERE "entry"=? AND "adjust_author_list"=1 AND "approved">=0;)");
	SQLite::Statement stmt_select2(db_rw,
		R"(SELECT "hash" FROM "history" WHERE "entry"=? AND "author"=? ORDER BY "time" DESC LIMIT 1;)");

	stmt_count.bind(1, entry);
	// loop through each author
	while (stmt_count.executeStep()) {
		Long author_id = stmt_count.getColumn(0).getInt64();
		// update time
		Long time = 5*stmt_count.getColumn(1).getInt64();
		if (author_minutes_last.count(author_id) != 0)
			SLS_ERR(SLS_WHERE);
		auto &minutes_last = author_minutes_last[author_id];
		minutes_last.first = time;
		// update last_backup hash
		stmt_select2.bind(1, entry);
		stmt_select2.bind(2, (int64_t)author_id);
		if (!stmt_select2.executeStep())
			throw internal_err(SLS_WHERE);
		minutes_last.second = stmt_select2.getColumn(0).getString();
		stmt_select2.reset();
	}
	stmt_count.reset();

	// consider "contrib_adjust" table
	stmt_select.bind(1, entry);
	while (stmt_select.executeStep()) {
		Long author_id = stmt_select.getColumn(0).getInt64();
		author_minutes_last[author_id].first += stmt_select.getColumn(1).getInt64();
	}
	stmt_select.reset();

	// update_sqlite_table()
	unordered_map<vecSQLval,vecSQLval> records;
	for (const auto &e : author_minutes_last) {
		vecSQLval key(2), val(2);
		key[0] = entry; key[1] = e.first;
		val[0] = e.second.first; val[1] = e.second.second;
		records[move(key)] = move(val);
	}
	Str condition; condition << "\"entry\"='" << entry << '\'';
	update_sqlite_table(records, "entry_authors", condition, {"entry", "author", "contrib", "last_backup"},
		2, db_rw, &sqlite_callback);
}

// update all authors
inline void db_update_authors(SQLite::Database &db)
{
	cout << "updating database for author lists...." << endl;
	SQLite::Statement stmt_select(db, R"(SELECT "id" FROM "entries" WHERE "id"!='' AND "deleted"=0;)");
	// TODO: why require "deleted"=0 ?
	Str entry;
	while (stmt_select.executeStep()) {
		entry = stmt_select.getColumn(0).getString();
		db_update_authors1(entry, db);
	}
	stmt_select.reset();
	cout << "done!" << endl;
}

// update db "authors" and "history" table from backup files
// assuming all backup files already named after "authors.aka" if applicable
inline void db_update_author_history(SQLite::Database &db_rw)
{
	vecStr fnames;
	Str sha1, time, entry;
	Str path = gv::path_in + (gv::is_wiki ? "../PhysWiki-backup/" : "backup/");
	file_list_ext(fnames, path, "tex", false);
	cout << "updating sqlite database \"history\" table (" << fnames.size()
		 << " backup) ..." << endl; cout.flush();

	// update "history" table
	SQLite::Statement stmt_select(db_rw,
		R"(SELECT "hash", "time", "author", "entry" FROM "history" WHERE "hash" <> '';)");

	//            hash        time author entry  file-exist
	unordered_map<Str,  tuple<Str, Long,  Str,   bool>> db_history;
	while (stmt_select.executeStep()) {
		Long author_id = stmt_select.getColumn(2).getInt64();
		db_history[stmt_select.getColumn(0)] =
				make_tuple(stmt_select.getColumn(1).getString(),
						   author_id , stmt_select.getColumn(3).getString(), false);
	}

	cout << "there are already " << db_history.size() << " backup (history) records in database." << endl;

	SQLite::Statement stmt_select2(db_rw, R"(SELECT "id", "name", "aka" FROM "authors")");
			//   autho_id -> (name, aka, contrib)
	unordered_map<Long,   tuple<Str, Long, Long>> author_info;
	while (stmt_select2.executeStep()) {
		Long author_id = stmt_select2.getColumn(0).getInt64();
		const Str &author_name = stmt_select2.getColumn(1);
		Long author_aka = stmt_select2.getColumn(2).getInt64();
		author_info[author_id] = make_tuple(author_name, aka, 0);
		stmt_select3.reset();
	}
	stmt_select2.reset();

	cout << "there are " << author_info.size() << " authors in database." << endl;

	SQLite::Statement stmt_insert(db_rw,
		R"(INSERT OR REPLACE INTO "history" ("hash", "time", "author", "entry") VALUES (?, ?, ?, ?);)");

	// get all entries from db
	vecStr entries0;
	get_column(entries0, "entries", "id", db_rw);
	unordered_set<Str> entries(entries0.begin(), entries0.end()), entries_deleted_inserted;
	entries0.clear(); entries0.shrink_to_fit();

	// insert a deleted entry (to ensure FOREIGN KEY exist)
	SQLite::Statement stmt_insert_entry(db_rw,
		R"(INSERT OR REPLACE INTO "entries" ("id", "deleted") VALUES (?, 1);)");

	SQLite::Statement stmt_select4(db_rw,
		R"(SELECT "hash" FROM "history" WHERE "time"=? AND "author"=? AND "entry"=?;)");
	SQLite::Statement stmt_update(db_rw, R"(UPDATE "history" SET "hash"=? WHERE "hash"=?;)");
	SQLite::Statement stmt_delete(db_rw, R"(DELETE FROM "history" WHERE "hash"=?;)");
	SQLite::Statement stmt_update2(db_rw, R"(UPDATE "authors" SET "contrib"=? WHERE "id"=?;)");

	Str fpath;

	for (Long i = 0; i < size(fnames); ++i) {
		auto &fname = fnames[i];
		fpath = path + fname + ".tex";
		read(sb, fpath); CRLF_to_LF(sb);
		sha1 = sha1sum(sb).substr(0, 16);
		bool sha1_exist = db_history.count(sha1);

		// fname = YYYYMMDDHHMM_authorID_entry
		time = fname.substr(0, 12);
		Long ind = (Long)fname.rfind('_');
		entry = fname.substr(ind+1);
		Long authorID;
		str2int(authorID, fname.substr(13, ind-13));
		if (get<1>(author_info[authorID]) >= 0) { // check aka
			scan_log_warn("检测到作者小号命名的备份文件（将重命名）");
			// fname_new = "../PhysWiki-backup/" + time2;
			// 		fname_new << '_' << authorID << '_' << entry << ".tex";
			// 		cout << "mv " << fname << ' ' << fname_new << endl;
			// 		file_move(fname_new, fname);
		}
		get<2>(author_info[authorID]) += 5;
		if (entries.count(entry) == 0 && entries_deleted_inserted.count(entry) == 0) {
			scan_log_warn(u8"内部警告：备份文件中的文章不在数据库中（将模拟编辑器添加）： " + entry);
			stmt_insert_entry.bind(1, entry);
			stmt_insert_entry.exec(); stmt_insert_entry.reset();
			entries_deleted_inserted.insert(entry);
		}

		if (sha1_exist) {
			auto &time_author_entry_fexist = db_history[sha1];
			if (get<0>(time_author_entry_fexist) != time) {
				clear(sb) << u8"备份 " + fname + u8" 信息与数据库中的时间不同， 数据库中为（将不更新）： " +
							 get<0>(time_author_entry_fexist);
				db_log_print(sb);
			}
			if (get<1>(time_author_entry_fexist) != authorID) {
				clear(sb) << u8"备份 " << fname << u8" 信息与数据库中的作者不同， 数据库中为（将不更新）： "
					<< to_string(get<1>(time_author_entry_fexist)) << '.'
					<< get<0>(author_info[get<1>(time_author_entry_fexist)]);
				db_log_print(sb);
			}
			if (get<2>(time_author_entry_fexist) != entry) {
				clear(sb) << u8"备份 " << fname << u8" 信息与数据库中的文件名不同， 数据库中为（将不更新）： "
					<< get<2>(time_author_entry_fexist);
				db_log_print(sb);
			}
			get<3>(time_author_entry_fexist) = true;
		}
		else { // sha1_exist == false
			stmt_select4.bind(1, time);
			stmt_select4.bind(2, (int64_t)authorID);
			stmt_select4.bind(3, entry);
			if (stmt_select4.executeStep()) {
				const Str &db_hash = stmt_select4.getColumn(0);
				clear(sb) << u8"检测到数据库的 history 表格的 hash 改变（将更新）："
								 << db_hash << " -> " << sha1 << ' ' << fname;
				db_log_print(sb);
				stmt_update.bind(1, sha1);
				stmt_update.bind(2, db_hash);
				if (stmt_update.exec() != 1)
					throw internal_err(SLS_WHERE);
				stmt_update.reset();
			}
			else {
				clear(sb) << u8"数据库的 history 表格中不存在备份文件（将添加）：" << sha1 << " " << fname;
				db_log_print(sb);
				stmt_insert.bind(1, sha1);
				stmt_insert.bind(2, time);
				stmt_insert.bind(3, (int64_t)authorID);
				stmt_insert.bind(4, entry);
				try { stmt_insert.exec(); }
				catch (std::exception &e) {
					throw internal_err(Str(e.what()) + SLS_WHERE);
				}
				stmt_insert.reset();
				db_history[sha1] = make_tuple(time, authorID, entry, true);
			}
			stmt_select4.reset();
		}
	}

	for (auto &row : db_history) {
		if (!get<3>(row.second)) {
			clear(sb) << u8"数据库 history 表中文件不存在（将删除）：" << row.first << ", " << get<0>(row.second) << ", " <<
				 get<1>(row.second) << ", " << get<2>(row.second);
			db_log_print(sb);
			stmt_delete.bind(1, row.first); stmt_delete.exec(); stmt_delete.reset();
		}
	}
	cout << "\ndone." << endl;
	cout << "\nupdating author contribution..." << endl;

	// update all authors.contrib
	for (auto &e : author_info) {
		stmt_update2.bind(1, (int64_t)get<2>(e.second)); // authors.contrib
		stmt_update2.bind(2, (int64_t)e.first); // authors.id
		if (stmt_update2.exec() != 1)
			throw internal_err(SLS_WHERE);
		stmt_update2.reset();
	}

	cout << "done." << endl;
}

// update all "history.last"
inline void db_update_history_last(SQLite::Database &db_rw)
{
	cout << "updating history.last..." << endl;
	SQLite::Statement stmt_select(db_rw,
		R"(SELECT "hash", "time", "entry", "last" FROM "history" WHERE "hash" <> '';)");
	unordered_map<Str, map<Str, pair<Str,Str>>> entry2time2hash_last; // entry -> (time -> (hash,last))
	while (stmt_select.executeStep()) {
		entry2time2hash_last[stmt_select.getColumn(2)]
		[stmt_select.getColumn(1).getString()] =
				pair<Str,Str>(stmt_select.getColumn(0), stmt_select.getColumn(3));
	}

	// update db
	SQLite::Statement stmt_update(db_rw,
		R"(UPDATE "history" SET "last"=? WHERE "hash"=?;)");
	SQLite::Statement stmt_select2(db_rw,
		R"(SELECT "last_backup" FROM "entries" WHERE "id"=?;)");
	SQLite::Statement stmt_update2(db_rw,
		R"(UPDATE "entries" SET "last_backup"=? WHERE "id"=?;)");
	Str last_hash;
	for (auto &e : entry2time2hash_last) {
		auto &entry = e.first;
		last_hash.clear();
		for (auto &time_hash_last : e.second) {
			auto &hash = time_hash_last.second.first;
			auto &db_last_hash = time_hash_last.second.second;
			if (last_hash != db_last_hash) {
				clear(sb) << u8"内部警告：检测到 history.last 改变，将模拟编辑器更新："
					<< db_last_hash << " -> " << last_hash;
				scan_log_warn(sb);
				stmt_update.bind(1, last_hash);
				stmt_update.bind(2, hash);
				if (stmt_update.exec() != 1) throw internal_err(SLS_WHERE);
				stmt_update.reset();
			}
			last_hash = hash;
		}
		// update entries.last_backup
		stmt_select2.bind(1, entry);
		if (!stmt_select2.executeStep())
			throw internal_err(SLS_WHERE);
		const Str &db_last_backup = stmt_select2.getColumn(0);
		stmt_select2.reset();
		const Str &last_backup = ((--e.second.end())->second).first;
		if (last_backup != db_last_backup) {
			clear(sb) << u8"内部警告：检测到 entry.last_backup 改变，将模拟编辑器更新："
				<< db_last_backup << " -> " << last_backup;
			scan_log_warn(sb);
			stmt_update2.bind(1, last_backup);
			stmt_update2.bind(2, entry);
			if (stmt_update2.exec() != 1) throw internal_err(SLS_WHERE);
			stmt_update2.reset();
		}
	}
	cout << "done." << endl;
}

// get all history.hash for an entry, by tracing entries.last_backup and history.last
// order is from new to old
inline void db_get_history(vecStr_O history_hash, Str_I entry, SQLite::Database &db_read)
{
	history_hash.clear();
	SQLite::Statement stmt_select(db_read,
		R"(SELECT "last_backup" FROM "entries" WHERE "id"=?;)");
	SQLite::Statement stmt_select1(db_read,
		R"(SELECT "last" FROM "history" WHERE "hash"=?;)");
	stmt_select.bind(1, entry);
	if (!stmt_select.executeStep())
		throw internal_err(u8"db_get_history()： 文章不存在：" + entry + SLS_WHERE);
	history_hash.push_back(stmt_select.getColumn(0));
	stmt_select.reset();
	while (!history_hash.back().empty()) {
		stmt_select1.bind(1, history_hash.back());
		if (!stmt_select1.executeStep())
			throw internal_err(u8"db_get_history()： history.hash 不存在：" + history_hash.back() + SLS_WHERE);
		history_hash.push_back(stmt_select1.getColumn(0));
		stmt_select1.reset();
	}
	history_hash.pop_back();
}

// calculate all "history.add" and "history.del"
// `redo_all = false` will only update "history.add/del=-1;" case
inline void history_add_del_all(SQLite::Database &db_rw, bool redo_all = false) {
	cout << "calculating history.add/del..." << endl;
	SQLite::Statement stmt_select(db_rw, R"(SELECT "id" FROM "entries";)");
	vecStr entries;
	while (stmt_select.executeStep())
		entries.push_back(stmt_select.getColumn(0));

	SQLite::Statement stmt_select2(db_rw,
		R"(SELECT "hash", "time", "author", "add", "del" FROM "history"
			WHERE "entry"=? ORDER BY "time";)");
	Str fname_old, fname, str, str_old;
	unordered_map<Str, pair<Long, Long>> hist_add_del; // backup hash -> (add, del)
	for (auto &entry : entries) {
		fname_old.clear(); fname.clear();
		stmt_select2.bind(1, entry);
		bool fname_exist = true;
		while (stmt_select2.executeStep()) {
			if (fname_exist) fname_old = fname;
			fname = "../PhysWiki-backup/";
			fname << stmt_select2.getColumn(1).getString() << '_';
			fname << to_string(stmt_select2.getColumn(2).getInt64()) << '_';
			fname << entry << ".tex";
			if (!file_exist(fname)) {
				fname_exist = false; continue;
			}
			fname_exist = true;
			const Str &hash = stmt_select2.getColumn(0);
			read(str, fname); CRLF_to_LF(str);
			if (fname_old.empty()) // first backup
				hist_add_del[hash] = make_pair(u8count(str), 0);
			else {
				Long db_add = stmt_select2.getColumn(3).getInt64();
				Long db_del = stmt_select2.getColumn(4).getInt64();
				if (!redo_all && db_add != -1 && db_del != -1)
					continue;
				auto &e = hist_add_del[hash];
				Long &add = e.first, &del = e.second;
				cout << fname << endl;
				read(str_old, fname_old);
				// compare str and str_old
				str_add_del(add, del, str_old, str);
//				static vector<tuple<size_t, size_t, Str>> diff;
//				str_diff(diff, str_old, str, true);
			}
		}
		stmt_select2.reset();
	}

	// update db "history.add/del"
	cout << "\n\nupdating db history.add/del..." << endl;
	SQLite::Statement stmt_update(db_rw,
		R"(UPDATE "history" SET "add"=?, "del"=? WHERE "hash"=?;)");
	for (auto &e : hist_add_del) {
		auto &hash = e.first;
		auto &add_del = e.second;
		stmt_update.bind(1, (int64_t)add_del.first);
		stmt_update.bind(2, (int64_t)add_del.second);
		stmt_update.bind(3, hash);
		if (stmt_update.exec() != 1) throw internal_err(SLS_WHERE);
		stmt_update.reset();
	}
	cout << "committing transaction..." << endl;
	cout << "done." << endl;
}

// simulate 5min backup rule, by renaming backup files
inline void history_normalize(SQLite::Database &db_rw)
{
	SQLite::Statement stmt_select(db_rw,
		R"(SELECT "entry", "author", "time", "hash" FROM "history" WHERE "hash" <> '';)");
	//            entry                author     time         hash  time2 (new time, or "" for nothing, "d" to delete)
	unordered_map<Str,   unordered_map<Str,    map<Str,   pair<Str,  Str>>>> entry_author_time_hash_time2;
	while (stmt_select.executeStep()) {
		entry_author_time_hash_time2[stmt_select.getColumn(0)]
		[stmt_select.getColumn(1)] [stmt_select.getColumn(2)].first
				= stmt_select.getColumn(3).getString();
	}

	time_t t1, t2, t;
	Str *time2_last;
	for (auto &e5 : entry_author_time_hash_time2) {
		t1 = t2 = 0;
		for (auto &e4 : e5.second) {
			t1 = t2 = 0;
			time2_last = nullptr;
			for (auto &time_hash_time2 : e4.second) {
				// debug
				// if (time_hash_time2.first == "202303231039")
				t = str2time_t(time_hash_time2.first);
				if (t <= t1) // t1 is a resume of a session
					*time2_last = "d";
				else if (!t1) // t is 1st backup in a session
					t1 = t;
				else if (!t2) { // t is 2nd backup
					here:
					if (t - t1 < 300)
						t2 = t;
					else if (t - t1 > 300) {
						if (t - t1 < 1800) {
							if ((t - t1) % 300) {
								t1 += ((t - t1) / 300 + 1) * 300;
								time_hash_time2.second.second = time_t2str(t1, "%Y%m%d%H%M");
							}
						}
						else // end of 30min session
							t1 = t;
					}
					else // t - t1 == 300
						t1 = t;
				}
				else { // t2 > 0, t is 3rd backup
					if (t - t1 < 300) {
						t2 = t;
						*time2_last = "d";
					}
					else if (t - t1 > 300) {
						t1 += 300;
						*time2_last = time_t2str(t1, "%Y%m%d%H%M");
						t2 = 0;
						goto here; // I know, but this is actually cleaner
					}
					else { // t - t1 == 300
						t1 = t; t2 = 0;
						*time2_last = "d";
					}
				}
				time2_last = &time_hash_time2.second.second;
			}
			if (t2 > 0)
				*time2_last = time_t2str(t1+300, "%Y%m%d%H%M");
		}
	}

	// remove or rename files, and update db
	SQLite::Statement stmt_select2(db_rw,
		R"(SELECT "author", "entry" FROM "history" WHERE "hash"=?;)");
	SQLite::Statement stmt_update(db_rw,
		R"(UPDATE "history" SET "time"=? WHERE "hash"=?;)");
	SQLite::Statement stmt_delete(db_rw,
		R"(DELETE FROM "history" WHERE "hash"=?;)");
	Str fname, fname_new;
	for (auto &e5 : entry_author_time_hash_time2) {
		for (auto &e4: e5.second) {
			for (auto &time_hash_time2: e4.second) {
				auto &time2 = time_hash_time2.second.second;
				if (time2.empty())
					continue;
				auto &time = time_hash_time2.first;
				auto &hash = time_hash_time2.second.first;

				// rename or rename backup file, update db
				stmt_select2.bind(1, hash);
				if (!stmt_select2.executeStep())
					throw internal_err(SLS_WHERE);
				Long authorID = stmt_select2.getColumn(0).getInt64();
				const Str &entry = stmt_select2.getColumn(1).getString();
				stmt_select2.reset();
				fname = "../PhysWiki-backup/" + time;
				fname << '_' << authorID << '_' << entry << ".tex";
				if (time2 == "d") {
					cout << "rm " << fname << endl;
					file_remove(fname);
					// db
					stmt_delete.bind(1, hash);
					if (stmt_delete.exec() != 1) throw internal_err(SLS_WHERE);
					stmt_delete.reset();
				}
				else {
					fname_new = "../PhysWiki-backup/" + time2;
					fname_new << '_' << authorID << '_' << entry << ".tex";
					cout << "mv " << fname << ' ' << fname_new << endl;
					file_move(fname_new, fname);
					// db
					stmt_update.bind(1, time2);
					stmt_update.bind(2, hash);
					if (stmt_update.exec() != 1) throw internal_err(SLS_WHERE);
					stmt_update.reset();
				}
			}
		}
	}
	db_update_history_last(db_rw);
}

// backup an entry to PhysWiki-backup
// use author.aka if available
inline void arg_backup(Str_I entry, Long_I author_id, SQLite::Database &db_rw)
{
	SQLite::Statement stmt_select(db_rw, R"(SELECT "last_backup" FROM "entries" WHERE "id"=?;)");
	SQLite::Statement stmt_select2(db_rw,
		R"(SELECT "time", "author", "entry", "last" FROM "history" WHERE "hash"=?;)");
	SQLite::Statement stmt_update2(db_rw, R"(UPDATE "entries" SET "last_backup"=? WHERE "id"=?;)");
	SQLite::Statement stmt_update3(db_rw,
		R"(UPDATE "entry_authors" SET "contrib"="contrib"+5, "last_backup"=? WHERE "entry"=? AND "author"=?;)");
	SQLite::Statement stmt_update4(db_rw,
		R"(UPDATE "authors" SET "contrib"="contrib"+5 WHERE "id"=?;)");
	SQLite::Statement stmt_insert(db_rw,
	   R"(INSERT INTO "history" ("hash", "time", "author", "entry", "add", "del", "last")
VALUES (?, ?, ?, ?, ?, ?, ?);)");
	SQLite::Statement stmt_insert5(db_rw,
		R"(INSERT OR REPLACE INTO "entry_authors" ("entry", "author", "contrib", "last_backup") VALUES (?, ?, 5, ?);)");

	Str str; // content of entry
	Str time_new_str;
	Str backup_path = gv::path_in + (gv::is_wiki ? "../PhysWiki-backup/" : "backup/");
	ensure_dir(backup_path);

	// get hash, check existence
	clear(sb) << gv::path_in;
	if (!(entry == "main" || entry == "bibliography"))
		sb << "contents/";
	sb << entry << ".tex";
	read(str, sb);
	if (str.empty())
		db_log_print(u8"--backup 忽略空文件：" + entry);
	CRLF_to_LF(str);
	Str hash = sha1sum(str).substr(0, 16);

	// check if hash exist
	stmt_select2.bind(1, hash);
	if (stmt_select2.executeStep()) {
		// hash already exists
		const Str &db_time_str = stmt_select2.getColumn(0);
		Long db_author_id = stmt_select2.getColumn(1).getInt64();
		const Str &db_entry = stmt_select2.getColumn(2);
		stmt_select2.reset();
		clear(sb) << "要备份的文件 hash 已经存在（将忽略）： "
			<< db_time_str << '_' << db_author_id << '_' << db_entry << ".tex"
			<< " hash=" << hash;
		SLS_WARN(sb);
		if (db_entry != entry)
			throw scan_err(u8"已存在的备份文件属于另一篇文章（这不可能，因为标题 entries.caption 禁止重复，不同文章的内容不可能一样）");
		return;
	}
	stmt_select2.reset();

	// get latest backup from `entries.last_backup`
	stmt_select.bind(1, entry);
	if (!stmt_select.executeStep())
		throw internal_err(u8"arg_backup(): 找不到要备份的文章：" + entry);
	const Str &hash_last = stmt_select.getColumn(0); // entries.last_backup

	// check if is the first backup
	if (hash_last.empty()) {
		db_log_print(u8"entries.last_backup 为空， 当前为第一次备份。");

		time_new_str = time_str("%Y%m%d%H%M");
		stmt_insert.bind(1, hash);
		stmt_insert.bind(2, time_new_str);
		stmt_insert.bind(3, (int64_t)author_id);
		stmt_insert.bind(4, entry);
		stmt_insert.bind(5, int64_t(u8count(str)));
		stmt_insert.bind(6, 0);
		stmt_insert.bind(7, "");
		stmt_insert.exec(); stmt_insert.reset();

		stmt_update2.bind(1, hash);
		stmt_update2.bind(2, entry);
		if (stmt_update2.exec() != 1) throw internal_err(SLS_WHERE);
		stmt_update2.reset();
		clear(sb) << backup_path
			<< time_new_str << '_' << author_id << '_' << entry << ".tex";
		if (file_exist(sb))
			scan_log_warn(u8"第一次备份的文件已存在（将覆盖）：" + sb);
		write(str, sb);

		// insert into "entry_authors", contrib 5min
		SQLite::Statement stmt_insert3(db_rw,
			R"(INSERT OR REPLACE INTO "entry_authors" ("entry", "author", "contrib", "last_backup") VALUES (?,?,5,?);)");
		stmt_insert3.bind(1, entry);
		stmt_insert3.bind(2, (int64_t)author_id);
		stmt_insert3.bind(3, hash); // last_backup
		stmt_insert3.exec(); stmt_insert3.reset();

		// update "authors.contrib", add 5min
		stmt_update4.bind(1, (int64_t)author_id);
		stmt_update4.exec(); stmt_update4.reset();
		return;
	}
	else if (size(hash_last) != 16) {
		clear(sb) << u8"备份文章的 entries.last_backup 长度不对：" << entry << '.' << hash_last;
		throw internal_err(sb);
	}

	stmt_select2.bind(1, hash_last);
	if (!stmt_select2.executeStep()) {
		clear(sb) << u8"entries.last_backup 在 history 中未找到： " << entry << '.' << hash_last;
		throw internal_err(sb);
	}
	Str time_last_str = stmt_select2.getColumn(0);
	std::time_t time_last = str2time_t(time_last_str);
	Long author_id_last = stmt_select2.getColumn(1).getInt64();
	if (stmt_select2.getColumn(2).getString() != entry)
		throw scan_err(SLS_WHERE);
	const Str &hash_last_last = stmt_select2.getColumn(3);
	stmt_select2.reset();

	// calculate `time_new_str` in current backup file name
	bool replace = false; // replace the last backup
	time_t time = std::time(nullptr);
	time_t time_new = time;
	if (author_id == author_id_last) {
		if (time <= time_last) {
			replace = true;
			time_new = time_last;
		}
		else if (time < time_last + 30*60) {
			if ((time-time_last) % 300 != 0)
				time_new = time_last + ((time-time_last)/300+1)*300;
		}
	}
	else { // author_id != author_id_last
		if (time <= time_last)
			time_new = time_last + 1;
	}

	Str str2;
	Long char_add, char_del;

	if (replace) { // replace last (latest) backup
		// calculate char add/del
		stmt_select2.bind(1, hash_last_last);
		if (!stmt_select2.executeStep())
			throw internal_err(SLS_WHERE);
		const Str &time_last_last_str = stmt_select2.getColumn(0);
		Long author_id_last_last = stmt_select2.getColumn(1).getInt64();
		stmt_select2.reset();
		clear(sb) << backup_path
			<< time_last_last_str << '_' << author_id_last_last << '_' << entry << ".tex";
		read(str2, sb);
		str_add_del(char_add, char_del, str2, str);

		// update db
		SQLite::Statement stmt_update(db_rw,
			R"(UPDATE "history" SET "hash"=?, "add"=?, "del"=? WHERE "hash"=?;)");
		stmt_update.bind(1, hash);
		stmt_update.bind(2, (int64_t)char_add);
		stmt_update.bind(3, (int64_t)char_del);
		stmt_update.bind(4, hash_last);
		if (stmt_update.exec() != 1) throw internal_err(SLS_WHERE);
		stmt_update.reset();
		clear(sb) << u8"更新 history： " << hash_last << " -> " << hash << ", add = " <<
			char_add << ", del = " << char_del;
		db_log_print(sb);

		// replace last (latest) backup
		clear(sb) << backup_path
			<< time_last_str << '_' << author_id_last << '_' << entry << ".tex";
		if (!file_exist(sb))
			SLS_WARN(u8"数据库中备份文件不存在：" + sb);
		write(str, sb);

		// update "entry_authors.last_backup"
		SQLite::Statement stmt_update5(db_rw,
			R"(UPDATE "entry_authors" SET "last_backup"=? WHERE "entry"=? AND "author"=?;)");
		stmt_update5.bind(1, hash); // last_backup
		stmt_update5.bind(2, entry);
		stmt_update5.bind(3, (int64_t)author_id);
		Long changed = stmt_update5.exec();
		if (changed != 1) {
			if (changed != 0) throw scan_err(SLS_WHERE);
			stmt_insert5.bind(1, entry); stmt_insert5.bind(2, (int64_t)author_id);
			stmt_insert5.bind(3, hash);
			stmt_insert5.exec(); stmt_insert5.reset();
		}
		stmt_update5.reset();
		db_log_print(u8"更新 entry_authors.last_backup");
	}
	else { // !replace  (new backup)
		// calculate char add/del
		clear(sb) << backup_path
			<< time_last_str << '_' << author_id_last << '_' << entry << ".tex";
		read(str2, sb);
		str_add_del(char_add, char_del, str2, str);

		// update db
		time_new_str = time_t2str(time_new, "%Y%m%d%H%M");
		stmt_insert.bind(1, hash);
		stmt_insert.bind(2, time_new_str);
		stmt_insert.bind(3, (int64_t)author_id);
		stmt_insert.bind(4, entry);
		stmt_insert.bind(5, (int64_t)char_add);
		stmt_insert.bind(6, (int64_t)char_del);
		stmt_insert.bind(7, hash_last);
		stmt_insert.exec(); stmt_insert.reset();

		clear(sb) << u8"插入新的 history 记录： hash=" << hash << ", time=" << time_new_str << ", author=" << author_id
			<< ", entry=" << entry << ", add=" << char_add << ", del=" << char_del << ", last=" << hash_last;
		db_log_print(sb);

		// write new file
		clear(sb) << backup_path
			<< time_new_str << '_' << author_id << '_' << entry << ".tex";
		if (file_exist(sb))
			SLS_WARN(u8"要备份的文件已经存在（将覆盖）：" + sb);
		write(str, sb);

		// update "entry_authors", add 5min
		stmt_update3.bind(1, hash); // last_backup
		stmt_update3.bind(2, entry);
		stmt_update3.bind(3, (int64_t)author_id);
		Long changed = stmt_update3.exec();
		if (changed != 1) {
			if (changed != 0) throw scan_err(SLS_WHERE);
			stmt_insert5.bind(1, entry); stmt_insert5.bind(2, (int64_t)author_id);
			stmt_insert5.bind(3, hash);
			stmt_insert5.exec(); stmt_insert5.reset();
		}
		stmt_update3.reset();
		db_log_print(u8"更新 entry_authors.contrib += 5 和 entry_authors.last_backup");

		// update "authors.contrib", add 5min
		stmt_update4.bind(1, (int64_t)author_id);
		if (stmt_update4.exec() != 1) throw internal_err(SLS_WHERE);
		stmt_update4.reset();
		db_log_print(u8"更新 authors.contrib += 5");
	}

	stmt_update2.bind(1, hash);
	stmt_update2.bind(2, entry);
	if (stmt_update2.exec() != 1) throw internal_err(SLS_WHERE);
	stmt_update2.reset();

	clear(sb) << u8"更新 entries.last_backup： " << entry << '.' << hash;
	db_log_print(sb);
}

// --history
// update db "history" table from backup files
inline void arg_history(SQLite::Database &db_rw)
{
		db_update_author_history(db_rw);
		db_update_authors(db_rw);
}

