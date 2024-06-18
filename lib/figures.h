#pragma once
#include "sqlite_db.h"

// replace figure environment with html code
// copy image files to gv::path_out
inline void figure_env(
		unordered_set<Str> &img_to_delete, // [out] image files to delete
		unordered_map<Str, unordered_map<Str, Str>> &fig_ext_hash, // [append] figures.id -> (ext -> hash)
		Str_IO str, // on input, all `\label{}` already deleted
		vecStr_O fig_captions, // [out] figures.caption
		Str_I entry,
		vecStr_I fig_ids, // parsed from env_labels(), with the correct order
		bool is_eng, // english mode
		SQLite::Database &db_read)
{
	Intvs intvFig;
	Str fig_fname, fname_in, fname_out, href, ext, caption, widthPt;
	Str fname_in2, str_mod, tex_fname, caption_div, image_hash, aka;
	fig_captions.clear();
	static const Str tmp1 = "id = \"fig_";
	SQLite::Statement stmt_select(db_read,
		R"(SELECT "aka" FROM "figures" WHERE "id"=?;)");
	SQLite::Statement stmt_select2(db_read,
		R"(SELECT "hash" FROM "images" WHERE "ext"='svg' AND "figure"=?;)");

	Long Nfig = find_env(intvFig, str, "figure", 'o');
	if (size(fig_ids) != Nfig)
		throw scan_err(u8"请确保每个图片环境都有一个 \\label{} 标签");

	for (Long i_fig = Nfig - 1; i_fig >= 0; --i_fig) {
		const Str &fig_id = fig_ids[i_fig];

		// verify label
		Long ind_label = rfind(str, tmp1, intvFig.L(i_fig));
		if (ind_label < 0 || (i_fig > 0 && ind_label < intvFig.R(i_fig - 1)))
			throw scan_err(u8"图片必须有标签 \\label{}， 请使用上传图片按钮。");
		ind_label += size(tmp1);
		Long ind_label_end = find(str, '\"', ind_label);
		SLS_ASSERT(ind_label_end > 0);
		const Str &fig_id0 = str.substr(ind_label, ind_label_end - ind_label);
		SLS_ASSERT(fig_id0 == fig_id);

		// get width of figure
		Long ind0 = find(str, "width", intvFig.L(i_fig)) + 5;
		ind0 = expect(str, "=", ind0);
		ind0 = find_num(str, ind0);
		Doub width = str2double(str, ind0); // figure width in cm
		if (width > 14.25)
			throw scan_err(clear(sb) << u8"图" << i_fig + 1 << u8"尺寸不能超过 14.25cm");

		// get file name of figure
		Long indName1 = find(str, "figures/", ind0) + 8;
		Long indName2 = find(str, '}', ind0) - 1;
		if (indName1 < 0 || indName2 < 0)
			throw scan_err(u8"读取图片名错误");
		fig_fname = str.substr(indName1, indName2 - indName1 + 1);
		trim(fig_fname);

		// get ext (png or svg)
		Long Nname = size(fig_fname);
		if (fig_fname.substr(Nname - 4, 4) == ".png") {
			ext = "png";
			fig_fname = fig_fname.substr(0, Nname - 4);
			clear(fname_in) << gv::path_in << "figures/" << fig_fname << "." << ext;
			if (!file_exist(fname_in))
				throw internal_err(u8"图片 \"" + fname_in + u8"\" 未找到");
			image_hash = sha1sum_f(fname_in).substr(0, 16);
			fig_ext_hash[fig_id][ext] = image_hash;
			// rename figure files with hash
			clear(fname_in2) << gv::path_in << "figures/" << image_hash << '.' << ext;
			if (fig_fname != image_hash) {
				clear(sb) << u8"发现图片 " << fig_fname << '.' << ext << u8" 不是 sha1sum 的前 16 位 "
						  << image_hash << u8" ，将重命名。";
				scan_log_warn(sb);
				file_copy(fname_in2, fname_in, true);
				img_to_delete.insert(fname_in);
				// modify .tex file
				clear(tex_fname) << gv::path_in << "contents/" << entry << ".tex";
				read(str_mod, tex_fname);
				clear(sb) << fig_fname << '.' << ext;
				clear(sb1) << image_hash << '.' << ext;
				replace(str_mod, sb, sb1);
				write(str_mod, tex_fname);
			}
		}
		else if (fig_fname.substr(Nname - 4, 4) == ".pdf") {
			fig_fname = fig_fname.substr(0, Nname - 4);

			// ==== pdf ====
			ext = "pdf";
			clear(fname_in) << gv::path_in << "figures/" << fig_fname << "." << ext;
			if (!file_exist(fname_in))
				throw internal_err(u8"图片 \"" + fname_in + u8"\" 未找到");
			image_hash = sha1sum_f(fname_in).substr(0, 16);
			fig_ext_hash[fig_id][ext] = image_hash;

			// rename figure files with hash
			if (fig_fname != image_hash) {
				clear(sb) << gv::path_in << "figures/" << image_hash << "." << ext;
				file_copy(sb, fname_in, true);
				img_to_delete.insert(fname_in);
				// modify .tex file
				clear(tex_fname) << gv::path_in << "contents/" << entry << ".tex";
				read(str_mod, tex_fname);
				clear(sb) << fig_fname << '.' << ext;
				clear(sb1) << image_hash << '.' << ext;
				replace(str_mod, sb, sb1);
				write(str_mod, tex_fname);
				clear(sb) << u8"检测到图片文件名 " << fname_in << u8"不等于图片 sha1 的前 16 位："
					<< image_hash << u8" 已经重命名，请关闭并重新打开文章。";
				throw internal_err(sb);
			}

			// ==== svg =====
			ext = "svg";

			// if current record has non-empty "aka", then use that figure
			stmt_select.bind(1, fig_id);
			if (!stmt_select.executeStep())
				throw internal_err("图片不存在数据库中：" + fig_id);
			aka = stmt_select.getColumn(0).getString();
			stmt_select.reset();

			if (aka.empty())
				stmt_select2.bind(1, fig_id);
			else
				stmt_select2.bind(1, aka);
			if (!stmt_select2.executeStep()) {
				clear(sb) << u8"pdf 图片 " << fname_in;
				if (!aka.empty())
					sb << u8"（aka " << aka << u8"）";
				sb << u8" 在数据库中找不到对应的 svg";
				throw internal_err(sb);
			}
			image_hash = stmt_select2.getColumn(0).getString();
			stmt_select2.reset();

			if (size(image_hash) != 16) {
				clear(sb) << u8"图片环境 " << fig_id << u8" 的 " << image_hash << u8".svg 长度不对。";
				throw internal_err(sb);
			}

			fig_ext_hash[fig_id][ext] = image_hash;
			clear(fname_in2) << gv::path_in << "figures/" << image_hash << '.' << ext;
		}
		else
			throw internal_err(u8"图片格式暂不支持：" + fig_fname);

		// copy image file
		clear(fname_out) << gv::path_out << image_hash << '.' << ext;
		if (!file_exist(fname_out))
			file_copy(fname_out, fname_in2, true);

		// get caption of figure
		ind0 = find_command(str, "caption", intvFig.L(i_fig));
		if (ind0 < 0 || ind0 > intvFig.R(i_fig))
			caption.clear();
		else
			command_arg(caption, str, ind0);
		if (find(caption, "\\footnote") >= 0)
			throw scan_err(u8"图片标题中不能添加 \\footnote{}");

		// replace fig env with html code
		widthPt = num2str((33 / 14.25 * width * 100)/100.0, 4);
		href.clear(); href << gv::url << image_hash << "." << ext;
		caption_div.clear();
		fig_captions.push_back(caption);
		if (!caption.empty())
			caption.insert(0, u8"：");
		clear(sb) << R"(<div class = "w3-content" style = "max-width:)" << widthPt << "em;\">\n"
			"<a href=\"" << href << R"(" target = "_blank"><img src = ")" << href
			<< "\" alt = \"" << (is_eng? "Fig" : u8"图")
			<< "\" style = \"width:100%;\"></a>\n</div>\n"
			<< "<div align = \"center\"> "
			<< (is_eng ? "Fig. " : u8"图 ") << i_fig + 1
			<< caption << "</div>";
		str.replace(intvFig.L(i_fig), intvFig.R(i_fig) - intvFig.L(i_fig) + 1, sb);
	}
}

// update db table "images"
inline void db_update_images(
		Str_I entry, vecStr_I fig_ids,
		const unordered_map<Str, unordered_map<Str,Str>> &fig_ext_hash, // figures.id -> (ext -> hash)
		SQLite::Database &db_rw)
{
	SQLite::Statement stmt_select(db_rw,
		R"(SELECT "ext", "figure" FROM "images" WHERE "hash"=?;)");
	SQLite::Statement stmt_select2(db_rw,
		R"(SELECT "aka" FROM "figures" WHERE "id"=?;)");
	SQLite::Statement stmt_insert(db_rw,
		R"(INSERT OR REPLACE INTO "images" ("hash", "ext", "figure") VALUES (?,?,?);)");
	SQLite::Statement stmt_update(db_rw, R"(UPDATE "images" SET "figure"=? WHERE "hash"=?;)");

	for (auto &fig_id : fig_ids) {
		stmt_select2.bind(1, fig_id);
		if (!stmt_select2.executeStep()) throw internal_err(SLS_WHERE);
		const Str &fig_aka = stmt_select2.getColumn(0);
		stmt_select2.reset();
		for (auto &ext_hash: fig_ext_hash.at(fig_id)) {
			auto &image_ext = ext_hash.first;
			auto &image_hash = ext_hash.second;
			stmt_select.bind(1, image_hash);
			if (!stmt_select.executeStep()) {
				clear(sb) << "内部警告：数据库中找不到图片文件（将模拟 editor 添加）：" << image_hash << '.' << image_ext;
				scan_log_warn(sb);
				stmt_insert.bind(1, image_hash);
				stmt_insert.bind(2, image_ext);
				stmt_insert.bind(3, "fig_id");
				stmt_insert.exec(); stmt_insert.reset();
			}
			else {
				const Str &db_image_ext = stmt_select.getColumn(0);
				const Str &db_image_figure = stmt_select.getColumn(1);
				if (image_ext != db_image_ext) {
					clear(sb) << u8"图片文件拓展名不允许手动改变： " << image_hash << '.'
						<< db_image_ext << " -> " << image_ext;
					throw internal_err(sb);
				}
				if (fig_aka.empty()) {
					if (db_image_figure != fig_id) {
						clear(sb) << u8"检测到 images.figure 改变（将更新）： "
							<< image_hash << '.' << db_image_figure << " -> " << fig_id;
						db_log_print(sb);
						stmt_update.bind(1, fig_id);
						stmt_update.bind(2, image_hash);
						if (stmt_update.exec() != 1) throw internal_err(SLS_WHERE);
						stmt_update.reset();
					}
				}
				else if (db_image_figure != fig_aka) {
					clear(sb) << u8"检测到 images.figure 改变（将更新）： "
						<< image_hash << '.' <<  db_image_figure << " -> " << fig_aka;
					db_log_print(sb);
					stmt_update.bind(1, fig_id);
					stmt_update.bind(2, fig_aka);
					if (stmt_update.exec() != 1) throw internal_err(SLS_WHERE);
					stmt_update.reset();
				}
			}
			stmt_select.reset();
		}
	}
}

// db table "figures"
inline void db_update_figures(
	unordered_set<Str> &update_entries, // [out] entries to be updated due to order change
	vecStr_I entries,
	const vector<vecStr> &entry_figs, // entry_figs[i] are the figures.id in entries[i], in order of appearance
	const vector<vecStr> &entry_fig_captions, // entry_fig_captions[i][j] is the figures.caption of entry_figs[i][j]
	const unordered_map<Str, unordered_map<Str,Str>> &fig_ext_hash, // fig_id -> (ext -> hash)
	SQLite::Database &db_rw)
{
	// cout << "updating db for figures environments..." << endl;
	update_entries.clear();  //entries that needs to rerun autoref(), since label order updated
	Long order;
	SQLite::Statement stmt_select(db_rw,
		R"(SELECT "id" FROM "figures" WHERE "entry"=?;)");
	SQLite::Statement stmt_select1(db_rw,
		R"(SELECT "order", "image", "deleted", "aka", "caption" FROM "figures" WHERE "id"=?;)");
	SQLite::Statement stmt_select2(db_rw,
		R"(SELECT "entry" FROM "entry_refs" WHERE "label"=?;)");
	SQLite::Statement stmt_insert(db_rw,
		R"(INSERT OR REPLACE INTO "figures" ("id", "entry", "order", "image") VALUES (?, ?, ?, ?);)");
	SQLite::Statement stmt_update(db_rw,
		R"(UPDATE "figures" SET "entry"=?, "order"=?, "image"=?, "caption"=?, "deleted"=0 WHERE "id"=?;)");
	// get all figure envs defined in `entries`, to detect deleted figures
	// db_xxx[i] are from the same row of "labels" table
	vecStr db_figs, db_fig_entries, db_fig_image, db_figs_caption;
	unordered_map<Str, Str> db_fig_aka; // figures.id -> figures.aka
	vecBool db_figs_deleted, figs_used;
	vecLong db_fig_orders;
	vector<vecStr> db_fig_ref_bys;
	set<Str> db_entry_figs;
	for (Long j = 0; j < size(entries); ++j) {
		auto &entry = entries[j];
		stmt_select.bind(1, entry);
		while (stmt_select.executeStep())
			db_entry_figs.insert(stmt_select.getColumn(0));
		stmt_select.reset();
		for (auto &fig_id: db_entry_figs) {
			db_figs.push_back(fig_id);
			db_fig_entries.push_back(entry);
			stmt_select1.bind(1, fig_id);
			if (!stmt_select1.executeStep()) {
				scan_log_warn("entries.figures 中图片标签不存在（将忽略，请手动删除）： fig_" + fig_id);
				stmt_select1.reset();
				continue;
			}
			db_fig_orders.push_back(stmt_select1.getColumn(0).getInt64());
			db_fig_image.push_back(stmt_select1.getColumn(1));
			db_figs_deleted.push_back(stmt_select1.getColumn(2).getInt());
			db_fig_aka[fig_id] = stmt_select1.getColumn(3).getString();
			db_figs_caption.push_back(stmt_select1.getColumn(4));
			stmt_select1.reset();

			stmt_select2.bind(1, "fig_" + fig_id);
			db_fig_ref_bys.emplace_back();
			auto &back = db_fig_ref_bys.back();
			while (stmt_select2.executeStep())
				back.push_back(stmt_select2.getColumn(0).getString());
			stmt_select2.reset();
		}
	}
	figs_used.resize(db_figs_deleted.size(), false);
	Str image, ext;
	set<Str> new_figs, figs;
	for (Long i = 0; i < size(entries); ++i) {
		auto &entry = entries[i];
		new_figs.clear();
		for (Long j = 0; j < size(entry_figs[i]); ++j) {
			const Str &fig_id = entry_figs[i][j];
			const Str &fig_caption = entry_fig_captions[i][j];
			order = j+1;
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
				clear(sb) << u8"内部警告：发现数据库中没有的图片环境（将模拟 editor 添加）："
						  << fig_id << ", " << entry << ", " << to_string(order);
				scan_log_warn(sb);
				stmt_insert.bind(1, fig_id);
				stmt_insert.bind(2, entry);
				stmt_insert.bind(3, (int64_t)order);
				stmt_insert.bind(4, image);
				stmt_insert.exec(); stmt_insert.reset();
				new_figs.insert(fig_id);
				continue;
			}
			figs_used[ind] = true;
			bool changed = false;
			// 图片 label 在 entries.figures 中，检查是否未被使用
			if (db_figs_deleted[ind]) {
				db_log_print(u8"发现数据库中未使用的图片被重新使用（将更新）：" + fig_id);
				changed = true;
			}
			if (fig_caption != db_figs_caption[ind]) {
				clear(sb) << u8"发现数据库中图片标题改变（将更新）："
					<< db_figs_caption[ind] << " -> " << fig_caption;
				db_log_print(sb);
				changed = true;
			}
			if (entry != db_fig_entries[ind]) {
				clear(sb) << u8"发现数据库中图片 entry 改变（将更新）：" << fig_id << ": "
								 << db_fig_entries[ind] << " -> " << entry;
				db_log_print(sb);
				changed = true;
			}
			if (order != db_fig_orders[ind]) {
				clear(sb) << u8"发现数据库中图片 order 改变（将更新）：" << fig_id << ": "
								 << to_string(db_fig_orders[ind]) << " -> " << to_string(order);
				db_log_print(sb);
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
				db_log_print(sb);
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
				stmt_update.bind(2, (int64_t)order);
				stmt_update.bind(3, image);
				stmt_update.bind(4, fig_caption);
				stmt_update.bind(5, fig_id);
				if (stmt_update.exec() != 1) throw internal_err(SLS_WHERE);
				stmt_update.reset();
			}
		}
		// add entries.figures
		stmt_select.bind(1, entry);
		if (!new_figs.empty()) {
			while (stmt_select.executeStep())
				figs.insert(stmt_select.getColumn(0));
			for (auto &e : new_figs)
				figs.insert(e);
		}
		stmt_select.reset();
	}
	// 检查被删除的图片（如果只被本文引用， 就留给 autoref() 报错）
	// 这是因为入本文的 autoref 还没有扫描不确定没有也被删除
	Str ref_by_str;
	SQLite::Statement stmt_update3(db_rw, R"(UPDATE "figures" SET "deleted"=1, "order"=0 WHERE "id"=?;)");
	for (Long i = 0; i < size(figs_used); ++i) {
		if (!figs_used[i] && !db_figs_deleted[i]) {
			if (db_fig_ref_bys[i].empty() ||
				(db_fig_ref_bys[i].size() == 1 && db_fig_ref_bys[i][0] == db_fig_entries[i])) {
				clear(sb) << u8"检测到 \\label{fig_"
					<< db_figs[i] << u8"}  被删除（图 " << db_fig_orders[i] << u8"）， 将标记未使用";
				db_log_print(sb);
				stmt_update3.bind(1, db_figs[i]);
				if (stmt_update3.exec() != 1) throw internal_err(SLS_WHERE);
				stmt_update3.reset();
			}
			else {
				join(ref_by_str, db_fig_ref_bys[i], ", ");
				throw scan_err(u8"检测到 \\label{fig_" + db_figs[i] + u8"}  被删除， 但是被这些文章引用。请撤销删除，把引用删除后再试： " + ref_by_str);
			}
		}
	}
}

// delete from "images" table, and the image file
// if "images.ext" != pdf/svg/png, delete without checking anything else
// "images.figure" must have `figures.deleted==1` and must not be violate `figures.last` foreign key
// check figures.image foreign key (might > 1), figure must have `figures.deleted==1`
inline void db_delete_images(
		vecStr_I images, // hashes, no extension
		SQLite::Database &db_rw, Str_I fig_id = "")
{
	SQLite::Statement stmt_select(db_rw,
		R"(SELECT "figure", "ext" FROM "images" WHERE "hash"=?;)");
	SQLite::Statement stmt_select1(db_rw,
		R"(SELECT "figure" FROM "images" WHERE "entry"=?;)");
	SQLite::Statement stmt_select2(db_rw,
		R"(SELECT "image", "deleted" FROM "figures" WHERE "id"=?;)");
	SQLite::Statement stmt_select3(db_rw,
		R"(SELECT "id" FROM "figures" WHERE "last"=?;)");
	SQLite::Statement stmt_select4(db_rw,
		R"(SELECT "id" FROM "figures" WHERE "image"=?;)");
	SQLite::Statement stmt_delete(db_rw, R"(DELETE FROM "images" WHERE "hash"=?;)");
	SQLite::Statement stmt_update(db_rw, R"(UPDATE "figures" SET "image"='' WHERE "id"=?;)");
	set<Str> figures; // images.figure + images.figures_aka

	for (auto &image : images) {
		scan_log_print("尝试删除图片 hash: " + image);

		// check existence
		stmt_select.bind(1, image);
		if (!stmt_select.executeStep()) {
			clear(sb) << u8"要删除的图片文件 hash 在数据库中不存在（将忽略）：" << image << SLS_WHERE;
			db_log_print(sb);
			continue;
		}
		const Str &img_figure = stmt_select.getColumn(0); // images.figure
		const Str &ext = stmt_select.getColumn(1); // images.ext
		stmt_select.reset();

		// check extension
		if (!(ext == "png" || ext == "pdf" || ext == "svg")) {
			stmt_delete.bind(1, image);
			if (stmt_delete.exec() != 1) throw internal_err(SLS_WHERE);
			stmt_delete.reset();
			clear(sb) << gv::path_in << "figures/" << image << ext;
			file_remove(sb);
			cout << "正在删除：" << image << '.' << ext << endl;
			continue;
		}

		// check images.figure's figures.last foreign key violation
		if (!img_figure.empty()) {
			stmt_select3.bind(1, img_figure);
			if (stmt_select3.executeStep()) {
				clear(sb) << u8"图片所属的环境 " << img_figure << u8" 被图片环境 " <<
					stmt_select3.getColumn(0).getString() << u8" 的 figures.last 引用，无法删除（将忽略）。";
				scan_log_warn(sb);
				stmt_select3.reset();
				continue;
			}
			stmt_select3.reset();
		}

		// check other related figures.deleted=1
		figures.clear();
		stmt_select4.bind(1, image);
		while (stmt_select4.executeStep())
			figures.insert(stmt_select4.getColumn(0));
		stmt_select4.reset();
		if (img_figure.empty()) {
			clear(sb) << u8"要删除的 image " << image << u8" 的 image.figure 为空，";
			if (figures.empty()) {
				sb << u8"也没有被 figures.image 引用（将继续删除）";
				scan_log_warn(sb);
				stmt_delete.bind(1, image);
				if (stmt_delete.exec() != 1) throw internal_err(SLS_WHERE);
				stmt_delete.reset();
				clear(sb) << gv::path_in << "figures/" << image << '.' << ext;
				file_remove(sb);
			}
			else {
				clear(sb) << u8"但被以下 figures 环境的 figures.image 引用（将忽略，请手动处理）：";
				for (auto &fig : figures) sb << ' ' << fig;
				scan_log_warn(sb);
			}
			continue;
		}
		figures.insert(img_figure);

		// check "figures.deleted"
		bool deleted = true;
		for (auto &fig : figures) {
			stmt_select2.bind(1, fig);
			if (!stmt_select2.executeStep())
				throw internal_err(SLS_WHERE);
			deleted = stmt_select2.getColumn(1).getInt();
			stmt_select2.reset();
			if (!deleted) {
				scan_log_warn(u8"无法删除被 figure 环境使用的 image，请先删除（将忽略）：" + fig);
				continue;
			}
		}
		if (!deleted) continue;

		// now delete db, set related "figures.image"='';
		for (auto &fig : figures) {
			stmt_update.bind(1, fig);
			if (stmt_update.exec() != 1) throw internal_err(SLS_WHERE);
			stmt_update.reset();
		}

		// delete from images
		stmt_delete.bind(1, image);
		if (stmt_delete.exec() != 1) throw internal_err(SLS_WHERE);
		stmt_delete.reset();

		// remove image file
		clear(sb) << gv::path_in << "figures/" << image << '.' << ext;
		file_remove(sb);
		clear(sb) << gv::path_out << image << '.' << ext;
		file_remove(sb);
	}
}

// delete all related db data and files of a figure
// must be marked figures.deleted
// if figures.aka is empty, must not be referenced by figures.aka
// if figures.aka is not empty, will only delete the record in figures, nothing else
inline void arg_delete_figs_hard(vecStr_I figures, SQLite::Database &db_rw)
{
	vecStr image_hash, images;
	set<Str> entries_figures;

	SQLite::Statement stmt_select(db_rw,
		R"(SELECT "entry", "image", "deleted", "aka", "last" FROM "figures" WHERE "id"=?;)");
	SQLite::Statement stmt_select2(db_rw, R"(SELECT "id", "deleted" FROM "figures" WHERE "aka"=?;)");
	SQLite::Statement stmt_select3(db_rw, R"(SELECT "hash" FROM "images" WHERE "figure"=?;)");
	SQLite::Statement stmt_select4(db_rw, R"(SELECT "id" FROM "figures" WHERE "last"=?;)");
	SQLite::Statement stmt_select5(db_rw, R"(SELECT 1 FROM "entries" WHERE "id"=?;)");
	SQLite::Statement stmt_update(db_rw, R"(UPDATE "figures" SET "deleted"=1 WHERE "id"=?;)");
	SQLite::Statement stmt_delete(db_rw, R"(DELETE FROM "figures" WHERE "id"=?;)");

	for (auto &figure : figures) {
		// get figure info
		cout << "永久删除图片和所有相关数据: " << figure << endl;
		stmt_select.bind(1, figure);
		if (!stmt_select.executeStep()) {
			scan_log_warn(u8"arg_delete_fig()：要删除的图片未找到（将忽略）：" + figure);
			stmt_select.reset();
			continue;
		}
		const Str &entry = stmt_select.getColumn(0);
		const Str &image = stmt_select.getColumn(1);
		bool deleted = stmt_select.getColumn(2).getInt();
		bool has_aka = !stmt_select.getColumn(3).getString().empty();
		bool has_last = !stmt_select.getColumn(4).getString().empty();
		stmt_select.reset();
		stmt_select4.bind(1, figure);
		bool has_next = stmt_select4.executeStep();
		stmt_select4.reset();

		// check figures.entry
		bool figures_entry_exist = false;
		if (!entry.empty()) {
			stmt_select5.bind(1, entry);
			figures_entry_exist = stmt_select5.executeStep();
			stmt_select5.reset();
		}
		if (!figures_entry_exist)
			scan_log_warn(u8"要删除的图片环境的 figures.entry 不存在（将删除）：" + entry);

		if (has_last || has_next) // TODO: deal with figures.last and figures.next
			throw internal_err("not implemented!" SLS_WHERE);
		if (has_aka) {
			// just delete 1 record from `entries` and done
			scan_log_warn(u8"要删除的图片 aka 不为空，图片文件将不会删除。");
			stmt_delete.bind(1, figure);
			if (stmt_delete.exec() != 1) throw internal_err(SLS_WHERE);
			stmt_delete.reset();
			return;
		}
		if (!deleted) {
			if (figures_entry_exist) {
				throw internal_err(
					u8"不允许删除未被标记 figures.deleted 的图片，请在文章代码中删除 figure 环境并运行编译："
					+ figure);
			}
			else {
				stmt_update.bind(1, figure);
				if (stmt_update.exec() != 1) throw internal_err(SLS_WHERE);
				stmt_update.reset();
			}
		}

		// check figures.aka
		stmt_select2.bind(1, figure);
		sb.clear();
		while (stmt_select2.executeStep())
			sb << " " << stmt_select2.getColumn(0).getString();
		stmt_select2.reset();
		if (!sb.empty()) {
			clear(sb1) << u8"不允许永久删除未被 figures.aka 引用的图片：" << figure << u8"请先删除：" << sb;
			throw internal_err(sb1);
		}

		// delete all related images
		images.clear();
		stmt_select3.bind(1, figure);
		while (stmt_select3.executeStep())
			images.push_back(stmt_select3.getColumn(0));
		stmt_select3.reset();
		db_delete_images(images, db_rw);

		// delete figures record
		stmt_delete.bind(1, figure);
		if (stmt_delete.exec() != 1) throw internal_err(SLS_WHERE);
		stmt_delete.reset();
	}
}
