#pragma once
#include "sqlite_db.h"
#include "backup_db.h"
#include "../SLISC/str/str_diff_patch2.h"
#include "../SLISC/file/file.h"
#ifndef _WIN32
#include <dirent.h>
#endif

inline Str backup_db_path()
{
	return gv::path_data + "backup.db";
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

	vecLong db_author_ids0;
	vecStr db_author_names0;
	unordered_map<Long, Long> author_aka;
	SQLite::Statement stmt_select2(db_rw, R"(SELECT "id", "name", "aka" FROM "authors")");
	while (stmt_select2.executeStep()) {
		Long author_id = stmt_select2.getColumn(0).getInt64();
		db_author_ids0.push_back(author_id);
		db_author_names0.push_back(stmt_select2.getColumn(1));
		author_aka[author_id] = stmt_select2.getColumn(2).getInt64();
	}
	stmt_select2.reset();

	cout << "there are already " << db_author_ids0.size() << " author records in database." << endl;
	unordered_map<Long, Str> db_id_to_author;
	for (Long i = 0; i < size(db_author_ids0); ++i)
		db_id_to_author[db_author_ids0[i]] = db_author_names0[i];

	db_author_ids0.clear(); db_author_names0.clear();
	db_author_ids0.shrink_to_fit(); db_author_names0.shrink_to_fit();

	auto resolve_author = [&](Long author_id) {
		Long cur = author_id;
		for (int i = 0; i < 8; ++i) {
			auto it = author_aka.find(cur);
			if (it == author_aka.end())
				break;
			Long aka = it->second;
			if (aka < 0 || aka == cur)
				break;
			cur = aka;
		}
		return cur;
	};

	// update "history" table
	SQLite::Statement stmt_select(db_rw,
		R"(SELECT "hash", "time", "author", "entry" FROM "history" WHERE "hash" <> '';)");
	SQLite::Statement stmt_history_select(db_rw,
		R"(SELECT "hash" FROM "history" WHERE "time"=? AND "author"=? AND "entry"=?;)");
	SQLite::Statement stmt_history_author(db_rw,
		R"(UPDATE "history" SET "author"=? WHERE "hash"=?;)");

	//            hash        time author entry  record-exist
	unordered_map<Str,  tuple<Str, Long,  Str,   bool>> db_history;
	while (stmt_select.executeStep()) {
		const Str hash = stmt_select.getColumn(0).getString();
		const Str time_str = stmt_select.getColumn(1).getString();
		Long author_id = stmt_select.getColumn(2).getInt64();
		const Str entry_str = stmt_select.getColumn(3).getString();
		Long author_id2 = resolve_author(author_id);
		if (author_id2 != author_id) {
			stmt_history_select.bind(1, time_str);
			stmt_history_select.bind(2, (int64_t)author_id2);
			stmt_history_select.bind(3, entry_str);
			if (stmt_history_select.executeStep()) {
				const Str hash_exist = stmt_history_select.getColumn(0).getString();
				if (hash_exist != hash) {
					clear(sb) << u8"history.author 与 authors.aka 冲突（将忽略更新）： hash=" << hash
						<< ", time=" << time_str << ", entry=" << entry_str
						<< ", author=" << author_id << ", aka=" << author_id2
						<< ", existing_hash=" << hash_exist;
					scan_log_warn(sb);
				}
			}
			else {
				stmt_history_author.bind(1, (int64_t)author_id2);
				stmt_history_author.bind(2, hash);
				if (stmt_history_author.exec() != 1) throw internal_err(SLS_WHERE);
				stmt_history_author.reset();
			}
			stmt_history_select.reset();
			author_id = author_id2;
		}

		db_history[hash] = make_tuple(time_str, author_id, entry_str, false);
	}
	stmt_select.reset();

	cout << "there are already " << db_history.size() << " backup (history) records in database." << endl;

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
		Long authorID = resolve_author(stmt_backup.getColumn(1).getInt64());
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
				clear(sb) << u8"备份记录时间与数据库不同（将不更新）： hash=" << sha1
					<< ", backup_time=" << time
					<< ", db_time=" << get<0>(time_author_entry_fexist)
					<< ", entry=" << entry
					<< ", author=" << authorID;
				db_log_print(sb);
			}
			if (get<1>(time_author_entry_fexist) != authorID) {
				clear(sb) << u8"备份记录作者与数据库不同（将不更新）： hash=" << sha1
					<< ", backup_author=" << authorID
					<< ", db_author=" << to_string(get<1>(time_author_entry_fexist)) << '.'
					<< db_id_to_author[get<1>(time_author_entry_fexist)]
					<< ", time=" << time
					<< ", entry=" << entry;
				db_log_print(sb);
			}
			if (get<2>(time_author_entry_fexist) != entry) {
				clear(sb) << u8"备份记录文章名与数据库不同（将不更新）： hash=" << sha1
					<< ", backup_entry=" << entry
					<< ", db_entry=" << get<2>(time_author_entry_fexist)
					<< ", time=" << time
					<< ", author=" << authorID;
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

	SQLite::Statement stmt_history(db_rw,
		R"(SELECT "hash", "add", "del" FROM "history" WHERE "entry"=?;)");

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
		unordered_map<Str, pair<Long, Long>> history_add_del;
		if (!redo_all) {
			stmt_history.bind(1, entry);
			while (stmt_history.executeStep()) {
				history_add_del[stmt_history.getColumn(0)] =
					make_pair(stmt_history.getColumn(1).getInt64(),
							  stmt_history.getColumn(2).getInt64());
			}
			stmt_history.reset();
		}

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

			bool need_add_del = redo_all;
				if (!redo_all) {
					auto it_hist = history_add_del.find(rec.hash);
					if (it_hist != history_add_del.end())
						need_add_del = (it_hist->second.first == -1 || it_hist->second.second == -1);
					else {
						clear(sb) << u8"内部警告：history 表缺少记录（将忽略）： entry=" << entry
							<< ", hash=" << rec.hash << ", backup_id=" << rec.id;
						scan_log_warn(sb);
						need_add_del = false;
					}
				}

			if (need_add_del) {
				Long add = 0, del = 0;
				if (first) {
					add = u8count(current);
					del = 0;
				}
				else {
					str_add_del(add, del, prev, current, "recycle/");
				}
				hist_add_del[rec.hash] = make_pair(add, del);
			}
			first = false;
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

inline bool backup_parse_tex_filename(Str_I name, Str_O time, Long &author, Str_O entry)
{
	if (name.size() < 5 || name.substr(name.size() - 4) != ".tex")
		return false;
	Long pos1 = (Long)name.find('_');
	Long pos2 = (Long)name.rfind('_');
	if (pos1 < 0 || pos2 < 0 || pos1 == pos2)
		return false;
	time = name.substr(0, pos1);
	Str author_str = name.substr(pos1 + 1, pos2 - pos1 - 1);
	entry = name.substr(pos2 + 1, name.size() - pos2 - 1 - 4);
	if (time.size() != 12 || author_str.empty() || entry.empty())
		return false;
	for (auto c : time) {
		if (c < '0' || c > '9')
			return false;
	}
	if (str2int(author, author_str) != size(author_str))
		return false;
	return true;
}

inline void backup_list_tex_files(vecStr_O names, Str_I dir)
{
	names.clear();
#ifdef _WIN32
	file_list_ext(names, dir, ".tex", true);
#else
	DIR *dp = opendir(dir.c_str());
	if (!dp)
		throw internal_err(u8"无法打开备份文件夹：" + dir);
	dirent *entry = nullptr;
	while ((entry = readdir(dp)) != nullptr) {
		Str name = entry->d_name;
		if (name == "." || name == "..")
			continue;
		if (name.size() >= 4 && name.substr(name.size() - 4) == ".tex")
			names.push_back(name);
	}
	closedir(dp);
	std::sort(names.begin(), names.end());
#endif
}

// return 0: written, 1: already exists (match), -1: exists but differs
inline int backup_write_recovered_file(Str_I backup_dir, Str_I name, Str_I content, bool verbose)
{
	Str out_path = backup_dir + name;
	if (file_exist(out_path)) {
		Str existing;
		read(existing, out_path);
		CRLF_to_LF(existing);
		if (existing == content) {
			if (verbose)
				cout << "backup file already exists: " << out_path << endl;
			return 1;
		}
		clear(sb) << u8"备份文件已存在且内容不同（将保留文件）： " << out_path;
		scan_log_warn(sb);
		return -1;
	}

	write(content, out_path);
	if (verbose)
		cout << "backup file recovered: " << out_path << endl;
	return 0;
}

inline void backup_recover_tex_file(Str_I filename)
{
	Str name = filename;
	Long pos = max((Long)name.rfind('/'), (Long)name.rfind('\\'));
	if (pos >= 0)
		name = name.substr(pos + 1);

	Str time, entry;
	Long author = 0;
	if (!backup_parse_tex_filename(name, time, author, entry))
		throw internal_err(u8"备份文件名格式错误：" + name);

	const Str backup_dir = "../PhysWiki-backup/";
	if (!dir_exist(backup_dir))
		throw internal_err(u8"备份文件夹不存在：" + backup_dir);

	Str path = backup_db_path();
	backup_db_require(path);
	SQLite::Database db_backup(path, SQLite::OPEN_READONLY);
	db_backup.exec("PRAGMA busy_timeout = 3000;");

	Str content;
	try {
		content = backup_restore_str(time, author, entry, db_backup);
	}
	catch (const std::exception &e) {
		throw internal_err(Str(e.what()) + SLS_WHERE);
	}

	backup_write_recovered_file(backup_dir, name, content, true);
}

inline bool backup_recover_entry_versions(Str_I entry, SQLite::Database &db_backup, Str_I backup_dir,
	Long &written, Long &matched, Long &mismatch)
{
	struct BackupChainRec {
		int64_t id = 0;
		Str time;
		Long author = 0;
		Str hash;
		int64_t size = 0;
		Str diff_json;
		int64_t last_id = 0;
		bool last_null = true;
	};

	vector<BackupChainRec> recs;
	SQLite::Statement stmt(db_backup,
		R"(SELECT "id", "time", "author", "hash", "size", "diff", "last_id"
		   FROM "backup_files" WHERE "entry"=?;)");
	stmt.bind(1, entry);
	while (stmt.executeStep()) {
		BackupChainRec rec;
		rec.id = stmt.getColumn(0).getInt64();
		rec.time = stmt.getColumn(1).getString();
		rec.author = stmt.getColumn(2).getInt64();
		rec.hash = stmt.getColumn(3).getString();
		rec.size = stmt.getColumn(4).getInt64();
		rec.diff_json = stmt.getColumn(5).getString();
		rec.last_null = stmt.getColumn(6).isNull();
		rec.last_id = rec.last_null ? 0 : stmt.getColumn(6).getInt64();
		recs.push_back(std::move(rec));
	}
	stmt.reset();

	if (recs.empty())
		return false;

	unordered_map<int64_t, size_t> id_index;
	unordered_map<int64_t, int64_t> next_map;
	int64_t head_id = 0;
	for (size_t i = 0; i < recs.size(); ++i)
		id_index[recs[i].id] = i;
	for (size_t i = 0; i < recs.size(); ++i) {
		const auto &rec = recs[i];
		if (rec.last_null) {
			if (head_id != 0)
				throw internal_err(u8"backup_files 记录出现多个首版本：" + entry);
			head_id = rec.id;
		}
		else {
			if (!id_index.count(rec.last_id)) {
				clear(sb) << u8"backup_files 记录 last_id 不存在： entry=" << entry
					<< ", id=" << rec.id << ", last_id=" << rec.last_id;
				throw internal_err(sb);
			}
			if (next_map.count(rec.last_id)) {
				clear(sb) << u8"backup_files 记录出现分叉： " << entry << ", last_id=" << rec.last_id;
				throw internal_err(sb);
			}
			next_map[rec.last_id] = rec.id;
		}
	}
	if (head_id == 0)
		throw internal_err(u8"backup_files 记录未找到首版本：" + entry);

	Str content;
	int64_t cur = head_id;
	while (cur != 0) {
		auto &rec = recs[id_index[cur]];
		vector<tuple<size_t, size_t, Str>> diff;
		str_diff_deserialize(diff, rec.diff_json);
		backup_apply_diff(content, diff);

		if (rec.size != (int64_t)content.size()) {
			clear(sb) << u8"backup_files size 校验失败： entry=" << entry
				<< ", id=" << rec.id << ", size=" << rec.size << ", got=" << content.size();
			throw internal_err(sb);
		}
		Str hash = sha1sum(content).substr(0, 16);
		if (hash != rec.hash) {
			clear(sb) << u8"backup_files hash 校验失败： entry=" << entry
				<< ", id=" << rec.id << ", hash=" << rec.hash << ", got=" << hash;
			throw internal_err(sb);
		}

		Str name = rec.time + "_" + num2str(rec.author) + "_" + entry + ".tex";
		int res = backup_write_recovered_file(backup_dir, name, content, false);
		if (res == 0)
			++written;
		else if (res > 0)
			++matched;
		else
			++mismatch;

		auto it_next = next_map.find(cur);
		cur = (it_next == next_map.end()) ? 0 : it_next->second;
	}
	return true;
}

inline void backup_recover_entry(Str_I entry)
{
	const Str backup_dir = "../PhysWiki-backup/";
	if (!dir_exist(backup_dir))
		throw internal_err(u8"备份文件夹不存在：" + backup_dir);

	Str path = backup_db_path();
	backup_db_require(path);
	SQLite::Database db_backup(path, SQLite::OPEN_READONLY);
	db_backup.exec("PRAGMA busy_timeout = 3000;");

	Long written = 0, matched = 0, mismatch = 0;
	if (!backup_recover_entry_versions(entry, db_backup, backup_dir, written, matched, mismatch)) {
		cout << "no backup records for entry: " << entry << endl;
		return;
	}
	cout << "backup recover entry done. written=" << written
		 << ", existed=" << matched << ", mismatched=" << mismatch << endl;
}

inline void backup_recover_all()
{
	const Str backup_dir = "../PhysWiki-backup/";
	if (!dir_exist(backup_dir))
		throw internal_err(u8"备份文件夹不存在：" + backup_dir);

	Str path = backup_db_path();
	backup_db_require(path);
	SQLite::Database db_backup(path, SQLite::OPEN_READONLY);
	db_backup.exec("PRAGMA busy_timeout = 3000;");

	SQLite::Statement stmt_entry(db_backup,
		R"(SELECT "entry" FROM "backup_files" GROUP BY "entry" ORDER BY "entry" ASC;)");
	Long written = 0, matched = 0, mismatch = 0;
	Long entries = 0;
	while (stmt_entry.executeStep()) {
		const Str entry = stmt_entry.getColumn(0).getString();
		if (backup_recover_entry_versions(entry, db_backup, backup_dir, written, matched, mismatch))
			++entries;
	}
	stmt_entry.reset();
	cout << "backup recover done. entries=" << entries
		 << ", written=" << written << ", existed=" << matched
		 << ", mismatched=" << mismatch << endl;
}

// read ../PhysWiki-backup/*.tex and update backup db + scan.db history
inline void backup_update_db_from_tex_files(SQLite::Database &db_rw)
{
	const Str backup_dir = "../PhysWiki-backup/";
	if (!dir_exist(backup_dir))
		throw internal_err(u8"备份文件夹不存在：" + backup_dir);

	vecStr tex_names;
	backup_list_tex_files(tex_names, backup_dir);
	if (tex_names.empty()) {
		cout << "no backup tex files found." << endl;
		return;
	}

	struct TexBackupFile {
		Str name;
		Str path;
		Str time;
		Long author = 0;
		Str entry;
		Str content;
		Str hash;
		int64_t size = 0;
	};

	unordered_map<Str, vector<TexBackupFile>> entry_files;
	for (auto &name : tex_names) {
		TexBackupFile rec;
		rec.name = name;
		rec.path = backup_dir + name;
		if (!backup_parse_tex_filename(name, rec.time, rec.author, rec.entry)) {
			scan_log_warn(u8"备份文件名格式错误（将忽略）： " + name);
			continue;
		}
		read(rec.content, rec.path);
		CRLF_to_LF(rec.content);
		if (!is_valid(rec.content)) {
			scan_log_warn(u8"备份文件不是合法 UTF-8（将忽略）： " + name);
			continue;
		}
		rec.size = (int64_t)rec.content.size();
		rec.hash = sha1sum(rec.content).substr(0, 16);
		entry_files[rec.entry].push_back(std::move(rec));
	}

	if (entry_files.empty()) {
		cout << "no valid backup tex files found." << endl;
		return;
	}

	Str path = backup_db_path();
	backup_db_require(path);
	SQLite::Database db_backup(path, SQLite::OPEN_READWRITE);
	db_backup.exec("PRAGMA busy_timeout = 3000;");

	SQLite::Transaction trans_backup(db_backup);
	SQLite::Transaction trans_scan(db_rw);

	SQLite::Statement stmt_hash(db_backup,
		R"(SELECT "id", "time", "author", "entry", "size" FROM "backup_files" WHERE "hash"=?;)");
	SQLite::Statement stmt_time(db_backup,
		R"(SELECT "id", "hash", "size" FROM "backup_files"
		   WHERE "time"=? AND "author"=? AND "entry"=?;)");
	SQLite::Statement stmt_entry(db_backup,
		R"(SELECT "id", "time", "author", "hash", "size", "last_id" FROM "backup_files" WHERE "entry"=?;)");

	SQLite::Statement stmt_author(db_rw, R"(SELECT "name" FROM "authors" WHERE "id"=?;)");
	SQLite::Statement stmt_author_insert(db_rw,
		R"(INSERT OR REPLACE INTO "authors" ("id", "name") VALUES (?, ?);)");
	SQLite::Statement stmt_entry_exist(db_rw, R"(SELECT 1 FROM "entries" WHERE "id"=?;)");
	SQLite::Statement stmt_entry_insert(db_rw,
		R"(INSERT OR REPLACE INTO "entries" ("id", "deleted") VALUES (?, 1);)");
	SQLite::Statement stmt_hist_hash(db_rw,
		R"(SELECT "time", "author", "entry" FROM "history" WHERE "hash"=?;)");
	SQLite::Statement stmt_hist_time(db_rw,
		R"(SELECT "hash" FROM "history" WHERE "time"=? AND "author"=? AND "entry"=?;)");
	SQLite::Statement stmt_hist_insert(db_rw,
		R"(INSERT INTO "history" ("hash", "time", "author", "entry") VALUES (?, ?, ?, ?);)");

	auto ensure_history = [&](const TexBackupFile &file_rec) {
		stmt_author.bind(1, (int64_t)file_rec.author);
		if (!stmt_author.executeStep()) {
			stmt_author.reset();
			stmt_author_insert.bind(1, (int64_t)file_rec.author);
			stmt_author_insert.bind(2, to_string(file_rec.author));
			stmt_author_insert.exec();
			stmt_author_insert.reset();
		}
		else {
			stmt_author.reset();
		}

		stmt_entry_exist.bind(1, file_rec.entry);
		if (!stmt_entry_exist.executeStep()) {
			stmt_entry_exist.reset();
			stmt_entry_insert.bind(1, file_rec.entry);
			stmt_entry_insert.exec();
			stmt_entry_insert.reset();
		}
		else {
			stmt_entry_exist.reset();
		}

		stmt_hist_hash.bind(1, file_rec.hash);
		if (stmt_hist_hash.executeStep()) {
			const Str db_time = stmt_hist_hash.getColumn(0).getString();
			Long db_author = stmt_hist_hash.getColumn(1).getInt64();
			const Str db_entry = stmt_hist_hash.getColumn(2).getString();
			stmt_hist_hash.reset();
			if (db_time != file_rec.time || db_author != file_rec.author || db_entry != file_rec.entry) {
				clear(sb) << u8"history 记录与备份文件信息不一致（将忽略）： hash=" << file_rec.hash
					<< ", db=" << db_time << '_' << db_author << '_' << db_entry
					<< ", file=" << file_rec.time << '_' << file_rec.author << '_' << file_rec.entry;
				scan_log_warn(sb);
			}
			return;
		}
		stmt_hist_hash.reset();

		stmt_hist_time.bind(1, file_rec.time);
		stmt_hist_time.bind(2, (int64_t)file_rec.author);
		stmt_hist_time.bind(3, file_rec.entry);
		if (stmt_hist_time.executeStep()) {
			clear(sb) << u8"history 已存在不同 hash（将忽略）： "
				<< file_rec.time << '_' << file_rec.author << '_' << file_rec.entry
				<< " db_hash=" << stmt_hist_time.getColumn(0).getString()
				<< ", file_hash=" << file_rec.hash;
			scan_log_warn(sb);
			stmt_hist_time.reset();
			return;
		}
		stmt_hist_time.reset();

		stmt_hist_insert.bind(1, file_rec.hash);
		stmt_hist_insert.bind(2, file_rec.time);
		stmt_hist_insert.bind(3, (int64_t)file_rec.author);
		stmt_hist_insert.bind(4, file_rec.entry);
		stmt_hist_insert.exec();
		stmt_hist_insert.reset();
	};

	for (auto &entry_pair : entry_files) {
		auto &files = entry_pair.second;
		sort(files.begin(), files.end(), [](const TexBackupFile &a, const TexBackupFile &b) {
			if (a.time != b.time)
				return a.time < b.time;
			return a.author < b.author;
		});

		struct BackupChainRec {
			int64_t id = 0;
			Str time;
			Long author = 0;
			Str hash;
			int64_t size = 0;
			int64_t last_id = 0;
			bool last_null = true;
		};

		unordered_map<int64_t, BackupChainRec> recs;
		unordered_map<int64_t, int64_t> next_map;
		int64_t head_id = 0;

		stmt_entry.bind(1, entry_pair.first);
		while (stmt_entry.executeStep()) {
			BackupChainRec rec;
			rec.id = stmt_entry.getColumn(0).getInt64();
			rec.time = stmt_entry.getColumn(1).getString();
			rec.author = stmt_entry.getColumn(2).getInt64();
			rec.hash = stmt_entry.getColumn(3).getString();
			rec.size = stmt_entry.getColumn(4).getInt64();
			rec.last_null = stmt_entry.getColumn(5).isNull();
			rec.last_id = rec.last_null ? 0 : stmt_entry.getColumn(5).getInt64();
			recs[rec.id] = rec;
		}
		stmt_entry.reset();

		for (auto &pair : recs) {
			const BackupChainRec &rec = pair.second;
			if (rec.last_null) {
				if (head_id != 0)
					throw internal_err(u8"backup_files 记录出现多个首版本：" + entry_pair.first);
				head_id = rec.id;
			}
			else {
				auto it_last = recs.find(rec.last_id);
				if (it_last == recs.end()) {
					clear(sb) << u8"backup_files 记录 last_id 不存在： entry=" << entry_pair.first
						<< ", id=" << rec.id << ", last_id=" << rec.last_id;
					throw internal_err(sb);
				}
				if (next_map.count(rec.last_id)) {
					clear(sb) << u8"backup_files 记录出现分叉： entry=" << entry_pair.first
						<< ", last_id=" << rec.last_id;
					throw internal_err(sb);
				}
				next_map[rec.last_id] = rec.id;
			}
		}

		if (!recs.empty() && head_id == 0)
			throw internal_err(u8"backup_files 记录未找到首版本：" + entry_pair.first);

		for (auto &file_rec : files) {
			bool delete_file = false;
			bool update_history = false;

			stmt_hash.bind(1, file_rec.hash);
			if (stmt_hash.executeStep()) {
				const int64_t db_id = stmt_hash.getColumn(0).getInt64();
				const Str db_time = stmt_hash.getColumn(1).getString();
				Long db_author = stmt_hash.getColumn(2).getInt64();
				const Str db_entry = stmt_hash.getColumn(3).getString();
				const int64_t db_size = stmt_hash.getColumn(4).getInt64();
				stmt_hash.reset();
				if (db_time == file_rec.time && db_author == file_rec.author &&
					db_entry == file_rec.entry && db_size == file_rec.size) {
					delete_file = true;
					update_history = true;
				}
				else {
					clear(sb) << u8"备份文件信息与数据库不一致（将保留文件）： file="
						<< file_rec.name << ", db_id=" << db_id
						<< ", db=" << db_time << '_' << db_author << '_' << db_entry
						<< ", file=" << file_rec.time << '_' << file_rec.author << '_' << file_rec.entry;
					scan_log_warn(sb);
				}
			}
			else {
				stmt_hash.reset();
				stmt_time.bind(1, file_rec.time);
				stmt_time.bind(2, (int64_t)file_rec.author);
				stmt_time.bind(3, file_rec.entry);
				if (stmt_time.executeStep()) {
					clear(sb) << u8"backup_files 已存在不同 hash（将保留文件）： "
						<< file_rec.time << '_' << file_rec.author << '_' << file_rec.entry
						<< ", db_hash=" << stmt_time.getColumn(1).getString()
						<< ", file_hash=" << file_rec.hash;
					scan_log_warn(sb);
					stmt_time.reset();
				}
				else {
					stmt_time.reset();
					int64_t prev_id = 0;
					int64_t next_id = head_id;
					int64_t cur = head_id;
					while (cur != 0) {
						auto &cur_rec = recs[cur];
						bool before = (cur_rec.time < file_rec.time) ||
							(cur_rec.time == file_rec.time && cur_rec.author <= file_rec.author);
						if (!before)
							break;
						prev_id = cur;
						auto it_next = next_map.find(cur);
						cur = (it_next == next_map.end()) ? 0 : it_next->second;
					}
					next_id = cur;
					int64_t new_id = backup_insert_version_between(db_backup, file_rec.entry, file_rec.time,
						(int64_t)file_rec.author, file_rec.content, prev_id, next_id, file_rec.hash);

					BackupChainRec new_rec;
					new_rec.id = new_id;
					new_rec.time = file_rec.time;
					new_rec.author = file_rec.author;
					new_rec.hash = file_rec.hash;
					new_rec.size = file_rec.size;
					new_rec.last_id = prev_id;
					new_rec.last_null = (prev_id == 0);
					recs[new_id] = new_rec;
					if (prev_id == 0)
						head_id = new_id;
					else
						next_map[prev_id] = new_id;
					if (next_id != 0)
						next_map[new_id] = next_id;

					delete_file = true;
					update_history = true;
				}
			}

			if (update_history)
				ensure_history(file_rec);
			if (delete_file)
				file_remove(file_rec.path);
		}
	}

	db_update_history_last(db_rw);
	trans_backup.commit();
	trans_scan.commit();
	cout << "backup db update done." << endl;
}

// check backup.db integrity
inline void backup_check()
{
	cout << "checking backup.db..." << endl;
	Str path = backup_db_path();
	backup_db_require(path);
	SQLite::Database db_backup(path, SQLite::OPEN_READONLY);
	db_backup.exec("PRAGMA busy_timeout = 3000;");

	struct BackupCheckRec {
		int64_t id = 0;
		Str time;
		Str entry;
		int64_t size = 0;
		Str hash;
		Str diff_json;
		int64_t last_id = 0;
		bool last_null = true;
	};

	unordered_map<int64_t, BackupCheckRec> recs;
	unordered_map<Str, vector<int64_t>> entry_ids;
	SQLite::Statement stmt(db_backup,
		R"(SELECT "id", "time", "entry", "size", "hash", "diff", "last_id" FROM "backup_files";)");
	while (stmt.executeStep()) {
		BackupCheckRec rec;
		rec.id = stmt.getColumn(0).getInt64();
		rec.time = stmt.getColumn(1).getString();
		rec.entry = stmt.getColumn(2).getString();
		rec.size = stmt.getColumn(3).getInt64();
		rec.hash = stmt.getColumn(4).getString();
		rec.diff_json = stmt.getColumn(5).getString();
		rec.last_null = stmt.getColumn(6).isNull();
		rec.last_id = rec.last_null ? 0 : stmt.getColumn(6).getInt64();
		if (recs.count(rec.id))
			throw internal_err(u8"backup_files 出现重复 id：" + num2str((Long)rec.id));
		recs[rec.id] = rec;
		entry_ids[rec.entry].push_back(rec.id);
	}
	stmt.reset();

	if (recs.empty()) {
		cout << "backup_files is empty." << endl;
		return;
	}

	unordered_map<int64_t, int64_t> next_map;
	unordered_map<Str, vector<int64_t>> entry_heads;
	for (auto &pair : recs) {
		const BackupCheckRec &rec = pair.second;
		if (rec.last_null) {
			entry_heads[rec.entry].push_back(rec.id);
			continue;
		}
		auto it_last = recs.find(rec.last_id);
		if (it_last == recs.end()) {
			clear(sb) << u8"backup_files 记录 last_id 不存在： entry=" << rec.entry
				<< ", id=" << rec.id << ", last_id=" << rec.last_id;
			throw internal_err(sb);
		}
		if (it_last->second.entry != rec.entry) {
			clear(sb) << u8"backup_files 记录跨文章链接： entry=" << rec.entry
				<< ", id=" << rec.id << ", last_id=" << rec.last_id
				<< ", last_entry=" << it_last->second.entry;
			throw internal_err(sb);
		}
		auto it_next = next_map.find(rec.last_id);
		if (it_next != next_map.end()) {
			clear(sb) << u8"backup_files 记录出现分叉： entry=" << rec.entry
				<< ", last_id=" << rec.last_id << ", id1=" << it_next->second
				<< ", id2=" << rec.id;
			throw internal_err(sb);
		}
		next_map[rec.last_id] = rec.id;
	}

	for (auto &entry_pair : entry_ids) {
		const Str &entry = entry_pair.first;
		auto it_head = entry_heads.find(entry);
		if (it_head == entry_heads.end()) {
			throw internal_err(u8"backup_files 记录未找到首版本：" + entry);
		}
		if (it_head->second.size() > 1) {
			clear(sb) << u8"backup_files 记录出现多个首版本：" << entry;
			throw internal_err(sb);
		}
	}

	unordered_set<int64_t> visited;
	for (auto &entry_pair : entry_ids) {
		const Str &entry = entry_pair.first;
		const int64_t head_id = entry_heads[entry].front();
		int64_t cur = head_id;
		Str content;
		bool has_prev_time = false;
		time_t prev_time = 0;
		while (cur != 0) {
			if (visited.count(cur)) {
				clear(sb) << u8"backup_files 链表出现环： entry=" << entry << ", id=" << cur;
				throw internal_err(sb);
			}
			visited.insert(cur);
			auto &rec = recs[cur];
			time_t cur_time = 0;
			try {
				cur_time = str2time_t(rec.time);
			}
			catch (const std::exception &e) {
				throw internal_err(Str(e.what()) + SLS_WHERE);
			}
			if (has_prev_time && cur_time < prev_time) {
				clear(sb) << u8"backup_files 时间顺序错误： entry=" << entry
					<< ", id=" << rec.id << ", time=" << rec.time;
				throw internal_err(sb);
			}
			prev_time = cur_time;
			has_prev_time = true;

			vector<tuple<size_t, size_t, Str>> diff;
			str_diff_deserialize(diff, rec.diff_json);
			backup_apply_diff(content, diff);
			if (rec.size != (int64_t)content.size()) {
				clear(sb) << u8"backup_files size 校验失败： entry=" << entry
					<< ", id=" << rec.id << ", size=" << rec.size
					<< ", got=" << content.size();
				throw internal_err(sb);
			}
			Str hash = sha1sum(content).substr(0, 16);
			if (hash != rec.hash) {
				clear(sb) << u8"backup_files hash 校验失败： entry=" << entry
					<< ", id=" << rec.id << ", hash=" << rec.hash
					<< ", got=" << hash;
				throw internal_err(sb);
			}

			auto it_next = next_map.find(cur);
			cur = (it_next == next_map.end()) ? 0 : it_next->second;
		}
	}

	if (visited.size() != recs.size()) {
		for (auto &pair : recs) {
			if (!visited.count(pair.first)) {
				clear(sb) << u8"backup_files 出现孤立记录： entry=" << pair.second.entry
					<< ", id=" << pair.second.id;
				throw internal_err(sb);
			}
		}
	}

	cout << "backup database check done. records=" << recs.size() << endl;
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
	SQLite::Statement stmt_backup_update_time(db_backup,
		R"(UPDATE "backup_files" SET "time"=? WHERE "id"=?;)");
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
				stmt_backup_select.reset();
				if (time2 == "d") {
					backup_delete_record(db_backup, backup_id);

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
	Long author_id2 = author_id;
	SQLite::Statement stmt_author(db_rw, R"(SELECT "aka" FROM "authors" WHERE "id"=?;)");
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

	stmt_author.bind(1, (int64_t)author_id2);
	if (!stmt_author.executeStep())
		throw internal_err(u8"arg_backup(): 作者不存在：" + num2str((Long)author_id2));
	Long aka = stmt_author.getColumn(0).getInt64();
	stmt_author.reset();
	if (aka >= 0)
		author_id2 = aka;

	Str backup_path = backup_db_path();
	backup_db_require(backup_path);
	SQLite::Database db_backup(backup_path, SQLite::OPEN_READWRITE);
	db_backup.exec("PRAGMA busy_timeout = 3000;");

	SQLite::Statement stmt_backup_select(db_backup,
		R"(SELECT "id", "last_id" FROM "backup_files"
		   WHERE "time"=? AND "author"=? AND "entry"=?;)");

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
		stmt_insert.bind(3, (int64_t)author_id2);
		stmt_insert.bind(4, entry);
		stmt_insert.bind(5, int64_t(u8count(str)));
		stmt_insert.bind(6, 0);
		stmt_insert.bind(7, "");
		stmt_insert.exec(); stmt_insert.reset();
		backup_append_version(db_backup, entry, 0, time_new_str, (int64_t)author_id2, str, hash);

		stmt_update2.bind(1, hash);
		stmt_update2.bind(2, entry);
		if (stmt_update2.exec() != 1) throw internal_err(SLS_WHERE);
		stmt_update2.reset();

		// insert into "entry_authors", contrib 5min
		SQLite::Statement stmt_insert3(db_rw,
			R"(INSERT OR REPLACE INTO "entry_authors" ("entry", "author", "contrib", "last_backup") VALUES (?,?,5,?);)");
		stmt_insert3.bind(1, entry);
		stmt_insert3.bind(2, (int64_t)author_id2);
		stmt_insert3.bind(3, hash); // last_backup
		stmt_insert3.exec(); stmt_insert3.reset();

		// update "authors.contrib", add 5min
		stmt_update4.bind(1, (int64_t)author_id2);
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
	if (author_id2 == author_id_last) {
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
		str_add_del(char_add, char_del, prev_content, str, "recycle/");

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
		backup_replace_version(db_backup, last_backup_id, str, hash, &prev_content);

		// update "entry_authors.last_backup"
		SQLite::Statement stmt_update5(db_rw,
			R"(UPDATE "entry_authors" SET "last_backup"=? WHERE "entry"=? AND "author"=?;)");
		stmt_update5.bind(1, hash); // last_backup
		stmt_update5.bind(2, entry);
		stmt_update5.bind(3, (int64_t)author_id2);
		Long changed = stmt_update5.exec();
		if (changed != 1) {
			if (changed != 0) throw scan_err(SLS_WHERE);
			stmt_insert5.bind(1, entry); stmt_insert5.bind(2, (int64_t)author_id2);
			stmt_insert5.bind(3, hash);
			stmt_insert5.exec(); stmt_insert5.reset();
		}
		stmt_update5.reset();
		db_log_print(u8"更新 entry_authors.last_backup");
	}
	else { // !replace  (new backup)
		Str prev_content = backup_restore_str_by_id(last_backup_id, db_backup);
		str_add_del(char_add, char_del, prev_content, str, "recycle/");

		// update db
		time_new_str = time_t2str(time_new, "%Y%m%d%H%M");
		stmt_insert.bind(1, hash);
		stmt_insert.bind(2, time_new_str);
		stmt_insert.bind(3, (int64_t)author_id2);
		stmt_insert.bind(4, entry);
		stmt_insert.bind(5, (int64_t)char_add);
		stmt_insert.bind(6, (int64_t)char_del);
		stmt_insert.bind(7, hash_last);
		stmt_insert.exec(); stmt_insert.reset();

		clear(sb) << u8"插入新的 history 记录： hash=" << hash << ", time=" << time_new_str << ", author=" << author_id2
			<< ", entry=" << entry << ", add=" << char_add << ", del=" << char_del << ", last=" << hash_last;
		db_log_print(sb);
		backup_append_version(db_backup, entry, last_backup_id, time_new_str,
			(int64_t)author_id2, str, hash);

		// update "entry_authors", add 5min
		stmt_update3.bind(1, hash); // last_backup
		stmt_update3.bind(2, entry);
		stmt_update3.bind(3, (int64_t)author_id2);
		Long changed = stmt_update3.exec();
		if (changed != 1) {
			if (changed != 0) throw scan_err(SLS_WHERE);
			stmt_insert5.bind(1, entry); stmt_insert5.bind(2, (int64_t)author_id2);
			stmt_insert5.bind(3, hash);
			stmt_insert5.exec(); stmt_insert5.reset();
		}
		stmt_update3.reset();
		db_log_print(u8"更新 entry_authors.contrib += 5 和 entry_authors.last_backup");

		// update "authors.contrib", add 5min
		stmt_update4.bind(1, (int64_t)author_id2);
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
