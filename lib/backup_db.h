#pragma once

#include "../SLISC/str/str_diff_patch2.h"
#include "../SLISC/util/sha1sum.h"
#include "../SLISC/util/time.h"
#include <SQLiteCpp/Database.h>
#include <SQLiteCpp/Statement.h>
#include <cstdint>
#include <stdexcept>

struct BackupRecord {
	int64_t id = 0;
	Str time;
	int64_t author = 0;
	Str entry;
	int64_t size = 0;
	Str hash;
	Str diff_json;
	int64_t last_id = 0;
	bool last_null = true;
};

inline void backup_apply_diff(Str_O out, const vector<tuple<size_t, size_t, Str>> &diff)
{
	for (auto it = diff.rbegin(); it != diff.rend(); ++it)
		out.replace(get<0>(*it), get<1>(*it), get<2>(*it));
}

inline bool backup_load_record(BackupRecord &rec, SQLite::Database &db, int64_t id)
{
	SQLite::Statement stmt(db,
		R"(SELECT "id", "time", "author", "entry", "size", "hash", "last_id", "diff"
		   FROM "backup_files" WHERE "id"=?;)");
	stmt.bind(1, id);
	if (!stmt.executeStep())
		return false;
	rec.id = stmt.getColumn(0).getInt64();
	rec.time = stmt.getColumn(1).getString();
	rec.author = stmt.getColumn(2).getInt64();
	rec.entry = stmt.getColumn(3).getString();
	rec.size = stmt.getColumn(4).getInt64();
	rec.hash = stmt.getColumn(5).getString();
	rec.last_null = stmt.getColumn(6).isNull();
	rec.last_id = rec.last_null ? 0 : stmt.getColumn(6).getInt64();
	rec.diff_json = stmt.getColumn(7).getString();
	return true;
}

inline Str backup_restore_str_by_id(int64_t id, SQLite::Database &db)
{
	if (id <= 0)
		throw runtime_error("backup_restore_str_by_id(): invalid id");

	std::vector<BackupRecord> chain;
	int64_t cur = id;
	while (cur != 0) {
		BackupRecord rec;
		if (!backup_load_record(rec, db, cur))
			throw runtime_error("backup_restore_str_by_id(): missing record");
		chain.push_back(rec);
		if (rec.last_null)
			break;
		cur = rec.last_id;
	}

	Str content;
	content.reserve(static_cast<size_t>(chain.front().size));
	for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
		vector<tuple<size_t, size_t, Str>> diff;
		str_diff_deserialize(diff, it->diff_json);
		backup_apply_diff(content, diff);
	}

	const BackupRecord &target = chain.front();
	if (target.size != static_cast<int64_t>(content.size()))
		throw runtime_error("backup_restore_str_by_id(): size mismatch");
	Str hash = sha1sum(content).substr(0, 16);
	if (hash != target.hash)
		throw runtime_error("backup_restore_str_by_id(): hash mismatch");
	if (!is_valid(content))
		throw runtime_error("backup_restore_str_by_id(): invalid UTF-8");
	return content;
}

inline Str backup_restore_str(Str_I time, int64_t author, Str_I entry, SQLite::Database &db)
{
	SQLite::Statement stmt(db,
		R"(SELECT "id" FROM "backup_files"
		   WHERE "time"=? AND "author"=? AND "entry"=?;)");
	stmt.bind(1, time);
	stmt.bind(2, author);
	stmt.bind(3, entry);
	if (!stmt.executeStep())
		throw runtime_error("backup_restore_str(): record not found");
	const int64_t id = stmt.getColumn(0).getInt64();
	return backup_restore_str_by_id(id, db);
}

inline Str backup_diff_json_by_id(int64_t id1, int64_t id2, SQLite::Database &db)
{
	Str str1 = backup_restore_str_by_id(id1, db);
	Str str2 = backup_restore_str_by_id(id2, db);
	vector<tuple<size_t, size_t, Str>> diff;
	str_diff(diff, str1, str2);
	Str json;
	str_diff_serialize(json, diff);
	return json;
}

inline Str backup_add_record(int64_t author, Str_I entry, Str_I new_ver, SQLite::Database &db)
{
	if (!is_valid(new_ver))
		throw runtime_error("backup_add_record(): invalid UTF-8");

	Str time = time_str("%Y%m%d%H%M");
	SQLite::Statement stmt_exist(db,
		R"(SELECT 1 FROM "backup_files"
		   WHERE "time"=? AND "author"=? AND "entry"=? LIMIT 1;)");
	while (true) {
		stmt_exist.bind(1, time);
		stmt_exist.bind(2, author);
		stmt_exist.bind(3, entry);
		bool exists = stmt_exist.executeStep();
		stmt_exist.reset();
		if (!exists)
			break;
		time_t t = str2time_t(time);
		t += 60;
		time = time_t2str(t, "%Y%m%d%H%M");
	}

	SQLite::Statement stmt_last(db,
		R"(SELECT "id" FROM "backup_files"
		   WHERE "entry"=?
		   ORDER BY "time" DESC, "author" DESC LIMIT 1;)");
	stmt_last.bind(1, entry);
	int64_t prev_id = 0;
	Str prev_content;
	if (stmt_last.executeStep()) {
		prev_id = stmt_last.getColumn(0).getInt64();
		prev_content = backup_restore_str_by_id(prev_id, db);
	}

	vector<tuple<size_t, size_t, Str>> diff;
	str_diff(diff, prev_content, new_ver);
	Str diff_json;
	str_diff_serialize(diff_json, diff);

	const Str hash = sha1sum(new_ver).substr(0, 16);
	SQLite::Statement insert_stmt(db,
		R"(INSERT INTO "backup_files" ("time", "author", "entry", "size", "hash", "last_id", "diff")
		   VALUES (?, ?, ?, ?, ?, ?, ?);)");
	insert_stmt.bind(1, time);
	insert_stmt.bind(2, author);
	insert_stmt.bind(3, entry);
	insert_stmt.bind(4, static_cast<int64_t>(new_ver.size()));
	insert_stmt.bind(5, hash);
	if (prev_id == 0)
		insert_stmt.bind(6);
	else
		insert_stmt.bind(6, prev_id);
	insert_stmt.bind(7, diff_json);
	insert_stmt.exec();
	return time;
}

// find the next backup record id (throws if more than one)
inline int64_t backup_next_id(int64_t id, SQLite::Database &db_backup)
{
	SQLite::Statement stmt(db_backup,
		R"(SELECT "id" FROM "backup_files" WHERE "last_id"=?;)");
	stmt.bind(1, id);
	int64_t next_id = 0;
	while (stmt.executeStep()) {
		int64_t candidate = stmt.getColumn(0).getInt64();
		if (next_id != 0)
			throw internal_err(u8"backup_files 出现多个后继记录：id=" + num2str((Long)id));
		next_id = candidate;
	}
	return next_id;
}

// relink a next record to a new predecessor and verify recovery unchanged
inline void backup_relink_next(SQLite::Database &db_backup, int64_t next_id, int64_t new_last_id,
	Str_I prev_content)
{
	Str next_content = backup_restore_str_by_id(next_id, db_backup);
	vector<tuple<size_t, size_t, Str>> diff;
	str_diff(diff, prev_content, next_content);
	Str diff_json;
	str_diff_serialize(diff_json, diff);

	SQLite::Statement stmt_update(db_backup,
		R"(UPDATE "backup_files" SET "last_id"=?, "diff"=? WHERE "id"=?;)");
	if (new_last_id == 0)
		stmt_update.bind(1);
	else
		stmt_update.bind(1, new_last_id);
	stmt_update.bind(2, diff_json);
	stmt_update.bind(3, next_id);
	if (stmt_update.exec() != 1) throw internal_err(SLS_WHERE);

	Str next_content_after = backup_restore_str_by_id(next_id, db_backup);
	if (next_content_after != next_content)
		throw internal_err(u8"backup_files 重新链接后内容变化：id=" + num2str((Long)next_id));
}

// delete a backup record, relinking its successor if needed
inline void backup_delete_record(SQLite::Database &db_backup, int64_t backup_id)
{
	BackupRecord rec;
	if (!backup_load_record(rec, db_backup, backup_id))
		throw internal_err(u8"backup_files 中找不到记录：id=" + num2str((Long)backup_id));

	const int64_t next_id = backup_next_id(backup_id, db_backup);
	const Str prev_content = rec.last_null ? Str() : backup_restore_str_by_id(rec.last_id, db_backup);
	if (next_id != 0)
		backup_relink_next(db_backup, next_id, rec.last_null ? 0 : rec.last_id, prev_content);

	SQLite::Statement stmt_delete(db_backup,
		R"(DELETE FROM "backup_files" WHERE "id"=?;)");
	stmt_delete.bind(1, backup_id);
	if (stmt_delete.exec() != 1) throw internal_err(SLS_WHERE);
}

// replace an existing backup version and keep the chain consistent
inline void backup_replace_version(SQLite::Database &db_backup, int64_t backup_id, Str_I new_content,
	Str_I new_hash = Str(), const Str *prev_content_override = nullptr)
{
	if (!is_valid(new_content))
		throw internal_err(u8"backup_replace_version(): invalid UTF-8");

	BackupRecord rec;
	if (!backup_load_record(rec, db_backup, backup_id))
		throw internal_err(u8"backup_replace_version(): record not found");

	Str prev_content_store;
	const Str *prev_content = prev_content_override;
	if (!prev_content) {
		if (!rec.last_null)
			prev_content_store = backup_restore_str_by_id(rec.last_id, db_backup);
		prev_content = &prev_content_store;
	}

	Str hash = new_hash;
	if (hash.empty())
		hash = sha1sum(new_content).substr(0, 16);
	else if (hash != sha1sum(new_content).substr(0, 16))
		throw internal_err(u8"backup_replace_version(): hash mismatch");

	vector<tuple<size_t, size_t, Str>> diff;
	str_diff(diff, *prev_content, new_content);
	Str diff_json;
	str_diff_serialize(diff_json, diff);

	SQLite::Statement stmt_update(db_backup,
		R"(UPDATE "backup_files" SET "size"=?, "hash"=?, "diff"=? WHERE "id"=?;)");
	stmt_update.bind(1, (int64_t)new_content.size());
	stmt_update.bind(2, hash);
	stmt_update.bind(3, diff_json);
	stmt_update.bind(4, backup_id);
	if (stmt_update.exec() != 1) throw internal_err(SLS_WHERE);

	const int64_t next_id = backup_next_id(backup_id, db_backup);
	if (next_id != 0)
		backup_relink_next(db_backup, next_id, backup_id, new_content);
}

// insert a backup version between prev and next (or append if next_id==0)
inline int64_t backup_insert_version_between(SQLite::Database &db_backup, Str_I entry, Str_I time,
	int64_t author, Str_I content, int64_t prev_id, int64_t next_id, Str_I hash = Str())
{
	if (!is_valid(content))
		throw internal_err(u8"backup_insert_version_between(): invalid UTF-8");

	if (next_id != 0) {
		BackupRecord next_rec;
		if (!backup_load_record(next_rec, db_backup, next_id))
			throw internal_err(u8"backup_insert_version_between(): next record not found");
		if (next_rec.entry != entry)
			throw internal_err(u8"backup_insert_version_between(): entry mismatch");
		const int64_t expected_prev = next_rec.last_null ? 0 : next_rec.last_id;
		if (expected_prev != prev_id)
			throw internal_err(u8"backup_insert_version_between(): prev_id mismatch");
	}
	else if (prev_id == 0) {
		SQLite::Statement stmt_exist(db_backup,
			R"(SELECT 1 FROM "backup_files" WHERE "entry"=? LIMIT 1;)");
		stmt_exist.bind(1, entry);
		if (stmt_exist.executeStep())
			throw internal_err(u8"backup_insert_version_between(): entry already has records");
	}
	else {
		const int64_t existing_next = backup_next_id(prev_id, db_backup);
		if (existing_next != 0)
			throw internal_err(u8"backup_insert_version_between(): prev_id is not tail");
		BackupRecord prev_rec;
		if (!backup_load_record(prev_rec, db_backup, prev_id))
			throw internal_err(u8"backup_insert_version_between(): prev record not found");
		if (prev_rec.entry != entry)
			throw internal_err(u8"backup_insert_version_between(): entry mismatch");
	}

	Str prev_content = (prev_id == 0) ? Str() : backup_restore_str_by_id(prev_id, db_backup);
	vector<tuple<size_t, size_t, Str>> diff;
	str_diff(diff, prev_content, content);
	Str diff_json;
	str_diff_serialize(diff_json, diff);

	Str final_hash = hash;
	if (final_hash.empty())
		final_hash = sha1sum(content).substr(0, 16);
	else if (final_hash != sha1sum(content).substr(0, 16))
		throw internal_err(u8"backup_insert_version_between(): hash mismatch");

	SQLite::Statement stmt_insert(db_backup,
		R"(INSERT INTO "backup_files" ("time", "author", "entry", "size", "hash", "last_id", "diff")
		   VALUES (?, ?, ?, ?, ?, ?, ?);)");
	stmt_insert.bind(1, time);
	stmt_insert.bind(2, author);
	stmt_insert.bind(3, entry);
	stmt_insert.bind(4, (int64_t)content.size());
	stmt_insert.bind(5, final_hash);
	if (prev_id == 0)
		stmt_insert.bind(6);
	else
		stmt_insert.bind(6, prev_id);
	stmt_insert.bind(7, diff_json);
	stmt_insert.exec();

	const int64_t new_id = db_backup.getLastInsertRowid();
	if (next_id != 0)
		backup_relink_next(db_backup, next_id, new_id, content);
	return new_id;
}

// insert a backup version directly before an existing record
inline int64_t backup_insert_version_before(SQLite::Database &db_backup, int64_t next_id, Str_I time,
	int64_t author, Str_I content, Str_I hash = Str())
{
	BackupRecord next_rec;
	if (!backup_load_record(next_rec, db_backup, next_id))
		throw internal_err(u8"backup_insert_version_before(): next record not found");
	const int64_t prev_id = next_rec.last_null ? 0 : next_rec.last_id;
	return backup_insert_version_between(db_backup, next_rec.entry, time, author, content, prev_id, next_id, hash);
}

// append a backup version at the end of an entry chain
inline int64_t backup_append_version(SQLite::Database &db_backup, Str_I entry, int64_t prev_id, Str_I time,
	int64_t author, Str_I content, Str_I hash = Str())
{
	return backup_insert_version_between(db_backup, entry, time, author, content, prev_id, 0, hash);
}
