// global variables, must be set only once
#pragma once

#include <regex>
#include "../SLISC/str/str.h"
#include "../SLISC/util/time.h"
#include "../SLISC/util/sha1sum.h"
#include "../SLISC/str/str.h"
#include "../SLISC/algo/graph.h"
#include "../SLISC/file/sqlite_ext.h"
#include "../SLISC/file/sqlitecpp_ext.h"

using namespace slisc;

namespace gv {
	Str path_in; // e.g. ../PhysWiki/
	Str path_out; // e.g. ../littleshi.cn/online/
	Str path_data; // e.g. ../littleshi.cn/data/
	Str url; // e.g. https://wuli.wiki/online/
	bool is_wiki; // editing wiki or note
	bool is_entire = false; // running one tex or the entire wiki
}

Str sb, sb1; // string buffer (danger if passed into function that use them!)

class scan_err : public std::exception
{
private:
	Str m_msg;
public:
	explicit scan_err(Str msg): m_msg(std::move(msg)) {}

	const char* what() const noexcept override {
		return m_msg.c_str();
	}
};

// internal error to throw
class internal_err : public scan_err
{
public:
	explicit internal_err(Str_I msg): scan_err(u8"内部错误（请联系管理员）： " + msg) {}
};

// append to log file
// if `print_time`, time will be inserted at first none LF
inline void scan_log(Str_I str, bool print_time = false)
{
	static const Str log_file = "scan_log.txt";
	static Str str1, time_str;

	// write to file
	ofstream file(log_file, std::ios::app);
	if (!file.is_open())
		throw internal_err(Str("Unable to open ") + log_file);
	// get time
	if (print_time) {
		time_t time = std::time(nullptr);
		std::tm *ptm = localtime(&time);
		stringstream ss;
		ss << std::put_time(ptm, "%Y-%m-%d %H:%M:%S  ");
		time_str = std::move(ss.str());
		Long ind = 0;
		while (str[ind] == '\n') ++ind;
		str1 = str;
		str1.insert(ind, time_str);
		file << str1 << endl;
	}
	else
		file << str << endl;
	file.close();
}

// scan_log() and print to `stdout`
inline void scan_log_print(Str_I str)
{
	scan_log(str);
	cout << str << endl;
}

// db log, also output to stdout
// in case of warning about db, use scan_log_warn()
inline void db_log_print(Str_I str)
{
	static Str str1;
	clear(str1) << "[DB] " << str;
	cout << SLS_CYAN_BOLD << str1 << SLS_NO_STYLE << endl;
	scan_log(str1);
}

// print warning and append to log file
inline void scan_log_warn(Str_I str)
{
	SLS_WARN(str);
	scan_log("[Warning] " + str);
}

// cerr and append to log file
inline void scan_cerr(Str_I str)
{
	scan_log("[Error] " + str);
	cerr << sb << endl;
}

// limit log size
inline void scan_log_limit()
{
	static const Str log_file = "scan_log.txt";
	static const Long size_min = 5e6, size_max = 10e6;

	// limit file size (reduce size to size_min if size > size_max)
	// only delete whole lines
	if (!file_exist(log_file)) {
		write("", log_file); return;
	}
	read(sb, log_file);
	if (size(sb) > size_max) {
		Long ind = find(sb, '\n', size(sb)-size_min);
		if (ind > 0) {
			write(sb.substr(ind + 1), log_file);
		}
	}
}

// callback function for update_sqlite_table() in sqlitecpp_ext.h
inline void sqlite_callback(char act, Str_I table, vecStr_I field_names,
	const pair<vecSQLval,vecSQLval> &row,
	vecLong_I cols_changed, const vecSQLval &old_vals, void *data)
{
	Str str;
	if (act == 'a') {
		str << "批量删除 " << cols_changed[0] << " 条记录，表 \"" << table << '\"';
		db_log_print(str);
		return;
	}
	else if (act == 'i') str = "插入记录 ";
	else if (act == 'd') str = "删除记录 ";
	else if (act == 'u') str = "更新记录 ";
	else
		throw internal_err(SLS_WHERE);
	str << "表 \"" << table << "\" ";
	Long Nkey = size(row.first), Nval = size(row.second);

	if (act == 'i' || act == 'd') {
		for (Long i = 0; i < Nkey+Nval; ++i) {
			auto &e = (i < Nkey ? row.first[i] : row.second[i-Nkey]);
			str << field_names[i] << ':';
			if (e.type == 's')
				str << e.s << ", ";
			else
				str << e.i << ", ";
		}
		str.resize(str.size() - 2);
	}
	else if (act == 'u') {
		for (Long i = 0; i < Nkey; ++i) {
			auto &e = row.first[i];
			str << field_names[i] << ':';
			if (e.type == 's')
				str << e.s << ", ";
			else
				str << e.i << ", ";
		}
		str.resize(str.size() - 2);
		str << " 更改 ";
		for (Long i = 0; i < size(cols_changed); ++i) {
			str << field_names[Nkey+i] << ':';
			if (old_vals[i].type == 's')
				str << old_vals[i].s << "->" << row.second[cols_changed[i]].s << ", ";
			else
				str << old_vals[i].i << "->" << row.second[cols_changed[i]].i << ", ";
		}
		str.resize(str.size() - 2);
	}
	else
		throw scan_err(SLS_WHERE);
	db_log_print(str);
}
