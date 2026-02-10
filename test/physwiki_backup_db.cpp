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

static bool parse_filename(const std::string &name, std::string &timestamp, std::string &author, std::string &article)
{
	if (name.size() < 5 || name.substr(name.size() - 4) != ".tex")
		return false;
	size_t pos1 = name.find('_');
	size_t pos2 = name.rfind('_');
	if (pos1 == std::string::npos || pos1 == pos2)
		return false;
	timestamp = name.substr(0, pos1);
	author = name.substr(pos1 + 1, pos2 - pos1 - 1);
	article = name.substr(pos2 + 1, name.size() - pos2 - 1 - 4);
	if (timestamp.size() != 12 || author.empty() || article.empty())
		return false;
	for (char c : timestamp) {
		if (c < '0' || c > '9')
			return false;
	}
	for (char c : author) {
		if (c < '0' || c > '9')
			return false;
	}
	return true;
}

struct BackupInfo {
	std::string path;
	std::string filename;
	std::string timestamp;
	std::string author;
	int64_t author_id = 0;
	std::string article;
};

int main()
{
	const std::string dir = "/mnt/g/github/PhysWiki-backup/";
	const std::string sql_path = "/mnt/g/github/PhysWikiScan/data/PhysWiki-backup.sql";
	const std::string db_path = "/mnt/g/github/PhysWikiScan/data/PhysWiki-backup.db";

	if (!file_exist(sql_path)) {
		std::cerr << "SQL schema not found: " << sql_path << '\n';
		return 1;
	}
	if (file_exist(db_path))
		file_remove(db_path);

	std::string schema;
	if (!read_file(sql_path, schema)) {
		std::cerr << "Failed to read schema: " << sql_path << '\n';
		return 1;
	}

	std::map<std::string, std::vector<BackupInfo>> groups;
	std::vector<std::string> delete_paths;
	vecStr fnames;
	file_list(fnames, dir);
	for (const auto &name_full : fnames) {
		std::string name = basename_only(name_full);
		std::string timestamp, author, article;
		if (!parse_filename(name, timestamp, author, article))
			continue;
		BackupInfo info;
		info.filename = name;
		info.path = name_full;
		if (name_full.find('/') == std::string::npos && name_full.find('\\') == std::string::npos)
			info.path = dir + name_full;
		info.timestamp = timestamp;
		info.author = author;
		info.author_id = std::stoll(author);
		info.article = article;
		groups[article].push_back(info);
		delete_paths.push_back(info.path);
	}

	SQLite::Database db(db_path, SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);
	db.exec("PRAGMA journal_mode = WAL;");
	db.exec("PRAGMA synchronous = NORMAL;");
	db.exec(schema);

	SQLite::Statement insert_stmt(db,
		R"(INSERT INTO "backup_files" ("timestamp", "author_id", "article_id", "size", "hash", "prev_ver", "diff")
		   VALUES (?, ?, ?, ?, ?, ?, ?);)");
	SQLite::Statement select_stmt(db,
		R"(SELECT "id", "prev_ver", "size", "hash", "diff" FROM "backup_files"
		   WHERE "timestamp"=? AND "author_id"=? AND "article_id"=?;)");

	SQLite::Transaction txn(db);
	size_t inserted = 0;
	for (auto &kv : groups) {
		auto &files = kv.second;
		std::sort(files.begin(), files.end(), [](const BackupInfo &a, const BackupInfo &b) {
			if (a.timestamp != b.timestamp)
				return a.timestamp < b.timestamp;
			if (a.author_id != b.author_id)
				return a.author_id < b.author_id;
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

			insert_stmt.bind(1, info.timestamp);
			insert_stmt.bind(2, info.author_id);
			insert_stmt.bind(3, info.article);
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
			if (a.timestamp != b.timestamp)
				return a.timestamp < b.timestamp;
			if (a.author_id != b.author_id)
				return a.author_id < b.author_id;
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

			select_stmt.bind(1, info.timestamp);
			select_stmt.bind(2, info.author_id);
			select_stmt.bind(3, info.article);
			if (!select_stmt.executeStep()) {
				std::cerr << "Missing db record for " << info.filename << '\n';
				return 1;
			}
			const int64_t id_db = select_stmt.getColumn(0).getInt64();
			const bool prev_null = select_stmt.getColumn(1).isNull();
			const int64_t prev_db = prev_null ? 0 : select_stmt.getColumn(1).getInt64();
			const int64_t size_db = select_stmt.getColumn(2).getInt64();
			const std::string hash_db = select_stmt.getColumn(3).getString();
			const std::string diff_db = select_stmt.getColumn(4).getString();
			select_stmt.reset();

			if (prev_id == 0) {
				if (!prev_null) {
					std::cerr << "Unexpected prev_ver for " << info.filename << '\n';
					return 1;
				}
			}
			else if (prev_db != prev_id) {
				std::cerr << "prev_ver mismatch for " << info.filename << '\n';
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
