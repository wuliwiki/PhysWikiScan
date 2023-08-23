#pragma once
#include "sqlite_db.h"

// fix foreign keys in table definitions
inline void fix_foreign_key_occupied(
		const unordered_map<Str, unordered_map<Long, unordered_map<Long, Str>>> &table_fkid_rowid_parent,
		SQLite::Database &db_rw
) {
	// fix occupied.entry -> entries
	if (table_fkid_rowid_parent.count("occupied") && table_fkid_rowid_parent.at("occupied").count(1)) {
		SQLite::Statement stmt_select(db_rw,
									  R"(SELECT "entry", "author" FROM "occupied" WHERE "rowid"=?;)");
		SQLite::Statement stmt_delete(db_rw,
									  R"(DELETE FROM "occupied" WHERE "rowid"=?;)");
		for (auto &e : table_fkid_rowid_parent.at("occupied").at(1)) {
			int64_t rowid = e.first;
			stmt_select.bind(1, rowid);
			stmt_select.executeStep();
			const Str &entry = stmt_select.getColumn(0);
			int64_t author_id = stmt_select.getColumn(1);
			stmt_select.reset();
			clear(sb) << u8"occupied.entry->entries 外键冲突（将删除）：" << entry << " 作者：" << author_id;
			scan_warn(sb);

			stmt_delete.bind(1, rowid);
			stmt_delete.exec(); stmt_delete.reset();
		}
	}

	// fix occupied.entry -> authors
	if (table_fkid_rowid_parent.count("occupied") && table_fkid_rowid_parent.at("occupied").count(0)) {
		SQLite::Statement stmt_select(db_rw,
									  R"(SELECT "entry", "author" FROM "occupied" WHERE "rowid"=?;)");
		SQLite::Statement stmt_delete(db_rw,
									  R"(DELETE FROM "occupied" WHERE "rowid"=?;)");
		for (auto &e : table_fkid_rowid_parent.at("occupied").at(0)) {
			int64_t rowid = e.first;
			stmt_select.bind(1, rowid);
			stmt_select.executeStep();
			const Str &entry = stmt_select.getColumn(0);
			int64_t author_id = stmt_select.getColumn(1);
			stmt_select.reset();
			clear(sb) << u8"occupied.entry->authors 外键冲突（将删除）：" << entry << " 作者：" << author_id;
			scan_warn(sb);

			stmt_delete.bind(1, rowid);
			stmt_delete.exec(); stmt_delete.reset();
		}
	}
}

inline void fix_foreign_key_figures(
		const unordered_map<Str, unordered_map<Long, unordered_map<Long, Str>>> &table_fkid_rowid_parent,
		SQLite::Database &db_rw
) {
	// fix occupied.entry -> entries
	if (table_fkid_rowid_parent.count("figures") && table_fkid_rowid_parent.at("figures").count(0)) {
		// SQLite::Statement stmt_select(db_rw, R"()");
		throw internal_err(u8"该功能正在拼命开发……");
	}
}

inline void fix_foreign_keys(SQLite::Database &db_rw)
{
	unordered_map<Str, unordered_map<Long, unordered_map<Long, Str>>> table_fkid_rowid_parent;
	check_existing_foreign_keys(table_fkid_rowid_parent, db_rw);
	fix_foreign_key_occupied(table_fkid_rowid_parent, db_rw);
	fix_foreign_key_figures(table_fkid_rowid_parent, db_rw);
}
