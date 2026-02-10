#pragma once
#include "sqlite_db.h"
#include "backup_db.h"
#include "../SLISC/str/str_diff_patch2.h"

inline Str backup_db_path()
{
	return gv::path_data + "PhysWiki-backup.db";
}

inline void backup_db_require(Str_I path)
{
	if (!file_exist(path))
		throw internal_err(u8"备份数据库不存在：" + path);
}

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

// update db table "entry_authors", based on backup count in "history" and "contrib_adjust"
// will destroy `author_minutes`
inline void db_update_authors1(unordered_map<Long, Long> &author_minutes, Str_I entry, SQLite::Database &db_rw)
{
	author_minutes.clear();
	SQLite::Statement stmt_count(db_rw,
		R"(SELECT "author", COUNT(*) as record_count FROM "history" WHERE "entry"=? GROUP BY "author";)");
	SQLite::Statement stmt_select(db_rw,
		R"(SELECT "author", "minutes" FROM "contrib_adjust" WHERE "entry"=? AND "adjust_author_list"=1 AND "approved">=0;)");

	stmt_count.bind(1, entry);
	while (stmt_count.executeStep()) {
		Long id = stmt_count.getColumn(0).getInt64();
		Long time = 5*stmt_count.getColumn(1).getInt64();
		author_minutes[id] += time;
	}
	stmt_count.reset();

	// consider "contrib_adjust" table
	stmt_select.bind(1, entry);
	while (stmt_select.executeStep()) {
		author_minutes[stmt_select.getColumn(0).getInt64()]
			+= stmt_select.getColumn(1).getInt64();
	}
	stmt_select.reset();

	// update_sqlite_table()
	unordered_map<vecSQLval,vecSQLval> records;
	for (auto &e : author_minutes) {
		vecSQLval key(2), val(1);
		key[0] = entry; key[1] = e.first; val[0] = e.second;
		records[move(key)] = move(val);
	}
	Str condition; condition << "\"entry\"='" << entry << '\'';
	update_sqlite_table(records, "entry_authors", condition, {"entry", "author", "contrib"},
		2, db_rw, &sqlite_callback);
}

// update all authors
inline void db_update_authors(SQLite::Database &db)
{
	cout << "updating database for author lists...." << endl;
	SQLite::Statement stmt_select(db,
		R"(SELECT "id" FROM "entries" WHERE "id"!='' AND "deleted"=0;)");
	Str entry;
	unordered_map<Long, Long> dummy;
	while (stmt_select.executeStep()) {
		entry = stmt_select.getColumn(0).getString();
		db_update_authors1(dummy, entry, db);
	}
	stmt_select.reset();
	cout << "done!" << endl;
}

// update db "authors" and "history" table from backup database
inline void db_update_author_history(SQLite::Database &db_rw)
{
	unordered_map<Long, Str> new_authors;
	unordered_map<Long, Long> author_contrib;
	Str sha1, time, entry;
	Str path = backup_db_path();
	backup_db_require(path);
	SQLite::Database db_backup(path, SQLite::OPEN_READONLY);
	db_backup.exec("PRAGMA busy_timeout = 3000;");
	cout << "updating sqlite database \"history\" table from backup database..." << endl;

	// update "history" table
	SQLite::Statement stmt_select(db_rw,
		R"(SELECT "hash", "time", "author", "entry" FROM "history" WHERE "hash" <> '';)");

	//            hash        time author entry  record-exist
	unordered_map<Str,  tuple<Str, Long,  Str,   bool>> db_history;
	while (stmt_select.executeStep()) {
		Long author_id = stmt_select.getColumn(2).getInt64();
		db_history[stmt_select.getColumn(0)] =
				make_tuple(stmt_select.getColumn(1).getString(),
						   author_id , stmt_select.getColumn(3).getString(), false);
	}
	stmt_select.reset();

	cout << "there are already " << db_history.size() << " backup (history) records in database." << endl;

	vecLong db_author_ids0;
	vecStr db_author_names0;
	SQLite::Statement stmt_select2(db_rw, R"(SELECT "id", "name" FROM "authors")");
	while (stmt_select2.executeStep()) {
		db_author_ids0.push_back(stmt_select2.getColumn(0).getInt64());
		db_author_names0.push_back(stmt_select2.getColumn(1));
	}
	stmt_select2.reset();

	cout << "there are already " << db_author_ids0.size() << " author records in database." << endl;
	unordered_map<Long, Str> db_id_to_author;
	for (Long i = 0; i < size(db_author_ids0); ++i)
		db_id_to_author[db_author_ids0[i]] = db_author_names0[i];

	db_author_ids0.clear(); db_author_names0.clear();
	db_author_ids0.shrink_to_fit(); db_author_names0.shrink_to_fit();

	SQLite::Statement stmt_insert(db_rw,
		R"(INSERT OR REPLACE INTO "history" ("hash", "time", "author", "entry") VALUES (?, ?, ?, ?);)");

	vecStr entries0;
	get_column(entries0, "entries", "id", db_rw);
	unordered_set<Str> entries(entries0.begin(), entries0.end()), entries_deleted_inserted;
	entries0.clear(); entries0.shrink_to_fit();

	// insert a deleted entry (to ensure FOREIGN KEY exist)
	SQLite::Statement stmt_insert_entry(db_rw,
		R"(INSERT OR REPLACE INTO "entries" ("id", "deleted") VALUES (?, 1);)");

	// insert new_authors to "authors" table
	SQLite::Statement stmt_insert_auth(db_rw,
		R"(INSERT OR REPLACE INTO "authors" ("id", "name") VALUES (?, ?);)");
	SQLite::Statement stmt_select4(db_rw,
		R"(SELECT "hash" FROM "history" WHERE "time"=? AND "author"=? AND "entry"=?;)");
	SQLite::Statement stmt_update(db_rw, R"(UPDATE "history" SET "hash"=? WHERE "hash"=?;)");
	SQLite::Statement stmt_delete(db_rw, R"(DELETE FROM "history" WHERE "hash"=?;)");
	SQLite::Statement stmt_backup(db_backup,
		R"(SELECT "time", "author", "entry", "hash" FROM "backup_files";)");

	while (stmt_backup.executeStep()) {
		time = stmt_backup.getColumn(0).getString();
		Long authorID = stmt_backup.getColumn(1).getInt64();
		entry = stmt_backup.getColumn(2).getString();
		sha1 = stmt_backup.getColumn(3).getString();
		bool sha1_exist = db_history.count(sha1);

		if (!db_id_to_author.count(authorID)) {
			Str name = to_string(authorID);
			clear(sb) << u8"备份数据库中的作者不在数据库中（将添加）： ID: " << authorID;
			db_log_print(sb);
			new_authors[authorID] = name;
			stmt_insert_auth.bind(1, (int64_t)authorID);
			stmt_insert_auth.bind(2, name);
			stmt_insert_auth.exec();
			stmt_insert_auth.reset();
			db_id_to_author[authorID] = name;
		}

		author_contrib[authorID] += 5;
		if (entries.count(entry) == 0 &&
			entries_deleted_inserted.count(entry) == 0) {
			scan_log_warn(u8"内部警告：备份数据库中的文章不在数据库中（将模拟编辑器添加）： " + entry);
			stmt_insert_entry.bind(1, entry);
			stmt_insert_entry.exec(); stmt_insert_entry.reset();
			entries_deleted_inserted.insert(entry);
		}

		if (sha1_exist) {
			auto &time_author_entry_fexist = db_history[sha1];
			if (get<0>(time_author_entry_fexist) != time) {
				clear(sb) << u8"备份记录信息与数据库中的时间不同， 数据库中为（将不更新）： " +
							 get<0>(time_author_entry_fexist);
				db_log_print(sb);
			}
			if (get<1>(time_author_entry_fexist) != authorID) {
				clear(sb) << u8"备份记录信息与数据库中的作者不同， 数据库中为（将不更新）： "
					<< to_string(get<1>(time_author_entry_fexist)) << '.'
					<< db_id_to_author[get<1>(time_author_entry_fexist)];
				db_log_print(sb);
			}
			if (get<2>(time_author_entry_fexist) != entry) {
				clear(sb) << u8"备份记录信息与数据库中的文件名不同， 数据库中为（将不更新）： "
					<< get<2>(time_author_entry_fexist);
				db_log_print(sb);
			}
			get<3>(time_author_entry_fexist) = true;
		}
		else {
			stmt_select4.bind(1, time);
			stmt_select4.bind(2, (int64_t)authorID);
			stmt_select4.bind(3, entry);
			if (stmt_select4.executeStep()) {
				const Str &db_hash = stmt_select4.getColumn(0);
				clear(sb) << u8"检测到数据库的 history 表格的 hash 改变（将更新）："
								 << db_hash << " -> " << sha1 << ' ' << entry;
				db_log_print(sb);
				stmt_update.bind(1, sha1);
				stmt_update.bind(2, db_hash);
				if (stmt_update.exec() != 1) throw internal_err(SLS_WHERE);
				stmt_update.reset();
			}
			else {
				clear(sb) << u8"数据库的 history 表格中不存在备份记录（将添加）：" << sha1 << " " << entry;
				db_log_print(sb);
				stmt_insert.bind(1, sha1);
				stmt_insert.bind(2, time);
				stmt_insert.bind(3, (int64_t)authorID);
				stmt_insert.bind(4, entry);
				try { stmt_insert.exec(); }
				catch (std::exception &e) { throw internal_err(Str(e.what()) + SLS_WHERE); }
				stmt_insert.reset();
				db_history[sha1] = make_tuple(time, authorID, entry, true);
			}
			stmt_select4.reset();
		}
	}
	stmt_backup.reset();

	for (auto &row : db_history) {
		if (!get<3>(row.second)) {
			clear(sb) << u8"数据库 history 中记录不存在于备份数据库（将删除）：" << row.first << ", "
				 << get<0>(row.second) << ", " << get<1>(row.second) << ", " << get<2>(row.second);
			db_log_print(sb);
			stmt_delete.bind(1, row.first); stmt_delete.exec(); stmt_delete.reset();
		}
	}
	cout << "\ndone." << endl;

	for (auto &new_author : new_authors)
		cout << u8"新作者： " << new_author.first << ". " << new_author.second << endl;

	cout << "\nupdating author contribution..." << endl;

	SQLite::Statement stmt_select3(db_rw, R"(SELECT "aka" FROM "authors" WHERE "id"=?;)");
	SQLite::Statement stmt_contrib(db_rw, R"(UPDATE "authors" SET "contrib"=? WHERE "id"=?;)");
	for (auto &e : author_contrib) {
		// check author existence & authors.aka (only for wiki)
		stmt_select3.bind(1, (int64_t)e.first); // authors.id
		if (!stmt_select3.executeStep()) throw internal_err(SLS_WHERE);
		if (gv::is_wiki && stmt_select3.getColumn(0).getInt64() >= 0)
			throw internal_err(u8"所有备份记录必须转换成 authors.aka 的！");
		stmt_select3.reset();
		// update authors.contrib
		stmt_contrib.bind(1, (int64_t)e.second); // authors.contrib
		stmt_contrib.bind(2, (int64_t)e.first); // authors.id
		if (stmt_contrib.exec() != 1) throw internal_err(SLS_WHERE);
		stmt_contrib.reset();
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
	Str path = backup_db_path();
	backup_db_require(path);
	SQLite::Database db_backup(path, SQLite::OPEN_READONLY);
	db_backup.exec("PRAGMA busy_timeout = 3000;");

	SQLite::Statement stmt_select(db_rw, R"(SELECT "id" FROM "entries";)");
	vecStr entries;
	while (stmt_select.executeStep())
		entries.push_back(stmt_select.getColumn(0));

	struct BackupRec {
		int64_t id = 0;
		int64_t last_id = 0;
		bool last_null = true;
		Str hash;
		Str diff_json;
	};

	SQLite::Statement stmt_backup(db_backup,
		R"(SELECT "id", "last_id", "hash", "diff" FROM "backup_files" WHERE "entry"=?;)");

	unordered_map<Str, pair<Long, Long>> hist_add_del; // backup hash -> (add, del)
	for (auto &entry : entries) {
		stmt_backup.bind(1, entry);
		vector<BackupRec> recs;
		while (stmt_backup.executeStep()) {
			BackupRec rec;
			rec.id = stmt_backup.getColumn(0).getInt64();
			rec.last_null = stmt_backup.getColumn(1).isNull();
			rec.last_id = rec.last_null ? 0 : stmt_backup.getColumn(1).getInt64();
			rec.hash = stmt_backup.getColumn(2).getString();
			rec.diff_json = stmt_backup.getColumn(3).getString();
			recs.push_back(std::move(rec));
		}
		stmt_backup.reset();
		if (recs.empty())
			continue;

		unordered_map<int64_t, size_t> id_index;
		unordered_map<int64_t, int64_t> next_map;
		int64_t head_id = 0;
		for (size_t i = 0; i < recs.size(); ++i) {
			const auto &rec = recs[i];
			id_index[rec.id] = i;
			if (rec.last_null) {
				if (head_id != 0)
					throw internal_err(u8"backup_files 记录出现多个首版本：" + entry);
				head_id = rec.id;
			}
			else {
				if (next_map.count(rec.last_id))
					throw internal_err(u8"backup_files 记录出现分叉：" + entry);
				next_map[rec.last_id] = rec.id;
			}
		}
		if (head_id == 0)
			throw internal_err(u8"backup_files 记录未找到首版本：" + entry);

		Str current, prev;
		bool first = true;
		int64_t cur = head_id;
		while (cur != 0) {
			auto &rec = recs[id_index[cur]];
			vector<tuple<size_t, size_t, Str>> diff;
			str_diff_deserialize(diff, rec.diff_json);
			backup_apply_diff(current, diff);

			Long add = 0, del = 0;
			if (first) {
				add = u8count(current);
				del = 0;
				first = false;
			}
			else {
				str_add_del(add, del, prev, current);
			}
			hist_add_del[rec.hash] = make_pair(add, del);
			prev = current;

			auto it = next_map.find(cur);
			cur = (it == next_map.end()) ? 0 : it->second;
		}
	}

	// update db "history.add/del"
	cout << "\n\nupdating db history.add/del..." << endl;
	SQLite::Statement stmt_update(db_rw, redo_all ?
		R"(UPDATE "history" SET "add"=?, "del"=? WHERE "hash"=?;)" :
		R"(UPDATE "history" SET "add"=?, "del"=? WHERE "hash"=? AND "add"=-1 AND "del"=-1;)");
	for (auto &e : hist_add_del) {
		auto &hash = e.first;
		auto &add_del = e.second;
		stmt_update.bind(1, (int64_t)add_del.first);
		stmt_update.bind(2, (int64_t)add_del.second);
		stmt_update.bind(3, hash);
		stmt_update.exec();
		stmt_update.reset();
	}
	cout << "committing transaction..." << endl;
	cout << "done." << endl;
}

// simulate 5min backup rule, by updating backup timestamps
inline void history_normalize(SQLite::Database &db_rw)
{
	Str path = backup_db_path();
	backup_db_require(path);
	SQLite::Database db_backup(path, SQLite::OPEN_READWRITE);
	db_backup.exec("PRAGMA busy_timeout = 3000;");

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

	// update db and backup database
	SQLite::Statement stmt_update(db_rw,
		R"(UPDATE "history" SET "time"=? WHERE "hash"=?;)");
	SQLite::Statement stmt_delete(db_rw,
		R"(DELETE FROM "history" WHERE "hash"=?;)");
	SQLite::Statement stmt_backup_select(db_backup,
		R"(SELECT "id", "last_id" FROM "backup_files"
		   WHERE "time"=? AND "author"=? AND "entry"=?;)");
	SQLite::Statement stmt_backup_next(db_backup,
		R"(SELECT "id" FROM "backup_files" WHERE "last_id"=?;)");
	SQLite::Statement stmt_backup_update_time(db_backup,
		R"(UPDATE "backup_files" SET "time"=? WHERE "id"=?;)");
	SQLite::Statement stmt_backup_update_link(db_backup,
		R"(UPDATE "backup_files" SET "last_id"=?, "diff"=? WHERE "id"=?;)");
	SQLite::Statement stmt_backup_delete(db_backup,
		R"(DELETE FROM "backup_files" WHERE "id"=?;)");
	for (auto &e5 : entry_author_time_hash_time2) {
		for (auto &e4: e5.second) {
			for (auto &time_hash_time2: e4.second) {
				auto &time2 = time_hash_time2.second.second;
				if (time2.empty())
					continue;
				auto &time = time_hash_time2.first;
				auto &hash = time_hash_time2.second.first;
				Long authorID;
				if (str2int(authorID, e4.first) != size(e4.first))
					throw internal_err(u8"history.author 非整数: " + e4.first);
				const Str &entry = e5.first;

				stmt_backup_select.bind(1, time);
				stmt_backup_select.bind(2, (int64_t)authorID);
				stmt_backup_select.bind(3, entry);
				if (!stmt_backup_select.executeStep())
					throw internal_err(u8"backup_files 中找不到备份记录：" + entry + " " + time);
				const int64_t backup_id = stmt_backup_select.getColumn(0).getInt64();
				const bool last_null = stmt_backup_select.getColumn(1).isNull();
				const int64_t last_id = last_null ? 0 : stmt_backup_select.getColumn(1).getInt64();
				stmt_backup_select.reset();
				if (time2 == "d") {
					vector<int64_t> next_ids;
					stmt_backup_next.bind(1, backup_id);
					while (stmt_backup_next.executeStep())
						next_ids.push_back(stmt_backup_next.getColumn(0).getInt64());
					stmt_backup_next.reset();
					if (next_ids.size() > 1)
						throw internal_err(u8"backup_files 出现多个后继记录：" + entry + " " + time);

					if (!next_ids.empty()) {
						const int64_t next_id = next_ids.front();
						Str prev_content = (last_id == 0) ? Str() : backup_restore_str_by_id(last_id, db_backup);
						Str next_content = backup_restore_str_by_id(next_id, db_backup);
						vector<tuple<size_t, size_t, Str>> diff;
						str_diff(diff, prev_content, next_content);
						Str diff_json;
						str_diff_serialize(diff_json, diff);
						if (last_id == 0)
							stmt_backup_update_link.bind(1);
						else
							stmt_backup_update_link.bind(1, last_id);
						stmt_backup_update_link.bind(2, diff_json);
						stmt_backup_update_link.bind(3, next_id);
						if (stmt_backup_update_link.exec() != 1) throw internal_err(SLS_WHERE);
						stmt_backup_update_link.reset();
					}

					stmt_backup_delete.bind(1, backup_id);
					if (stmt_backup_delete.exec() != 1) throw internal_err(SLS_WHERE);
					stmt_backup_delete.reset();

					stmt_delete.bind(1, hash);
					if (stmt_delete.exec() != 1) throw internal_err(SLS_WHERE);
					stmt_delete.reset();
				}
				else {
					stmt_backup_update_time.bind(1, time2);
					stmt_backup_update_time.bind(2, backup_id);
					if (stmt_backup_update_time.exec() != 1) throw internal_err(SLS_WHERE);
					stmt_backup_update_time.reset();

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

// backup an entry to the backup database
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

	Str backup_path = backup_db_path();
	backup_db_require(backup_path);
	SQLite::Database db_backup(backup_path, SQLite::OPEN_READWRITE);
	db_backup.exec("PRAGMA busy_timeout = 3000;");

	SQLite::Statement stmt_backup_select(db_backup,
		R"(SELECT "id", "last_id" FROM "backup_files"
		   WHERE "time"=? AND "author"=? AND "entry"=?;)");
	SQLite::Statement stmt_backup_insert(db_backup,
		R"(INSERT INTO "backup_files" ("time", "author", "entry", "size", "hash", "last_id", "diff")
		   VALUES (?, ?, ?, ?, ?, ?, ?);)");
	SQLite::Statement stmt_backup_update(db_backup,
		R"(UPDATE "backup_files" SET "size"=?, "hash"=?, "diff"=? WHERE "id"=?;)");

	Str str; // content of entry
	Str time_new_str;

	// get hash, check existence
	clear(sb) << gv::path_in;
	if (!(entry == "main" || entry == "bibliography"))
		sb << "contents/";
	sb << entry << ".tex";
	read(str, sb);
	if (str.empty())
		db_log_print(u8"--backup 忽略空文件：" + entry);
	CRLF_to_LF(str);
	if (!is_valid(str))
		throw scan_err(u8"备份失败，文件不是合法 UTF-8：" + entry);
	Str hash = sha1sum(str).substr(0, 16);

	// check if hash exist
	stmt_select2.bind(1, hash);
	if (stmt_select2.executeStep()) {
		// hash already exists
		const Str &db_time_str = stmt_select2.getColumn(0);
		Long db_author_id = stmt_select2.getColumn(1).getInt64();
		const Str &db_entry = stmt_select2.getColumn(2);
		stmt_select2.reset();
		clear(sb) << "要备份的内容 hash 已经存在（将忽略）： "
			<< db_time_str << '_' << db_author_id << '_' << db_entry
			<< " hash=" << hash;
		SLS_WARN(sb);
		if (db_entry != entry)
			throw scan_err(u8"已存在的备份记录属于另一篇文章（这不可能，因为标题 entries.caption 禁止重复，不同文章的内容不可能一样）");
		return;
	}
	stmt_select2.reset();

	// get latest backup from `entries.last_backup`
	stmt_select.bind(1, entry);
	if (!stmt_select.executeStep())
		throw internal_err(u8"arg_backup(): 找不到要备份的文章：" + entry);
	const Str &hash_last = stmt_select.getColumn(0); // entries.last_backup
	stmt_select.reset();

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

		vector<tuple<size_t, size_t, Str>> diff;
		str_diff(diff, Str(), str);
		Str diff_json;
		str_diff_serialize(diff_json, diff);
		stmt_backup_insert.bind(1, time_new_str);
		stmt_backup_insert.bind(2, (int64_t)author_id);
		stmt_backup_insert.bind(3, entry);
		stmt_backup_insert.bind(4, (int64_t)str.size());
		stmt_backup_insert.bind(5, hash);
		stmt_backup_insert.bind(6);
		stmt_backup_insert.bind(7, diff_json);
		stmt_backup_insert.exec(); stmt_backup_insert.reset();

		stmt_update2.bind(1, hash);
		stmt_update2.bind(2, entry);
		if (stmt_update2.exec() != 1) throw internal_err(SLS_WHERE);
		stmt_update2.reset();

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
	stmt_select2.reset();

	stmt_backup_select.bind(1, time_last_str);
	stmt_backup_select.bind(2, (int64_t)author_id_last);
	stmt_backup_select.bind(3, entry);
	if (!stmt_backup_select.executeStep())
		throw internal_err(u8"backup_files 中找不到备份记录：" + entry + " " + time_last_str);
	const int64_t last_backup_id = stmt_backup_select.getColumn(0).getInt64();
	const bool prev_null = stmt_backup_select.getColumn(1).isNull();
	const int64_t prev_backup_id = prev_null ? 0 : stmt_backup_select.getColumn(1).getInt64();
	stmt_backup_select.reset();

	// calculate `time_new_str` in current backup time
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

	Long char_add, char_del;

	if (replace) { // replace last (latest) backup
		Str prev_content = (prev_backup_id == 0) ? Str() : backup_restore_str_by_id(prev_backup_id, db_backup);
		str_add_del(char_add, char_del, prev_content, str);

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

		vector<tuple<size_t, size_t, Str>> diff;
		str_diff(diff, prev_content, str);
		Str diff_json;
		str_diff_serialize(diff_json, diff);
		stmt_backup_update.bind(1, (int64_t)str.size());
		stmt_backup_update.bind(2, hash);
		stmt_backup_update.bind(3, diff_json);
		stmt_backup_update.bind(4, last_backup_id);
		if (stmt_backup_update.exec() != 1) throw internal_err(SLS_WHERE);
		stmt_backup_update.reset();

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
		Str prev_content = backup_restore_str_by_id(last_backup_id, db_backup);
		str_add_del(char_add, char_del, prev_content, str);

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

		vector<tuple<size_t, size_t, Str>> diff;
		str_diff(diff, prev_content, str);
		Str diff_json;
		str_diff_serialize(diff_json, diff);
		stmt_backup_insert.bind(1, time_new_str);
		stmt_backup_insert.bind(2, (int64_t)author_id);
		stmt_backup_insert.bind(3, entry);
		stmt_backup_insert.bind(4, (int64_t)str.size());
		stmt_backup_insert.bind(5, hash);
		stmt_backup_insert.bind(6, last_backup_id);
		stmt_backup_insert.bind(7, diff_json);
		stmt_backup_insert.exec(); stmt_backup_insert.reset();

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
// update db "history" table from backup database
inline void arg_history(SQLite::Database &db_rw)
{
		db_update_author_history(db_rw);
		db_update_authors(db_rw);
}
