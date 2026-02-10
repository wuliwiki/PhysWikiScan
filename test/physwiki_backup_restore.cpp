#include "../SLISC/str/str.h"
#include "../SLISC/str/str_diff_patch2.h"
#include "../SLISC/util/sha1sum.h"

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

int main()
{
	try {
		setenv("SQLITE_TMPDIR", "/tmp", 1);
		const std::string dir = "/mnt/g/github/PhysWiki-backup/";
		const std::string db_path = dir + "PhysWiki-backup.db";

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

		SQLite::Statement stmt_article(*db,
			R"(SELECT "article_id" FROM "backup_files" GROUP BY "article_id" ORDER BY "article_id" ASC;)");

		size_t restored = 0;
		while (stmt_article.executeStep()) {
			const std::string article = stmt_article.getColumn(0).getString();
			SQLite::Statement stmt(*db,
				R"(SELECT "id", "timestamp", "author_id", "size", "hash", "prev_ver", "diff"
				   FROM "backup_files"
				   WHERE "article_id"=?
				   ORDER BY "timestamp" ASC, "author_id" ASC;)");
			stmt.bind(1, article);

			struct Record {
				int64_t id = 0;
				std::string timestamp;
				int64_t author_id = 0;
				int64_t size;
				std::string hash;
				std::string diff_json;
				int64_t prev_ver = 0;
				bool prev_null = true;
			};
			std::vector<Record> records;
			while (stmt.executeStep()) {
				Record rec;
				rec.id = stmt.getColumn(0).getInt64();
				rec.timestamp = stmt.getColumn(1).getString();
				rec.author_id = stmt.getColumn(2).getInt64();
				rec.size = stmt.getColumn(3).getInt64();
				rec.hash = stmt.getColumn(4).getString();
				rec.prev_null = stmt.getColumn(5).isNull();
				rec.prev_ver = rec.prev_null ? 0 : stmt.getColumn(5).getInt64();
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
					if (!records[i].prev_null) {
						std::cerr << "First version has prev_ver for " << article << '\n';
						return 1;
					}
				}
				else if (records[i].prev_ver != records[i - 1].id) {
					std::cerr << "prev_ver mismatch order for " << article << " @ "
							  << records[i].timestamp << '\n';
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
					if (rec.prev_null)
						break;
					cur = rec.prev_ver;
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
							  << records[i].timestamp << '\n';
					return 1;
				}
				const std::string hash = sha1sum(content).substr(0, 16);
				if (hash != records[i].hash) {
					std::cerr << "Hash mismatch for " << article << " @ "
							  << records[i].timestamp << '\n';
					return 1;
				}

				std::string filename = records[i].timestamp + "_" + std::to_string(records[i].author_id)
					+ "_" + article + ".tex";
				if (!write_file(dir + filename, content)) {
					std::cerr << "Failed to write " << filename << '\n';
					return 1;
				}

				++restored;
				if (restored % 5000 == 0)
					std::cout << "Restored " << restored << " files..." << std::endl;
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
