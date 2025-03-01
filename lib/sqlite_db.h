#pragma once

// add or delete elements from a set
template <class T>
inline void change_set(set<T> &s, const unordered_map<T, bool> &change)
{
	for (auto &e : change)
		if (e.second)
			s.insert(e.first);
		else
			s.erase(e.first);
}

// get table of content info from db "chapters", in ascending "order"
inline void db_get_chapters(vecStr_O ids, vecStr_O captions, vecStr_O parts,
							SQLite::Database &db)
{
	SQLite::Statement stmt(db,
		R"(SELECT "id", "order", "caption", "part" FROM "chapters" ORDER BY "order" ASC)");
	vecLong orders;
	while (stmt.executeStep()) {
		ids.push_back(stmt.getColumn(0));
		orders.push_back(stmt.getColumn(1).getInt64());
		captions.push_back(stmt.getColumn(2));
		parts.push_back(stmt.getColumn(3));
	}
	for (Long i = 0; i < size(orders); ++i)
		if (orders[i] != i)
			throw internal_err(SLS_WHERE);
}

// get table of content info from db "parts", in ascending "order"
inline void db_get_parts(vecStr_O ids, vecStr_O captions, SQLite::Database &db)
{
	SQLite::Statement stmt(db,
		R"(SELECT "id", "order", "caption" FROM "parts" ORDER BY "order" ASC)");
	vecLong orders;
	while (stmt.executeStep()) {
		ids.push_back(stmt.getColumn(0));
		orders.push_back(stmt.getColumn(1).getInt64());
		captions.push_back(stmt.getColumn(2));
	}
	for (Long i = 0; i < size(orders); ++i)
		if (orders[i] != i)
			throw internal_err(SLS_WHERE);
}

// for entry files in the folder, make sure it's in db and not deleted
inline void db_check_add_entry_simulate_editor(vecStr_I entries, SQLite::Database &db_rw)
{
	SQLite::Statement stmt_select(db_rw,
		R"(SELECT "caption", "deleted" FROM "entries" WHERE "id"=?;)");
	SQLite::Statement stmt_insert(db_rw,
		R"(INSERT OR REPLACE INTO "entries" ("id", "caption", "draft", "deleted") VALUES (?, ?, 2, 0);)");
	SQLite::Statement stmt_undelete(db_rw,
		R"(UPDATE "entries" SET "deleted"=0 WHERE "id"=?;)");
	Str str, title;

	for (auto &entry : entries) {
		clear(sb) << gv::path_in; sb << "contents/" << entry << ".tex";
		if (!file_exist(sb))
			throw scan_err("文章文件不存在：" + entry + ".tex");
		stmt_select.bind(1, entry);
		bool deleted = false;
		if (!stmt_select.executeStep()) {
			scan_log_warn(u8"内部警告：文章不存在数据库中， 将模拟 editor 添加： " + entry);
			// 从 tex 文件获取标题
			read(str, sb); // read tex file
			CRLF_to_LF(str);
			get_title(title, str);
			// 写入数据库
			stmt_insert.bind(1, entry);
			stmt_insert.bind(2, title);
			stmt_insert.exec(); stmt_insert.reset();
		}
		else {
			deleted = stmt_select.getColumn(1).getInt();
			if (deleted) {
				title = stmt_select.getColumn(0).getString();
				clear(sb) << u8"文章文件存在，但数据库却标记了已删除（将恢复）："
					<< entry << " (" << title << ')';
				db_log_print(sb);
				stmt_undelete.bind(1, entry);
				if (stmt_undelete.exec() != 1) throw internal_err(SLS_WHERE);
				stmt_undelete.reset();
			}
		}
		stmt_select.reset();
	}
}

// sum history.add and history.del for an author for a given time period
// time_start and time_end are in yyyymmddmmss format
inline void author_char_stat(Str_I time_start, Str_I time_end, Str author, SQLite::Database &db_read)
{
	if (time_start.size() != 12 || time_end.size() != 12)
		throw scan_err(u8"日期格式不正确" SLS_WHERE);
	Long authorID = -1;
	SQLite::Statement stmt_author0(db_read,
		R"(SELECT "id", "name" FROM "authors" WHERE "name"=?;)");
	stmt_author0.bind(1, author);
	Long Nfound = 0;
	while (stmt_author0.executeStep()) {
		++Nfound;
		authorID = stmt_author0.getColumn(0).getInt64();
		cout << "作者： " << stmt_author0.getColumn(1).getString() << endl;
		cout << "id: " << authorID << endl;
	}
	if (Nfound > 1)
		throw scan_err(u8"数据库中找到多于一个作者名（精确匹配）： " + author);
	else if (Nfound == 0) {
		cout << "精确匹配失败， 尝试模糊匹配……" << endl;
		SQLite::Statement stmt_author(db_read,
			R"(SELECT "id", "name" FROM "authors" WHERE "name" LIKE ? ESCAPE '\';)");
		replace(author, "_", "\\_"); replace(author, "%", "\\%");
		stmt_author.bind(1, '%' + author + '%');
		Nfound = 0;
		while (stmt_author.executeStep()) {
			++Nfound;
			authorID = stmt_author.getColumn(0).getInt64();
			cout << "作者： " << stmt_author.getColumn(1).getString() << endl;
			cout << "id: " << authorID << endl;
		}
		if (Nfound == 0)
			throw scan_err(u8"数据库中找不到作者名（模糊匹配）： " + author);
		if (Nfound > 1)
			throw scan_err(u8"数据库中找到多于一个作者名（模糊匹配）： " + author);
	}

	sb = R"(SELECT SUM("add"), SUM("del") FROM "history" WHERE "author"=)";
	sb << authorID << R"( AND "time" >= ')" << time_start << R"(' AND "time" <= ')" << time_end << "';";
	cout << "SQL 命令:\n" << sb << endl;
	SQLite::Statement stmt_select(db_read, sb);
	if (!stmt_select.executeStep())
		throw internal_err(SLS_WHERE);

	auto data = stmt_select.getColumn(0);
	Long tot_add = data.isNull() ? 0 : data.getInt64();

	data = stmt_select.getColumn(1);
	Long tot_del = data.isNull() ? 0 : data.getInt64();

	cout << "新增字符：" << tot_add << endl;
	cout << "删除字符：" << tot_del << endl;
	cout << "新增减删除：" << tot_add - tot_del << endl;
}

// make db consistent
// (regenerate derived fields)
// TODO: fix other generated field, i.e. entries.ref_by
inline void arg_fix_db(SQLite::Database &db_rw)
{
	Str refs_str, entry;
	set<Str> refs;
	unordered_map<Str, set<Str>> figs_ref_by, labels_ref_by, bib_ref_by;

	// get all labels.id and figures.id from database, to detect unused ones
	unordered_set<Str> db_figs_unused, db_labels_unused;
	{
		vecStr db_figs0, db_labels0;
		get_column(db_figs0, "figures", "id", db_rw);
		get_column(db_labels0, "labels", "id", db_rw);
		db_figs_unused.insert(db_figs0.begin(), db_figs0.end());
		db_labels_unused.insert(db_labels0.begin(), db_labels0.end());
	}

	// === regenerate "figures.entry", "labels.entry" from "entries.figures", "entries.labels" ======
	cout << R"(regenerate "figures.entry", "labels.entry" from "entries.figures", "entries.labels"...)" << endl;

	SQLite::Statement stmt_select_labels(db_rw, R"(SELECT "id" FROM "entries";)");
	SQLite::Statement stmt_update_labels_entry(db_rw, R"(UPDATE "labels" SET "entry"=? WHERE "id"=?;)");
	unordered_set<Str> labels;
	while (stmt_select_labels.executeStep()) {
		entry = stmt_select_labels.getColumn(0).getString();
		split(labels, stmt_select_labels.getColumn(1));
		for (auto &label : labels) {
			db_labels_unused.erase(label);
			stmt_update_labels_entry.bind(1, entry);
			stmt_update_labels_entry.bind(2, label);
			if (stmt_update_labels_entry.exec() != 1) throw internal_err(SLS_WHERE);
			if (!stmt_update_labels_entry.getChanges())
				throw internal_err("数据库 labels 表格中未找到： " + label + SLS_WHERE);
			stmt_update_labels_entry.reset();
		}
	}

	SQLite::Statement stmt_select_figs(db_rw, R"(SELECT "id", "figures" FROM "entries" WHERE "figures" != '';)");
	SQLite::Statement stmt_update_figs_entry(db_rw, R"(UPDATE "figures" SET "entry"=? WHERE "id"=?;)");
	unordered_set<Str> figs;
	while (stmt_select_figs.executeStep()) {
		entry = stmt_select_figs.getColumn(0).getString();
		split(figs, stmt_select_figs.getColumn(1));
		for (auto &fig_id : figs) {
			db_figs_unused.erase(fig_id);
			stmt_update_figs_entry.bind(1, entry);
			stmt_update_figs_entry.bind(2, fig_id);
			if (stmt_update_figs_entry.exec() != 1) throw internal_err(SLS_WHERE);
			if (!stmt_update_figs_entry.getChanges())
				throw internal_err("数据库 figures 表格中未找到 entries.figures 中的： " + fig_id + SLS_WHERE);
			stmt_update_figs_entry.reset();
		}
	}
	cout << "done!" << endl;

	// === regenerate images.figures from figures.image ========
	unordered_map<Str, set<Str>> images_figures, images_figures_old; // images.figures* generated from figures.image
	SQLite::Statement stmt_select3(db_rw, R"(SELECT "id", "image", "image_old" FROM "figures" WHERE "id" != '';)");
	SQLite::Statement stmt_select4(db_rw, R"(SELECT "figures" FROM "images" WHERE "hash"=?;)");
	vecStr image_old;
	Str fig_id, image_hash;
	while (stmt_select3.executeStep()) {
		fig_id = stmt_select3.getColumn(0).getString();
		image_hash = stmt_select3.getColumn(1).getString();
		split(image_old, stmt_select3.getColumn(2));
		if (size(image_old) > 1)
			throw internal_err(u8"暂时不允许多于一个 image_old！");
		stmt_select4.bind(1, image_hash);
		if (!stmt_select4.executeStep()) {
			clear(sb) << u8"图片 " << fig_id << " 的 image " << image_hash << u8" 不存在！";
			throw internal_err(sb);
		}
		stmt_select4.reset();

		images_figures[image_hash].insert(fig_id);
//		if (!image_alt.empty())
//			images_figures[image_alt[0]].insert(fig_id);
		if (!image_old.empty())
			images_figures_old[image_old[0]].insert(fig_id);
	}
	stmt_select3.reset();

	vecStr db_images;
	get_column(db_images, "images", "hash", db_rw);
	SQLite::Statement stmt_update4(db_rw, R"(UPDATE "images" SET "figures"=?, "figures_old"=? WHERE "hash"=?;)");
	SQLite::Statement stmt_delete4(db_rw, R"(DELETE FROM "images" WHERE "hash"=?;)");
	for (auto &img_hash : db_images) {
		if (!images_figures.count(img_hash) && !images_figures_old.count(img_hash)) {
			clear(sb) << u8"图片 " << img_hash << u8" 没有被 figures 表中任何环境引用（将删除）。";
			db_log_print(sb);
			stmt_delete4.bind(1, img_hash);
			if (stmt_delete4.exec() != 1) throw internal_err(SLS_WHERE);
			stmt_delete4.reset();
		}
		join(sb, images_figures[img_hash]);
		stmt_update4.bind(1, sb);
		join(sb, images_figures_old[img_hash]);
		stmt_update4.bind(2, sb);
		stmt_update4.bind(3, img_hash);
		if (stmt_update4.exec() != 1) throw internal_err(SLS_WHERE);
		stmt_update4.reset();
	}

	cout << "done!" << endl;
}

// upgrade `user-notes/*/cmd_data/scan.db` with the schema of `note-template/cmd_data/scan.db`
// data of `note-template/cmd_data/scan.db` has no effect
inline void migrate_user_db() {
	Str file_old, file;
	vecStr folders;
	folder_list_full(folders, "../user-notes/");
	Str err_msg;
	for (auto &folder: folders) {
		try {
			if (!file_exist(folder + "main.tex") || folder == "../user-notes/note-template/")
				continue;
			file_old = folder + "cmd_data/scan-old.db";
			file = folder + "cmd_data/scan.db";
			if (!file_exist(file))
				throw scan_err("文件不存在：" + file);
			cout << "migrating " << file << endl;
			file_move(file_old, file, true);
			file_copy(file, "../user-notes/note-template/cmd_data/scan.db", true);
			migrate_db(file, file_old);
			file_remove(file_old);
		}
		catch (const std::exception &e) {
			err_msg << e.what() << '\n';
			SLS_WARN(e.what());
		}
	}
	if (!err_msg.empty())
		throw internal_err(err_msg);
}

// `git diff --no-index --word-diff-regex=. file1.tex file2.tex`
// then search for each `{+..+}` and `{-..-}`
inline void str_add_del(Long_O add, Long_O del, Str str_old, Str str_new)
{
	std::regex rm_s(u8"\\s");
	add = del = 0;

	// remove all space, tab, return etc
	str_old = std::regex_replace(str_old, rm_s, "");
	str_new = std::regex_replace(str_new, rm_s, "");
	SLS_ASSERT(utf8::is_valid(str_old));
	SLS_ASSERT(utf8::is_valid(str_new));

	if (str_old == str_new) {
		add = del = 0; return;
	}
	Str str;
	// check if git is installed (only once)
	static bool checked = false;
	if (!checked) {
		exec_str(str, "which git");
		trim(str, " \n");
		if (str.empty())
			throw std::runtime_error("git binary not found!" SLS_WHERE);
	}

	// replace "{+", "+}", "[-", "-]" to avoid conflict
	static const Str file1 = "file_add_del_tmp_file1.txt",
		file2 = "file_add_del_tmp_file2.txt",
		file_diff = "file_add_del_git_diff_output.txt";
	replace(str_old, "{+", u8"{➕"); replace(str_old, "+}", u8"➕}");
	replace(str_old, "[-", u8"[➖"); replace(str_old, "-]", u8"➖]");
	write(str_old, file1);

	replace(str_new, "{+", u8"{➕"); replace(str_new, "+}", u8"➕}");
	replace(str_new, "[-", u8"[➖"); replace(str_new, "-]", u8"➖]");
	write(str_new, file2);

	clear(sb) << "git diff --no-index --word-diff-regex=. "
		<< '"' << file1 << "\" \"" << file2 << '"';
	// git diff will return 0 iff there is no difference
	if (exec_str(str, sb) == 0)
		return;
#ifndef NDEBUG
	write(str, file_diff);
#endif
	if (str.empty()) return;
	// parse git output, find addition
	Long ind = -1;
	while (1) {
		ind = find(str, "{+", ind+1);
		if (ind < 0) break;
		ind += 2;
		Long ind1 = find(str, "+}", ind);
		if (ind1 < 0) {
			file_rm(file1); file_rm(file2);
#ifndef NDEBUG
			file_rm(file_diff);
#endif
			throw std::runtime_error("matching +} not found!" SLS_WHERE);
		}
		add += u8count(str, ind, ind1);
		ind = ind1 + 1;
	}
	// parse git output, find deletion
	ind = -1;
	while (1) {
		ind = find(str, "[-", ind+1);
		if (ind < 0) break;
		ind += 2;
		Long ind1 = find(str, "-]", ind);
		if (ind1 < 0) {
			file_rm(file1); file_rm(file2);
			throw std::runtime_error("matching -] not found!" SLS_WHERE);
		}
		del += u8count(str, ind, ind1);
		ind = ind1 + 1;
	}

	// check
	Long Nchar1 = u8count(str_old), Nchar2 = u8count(str_new);
	Long net_add = Nchar2 - Nchar1;
	if (net_add != add - del) {
		sb << "something wrong" << to_string(net_add) <<
			", add = " << to_string(add) << ", del = " << to_string(del) <<
			", add-del = " << to_string(add - del) << SLS_WHERE;
		throw std::runtime_error(sb);
	}

	file_rm(file1); file_rm(file2);
#ifndef NDEBUG
	file_rm(file_diff);
#endif
}
