#include "../SLISC/str/str.h"
#include "../SLISC/str/str_diff_patch2.h"
#include "../SLISC/util/sha1sum.h"

#include <SQLiteCpp/Database.h>
#include <SQLiteCpp/Statement.h>
#include <SQLiteCpp/Transaction.h>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
#include <unordered_set>
#include <vector>

using namespace slisc;

static bool read_file(const std::string &path, std::string &out)
{
	std::ifstream in(path.c_str(), std::ios::binary);
	if (!in)
		return false;
	out.assign((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	return true;
}

static void apply_diff(std::string &out, const std::vector<std::tuple<size_t, size_t, Str>> &diff)
{
	for (auto it = diff.rbegin(); it != diff.rend(); ++it)
		out.replace(get<0>(*it), get<1>(*it), get<2>(*it));
}

static std::string basename_only(const std::string &path)
{
	size_t pos = path.find_last_of("/\\");
	if (pos == std::string::npos)
		return path;
	return path.substr(pos + 1);
}

static bool parse_filename(const std::string &name, std::string &time, int64_t &author, std::string &entry)
{
	if (name.size() < 5 || name.substr(name.size() - 4) != ".tex")
		return false;
	size_t pos1 = name.find('_');
	size_t pos2 = name.rfind('_');
	if (pos1 == std::string::npos || pos1 == pos2)
		return false;
	time = name.substr(0, pos1);
	std::string author_str = name.substr(pos1 + 1, pos2 - pos1 - 1);
	entry = name.substr(pos2 + 1, name.size() - pos2 - 1 - 4);
	if (time.size() != 12 || author_str.empty() || entry.empty())
		return false;
	for (char c : time) {
		if (c < '0' || c > '9')
			return false;
	}
	for (char c : author_str) {
		if (c < '0' || c > '9')
			return false;
	}
	author = std::stoll(author_str);
	return true;
}

struct BackupInfo {
	std::string path;
	std::string filename;
	std::string time;
	int64_t author = 0;
	std::string entry;
};

int main()
{
	const std::string dir = "/mnt/g/github/PhysWiki-backup/";
	const std::string sql_path = "/mnt/g/github/PhysWikiScan/data/PhysWiki-backup-template.sql";
	const std::string db_path = "/mnt/g/github/PhysWikiScan/data/PhysWiki-backup.db";

	if (!file_exist(sql_path)) {
		std::cerr << "SQL schema not found: " << sql_path << '\n';
		return 1;
	}
	if (file_exist(db_path))
		file_remove(db_path);
	if (file_exist(db_path + "-wal"))
		file_remove(db_path + "-wal");
	if (file_exist(db_path + "-shm"))
		file_remove(db_path + "-shm");

	std::string schema;
	if (!read_file(sql_path, schema)) {
		std::cerr << "Failed to read schema: " << sql_path << '\n';
		return 1;
	}

	std::map<std::string, std::vector<BackupInfo>> groups;
	std::unordered_set<std::string> seen_keys;
	std::vector<std::string> delete_paths;
	vecStr fnames;
	file_list(fnames, dir);
	for (const auto &name_full : fnames) {
		std::string name = basename_only(name_full);
		std::string time, entry;
		int64_t author = 0;
		if (!parse_filename(name, time, author, entry))
			continue;
		std::string key = time + "|" + std::to_string(author) + "|" + entry;
		if (!seen_keys.insert(key).second) {
			std::cerr << "Duplicate backup entry detected, skipping: " << name << '\n';
			continue;
		}
		BackupInfo info;
		info.filename = name;
		info.path = name_full;
		if (name_full.find('/') == std::string::npos && name_full.find('\\') == std::string::npos)
			info.path = dir + name_full;
		info.time = time;
		info.author = author;
		info.entry = entry;
		groups[entry].push_back(info);
		delete_paths.push_back(info.path);
	}

	SQLite::Database db(db_path, SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);
	db.exec("PRAGMA journal_mode = WAL;");
	db.exec("PRAGMA synchronous = NORMAL;");
	db.exec(schema);

	SQLite::Statement insert_stmt(db,
		R"(INSERT INTO "backup_files" ("time", "author", "entry", "size", "hash", "last_id", "diff")
		   VALUES (?, ?, ?, ?, ?, ?, ?);)");
	SQLite::Statement select_stmt(db,
		R"(SELECT "id", "last_id", "size", "hash", "diff" FROM "backup_files"
		   WHERE "time"=? AND "author"=? AND "entry"=?;)");

	SQLite::Transaction txn(db);
	size_t inserted = 0;
	for (auto &kv : groups) {
		auto &files = kv.second;
		std::sort(files.begin(), files.end(), [](const BackupInfo &a, const BackupInfo &b) {
			if (a.time != b.time)
				return a.time < b.time;
			if (a.author != b.author)
				return a.author < b.author;
			return a.filename < b.filename;
		});

		std::string prev_content;
		int64_t prev_id = 0;
		for (const auto &info : files) {
			std::string content;
			if (!read_file(info.path, content)) {
				std::cerr << "Failed to read file: " << info.path << '\n';
				return 1;
			}
			CRLF_to_LF(content);
			if (!is_valid(content)) {
				std::cerr << "Invalid UTF-8 in " << info.path << '\n';
				return 1;
			}

			vector<tuple<size_t, size_t, Str>> diff;
			str_diff(diff, prev_content, content);
			std::string diff_json;
			str_diff_serialize(diff_json, diff);

			const std::string hash = sha1sum(content).substr(0, 16);

			insert_stmt.bind(1, info.time);
			insert_stmt.bind(2, info.author);
			insert_stmt.bind(3, info.entry);
			insert_stmt.bind(4, static_cast<int64_t>(content.size()));
			insert_stmt.bind(5, hash);
			if (prev_id == 0) {
				insert_stmt.bind(6);
			}
			else {
				insert_stmt.bind(6, prev_id);
			}
			insert_stmt.bind(7, diff_json);
			insert_stmt.exec();
			insert_stmt.reset();
			prev_id = db.getLastInsertRowid();
			++inserted;
			if (inserted % 5000 == 0)
				std::cout << "Inserted " << inserted << " records..." << std::endl;

			prev_content = std::move(content);
		}
	}
	txn.commit();

	for (auto &kv : groups) {
		auto &files = kv.second;
		std::sort(files.begin(), files.end(), [](const BackupInfo &a, const BackupInfo &b) {
			if (a.time != b.time)
				return a.time < b.time;
			if (a.author != b.author)
				return a.author < b.author;
			return a.filename < b.filename;
		});
		std::string prev_content;
		int64_t prev_id = 0;
		for (const auto &info : files) {
			std::string content;
			if (!read_file(info.path, content)) {
				std::cerr << "Failed to read file: " << info.path << '\n';
				return 1;
			}
			CRLF_to_LF(content);
			if (!is_valid(content)) {
				std::cerr << "Invalid UTF-8 in " << info.path << '\n';
				return 1;
			}

			select_stmt.bind(1, info.time);
			select_stmt.bind(2, info.author);
			select_stmt.bind(3, info.entry);
			if (!select_stmt.executeStep()) {
				std::cerr << "Missing db record for " << info.filename << '\n';
				return 1;
			}
			const int64_t id_db = select_stmt.getColumn(0).getInt64();
			const bool last_null = select_stmt.getColumn(1).isNull();
			const int64_t last_db = last_null ? 0 : select_stmt.getColumn(1).getInt64();
			const int64_t size_db = select_stmt.getColumn(2).getInt64();
			const std::string hash_db = select_stmt.getColumn(3).getString();
			const std::string diff_db = select_stmt.getColumn(4).getString();
			select_stmt.reset();

			if (prev_id == 0) {
				if (!last_null) {
					std::cerr << "Unexpected last_id for " << info.filename << '\n';
					return 1;
				}
			}
			else if (last_db != prev_id) {
				std::cerr << "last_id mismatch for " << info.filename << '\n';
				return 1;
			}

			if (size_db != static_cast<int64_t>(content.size())) {
				std::cerr << "Size mismatch for " << info.filename << '\n';
				return 1;
			}
			const std::string hash = sha1sum(content).substr(0, 16);
			if (hash != hash_db) {
				std::cerr << "Hash mismatch for " << info.filename << '\n';
				return 1;
			}

			vector<tuple<size_t, size_t, Str>> diff;
			str_diff_deserialize(diff, diff_db);
			std::string reconstructed = prev_content;
			apply_diff(reconstructed, diff);
			if (reconstructed != content) {
				std::cerr << "Reconstruction mismatch for " << info.filename << '\n';
				return 1;
			}
			prev_content.swap(reconstructed);
			prev_id = id_db;
		}
	}

	for (const auto &path : delete_paths)
		file_remove(path);

	std::cout << "Inserted " << inserted << " records into " << db_path << std::endl;
	std::cout << "Removed " << delete_paths.size() << " .tex files (restore with git checkout . && git clean -fd)." << std::endl;
	return 0;
}
