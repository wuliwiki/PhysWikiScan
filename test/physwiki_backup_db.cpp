#include "../SLISC/str/str.h"

#include <SQLiteCpp/Database.h>
#include <SQLiteCpp/Statement.h>
#include <SQLiteCpp/Transaction.h>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

static bool read_file(const fs::path &path, std::string &out)
{
	std::ifstream in(path, std::ios::binary);
	if (!in)
		return false;
	out.assign((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	return true;
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

int main()
{
	const fs::path dir = "/mnt/g/github/PhysWiki-backup";
	const fs::path sql_path = dir / "PhysWiki-backup.sql";
	const fs::path db_path = dir / "PhysWiki-backup.db";

	if (!fs::exists(sql_path)) {
		std::cerr << "SQL schema not found: " << sql_path << '\n';
		return 1;
	}
	if (fs::exists(db_path))
		fs::remove(db_path);

	std::string schema;
	if (!read_file(sql_path, schema)) {
		std::cerr << "Failed to read schema: " << sql_path << '\n';
		return 1;
	}

	SQLite::Database db(db_path.string(), SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);
	db.exec("PRAGMA journal_mode = WAL;");
	db.exec("PRAGMA synchronous = NORMAL;");
	db.exec(schema);

	SQLite::Statement insert_stmt(db,
		R"(INSERT INTO "backup_files" ("filename", "timestamp", "author_id", "article_id", "content", "size")
		   VALUES (?, ?, ?, ?, ?, ?);)");

	SQLite::Transaction txn(db);
	size_t inserted = 0;
	for (const auto &entry : fs::directory_iterator(dir)) {
		if (!entry.is_regular_file())
			continue;
		const std::string name = entry.path().filename().string();
		std::string timestamp, author, article;
		if (!parse_filename(name, timestamp, author, article))
			continue;
		std::string content;
		if (!read_file(entry.path(), content)) {
			std::cerr << "Failed to read file: " << entry.path() << '\n';
			return 1;
		}

		insert_stmt.bind(1, name);
		insert_stmt.bind(2, timestamp);
		insert_stmt.bind(3, static_cast<int64_t>(std::stoll(author)));
		insert_stmt.bind(4, article);
		insert_stmt.bind(5, content.data(), static_cast<int>(content.size()));
		insert_stmt.bind(6, static_cast<int64_t>(content.size()));
		insert_stmt.exec();
		insert_stmt.reset();
		++inserted;
	}
	txn.commit();

	std::cout << "Inserted " << inserted << " records into " << db_path << std::endl;
	return 0;
}
