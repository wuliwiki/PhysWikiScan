#pragma once
#include <sqlite3.h>
#include "../global.h"

namespace slisc {

// check if an entry exists
inline bool exist(sqlite3* db, Str_I table, Str_I field, Str_I text)
{
    Str cmd = "SELECT 1 FROM " + table + " WHERE " + field + " = ?;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, cmd.c_str(), -1, &stmt, NULL) != SQLITE_OK)
        throw Str("sqlite3_prepare_v2(): ") + sqlite3_errmsg(db);
    sqlite3_bind_text(stmt, 1, text.c_str(), -1, SQLITE_TRANSIENT);

    int ret = sqlite3_step(stmt);
    bool res;
    if (ret == SQLITE_ROW)
        res = true;
    else if (ret == SQLITE_DONE)
        res = false;
    else
        throw Str("sqlite3_prepare_v2(): ") + sqlite3_errmsg(db);
    sqlite3_finalize(stmt);
    return res;
}

// count 
inline bool count(sqlite3* db, Str_I table, Str_I field, Str_I text)
{
    Str cmd = "SELECT COUNT(*) FROM " + table + " WHERE " + field + " = ?;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, cmd.c_str(), -1, &stmt, NULL) != SQLITE_OK)
        throw Str("count():sqlite3_prepare_v2(): ") + sqlite3_errmsg(db);
    sqlite3_bind_text(stmt, 1, text.c_str(), -1, SQLITE_TRANSIENT);

    int ret = sqlite3_step(stmt);
    int count = -1;
    if (ret == SQLITE_ROW)
        count = sqlite3_column_int(stmt, 0);
    else
        throw Str("count():sqlite3_step(): ") + sqlite3_errmsg(db);
    sqlite3_finalize(stmt);
    return count;
}

// get single column
inline void get_column(vecStr_O data, sqlite3* db, Str_I table, Str_I field)
{
	data.clear();
    sqlite3_stmt* stmt;
    Str str = "SELECT " + field + " FROM " + table;
    if (sqlite3_prepare_v2(db, str.c_str(), -1, &stmt, NULL) != SQLITE_OK)
        throw Str("get_column():sqlite3_prepare_v2(): ") + sqlite3_errmsg(db);
    int ret;
	while ((ret = sqlite3_step(stmt)) == SQLITE_ROW)
		data.push_back((char*)sqlite3_column_text(stmt, 0));
    if (ret != SQLITE_DONE)
        throw Str("get_column():sqlite3_step(): ") + sqlite3_errmsg(db);
    sqlite3_finalize(stmt);
}

// get single row
inline void get_row(vecStr_O data, sqlite3* db, Str_I table, Str_I field, Str_I val, vecStr_I fields_out)
{
    data.clear();
    sqlite3_stmt* stmt;
    Str str = "SELECT ";
    for (auto &field_out : fields_out)
        str += field_out + ',';
    str.pop_back();
    str += " FROM " + table + " WHERE " + field + " = '" + val + "';";
    if (sqlite3_prepare_v2(db, str.c_str(), -1, &stmt, NULL) != SQLITE_OK)
        throw Str("get_row(vecStr):sqlite3_prepare_v2(): ") + sqlite3_errmsg(db);
	if (sqlite3_step(stmt) == SQLITE_ROW) {
        for (Long i = 0; i < fields_out.size(); ++i)
		    data.push_back((char*)sqlite3_column_text(stmt, i));
    }
    else
        throw Str("get_row(vecStr):sqlite3_step(): ") + sqlite3_errmsg(db);
    sqlite3_finalize(stmt);
}

inline void get_row(vecLong_O data, sqlite3* db, Str_I table, Str_I field, Str_I val, vecStr_I fields_out)
{
    data.clear();
    sqlite3_stmt* stmt;
    Str str = "SELECT ";
    for (auto &field_out : fields_out)
        str += field_out + ',';
    str.pop_back();
    str += " FROM " + table + " WHERE " + field + " = '" + val + "';";
    if (sqlite3_prepare_v2(db, str.c_str(), -1, &stmt, NULL) != SQLITE_OK)
        throw Str("get_row(vecLong):sqlite3_prepare_v2(): ") + sqlite3_errmsg(db);
	if (sqlite3_step(stmt) == SQLITE_ROW) {
        for (Long i = 0; i < fields_out.size(); ++i)
		    data.push_back(sqlite3_column_int64(stmt, i));
    }
    else
        throw Str("get_row(vecLong):sqlite3_step(): ") + sqlite3_errmsg(db);
    sqlite3_finalize(stmt);
}

} // namespace slisc
