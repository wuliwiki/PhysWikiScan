#pragma once

// search all commands
inline void all_commands(vecStr_O commands, Str_I in_path)
{
	vecStr fnames;
	Str str, name;
	file_list_ext(fnames, in_path, "tex");
	for (Long i = 0; i < size(fnames); ++i) {
		read(str, in_path + fnames[i]);
		Long ind0 = 0;
		while (1) {
			ind0 = find(str, '\\', ind0);
			if (ind0 < 0)
				break;
			command_name(name, str, ind0);
			if (!search(name, commands))
				commands.push_back(name);
			++ind0;
		}
	}
}

// count number of Chinese characters (including punc)
inline void word_count(SQLite::Database &db_read)
{
	vecStr entries;
	Str str;
	Long N = 0;
	get_column(entries, "entries", "id", db_read);
	for (Long i = 0; i < size(entries); ++i) {
		read(str, gv::path_in + "contents/" + entries[i] + ".tex");
		rm_comments(str);
		for (Long j = 0; j < (Long) str.size(); ++j) {
			if (!is_char8_start(str, j)) continue;
			if (is_chinese(str, j) || is_chinese_punc(str, j))
				++N;
		}
	}
	cout << u8"中文字符数 (含标点): " << N << endl;
}

inline void arg_stat(Str_I time_start, Str_I time_end, SQLite::Database &db_read)
{
	SQLite::Statement stmt_select(db_read,
		R"(SELECT "author", "entry" FROM "history" WHERE "time">=? AND "time"<=?;)");
	SQLite::Statement stmt_select2(db_read,
		R"(SELECT "salary", "name" FROM "authors" WHERE "id"=?;)");
	SQLite::Statement stmt_select3(db_read,
		R"(SELECT "license" FROM "entries" WHERE "id"=?;)");
	SQLite::Statement stmt_select4(db_read,
		R"(SELECT "begin", "percent" FROM "discount" WHERE "entry"=? ORDER BY "begin" DESC;)");

	// ---- raw data ----
	// entry -> (license, discount)
	unordered_map<Str, pair<Str, Long>> entry_info; // all entries envolved
	// author -> (name, salary, (entry -> minutes))
	unordered_map<Str, tuple<Str, Long, unordered_map<Str, Long>>> author_info;

	stmt_select.bind(1, time_start);
	stmt_select.bind(2, time_end);
	while (stmt_select.executeStep()) {
		const Str &author = stmt_select.getColumn(0);
		const Str &entry = stmt_select.getColumn(1);
		// get entry info
		if (!entry_info.count(entry)) {
			// entry.license
			stmt_select3.bind(1, entry);
			stmt_select3.executeStep();
			const Str &license = stmt_select3.getColumn(0).getString();
			entry_info[entry].first = license;
			stmt_select3.reset();
			// discount
			stmt_select4.bind(1, entry);
			while (stmt_select4.executeStep()) {
				const Str &discount_begin = stmt_select4.getColumn(1);
				if (discount_begin > time_end) continue;
				else if (discount_begin > time_start)
					throw internal_err(u8"暂不支持词条报酬折扣在所选时间内具有多个值： entry=" + entry);
				else {
					Long discount = (int)stmt_select4.getColumn(1);
					entry_info[entry].second = discount;
					break;
				}
			}
		}
		// get authors.salary
		if (!author_info.count(author)) {
			stmt_select2.bind(1, author);
			stmt_select2.executeStep();
			Long salary = (int)stmt_select2.getColumn(0);
			const Str &name = stmt_select2.getColumn(1);
			get<0>(author_info[author]) = name;
			get<1>(author_info[author]) = salary;
			stmt_select2.reset();
		}
		// author -> (entry -> minutes)
		get<2>(author_info[author])[entry] += 5;
	}
	stmt_select.reset();

	// print author stat
	for (auto &e : author_info) {
		auto &author = e.first;
		auto &author_name = get<0>(e.second);
		cout << "\n---------- " << author << '.' << author_name << " (" << get<1>(e.second) << " rmb/h) -------" << endl;
		for (auto &entry_minute : get<2>(e.second)) {
			cout << entry_minute.first << " -- " << entry_minute.second << endl;
		}
		// TODO: print total payment for this author
	}
}
