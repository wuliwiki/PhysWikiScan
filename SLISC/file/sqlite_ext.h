#pragma once
#include <sqlite3.h>
#include "../global.h"

namespace slisc {
inline void get_column_str(vecStr data, sqlite3* db, Str_I table, Str_I field)
{
	data.clear();
    sqlite3_stmt* stmt_get_entry;
    Str str = "SELECT " + field + " FROM " + table;
    if (sqlite3_prepare_v2(db, str.c_str(), -1, &stmt_get_entry, NULL) != SQLITE_OK)
        throw Str("sqlite3_prepare_v2(): ") + sqlite3_errmsg(db);
	while (sqlite3_step(stmt_get_entry) == SQLITE_ROW)
		data.push_back((char*)sqlite3_column_text(stmt_get_entry, 0));
}
}
