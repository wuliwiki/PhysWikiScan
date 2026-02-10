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
	Str timestamp;
	int64_t author_id = 0;
	Str article_id;
	int64_t size = 0;
	Str hash;
	Str diff_json;
	int64_t prev_ver = 0;
	bool prev_null = true;
};

inline void backup_apply_diff(Str_O out, const vector<tuple<size_t, size_t, Str>> &diff)
{
	for (auto it = diff.rbegin(); it != diff.rend(); ++it)
		out.replace(get<0>(*it), get<1>(*it), get<2>(*it));
}

inline bool backup_load_record(BackupRecord &rec, SQLite::Database &db, int64_t id)
{
	SQLite::Statement stmt(db,
		R"(SELECT "id", "timestamp", "author_id", "article_id", "size", "hash", "prev_ver", "diff"
		   FROM "backup_files" WHERE "id"=?;)");
	stmt.bind(1, id);
	if (!stmt.executeStep())
		return false;
	rec.id = stmt.getColumn(0).getInt64();
	rec.timestamp = stmt.getColumn(1).getString();
	rec.author_id = stmt.getColumn(2).getInt64();
	rec.article_id = stmt.getColumn(3).getString();
	rec.size = stmt.getColumn(4).getInt64();
	rec.hash = stmt.getColumn(5).getString();
	rec.prev_null = stmt.getColumn(6).isNull();
	rec.prev_ver = rec.prev_null ? 0 : stmt.getColumn(6).getInt64();
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
		if (rec.prev_null)
			break;
		cur = rec.prev_ver;
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

inline Str backup_restore_str(Str_I timestamp, int64_t author_id, Str_I article_id, SQLite::Database &db)
{
	SQLite::Statement stmt(db,
		R"(SELECT "id" FROM "backup_files"
		   WHERE "timestamp"=? AND "author_id"=? AND "article_id"=?;)");
	stmt.bind(1, timestamp);
	stmt.bind(2, author_id);
	stmt.bind(3, article_id);
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

inline Str backup_add_record(int64_t author_id, Str_I article_id, Str_I new_ver, SQLite::Database &db)
{
	if (!is_valid(new_ver))
		throw runtime_error("backup_add_record(): invalid UTF-8");

	Str timestamp = time_str("%Y%m%d%H%M");
	SQLite::Statement stmt_exist(db,
		R"(SELECT 1 FROM "backup_files"
		   WHERE "timestamp"=? AND "author_id"=? AND "article_id"=? LIMIT 1;)");
	while (true) {
		stmt_exist.bind(1, timestamp);
		stmt_exist.bind(2, author_id);
		stmt_exist.bind(3, article_id);
		bool exists = stmt_exist.executeStep();
		stmt_exist.reset();
		if (!exists)
			break;
		time_t t = str2time_t(timestamp);
		t += 60;
		timestamp = time_t2str(t, "%Y%m%d%H%M");
	}

	SQLite::Statement stmt_last(db,
		R"(SELECT "id" FROM "backup_files"
		   WHERE "article_id"=?
		   ORDER BY "timestamp" DESC, "author_id" DESC LIMIT 1;)");
	stmt_last.bind(1, article_id);
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
		R"(INSERT INTO "backup_files" ("timestamp", "author_id", "article_id", "size", "hash", "prev_ver", "diff")
		   VALUES (?, ?, ?, ?, ?, ?, ?);)");
	insert_stmt.bind(1, timestamp);
	insert_stmt.bind(2, author_id);
	insert_stmt.bind(3, article_id);
	insert_stmt.bind(4, static_cast<int64_t>(new_ver.size()));
	insert_stmt.bind(5, hash);
	if (prev_id == 0)
		insert_stmt.bind(6);
	else
		insert_stmt.bind(6, prev_id);
	insert_stmt.bind(7, diff_json);
	insert_stmt.exec();
	return timestamp;
}
