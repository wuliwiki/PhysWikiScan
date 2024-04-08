// delete a table (or sub table satisfying a condition) and insert new records are slow, when there are lots of unchanged records
// this is a optimized version

#include "../str/str.h"
#include "../util/STL_util.h"
#include <forward_list>
#include <SQLiteCpp/Statement.h>
#include <SQLiteCpp/Database.h>
#include <SQLiteCpp/Transaction.h>

namespace slisc  {

// SQL table cell value (unsafe!)
struct SQLval {
	char type;
	union {
        int64_t i;
        const char *s;
    };
	SQLval &operator=(Str_I val) { type = 's'; s = val.c_str(); return *this; };
	SQLval &operator=(Long_I val) { type = 'i'; i = val; return *this; };
	bool operator==(const SQLval &rhs) const {
		if (type == 's')
			return strcmp(s, rhs.s) == 0;
		else
			return i == rhs.i;
	}
};

typedef vector<SQLval> vecSQLval;

} // namespace slisc

namespace std {

template <>
struct hash<slisc::SQLval> {
	size_t operator()(const SQLval &val) {
		if (val.type == 's')
			return slisc::hash_cstr(val.s);
		else
			return std::hash<int64_t>{}(val.i);
	}
};

} // namespace std

namespace slisc {

// queries for update_sqlite_table()
inline void update_sqlite_table_query(
		Str_O q_select, Str_O q_insert, Str_O q_update, Str_O q_delete,
		Str_I table_name, Str_I condition, vecStr_I field_names, Long_I Npk
) {
	// SELECT statement
	clear(q_select) << "SELECT ";
	for (auto &field : field_names)
		q_select << '"' << field << "\", ";
	q_select.resize(q_select.size()-2);
	q_select << " FROM \"" << table_name << '"';
	if (!condition.empty())
		q_select << " WHERE " << condition << ';';

	// INSERT statement
	clear(q_insert) << "INSERT INTO \"" << table_name << "\" (";
	for (auto &field : field_names)
		q_insert << '"' << field << "\", ";
	q_insert.resize(q_insert.size()-2);
	q_insert << ") VALUES (";
	for (Long i = 0; i < size(field_names); ++i)
		q_insert << "?, ";
	q_insert.resize(q_insert.size()-2);
	q_insert << ");";

	// UPDATE statement
	if (Npk < size(field_names)) {
		clear(q_update) << "UPDATE \"" << table_name << "\" SET ";
		for (Long i = Npk; i < size(field_names); ++i)
			q_update << '"' << field_names[i] << "\"=?, ";
		q_update.resize(q_update.size()-2);
		q_update << " WHERE ";
		for (Long i = 0; i < Npk; ++i)
			q_update << '"' << field_names[i] << "\"=? AND ";
		q_update.resize(q_update.size()-5);
		q_update << ';';
	}

	// DELETE statement
	clear(q_delete) << "DELETE FROM \"" << table_name << "\" WHERE ";
	for (Long i = 0; i < Npk; ++i)
		q_delete << '"' << field_names[i] << "\"=? AND ";
	q_delete.resize(q_delete.size()-5);
}

inline void update_sqlite_table(
	unordered_map<vecSQLval, vecSQLval> &data, // (primary keys) -> (other fields)
	Str_I table_name,
	Str_I condition, // the SQL statement after "WHERE"
	vecStr_I field_names, // field names of `data`, in order
	Long_I Npk, // first Npk of field_names are primary keys
	SQLite::Database &db_rw,
	void (*callback) ( // callback for db change
		char act, // [i] insert [u] update [d] delete [a] all deleted (cols_changed[0] will be number of deleted records)
		Str_I table, vecStr_I field_names, const pair<vecSQLval, vecSQLval> &row,
		vecLong_I cols_changed, const vecSQLval &old_vals // row.second(cols_changed[j]) was old_vals[j]
	) = nullptr
) {
	vecSQLval old_vals;
	vecLong cols_changed;

	if (data.empty()) {
		// delete all records
		clear(sb) << "DELETE FROM \"" << table_name << '"';
		if (!condition.empty())
			sb << " WHERE " << condition;
		Long Ndel = db_rw.exec(sb);
		if (callback) {
			cols_changed.push_back(Ndel);
			callback('a', table_name, field_names, make_pair(vecSQLval(), vecSQLval()), cols_changed, old_vals);
		}
		return;
	}

	Long Nval = size(field_names) - Npk;
	Str q_select, q_insert, q_update, q_delete;
	update_sqlite_table_query(q_select, q_insert, q_update, q_delete,
		table_name, condition, field_names, Npk);
	SQLite::Statement stmt_select(db_rw, q_select);
	SQLite::Statement stmt_insert(db_rw, q_insert);
	SQLite::Statement stmt_update(db_rw, q_update);
	SQLite::Statement stmt_delete(db_rw, q_delete);

	pair<vecSQLval, vecSQLval> old_row;
	vecSQLval key(Npk);
	std::forward_list<Str> vs; // temporary strings for SQLval to point to

	// field types
	auto &row0 = *data.begin();
	for (Long i = 0; i < Npk; ++i)
		key[i].type = row0.first[i].type;

	while (stmt_select.executeStep()) {
		for (Int j = 0; j < Npk; ++j) {
			if (key[j].type == 's') {
				vs.push_front(stmt_select.getColumn(j).getString());
				key[j] = vs.front();
			}
			else
				key[j] = stmt_select.getColumn(j).getInt64();
		}
		auto p_row = data.find(key);
		if (p_row == data.end()) {
			// key not found, deleted
			for (Long j = 0; j < Npk; ++j) {
				if (key[j].type == 's')
					stmt_delete.bind(j+1, key[j].s);
				else
					stmt_delete.bind(j+1, key[j].i);
			}
			if (stmt_delete.exec() != 1)
				SLS_ERR(SLS_WHERE);
			stmt_delete.reset();
			if (callback)
				callback('d', table_name, field_names, row0, cols_changed, old_vals);
			continue;
		}
		// check for change
		if (Nval > 0) {
			auto &vals = p_row->second;
			for (Long j = 0; j < Nval; ++j) {
				Long col = Npk+j;
				if (vals[j].type == 's') {
					vs.push_front(stmt_select.getColumn(col).getString());
					if (vals[j].s != vs.front()) {
						cols_changed.push_back(j);
						old_vals.emplace_back();
						old_vals.back() = vs.front();
					}
				}
				else {
					Long old_i = stmt_select.getColumn(col).getInt64();
					if (vals[j].i != old_i) {
						cols_changed.push_back(j);
						old_vals.emplace_back();
						old_vals.back() = old_i;
					}
				}
			}
			if (!cols_changed.empty()) {
				// update db record
				for (Long j = 0; j < Nval; ++j) {
					if (vals[j].type == 's')
						stmt_update.bind(j+1, vals[j].s);
					else
						stmt_update.bind(j+1, vals[j].i);
				}
				for (Long j = 0; j < Npk; ++j) {
					if (key[j].type == 's')
						stmt_update.bind(Nval+j+1, key[j].s);
					else
						stmt_update.bind(Nval+j+1, key[j].i);
				}
				if (stmt_update.exec() != 1) SLS_ERR(SLS_WHERE);
				stmt_update.reset();
				if (callback)
					callback('u', table_name, field_names, *p_row, cols_changed, old_vals);
			}
		}
		data.erase(p_row);
		cols_changed.clear();
		vs.clear();
	}
	stmt_select.reset();

	// new records
	for (auto &row : data) {
		auto &key = row.first; auto &val = row.second;
		for (Long j = 0; j < Npk; ++j) {
			if (key[j].type == 's')
				stmt_insert.bind(j+1, key[j].s);
			else
				stmt_insert.bind(j+1, key[j].i);
		}
		for (Long j = 0; j < Nval; ++j) {
			Long col = Npk+j+1;
			if (val[j].type == 's')
				stmt_insert.bind(col, val[j].s);
			else
				stmt_insert.bind(col, val[j].i);
		}
		stmt_insert.exec(); stmt_insert.reset();
		if (callback)
			callback('i', table_name, field_names, row, cols_changed, old_vals);
	}
}

} // namespace
