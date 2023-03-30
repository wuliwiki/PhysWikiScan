#pragma once
#include <sqlite3.h>
#include "../arith/scalar_arith.h"
#include "../str/unicode.h"

namespace slisc {

// check foreign key existence by default
// need to run everytime db is opened
inline void check_foreign_key(sqlite3* db)
{
    char* errMsg = NULL;
    if (sqlite3_exec(db, "PRAGMA foreign_keys = ON;", NULL, NULL, &errMsg) != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", errMsg);
        Str err_msg("check_foreign_key(): sqlite3_exec(): ");
        err_msg += errMsg; sqlite3_free(errMsg);
        throw err_msg;
    }
}

// check if an entry exists
inline bool exist(sqlite3* db, Str_I table, Str_I field, Str_I text)
{
    Str cmd = "SELECT 1 FROM " + table + " WHERE " + field + " = ?;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, cmd.c_str(), -1, &stmt, NULL) != SQLITE_OK) {
        sqlite3_close(db);
        throw Str("sqlite3_prepare_v2(): ") + sqlite3_errmsg(db);
    }
    sqlite3_bind_text(stmt, 1, text.c_str(), -1, SQLITE_TRANSIENT);

    int ret = sqlite3_step(stmt);
    bool res;
    if (ret == SQLITE_ROW)
        res = true;
    else if (ret == SQLITE_DONE)
        res = false;
    else {
        sqlite3_close(db);
        throw Str("sqlite3_prepare_v2(): ") + sqlite3_errmsg(db);
    }
    sqlite3_finalize(stmt);
    return res;
}

// count 
inline bool count(sqlite3* db, Str_I table, Str_I field, Str_I text)
{
    Str cmd = "SELECT COUNT(*) FROM " + table + " WHERE " + field + " = ?;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, cmd.c_str(), -1, &stmt, NULL) != SQLITE_OK) {
        sqlite3_close(db);
        throw Str("count():sqlite3_prepare_v2(): ") + sqlite3_errmsg(db);
    }
    sqlite3_bind_text(stmt, 1, text.c_str(), -1, SQLITE_TRANSIENT);

    int ret = sqlite3_step(stmt);
    int count = -1;
    if (ret == SQLITE_ROW)
        count = sqlite3_column_int(stmt, 0);
    else {
        sqlite3_close(db);
        throw Str("count():sqlite3_step(): ") + sqlite3_errmsg(db);
    }
    sqlite3_finalize(stmt);
    return count;
}

// get an integer
inline Str get_text(sqlite3* db, Str_I table, Str_I field, Str_I val, Str_I field_out)
{
    Str ret;
    sqlite3_stmt* stmt;
    Str str = "SELECT " + field_out + " FROM " + table + " WHERE " + field + " = '" + val + "';";
    if (sqlite3_prepare_v2(db, str.c_str(), -1, &stmt, NULL) != SQLITE_OK) {
        sqlite3_close(db);
        throw Str("get(Llong):sqlite3_prepare_v2(): ") + sqlite3_errmsg(db);
    }
	if (sqlite3_step(stmt) == SQLITE_ROW)
        ret = (char*)sqlite3_column_text(stmt, 0);
    else {
        sqlite3_close(db);
        throw Str("get(Llong):sqlite3_step(): ") + sqlite3_errmsg(db);
    }
    sqlite3_finalize(stmt);
    return ret;
}

inline Llong get_int(sqlite3* db, Str_I table, Str_I field, Str_I val, Str_I field_out)
{
    Llong ret;
    sqlite3_stmt* stmt;
    Str str = "SELECT " + field_out + " FROM " + table + " WHERE " + field + " = '" + val + "';";
    if (sqlite3_prepare_v2(db, str.c_str(), -1, &stmt, NULL) != SQLITE_OK) {
        sqlite3_close(db);
        throw Str("get(Llong):sqlite3_prepare_v2(): ") + sqlite3_errmsg(db);
    }
	if (sqlite3_step(stmt) == SQLITE_ROW)
        ret = sqlite3_column_int64(stmt, 0);
    else {
        sqlite3_close(db);
        throw Str("get(Llong):sqlite3_step(): ") + sqlite3_errmsg(db);
    }
    sqlite3_finalize(stmt);
    return ret;
}

// get single column
inline void get_column(vecStr_O data, sqlite3* db, Str_I table, Str_I field)
{
	data.clear();
    sqlite3_stmt* stmt;
    Str str = "SELECT " + field + " FROM " + table;
    if (sqlite3_prepare_v2(db, str.c_str(), -1, &stmt, NULL) != SQLITE_OK) {
        sqlite3_close(db);
        throw Str("get_column():sqlite3_prepare_v2(): ") + sqlite3_errmsg(db);
    }
    int ret;
	while ((ret = sqlite3_step(stmt)) == SQLITE_ROW)
		data.push_back((char*)sqlite3_column_text(stmt, 0));
    if (ret != SQLITE_DONE) {
        sqlite3_close(db);
        throw Str("get_column():sqlite3_step(): ") + sqlite3_errmsg(db);
    }
    sqlite3_finalize(stmt);
}

inline void get_column(vecStr32_O data, sqlite3* db, Str_I table, Str_I field)
{
	data.clear();
    sqlite3_stmt* stmt;
    Str str = "SELECT " + field + " FROM " + table;
    if (sqlite3_prepare_v2(db, str.c_str(), -1, &stmt, NULL) != SQLITE_OK) {
        sqlite3_close(db);
        throw Str("get_column():sqlite3_prepare_v2(): ") + sqlite3_errmsg(db);
    }
    int ret;
	while ((ret = sqlite3_step(stmt)) == SQLITE_ROW)
		data.push_back(utf8to32((char*)sqlite3_column_text(stmt, 0)));
    if (ret != SQLITE_DONE) {
        sqlite3_close(db);
        throw Str("get_column():sqlite3_step(): ") + sqlite3_errmsg(db);
    }
    sqlite3_finalize(stmt);
}

inline void get_column(vecLlong_O data, sqlite3* db, Str_I table, Str_I field)
{
	data.clear();
    sqlite3_stmt* stmt;
    Str str = "SELECT " + field + " FROM " + table;
    if (sqlite3_prepare_v2(db, str.c_str(), -1, &stmt, NULL) != SQLITE_OK) {
        sqlite3_close(db);
        throw Str("get_column():sqlite3_prepare_v2(): ") + sqlite3_errmsg(db);
    }
    int ret;
	while ((ret = sqlite3_step(stmt)) == SQLITE_ROW)
		data.push_back(sqlite3_column_int64(stmt, 0));
    if (ret != SQLITE_DONE) {
        sqlite3_close(db);
        throw Str("get_column():sqlite3_step(): ") + sqlite3_errmsg(db);
    }
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
    if (sqlite3_prepare_v2(db, str.c_str(), -1, &stmt, NULL) != SQLITE_OK) {
        sqlite3_close(db);
        throw Str("get_row(vecStr):sqlite3_prepare_v2(): ") + sqlite3_errmsg(db);
    }
	if (sqlite3_step(stmt) == SQLITE_ROW) {
        for (Long i = 0; i < size(fields_out); ++i)
		    data.push_back((char*)sqlite3_column_text(stmt, i));
    }
    else {
        sqlite3_close(db);
        throw Str("get_row(vecStr):sqlite3_step(): ") + sqlite3_errmsg(db);
    }
    sqlite3_finalize(stmt);
}

inline void get_row(vecLlong_O data, sqlite3* db, Str_I table, Str_I field, Str_I val, vecStr_I fields_out)
{
    data.clear();
    sqlite3_stmt* stmt;
    Str str = "SELECT ";
    for (auto &field_out : fields_out)
        str += field_out + ',';
    str.pop_back();
    str += " FROM " + table + " WHERE " + field + " = '" + val + "';";
    if (sqlite3_prepare_v2(db, str.c_str(), -1, &stmt, NULL) != SQLITE_OK) {
        sqlite3_close(db);
        throw Str("get_row(vecLlong):sqlite3_prepare_v2(): ") + sqlite3_errmsg(db);
    }
	if (sqlite3_step(stmt) == SQLITE_ROW) {
        for (Long i = 0; i < size(fields_out); ++i)
		    data.push_back(sqlite3_column_int64(stmt, i));
    }
    else {
        sqlite3_close(db);
        throw Str("get_row(vecLlong):sqlite3_step(): ") + sqlite3_errmsg(db);
    }
    sqlite3_finalize(stmt);
}

// get multiple fields (columns) from the table
// data[i] is the i-th row
inline void get_matrix(vector<vecStr> &data, sqlite3* db, Str_I table, vecStr_I fields)
{
	data.clear();
    sqlite3_stmt* stmt;
    Str str = "SELECT ";
    for (auto &field : fields)
        str += field + ',';
    str.pop_back();
    str += " FROM " + table + ";";
    if (sqlite3_prepare_v2(db, str.c_str(), -1, &stmt, NULL) != SQLITE_OK) {
        sqlite3_close(db);
        throw Str("get_matrix(vector<vecStr>):sqlite3_prepare_v2(): ") + sqlite3_errmsg(db);
    }
    int ret;
	while ((ret = sqlite3_step(stmt)) == SQLITE_ROW) {
        data.emplace_back();
        for (Long i = 0; i < size(fields); ++i)
		    data.back().push_back((char*)sqlite3_column_text(stmt, i));
    }
    if (ret != SQLITE_DONE) {
        sqlite3_close(db);
        throw Str("get_matrix(vector<vecStr>):sqlite3_step(): ") + sqlite3_errmsg(db);
    }
    sqlite3_finalize(stmt);
}

inline void get_matrix(vector<vecStr32> &data, sqlite3* db, Str_I table, vecStr_I fields)
{
	data.clear();
    sqlite3_stmt* stmt;
    Str str = "SELECT ";
    for (auto &field : fields)
        str += field + ',';
    str.pop_back();
    str += " FROM " + table + ";";
    if (sqlite3_prepare_v2(db, str.c_str(), -1, &stmt, NULL) != SQLITE_OK) {
        sqlite3_close(db);
        throw Str("get_matrix(vector<vecStr>):sqlite3_prepare_v2(): ") + sqlite3_errmsg(db);
    }
    int ret;
	while ((ret = sqlite3_step(stmt)) == SQLITE_ROW) {
        data.emplace_back();
        for (Long i = 0; i < size(fields); ++i)
		    data.back().push_back(utf8to32((char*)sqlite3_column_text(stmt, i)));
    }
    if (ret != SQLITE_DONE) {
        sqlite3_close(db);
        throw Str("get_matrix(vector<vecStr>):sqlite3_step(): ") + sqlite3_errmsg(db);
    }
    sqlite3_finalize(stmt);
}

// get multiple fields (columns) from the table
// data[i] is the i-th row
inline void get_matrix(vector<vecLlong> &data, sqlite3* db, Str_I table, vecStr_I fields)
{
	data.clear();
    sqlite3_stmt* stmt;
    Str str = "SELECT ";
    for (auto &field : fields)
        str += field + ',';
    str.pop_back();
    str += " FROM " + table + ";";
    if (sqlite3_prepare_v2(db, str.c_str(), -1, &stmt, NULL) != SQLITE_OK) {
        sqlite3_close(db);
        throw Str("get_matrix(vector<vecStr>):sqlite3_prepare_v2(): ") + sqlite3_errmsg(db);
    }
    int ret;
	while ((ret = sqlite3_step(stmt)) == SQLITE_ROW) {
        data.emplace_back();
        for (Long i = 0; i < size(fields); ++i)
		    data.back().push_back(sqlite3_column_int64(stmt, i));
    }
    if (ret != SQLITE_DONE) {
        sqlite3_close(db);
        throw Str("get_matrix(vector<vecLlong>):sqlite3_step(): ") + sqlite3_errmsg(db);
    }
    sqlite3_finalize(stmt);
}

// remove everything inside table
inline void table_clear(sqlite3* db, Str_I table)
{
    Str str = "DELETE FROM " + table + ";";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db, str.c_str(), -1, &stmt, NULL) != SQLITE_OK) {
        sqlite3_close(db);
        throw Str("table_clear():sqlite3_prepare_v2(): ") + sqlite3_errmsg(db);
    }

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_close(db);
        throw Str("table_clear():sqlite3_prepare_v2(): ") + sqlite3_errmsg(db);
    }
    sqlite3_finalize(stmt);
}

} // namespace slisc
