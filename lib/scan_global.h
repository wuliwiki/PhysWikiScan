// global variables, must be set only once
#pragma once

#include "../SLISC/file/file.h"
#include "../SLISC/file/sqlite_ext.h"
#include "../SLISC/str/str.h"
#include "../SLISC/algo/graph.h"

using namespace slisc;

namespace gv {
	Str path_in; // e.g. ../PhysWiki/
	Str path_out; // e.g. ../littleshi.cn/online/
	Str path_data; // e.g. ../littleshi.cn/data/
	Str url; // e.g. https://wuli.wiki/online/
	Bool is_wiki; // editing wiki or note
	Bool is_eng = false; // use english for auto-generated text (Eq. Fig. etc.)
	Bool is_entire = false; // running one tex or the entire wiki
}

Str sb, sb1; // string buffer

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

// write log
inline void scan_log(Str_I str, bool print_time = false)
{
	static Str log_file = "scan_log.txt";

	// write to file
	ofstream file(log_file, std::ios::app);
	if (!file.is_open())
		throw internal_err(Str("Unable to open ") + log_file);
	// get time
	if (print_time) {
		time_t time = std::time(nullptr);
		std::tm *ptm = localtime(&time);
		stringstream ss;
		ss << std::put_time(ptm, "%Y-%m-%d %H:%M:%S");
		static Str time_str = std::move(ss.str());
		file << time_str << "  ";
	}
	file << str << endl;
	file.close();
}

// db log, also output to stdout
inline void db_log(Str_I str)
{
	cout << SLS_CYAN_BOLD << "[DB] " << str << SLS_NO_STYLE << endl;
	scan_log(str);
}

inline void scan_warn(Str_I str)
{
	SLS_WARN(str);
	scan_log("[Warning] " + str);
}

// limit log size
inline void limit_log()
{
	const Long size_min = 5e6, size_max = 10e6;

	// limit file size (reduce size to size_min if size > size_max)
	// only delete whole lines
	static Str log_file = "scan_log.txt";
	read(sb, log_file);
	if (size(sb) > size_max) {
		Long ind = find(sb, '\n', size(sb)-size_min);
		if (ind > 0) {
			write(sb.substr(ind + 1), log_file);
		}
	}
}
