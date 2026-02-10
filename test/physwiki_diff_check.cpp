#include "../SLISC/str/str.h"
#include "../SLISC/str/str_diff_patch2.h"
#include "../SLISC/util/sha1sum.h"

#include <SQLiteCpp/Database.h>
#include <SQLiteCpp/Statement.h>
#include <algorithm>
#include <iostream>
#include <memory>
#include <vector>

using namespace slisc;

struct GroupInfo {
	std::string entry;
	int64_t max_size = 0;
	int64_t count = 0;
};

struct Record {
	int64_t id = 0;
	std::string time;
	int64_t author = 0;
	int64_t size = 0;
	std::string hash;
	bool last_null = true;
	int64_t last_id = 0;
	std::string diff_json;
};

static void apply_diff(Str_O out, const vector<tuple<size_t, size_t, Str>> &diff)
{
	for (auto it = diff.rbegin(); it != diff.rend(); ++it)
		out.replace(get<0>(*it), get<1>(*it), get<2>(*it));
}

int main()
{
	const std::string db_path = "/mnt/g/github/PhysWikiScan/data/PhysWiki-backup.db";
	if (!file_exist(db_path)) {
		std::cerr << "Database not found: " << db_path << '\n';
		return 1;
	}

	std::unique_ptr<SQLite::Database> db;
	try {
		const std::string db_uri = "file:" + db_path + "?immutable=1";
		db.reset(new SQLite::Database(db_uri, SQLite::OPEN_READONLY | SQLite::OPEN_URI));
	}
	catch (const std::exception &) {
		db.reset(new SQLite::Database(db_path, SQLite::OPEN_READONLY));
	}
	db->exec("PRAGMA temp_store = MEMORY;");

	std::vector<GroupInfo> groups;
	SQLite::Statement stmt_group(*db,
		R"(SELECT "entry", MAX("size"), COUNT(*) FROM "backup_files" GROUP BY "entry";)");
	while (stmt_group.executeStep()) {
		GroupInfo info;
		info.entry = stmt_group.getColumn(0).getString();
		info.max_size = stmt_group.getColumn(1).getInt64();
		info.count = stmt_group.getColumn(2).getInt64();
		if (info.count >= 2)
			groups.push_back(info);
	}

	if (groups.empty()) {
		std::cerr << "No entries with at least two versions, skipping." << std::endl;
		return 0;
	}

	std::sort(groups.begin(), groups.end(), [](const GroupInfo &a, const GroupInfo &b) {
		if (a.max_size != b.max_size)
			return a.max_size < b.max_size;
		return a.entry < b.entry;
	});
	if (groups.size() > 100)
		groups.resize(100);

	size_t processed = 0;
	for (const auto &group : groups) {
		SQLite::Statement stmt(*db,
			R"(SELECT "id", "time", "author", "size", "hash", "last_id", "diff"
			   FROM "backup_files"
			   WHERE "entry"=?
			   ORDER BY "time" ASC, "author" ASC;)");
		stmt.bind(1, group.entry);

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
		if (records.size() < 2)
			continue;

		Str current;
		for (size_t i = 0; i < records.size(); ++i) {
			const auto &rec = records[i];
			if (i == 0) {
				if (!rec.last_null) {
					std::cerr << "First version has last_id for " << group.entry << '\n';
					return 1;
				}
			}
			else if (rec.last_id != records[i - 1].id) {
				std::cerr << "last_id mismatch for " << group.entry << " @ " << rec.time << '\n';
				return 1;
			}

			vector<tuple<size_t, size_t, Str>> diff;
			str_diff_deserialize(diff, rec.diff_json);
			Str reconstructed = current;
			apply_diff(reconstructed, diff);

			if (rec.size != static_cast<int64_t>(reconstructed.size())) {
				std::cerr << "Size mismatch for " << group.entry << " @ " << rec.time << '\n';
				return 1;
			}
			const Str hash = sha1sum(reconstructed).substr(0, 16);
			if (hash != rec.hash) {
				std::cerr << "Hash mismatch for " << group.entry << " @ " << rec.time << '\n';
				return 1;
			}
			if (!is_valid(reconstructed)) {
				std::cerr << "Invalid UTF-8 for " << group.entry << " @ " << rec.time << '\n';
				return 1;
			}

			vector<tuple<size_t, size_t, Str>> diff_check;
			str_diff(diff_check, current, reconstructed);
			Str serialized;
			str_diff_serialize(serialized, diff_check);
			vector<tuple<size_t, size_t, Str>> decoded;
			str_diff_deserialize(decoded, serialized);
			Str patched = current;
			apply_diff(patched, decoded);
			if (patched != reconstructed) {
				std::cerr << "Diff apply mismatch for " << group.entry << " @ " << rec.time << '\n';
				return 1;
			}

			current.swap(reconstructed);
		}

		++processed;
		if (processed % 10 == 0)
			std::cout << "Checked " << processed << " groups..." << std::endl;
	}

	std::cout << "Processed " << processed << " groups." << std::endl;
	return 0;
}
