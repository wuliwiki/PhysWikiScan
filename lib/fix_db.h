#pragma once
#include "sqlite_db.h"

// fix foreign keys in table definitions
inline Long fix_foreign_key_occupied(
		const unordered_map<Str, unordered_map<Long, unordered_map<Long, Str>>> &table_fkid_rowid_parent,
		SQLite::Database &db_rw
) {
	Long N = 0;

	// fix occupied.entry -> entries.id
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
			clear(sb) << u8"occupied.entry->entries 外键不存在（将删除）：" << entry << " 作者：" << author_id;
			scan_log_warn(sb);

			stmt_delete.bind(1, rowid);
			if (stmt_delete.exec() != 1) throw internal_err(SLS_WHERE);
			stmt_delete.reset();
			++N;
		}
	}

	// fix occupied.author -> authors.id
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
			clear(sb) << u8"occupied.entry->authors 外键不存在（将删除）：" << entry << " 作者：" << author_id;
			scan_log_warn(sb);

			stmt_delete.bind(1, rowid);
			if (stmt_delete.exec() != 1) throw internal_err(SLS_WHERE);
			stmt_delete.reset();
			++N;
		}
	}
	return N;
}

inline Long fix_foreign_key_figures(
		const unordered_map<Str, unordered_map<Long, unordered_map<Long, Str>>> &table_fkid_rowid_parent,
		SQLite::Database &db_rw
) {
	Long N = 0;

	// fix figures.image -> images.hash
	if (table_fkid_rowid_parent.count("figures") && table_fkid_rowid_parent.at("figures").count(2)) {
		SQLite::Statement stmt_select(db_rw,
			R"(SELECT "id", "entry", "deleted", "image" FROM "figures" WHERE "rowid"=?;)");
		SQLite::Statement stmt_delete(db_rw,
			R"(DELETE FROM "figures" WHERE "rowid"=?;)");
		for (auto &e : table_fkid_rowid_parent.at("figures").at(2)) {
			int64_t rowid = e.first;
			stmt_select.bind(1, rowid);
			stmt_select.executeStep();
			const Str &fig_id = stmt_select.getColumn(0);
			const Str &entry = stmt_select.getColumn(1);
			bool deleted = stmt_select.getColumn(2).getInt();
			const Str &image_hash = stmt_select.getColumn(3);
			stmt_select.reset();

			if (!deleted) {
				clear(sb) << u8"找不到 figures.image，且 figures 未标记删除（将忽略）：" << fig_id << '.' << image_hash;
				scan_log_warn(sb);
				continue;
			}

			clear(sb) << u8"figures.image->images.hash 外键不存在（已标记删除）（将彻底删除）：" << fig_id << '.' << image_hash << " 文章 " << entry;
			scan_log_warn(sb);
			stmt_delete.bind(1, rowid);
			if (stmt_delete.exec() != 1) throw internal_err(SLS_WHERE);
			stmt_delete.reset();
			++N;
		}
	}

	// fix figures.entry -> entries.id
	if (table_fkid_rowid_parent.count("figures") && table_fkid_rowid_parent.at("figures").count(3)) {
		SQLite::Statement stmt_select(db_rw,
			R"(SELECT "id", "entry", "deleted", "image" FROM "figures" WHERE "rowid"=?;)");
		SQLite::Statement stmt_update(db_rw,
			R"(UPDATE "figures" SET "entry"='' WHERE "rowid"=?;)");
		for (auto &e : table_fkid_rowid_parent.at("figures").at(3)) {
			int64_t rowid = e.first;
			stmt_select.bind(1, rowid);
			stmt_select.executeStep();
			const Str &fig_id = stmt_select.getColumn(0);
			const Str &entry = stmt_select.getColumn(1);
			// bool deleted = stmt_select.getColumn(2).getInt();
			const Str &image_hash = stmt_select.getColumn(3);
			stmt_select.reset();

			clear(sb) << u8"figures.entry->entries.id 外键不存在（将改为 ''，请人工判断是否应该删除）："
				<< fig_id << '.' << image_hash << " 文章 " << entry;
			scan_log_warn(sb);
			stmt_update.bind(1, rowid);
			stmt_update.exec(); stmt_update.reset();
			++N;
		}
	}

	// TODO: 检查其他常见外键

	return N;
}

inline void fix_foreign_keys(SQLite::Database &db_rw)
{
	unordered_map<Str, unordered_map<Long, unordered_map<Long, Str>>> table_fkid_rowid_parent;
	while (1) {
		check_existing_foreign_keys(table_fkid_rowid_parent, db_rw);
		Long N = fix_foreign_key_occupied(table_fkid_rowid_parent, db_rw);
		N += fix_foreign_key_figures(table_fkid_rowid_parent, db_rw);
		if (N == 0) break;
	}
}
