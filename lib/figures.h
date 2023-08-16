#pragma once
#include "sqlite_db.h"

// db table "images"
inline void db_update_images(
		Str_I entry, vecStr_I fig_ids,
		const unordered_map<Str, unordered_map<Str,Str>> &fig_ext_hash, // figures.id -> (ext -> hash)
		SQLite::Database &db_rw)
{
	SQLite::Transaction transaction(db_rw);
	SQLite::Statement stmt_select(db_rw,
		R"(SELECT "ext" FROM "images" WHERE "hash"=?;)");
	SQLite::Statement stmt_insert(db_rw,
		R"(INSERT INTO "images" ("hash", "ext", "figure", "figures_aka") VALUES (?,?,?,?);)");
	SQLite::Statement stmt_update(db_rw,
		R"(UPDATE "images" SET "figure"=?, "figures_aka"=? WHERE "hash"=?;)");
	Str db_image_ext;

	for (auto &fig_id :fig_ids) {
		for (auto &ext_hash: fig_ext_hash.at(fig_id)) {
			auto &image_ext = ext_hash.first;
			auto &image_hash = ext_hash.second;
			stmt_select.bind(1, image_hash);
			if (!stmt_select.executeStep()) {
				clear(sb) << "数据库中找不到图片文件（将模拟 editor 添加）：" << image_hash << image_ext;
				db_log(sb);
				stmt_insert.bind(1, image_hash);
				stmt_insert.bind(2, image_ext);
				stmt_insert.bind(3, fig_id);
				stmt_insert.exec(); stmt_insert.reset();
			}
			else {
				db_image_ext = (const char*)stmt_select.getColumn(0);
				if (image_ext != db_image_ext) {
					clear(sb) << u8"图片文件拓展名不允许改变: " << image_hash << '.'
						<< db_image_ext << " -> " << image_ext;
					throw internal_err(sb);
				}
			}
			stmt_select.reset();
		}
	}
	transaction.commit();
}

// db table "figures"
inline void db_update_figures(
	unordered_set<Str> &update_entries, // [out] entries to be updated due to order change
	vecStr_I entries,
	const vector<vecStr> &entry_figs, // entry_figs[i] are the figures.id in entries[i]
	const vector<vecLong> &entry_fig_orders, // entry_fig_orders[i] are the figures.order in entries[i]
	const unordered_map<Str, unordered_map<Str,Str>> &fig_ext_hash, // fig_id -> (ext -> hash)
	SQLite::Database &db_rw)
{
	// cout << "updating db for figures environments..." << endl;
	update_entries.clear();  //entries that needs to rerun autoref(), since label order updated
	Long order;
	SQLite::Transaction transaction(db_rw);
	SQLite::Statement stmt_select0(db_rw,
		R"(SELECT "figures" FROM "entries" WHERE "id"=?;)");
	SQLite::Statement stmt_select1(db_rw,
		R"(SELECT "order", "ref_by", "image", "deleted", "aka" FROM "figures" WHERE "id"=?;)");
	SQLite::Statement stmt_insert(db_rw,
		R"(INSERT INTO "figures" ("id", "entry", "order", "image") VALUES (?, ?, ?, ?);)");
	SQLite::Statement stmt_update(db_rw,
		R"(UPDATE "figures" SET "entry"=?, "order"=?, "image"=?, "deleted"=0 WHERE "id"=?;)");
	SQLite::Statement stmt_update2(db_rw,
		R"(UPDATE "entries" SET "figures"=? WHERE "id"=?;)");
	// get all figure envs defined in `entries`, to detect deleted figures
	// db_xxx[i] are from the same row of "labels" table
	vecStr db_figs, db_fig_entries, db_fig_image;
	unordered_map<Str, Str> db_fig_aka; // figures.id -> figures.aka
	vecBool db_figs_used, figs_used;
	vecLong db_fig_orders;
	vector<vecStr> db_fig_ref_bys;
	set<Str> db_entry_figs;
	for (Long j = 0; j < size(entries); ++j) {
		auto &entry = entries[j];
		stmt_select0.bind(1, entry);
		if (!stmt_select0.executeStep())
			throw scan_err("词条不存在： " + entry);
		parse(db_entry_figs, stmt_select0.getColumn(0));
		stmt_select0.reset();
		for (auto &fig_id: db_entry_figs) {
			db_figs.push_back(fig_id);
			db_fig_entries.push_back(entry);
			stmt_select1.bind(1, fig_id);
			if (!stmt_select1.executeStep())
				throw scan_err("图片标签不存在： fig_" + fig_id);
			db_fig_orders.push_back((int)stmt_select1.getColumn(0));
			db_fig_ref_bys.emplace_back();
			parse(db_fig_ref_bys.back(), stmt_select1.getColumn(1));
			db_fig_image.push_back(stmt_select1.getColumn(2));
			db_fig_aka[fig_id] = stmt_select1.getColumn(4).getString();
			db_figs_used.push_back(!(bool)stmt_select1.getColumn(3).getInt());
			stmt_select1.reset();
		}
	}
	figs_used.resize(db_figs_used.size(), false);
	Str figs_str, image, ext;
	set<Str> new_figs, figs;
	for (Long i = 0; i < size(entries); ++i) {
		auto &entry = entries[i];
		new_figs.clear();
		for (Long j = 0; j < size(entry_figs[i]); ++j) {
			const Str &fig_id = entry_figs[i][j]; order = entry_fig_orders[i][j];
			Long ind = search(fig_id, db_figs);
			auto &ext_hash = fig_ext_hash.at(fig_id);
			if (ext_hash.size() == 1) { // png
				if (!ext_hash.count("png"))
					throw internal_err("db_update_figures(): unexpected fig format!");
				image = ext_hash.at("png"); ext = "png";
			}
			else if (ext_hash.size() == 2) { // svg + pdf
				if (!ext_hash.count("svg") || !ext_hash.count("pdf"))
					throw internal_err("db_update_figures(): unexpected fig format!");
				image = ext_hash.at("pdf"); ext = "pdf";
			}
			if (ind < 0) { // 图片 label 不在 entries.figures 中
				clear(sb) << u8"发现数据库中没有的图片环境（将模拟 editor 添加）："
						  << fig_id << ", " << entry << ", " << to_string(order);
				db_log(sb);
				stmt_insert.bind(1, fig_id);
				stmt_insert.bind(2, entry);
				stmt_insert.bind(3, (int)order);
				stmt_insert.bind(4, image);
				stmt_insert.exec(); stmt_insert.reset();
				new_figs.insert(fig_id);
				continue;
			}
			figs_used[ind] = true;
			bool changed = false;
			// 图片 label 在 entries.figures 中，检查是否未被使用
			if (!db_figs_used[ind]) {
				db_log(u8"发现数据库中未使用的图片被重新使用（将更新）：" + fig_id);
				changed = true;
			}
			if (entry != db_fig_entries[ind]) {
				clear(sb) << u8"发现数据库中图片 entry 改变（将更新）：" << fig_id << ": "
								 << db_fig_entries[ind] << " -> " << entry;
				db_log(sb);
				changed = true;
			}
			if (order != db_fig_orders[ind]) {
				clear(sb) << u8"发现数据库中图片 order 改变（将更新）：" << fig_id << ": "
								 << to_string(db_fig_orders[ind]) << " -> " << to_string(order);
				db_log(sb);
				changed = true;
				// order change means other ref_by entries needs to be updated with autoref() as well.
				if (!gv::is_entire) {
					for (auto &by_entry: db_fig_ref_bys[ind])
						if (search(by_entry, entries) < 0)
							update_entries.insert(by_entry);
				}
			}
			if (image != db_fig_image[ind]) {
				clear(sb) << u8"发现数据库中图片 image 改变（将更新）：" << fig_id << ": "
								 << db_fig_image[ind] << " -> " << image;
				db_log(sb);
				if (!db_fig_aka[fig_id].empty()) {
					clear(sb) << u8"图片环境（" << fig_id << "）是另一环境（" << db_fig_aka[fig_id] << "）的镜像，不支持手动修改图片文件，请撤回 "
							  << image << '.' << ext << "， 并新建一个 figure 环境。 TODO: 其实不应该报错。例如用户替换了图片，应该把原来环境的 fig 重命名，然后把新版本图片用原来的 fig。";
					// TODO: 其实不应该报错。例如用户替换了图片，应该把原来环境的 fig 重命名，然后把新版本图片用原来的 fig。
					throw scan_err(sb);
				}
				changed = true;
			}
			if (changed) {
				stmt_update.bind(1, entry);
				stmt_update.bind(2, (int)order);
				stmt_update.bind(3, image);
				stmt_update.bind(4, fig_id);
				stmt_update.exec(); stmt_update.reset();
			}
		}
		// add entries.figures
		stmt_select0.bind(1, entry);
		if (!new_figs.empty()) {
			if (!stmt_select0.executeStep())
				throw internal_err("db_update_labels(): entry 不存在： " + entry);
			parse(figs, stmt_select0.getColumn(0));
			for (auto &e : new_figs)
				figs.insert(e);
			join(figs_str, figs);
			stmt_update2.bind(1, figs_str);
			stmt_update2.bind(2, entry);
			stmt_update2.exec(); stmt_update2.reset();
		}
		stmt_select0.reset();
	}
	// 检查被删除的图片（如果只被本词条引用， 就留给 autoref() 报错）
	// 这是因为入本词条的 autoref 还没有扫描不确定没有也被删除
	Str ref_by_str;
	SQLite::Statement stmt_update3(db_rw, R"(UPDATE "figures" SET "deleted"=1, "order"=0 WHERE "fig"=?;)");
	for (Long i = 0; i < size(figs_used); ++i) {
		if (!figs_used[i] && db_figs_used[i]) {
			if (db_fig_ref_bys[i].empty() ||
				(db_fig_ref_bys[i].size() == 1 && db_fig_ref_bys[i][0] == db_fig_entries[i])) {
				clear(sb) << u8"检测到 \\label{fig_"
								 << db_figs[i] << u8"}  被删除（图 " << db_fig_orders[i] << u8"）， 将标记未使用";
				db_log(sb);
				// set "figures.entry" = ''
				stmt_update3.bind(1, db_figs[i]);
				stmt_update3.exec(); stmt_update3.reset();
			}
			else {
				join(ref_by_str, db_fig_ref_bys[i], ", ");
				throw scan_err(u8"检测到 \\label{fig_" + db_figs[i] + u8"}  被删除， 但是被这些词条引用。请撤销删除，把引用删除后再试： " + ref_by_str);
			}
		}
	}
	transaction.commit();
	// cout << "done!" << endl;
}

// delete from "images" table, and the image file
// if "images.figure/figures_aka" use this image as "image" or "image_alt",
//     then it must have `figures.deleted==1`
inline void db_delete_images(
		vecStr_I images, // hashes, no extension
		SQLite::Database &db_read, SQLite::Database &db_rw, Str_I fig_id = "")
{
	SQLite::Statement stmt_select(db_read,
		R"(SELECT "figure", "figures_aka", "ext" FROM "images" WHERE "hash"=?;)");
	SQLite::Statement stmt_select2(db_read,
		R"(SELECT "image", "image_alt", "deleted" FROM "figures" WHERE "id"=?;)");
	SQLite::Statement stmt_delete(db_rw, R"(DELETE FROM "images" WHERE "hash"=?;)");
	SQLite::Statement stmt_update(db_rw, R"(UPDATE "figures" SET "image"='' WHERE "id"=?;)");
	SQLite::Statement stmt_update2(db_rw, R"(UPDATE "figures" SET "image_alt"=? WHERE "id"=?;)");
	set<Str> figures, figures_image_alt; // images.figure + images.figures_aka

	for (auto &image : images) {
		cout << "deleting image with hash: " << image << endl;
		stmt_select.bind(1, image);
		if (!stmt_select.executeStep()) {
			clear(sb) << u8"要删除的图片文件 hash 不存在（将忽略）：" << image << SLS_WHERE;
			db_log(sb);
			continue;
		}
		const char *figure = stmt_select.getColumn(0); // images.figure
		parse(figures, stmt_select.getColumn(1)); // images.figures_aka
		figures.insert(figure);
		const char *ext = stmt_select.getColumn(2); // images.ext
		stmt_select.reset();
		if (figures.empty()) {
			clear(sb) << u8"要删除的 image 不属于任何环境， 即 images.figure 和 .figures_aka 都为空（将继续删除）：" << image << SLS_WHERE;
			db_log(sb);
			stmt_delete.bind(1, image);
			stmt_delete.exec(); stmt_delete.reset();
			clear(sb) << gv::path_in << "figures/" << image << ext;
			file_remove(sb);
			cout << "正在删除：" << image << ext << endl;
		}

		// check figures.deleted
		for (auto &fig : figures) {
			bool has_aka = (fig == figure);
			stmt_select2.bind(1, fig);
			if (!stmt_select2.executeStep()) {
				if (has_aka)
					clear(sb) << u8"images.figure 中";
				else
					clear(sb) << u8"images.figures_aka 中";
				sb << u8"引用 image " << image << u8" 的图片环境 " << fig << u8" 不存在（将忽略）"
					<< SLS_WHERE;
				db_log(sb);
				figures.erase(fig);
				continue;
			}
			// const char *fig_image = stmt_select2.getColumn(0);
			parse(figures_image_alt, stmt_select2.getColumn(1));
			bool deleted = (int)stmt_select2.getColumn(2);
			if (!deleted) {
				bool in_figures_image = (fig == figure);
				bool in_figures_image_alt = figures_image_alt.count(fig);
				if (in_figures_image || in_figures_image_alt)
					throw scan_err(u8"无法删除被 figure 环境使用的 image，请先删除：" + image);
			}
		}

		// now delete db
		for (auto &fig : figures) {
			// bool has_aka = (fig == figure);
			stmt_select2.bind(1, fig);
			const char *fig_image = stmt_select2.getColumn(0);
			parse(figures_image_alt, stmt_select2.getColumn(1));
			// bool deleted = (int)stmt_select2.getColumn(3);

			bool in_figures_image = (fig == fig_image);
			bool in_figures_image_alt = figures_image_alt.count(fig);

			if (in_figures_image) {
				// set figures.image = ''
				stmt_update.bind(1, fig);
				stmt_update.exec(); stmt_update.reset();
			}
			if (in_figures_image_alt) {
				join(sb, figures_image_alt);
				stmt_update2.bind(1, sb);
				stmt_update2.bind(2, fig);
				stmt_update2.exec(); stmt_update2.reset();
			}
		}

		// delete from images
		stmt_delete.bind(1, image);
		stmt_delete.exec(); stmt_delete.reset();
		clear(sb) << gv::path_in << "figures/" << image << '.' << ext;
		file_remove(sb);
		clear(sb) << gv::path_out << image << '.' << ext;
		file_remove(sb);
	}
}
