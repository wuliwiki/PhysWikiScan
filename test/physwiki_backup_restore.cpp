#include "../SLISC/str/str.h"
#include "../SLISC/str/str_diff_patch2.h"
#include "../SLISC/util/sha1sum.h"
#include "../SLISC/file/file.h"

#include <SQLiteCpp/Database.h>
#include <SQLiteCpp/Statement.h>
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <unordered_map>

using namespace slisc;

static void apply_diff(std::string &out, const std::vector<std::tuple<size_t, size_t, Str>> &diff)
{
	for (auto it = diff.rbegin(); it != diff.rend(); ++it)
		out.replace(get<0>(*it), get<1>(*it), get<2>(*it));
}

static bool write_file(const std::string &path, const std::string &data)
{
	std::ofstream out(path.c_str(), std::ios::binary);
	if (!out)
		return false;
	out.write(data.data(), static_cast<std::streamsize>(data.size()));
	return out.good();
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

struct Record {
	int64_t id = 0;
	std::string time;
	int64_t author = 0;
	int64_t size = 0;
	std::string hash;
	std::string diff_json;
	int64_t last_id = 0;
	bool last_null = true;
};

static bool load_record(SQLite::Database &db, int64_t id, Record &rec)
{
	SQLite::Statement stmt(db,
		R"(SELECT "id", "time", "author", "size", "hash", "last_id", "diff"
		   FROM "backup_files" WHERE "id"=?;)");
	stmt.bind(1, id);
	if (!stmt.executeStep())
		return false;
	rec.id = stmt.getColumn(0).getInt64();
	rec.time = stmt.getColumn(1).getString();
	rec.author = stmt.getColumn(2).getInt64();
	rec.size = stmt.getColumn(3).getInt64();
	rec.hash = stmt.getColumn(4).getString();
	rec.last_null = stmt.getColumn(5).isNull();
	rec.last_id = rec.last_null ? 0 : stmt.getColumn(5).getInt64();
	rec.diff_json = stmt.getColumn(6).getString();
	return true;
}

int main(int argc, char **argv)
{
	try {
		setenv("SQLITE_TMPDIR", "/tmp", 1);
		const std::string db_path = "/mnt/g/github/PhysWikiScan/data/backup.db";
		std::string out_dir = path2dir(db_path) + "PhysWiki-backup-files/";

		std::vector<std::string> args;
		for (int i = 1; i < argc; ++i) {
			std::string arg = argv[i];
			if (arg == "--out") {
				if (i + 1 >= argc) {
					std::cerr << "Missing value for --out\n";
					return 1;
				}
				out_dir = argv[++i];
				if (!out_dir.empty() && out_dir.back() != '/')
					out_dir.push_back('/');
			}
			else {
				args.push_back(arg);
			}
		}

		ensure_dir(out_dir);

		if (!file_exist(db_path)) {
			std::cerr << "Database not found: " << db_path << '\n';
			return 1;
		}

		std::unique_ptr<SQLite::Database> db;
		try {
			const std::string db_uri = "file:" + db_path + "?immutable=1";
			db.reset(new SQLite::Database(db_uri, SQLite::OPEN_READONLY | SQLite::OPEN_URI));
			std::cout << "Opened database via URI." << std::endl;
		}
		catch (const std::exception &e) {
			std::cerr << "SQLiteCpp URI open failed: " << e.what() << '\n'
					  << "Retrying without URI..." << std::endl;
			db.reset(new SQLite::Database(db_path, SQLite::OPEN_READONLY));
			std::cout << "Opened database without URI." << std::endl;
		}

		db->exec("PRAGMA temp_store = MEMORY;");

		size_t restored = 0;
		if (!args.empty() && args[0] == "checkout") {
			if (args.size() != 2) {
				std::cerr << "Usage: physwiki_backup_restore [--out <dir>] checkout <file>\n";
				return 1;
			}
			std::string time;
			int64_t author = 0;
			std::string entry;
			if (!parse_filename(args[1], time, author, entry)) {
				std::cerr << "Invalid filename: " << args[1] << '\n';
				return 1;
			}
			SQLite::Statement stmt(*db,
				R"(SELECT "id", "size", "hash" FROM "backup_files"
				   WHERE "time"=? AND "author"=? AND "entry"=?;)");
			stmt.bind(1, time);
			stmt.bind(2, author);
			stmt.bind(3, entry);
			if (!stmt.executeStep()) {
				std::cerr << "Record not found for " << args[1] << '\n';
				return 1;
			}
			const int64_t target_id = stmt.getColumn(0).getInt64();
			const int64_t target_size = stmt.getColumn(1).getInt64();
			const std::string target_hash = stmt.getColumn(2).getString();

			std::vector<Record> chain;
			int64_t cur = target_id;
			while (cur != 0) {
				Record rec;
				if (!load_record(*db, cur, rec)) {
					std::cerr << "Missing record id " << cur << '\n';
					return 1;
				}
				chain.push_back(rec);
				if (rec.last_null)
					break;
				cur = rec.last_id;
			}

			std::string content;
			content.reserve(static_cast<size_t>(target_size));
			for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
				vector<tuple<size_t, size_t, Str>> diff;
				str_diff_deserialize(diff, it->diff_json);
				apply_diff(content, diff);
			}

			if (target_size != static_cast<int64_t>(content.size())) {
				std::cerr << "Size mismatch for " << args[1] << '\n';
				return 1;
			}
			const std::string hash = sha1sum(content).substr(0, 16);
			if (hash != target_hash) {
				std::cerr << "Hash mismatch for " << args[1] << '\n';
				return 1;
			}

			const std::string out_path = out_dir + args[1];
			if (!write_file(out_path, content)) {
				std::cerr << "Failed to write " << out_path << '\n';
				return 1;
			}
			std::cout << "Restored " << args[1] << " -> " << out_path << '\n';
		}
		else if (!args.empty()) {
			std::cerr << "Usage: physwiki_backup_restore [--out <dir>] [checkout <file>]\n";
			return 1;
		}
		else {
			SQLite::Statement stmt_article(*db,
				R"(SELECT "entry" FROM "backup_files" GROUP BY "entry" ORDER BY "entry" ASC;)");
			while (stmt_article.executeStep()) {
				const std::string article = stmt_article.getColumn(0).getString();
				SQLite::Statement stmt(*db,
					R"(SELECT "id", "time", "author", "size", "hash", "last_id", "diff"
					   FROM "backup_files"
					   WHERE "entry"=?
					   ORDER BY "time" ASC, "author" ASC;)");
				stmt.bind(1, article);

				std::vector<Record> records;
				while (stmt.executeStep()) {
					Record rec;
					rec.id = stmt.getColumn(0).getInt64();
					rec.time = stmt.getColumn(1).getString();
					rec.author = stmt.getColumn(2).getInt64();
					rec.size = stmt.getColumn(3).getInt64();
					rec.hash = stmt.getColumn(4).getString();
					rec.last_null = stmt.getColumn(5).isNull();
					rec.last_id = rec.last_null ? 0 : stmt.getColumn(5).getInt64();
					rec.diff_json = stmt.getColumn(6).getString();
					records.push_back(rec);
				}
				if (records.empty())
					continue;

				std::unordered_map<int64_t, size_t> id_index;
				id_index.reserve(records.size());
				for (size_t i = 0; i < records.size(); ++i)
					id_index[records[i].id] = i;

				for (size_t i = 0; i < records.size(); ++i) {
					if (i == 0) {
						if (!records[i].last_null) {
							std::cerr << "First version has last_id for " << article << '\n';
							return 1;
						}
					}
					else if (records[i].last_id != records[i - 1].id) {
						std::cerr << "last_id mismatch order for " << article << " @ "
								  << records[i].time << '\n';
						return 1;
					}

					std::vector<size_t> chain;
					int64_t cur = records[i].id;
					while (cur != 0) {
						auto it = id_index.find(cur);
						if (it == id_index.end()) {
							std::cerr << "Missing prev chain for " << article << '\n';
							return 1;
						}
						chain.push_back(it->second);
						const Record &rec = records[it->second];
						if (rec.last_null)
							break;
						cur = rec.last_id;
					}

					std::string content;
					content.reserve(static_cast<size_t>(records[i].size));
					for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
						vector<tuple<size_t, size_t, Str>> diff;
						str_diff_deserialize(diff, records[*it].diff_json);
						apply_diff(content, diff);
					}

					if (records[i].size != static_cast<int64_t>(content.size())) {
						std::cerr << "Size mismatch for " << article << " @ "
								  << records[i].time << '\n';
						return 1;
					}
					const std::string hash = sha1sum(content).substr(0, 16);
					if (hash != records[i].hash) {
						std::cerr << "Hash mismatch for " << article << " @ "
								  << records[i].time << '\n';
						return 1;
					}

					std::string filename = records[i].time + "_" + std::to_string(records[i].author)
						+ "_" + article + ".tex";
					const std::string out_path = out_dir + filename;
					if (!write_file(out_path, content)) {
						std::cerr << "Failed to write " << out_path << '\n';
						return 1;
					}

					++restored;
					if (restored % 5000 == 0)
						std::cout << "Restored " << restored << " files..." << std::endl;
				}
			}
		}

		std::cout << "Restored " << restored << " files from " << db_path << std::endl;
		return 0;
	}
	catch (const SQLite::Exception &e) {
		std::cerr << "SQLite error: " << e.what()
				  << " code=" << e.getErrorCode()
				  << " ext=" << e.getExtendedErrorCode() << '\n';
		return 1;
	}
	catch (const std::exception &e) {
		std::cerr << "Unexpected error: " << e.what() << '\n';
		return 1;
	}
}
