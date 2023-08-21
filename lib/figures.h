#pragma once
#include "sqlite_db.h"

// replace figure environment with html code
// copy image files to gv::path_out
inline void figure_env(
		unordered_set<Str> &img_to_delete, // [out] image files to delete
		unordered_map<Str, unordered_map<Str, Str>> &fig_ext_hash, // [append] figures.id -> (ext -> hash)
		Str_IO str, Str_I entry,
		vecStr_I fig_ids, // parsed from env_labels(), `\label{}` already deleted
		vecLong_I fig_orders, // figures.order
		SQLite::Database &db_rw)
{
	Intvs intvFig;
	Str fig_fname, fname_in, fname_out, href, ext, caption, widthPt;
	Str fname_in2, str_mod, tex_fname, caption_div, image_hash;
	static const Str tmp1 = "id = \"fig_";
	SQLite::Statement stmt_update(db_rw, u8R"(UPDATE "figures" SET "image_alt"=? WHERE "id"=?;)");

	Long Nfig = find_env(intvFig, str, "figure", 'o');
	if (size(fig_orders) != Nfig || size(fig_ids) != Nfig)
		throw scan_err(u8"请确保每个图片环境都有一个 \\label{} 标签");

	for (Long i_fig = Nfig - 1; i_fig >= 0; --i_fig) {
		auto &fig_id = fig_ids[i_fig];
		const Str &aka = get_text("figures", "id", fig_id, "aka", db_rw);

		// verify label
		Long ind_label = rfind(str, tmp1, intvFig.L(i_fig));
		if (ind_label < 0 || (i_fig > 0 && ind_label < intvFig.R(i_fig - 1)))
			throw scan_err(u8"图片必须有标签， 请使用上传图片按钮。");
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
				scan_warn(sb);
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
			fname_in2.clear(); fname_in2 << gv::path_in << "figures/" << image_hash << "." << ext;
			if (fig_fname != image_hash) {
				file_copy(fname_in2, fname_in, true);
				img_to_delete.insert(fname_in);
				// modify .tex file
				tex_fname.clear(); tex_fname << gv::path_in << "contents/" << entry << ".tex";
				read(str_mod, tex_fname);
				clear(sb) << fig_fname << '.' << ext;
				clear(sb1) << image_hash << '.' << ext;
				replace(str_mod, sb, sb1);
				write(str_mod, tex_fname);
			}

			// ==== svg =====
			ext = "svg";
			image_hash = get_text("figures", "id", fig_id, "image_alt", db_rw);
			if(find(image_hash, ' ') >= 0)
				throw internal_err(u8"目前 figures.image_alt 仅支持一张 svg");
			// if current record has non-empty "aka", then use that figure
			if (image_hash.empty()) {
				if (aka.empty())
					throw internal_err(u8"pdf 图片找不到对应的 svg 且 figures.aka 为空：" + fig_id);
				image_hash = get_text("figures", "id", aka, "image_alt", db_rw);
			}
			else { // !image_hash.empty()
				if (!aka.empty()) {
					clear(sb) << u8"图片环境 " << fig_id
						<< u8" 的 figures.aka 不为空， 且 figures.image_alt 不为空（将清空）。";
					scan_warn(sb);
					stmt_update.bind(1, "");
					stmt_update.bind(2, fig_id);
					stmt_update.exec(); stmt_update.reset();
				}
				if (size(image_hash) != 16) {
					if (size(image_hash) > 16 && image_hash.substr(16) == ".svg") {
						scan_warn(u8"内部警告：发现 images.image_alt 仍然带有 svg 拓展名，将模拟编辑器删除。");
						image_hash.resize(16);
						stmt_update.bind(1, image_hash);
						stmt_update.bind(2, fig_id);
						stmt_update.exec(); stmt_update.reset();
					}
					clear(sb) << u8"图片环境 " << fig_id << u8" 的图片 images_alt 长度不对：" << image_hash;
				}
			}
			fig_ext_hash[fig_id][ext] = image_hash;
			clear(fname_in2) << gv::path_in << "figures/" << image_hash << '.' << ext;
		}
		else
			throw internal_err(u8"图片格式暂不支持：" + fig_fname);

		if (aka.empty()) {
			clear(fname_out) << gv::path_out << image_hash << "." << ext;
			file_copy(fname_out, fname_in2, true);
		}

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
		if (!caption.empty())
			caption.insert(0, u8"：");
		clear(sb) << R"(<div class = "w3-content" style = "max-width:)" << widthPt << "em;\">\n"
			"<a href=\"" << href << R"(" target = "_blank"><img src = ")" << href
			<< "\" alt = \"" << (gv::is_eng? "Fig" : u8"图")
			<< "\" style = \"width:100%;\"></a>\n</div>\n"
			<< "<div align = \"center\"> "
			<< (gv::is_eng ? "Fig. " : u8"图 ") << i_fig + 1
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
		R"(SELECT "ext", "figure", "figures_aka" FROM "images" WHERE "hash"=?;)");
	SQLite::Statement stmt_insert(db_rw,
		R"(INSERT INTO "images" ("hash", "ext", "figure", "figures_aka") VALUES (?,?,?,?);)");
	SQLite::Statement stmt_update(db_rw, R"(UPDATE "images" SET "figure"=? WHERE "hash"=?;)");
	SQLite::Statement stmt_update2(db_rw, R"(UPDATE "images" SET "figures_aka"=? WHERE "hash"=?;)");
	set<Str> db_image_figures_aka;

	for (auto &fig_id : fig_ids) {
		const Str &fig_aka = get_text("figures", "id", fig_id, "aka", db_rw);
		for (auto &ext_hash: fig_ext_hash.at(fig_id)) {
			auto &image_ext = ext_hash.first;
			auto &image_hash = ext_hash.second;
			stmt_select.bind(1, image_hash);
			if (!stmt_select.executeStep()) {
				clear(sb) << "内部警告：数据库中找不到图片文件（将模拟 editor 添加）：" << image_hash << image_ext;
				scan_warn(sb);
				stmt_insert.bind(1, image_hash);
				stmt_insert.bind(2, image_ext);
				if (fig_aka.empty()) {
					stmt_insert.bind(3, fig_id);
					stmt_insert.bind(4, "");
				}
				else {
					stmt_insert.bind(3, fig_aka);
					stmt_insert.bind(4, "fig_id");
				}
				stmt_insert.exec(); stmt_insert.reset();
			}
			else {
				const Str &db_image_ext = stmt_select.getColumn(0);
				const Str &db_image_figure = stmt_select.getColumn(1);
				parse(db_image_figures_aka, stmt_select.getColumn(2));
				stmt_select.reset();
				if (image_ext != db_image_ext) {
					clear(sb) << u8"图片文件拓展名不允许手动改变： " << image_hash << '.'
						<< db_image_ext << " -> " << image_ext;
					throw internal_err(sb);
				}
				if (fig_aka.empty()) {
					if (db_image_figure != fig_id) {
						clear(sb) << u8"检测到 images.figure 改变（将更新）： "
							<< image_hash << '.' << db_image_figure << " -> " << fig_id;
						db_log(sb);
						stmt_update.bind(1, fig_id);
						stmt_update.bind(2, image_hash);
						stmt_update.exec(); stmt_update.reset();
					}
				}
				else {
					if (db_image_figure != fig_aka) {
						clear(sb) << u8"检测到 images.figure 改变（将更新）： "
							<< image_hash << '.' <<  db_image_figure << " -> " << fig_aka;
						db_log(sb);
						stmt_update.bind(1, fig_id);
						stmt_update.bind(2, fig_aka);
						stmt_update.exec(); stmt_update.reset();
					}
					if (!db_image_figures_aka.count(fig_id)) {
						clear(sb) << u8"检测到新增的 images.figures_aka（将更新）： " <<  image_hash << '.' << fig_id;
						db_log(sb);
						db_image_figures_aka.insert(fig_id);
						join(sb, db_image_figures_aka);
						stmt_update2.bind(1, sb);
						stmt_update2.bind(2, image_hash);
						stmt_update2.exec(); stmt_update2.reset();
					}
				}
			}
		}
	}
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
	vecBool db_figs_deleted, figs_used;
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
			if (!stmt_select1.executeStep()) {
				scan_warn("entries.figures 中图片标签不存在（将忽略，请手动删除）： fig_" + fig_id);
				stmt_select1.reset();
				continue;
			}
			db_fig_orders.push_back((int)stmt_select1.getColumn(0));
			db_fig_ref_bys.emplace_back();
			parse(db_fig_ref_bys.back(), stmt_select1.getColumn(1));
			db_fig_image.push_back(stmt_select1.getColumn(2));
			db_fig_aka[fig_id] = stmt_select1.getColumn(4).getString();
			db_figs_deleted.push_back((int)stmt_select1.getColumn(3));
			stmt_select1.reset();
		}
	}
	figs_used.resize(db_figs_deleted.size(), false);
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
				clear(sb) << u8"内部警告：发现数据库中没有的图片环境（将模拟 editor 添加）："
						  << fig_id << ", " << entry << ", " << to_string(order);
				scan_warn(sb);
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
			if (db_figs_deleted[ind]) {
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
	SQLite::Statement stmt_update3(db_rw, R"(UPDATE "figures" SET "deleted"=1, "order"=0 WHERE "id"=?;)");
	for (Long i = 0; i < size(figs_used); ++i) {
		if (!figs_used[i] && !db_figs_deleted[i]) {
			if (db_fig_ref_bys[i].empty() ||
				(db_fig_ref_bys[i].size() == 1 && db_fig_ref_bys[i][0] == db_fig_entries[i])) {
				clear(sb) << u8"检测到 \\label{fig_"
					<< db_figs[i] << u8"}  被删除（图 " << db_fig_orders[i] << u8"）， 将标记未使用";
				db_log(sb);
				stmt_update3.bind(1, db_figs[i]);
				stmt_update3.exec(); stmt_update3.reset();
			}
			else {
				join(ref_by_str, db_fig_ref_bys[i], ", ");
				throw scan_err(u8"检测到 \\label{fig_" + db_figs[i] + u8"}  被删除， 但是被这些词条引用。请撤销删除，把引用删除后再试： " + ref_by_str);
			}
		}
	}
	// cout << "done!" << endl;
}

// delete from "images" table, and the image file
// if "images.figure/figures_aka" use this image as "image" or "image_alt",
//     then it must have `figures.deleted==1`
inline void db_delete_images(
		vecStr_I images, // hashes, no extension
		SQLite::Database &db_rw, Str_I fig_id = "")
{
	SQLite::Statement stmt_select(db_rw,
		R"(SELECT "figure", "figures_aka", "ext" FROM "images" WHERE "hash"=?;)");
	SQLite::Statement stmt_select2(db_rw,
		R"(SELECT "image", "image_alt", "deleted" FROM "figures" WHERE "id"=?;)");
	SQLite::Statement stmt_delete(db_rw, R"(DELETE FROM "images" WHERE "hash"=?;)");
	SQLite::Statement stmt_update(db_rw, R"(UPDATE "figures" SET "image"='' WHERE "id"=?;)");
	SQLite::Statement stmt_update2(db_rw, R"(UPDATE "figures" SET "image_alt"=? WHERE "id"=?;)");
	set<Str> figures, figures_image_alt; // images.figure + images.figures_aka

	for (auto &image : images) {
		db_log("尝试删除图片 hash: " + image);
		stmt_select.bind(1, image);
		if (!stmt_select.executeStep()) {
			clear(sb) << u8"要删除的图片文件 hash 不存在（将忽略）：" << image << SLS_WHERE;
			db_log(sb);
			continue;
		}
		const Str &figure = stmt_select.getColumn(0); // images.figure
		parse(figures, stmt_select.getColumn(1)); // images.figures_aka
		figures.insert(figure);
		const Str &ext = stmt_select.getColumn(2); // images.ext
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
			parse(figures_image_alt, stmt_select2.getColumn(1));
			bool deleted = (int)stmt_select2.getColumn(2);
			stmt_select2.reset();
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
			if (!stmt_select2.executeStep())
				throw internal_err(SLS_WHERE);
			const Str &fig_image = stmt_select2.getColumn(0);
			parse(figures_image_alt, stmt_select2.getColumn(1));
			// bool deleted = (int)stmt_select2.getColumn(3);
			stmt_select2.reset();

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

// delete all related db data and files of a figure
// must be marked figures.deleted
// if figures.aka is empty, must not be referenced by figures.aka
// if figures.aka is not empty, will only delete the record in figures, nothing else
inline void arg_delete_figs_hard(vecStr_I figures, SQLite::Database &db_rw)
{
	vecStr image_hash, images_alt;
	set<Str> entries_figures;

	SQLite::Statement stmt_select(db_rw,
		R"(SELECT "entry", "image", "image_alt", "deleted", "aka", "last", "next" FROM "figures" WHERE "id"=?;)");
	SQLite::Statement stmt_select2(db_rw, R"(SELECT "id", "deleted" FROM "figures" WHERE "aka"=?;)");
	SQLite::Statement stmt_select3(db_rw, R"(SELECT "figures" FROM "entries" WHERE "id"=?;)");
	SQLite::Statement stmt_update3(db_rw, R"(UPDATE "entries" SET "figures"=? WHERE "id"=?;)");
	SQLite::Statement stmt_delete(db_rw, R"(DELETE FROM "figures" WHERE "id"=?;)");

	for (auto &figure : figures) {
		cout << "deleting figure: " << figure << endl;
		stmt_select.bind(1, figure);
		if (!stmt_select.executeStep()) {
			scan_warn(u8"arg_delete_fig()：要删除的图片未找到（将忽略）：" + figure);
			stmt_select.reset();
			continue;
		}
		const Str &entry = stmt_select.getColumn(0);
		const Str &image = stmt_select.getColumn(1);
		parse(images_alt, stmt_select.getColumn(2));
		bool deleted = (int)stmt_select.getColumn(3);
		bool has_aka = !stmt_select.getColumn(4).getString().empty();
		bool has_last = !stmt_select.getColumn(5).getString().empty();
		bool has_next = !stmt_select.getColumn(6).getString().empty();
		stmt_select.reset();
		if (has_last || has_next) // TODO: deal with figures.last and figures.next
			throw internal_err("not implemented!" SLS_WHERE);
		if (has_aka) {
			// just delete 1 record from `entries` and done
			stmt_delete.bind(1, figure);
			stmt_delete.executeStep(); stmt_delete.reset();
			return;
		}
		if (!deleted)
			throw internal_err(u8"不允许删除未被标记 figures.deleted 的图片：" + figure);

		// erase figure from entries.figures
		stmt_select3.bind(1, entry);
		if (!stmt_select3.executeStep())
			throw internal_err(SLS_WHERE);
		parse(entries_figures, stmt_select3.getColumn(0));
		if (entries_figures.count(figure)) {
			entries_figures.erase(figure);
			join(sb, entries_figures);
			stmt_update3.bind(1, sb);
			stmt_update3.bind(2, entry);
			stmt_update3.exec(); stmt_update3.reset();
		}
		else {
			clear(sb) << u8"要删除的 fig 环境已经标记 deleted，但不在 entries.figures 中（将忽略）："
				<< entry << '.' << figure;
			db_log(sb);
		}
		stmt_select3.reset();

		// check figures.aka
		stmt_select2.bind(1, entry);
		sb.clear();
		while (stmt_select2.executeStep()) {
			const Str &id = stmt_select2.getColumn(0);
			sb << " " << id;
		}
		stmt_select2.reset();
		if (!sb.empty()) {
			clear(sb1) << u8"不允许删除未被 figures.aka 引用的图片：" << figure << u8"请先删除：" << sb;
			throw internal_err(sb1);
		}

		// delete figures.image/image_alt
		db_delete_images({image}, db_rw);
		db_delete_images(images_alt, db_rw);

		// delete figures record
		stmt_delete.bind(1, figure);
		stmt_delete.executeStep(); stmt_delete.reset();
	}
}
