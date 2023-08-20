#pragma once

// remove special .tex files from a list of name
// return number of names removed
// names has ".tex" extension
inline Long ignore_entries(vecStr_IO names)
{
	Long i{}, j{}, N{}, Nnames{}, Nnames0;
	vecStr names0; // names to remove
	names0.emplace_back("FrontMatters");
	names0.emplace_back("FrontMattersSample");
	names0.emplace_back("bibliography");
	// add other names here
	Nnames = size(names);
	Nnames0 = size(names0);
	for (i = 0; i < Nnames; ++i) {
		for (j = 0; j < Nnames0; ++j) {
			if (names[i] == names0[j]) {
				names.erase(names.begin() + i);
				++N; --Nnames; --i; break;
			}
		}
	}
	return N;
}

// get `entries` from "contents/*.tex" (ignore some)
// get `titles` from main.tex
// warn repeated `titles` or `entries` in main.tex (case insensitive)
// warn `entries` not in main.tex (including comments)
inline void entries_titles(vecStr_O entries, vecStr_O titles)
{
	// 文件夹中的词条文件（忽略不用编译的）
	entries.clear(); titles.clear();
	file_list_ext(entries, gv::path_in + "contents/", Str("tex"), false);
	ignore_entries(entries);
	if (entries.empty())
		throw internal_err(u8"未找到任何词条");
	sort_case_insens(entries);

	{ // 检查同名词条文件（不区分大小写）
		vecStr entries_low;
		to_lower(entries_low, entries);
		Long ind = find_repeat(entries_low);
		if (ind >= 0)
			throw internal_err(u8"发现同名词条文件（不区分大小写）：" + entries[ind]);
	}

	Long ind0 = 0;
	titles.resize(entries.size());
	vecBool in_toc(entries.size(), false); // in main.tex
	vecBool in_toc_comment(entries.size(), false); // in main.tex but commented

	Str title, entry, str;
	read(str, gv::path_in + "main.tex");
	CRLF_to_LF(str);
	Str str0 = str;
	rm_comments(str); // remove comments
	if (str.empty()) str = " ";
	if (str0.empty()) str = " ";

	// go through uncommented entries in main.tex
	while (1) {
		ind0 = find(str, "\\entry", ind0);
		if (ind0 < 0)
			break;

		// get chinese title and entry label
		command_arg(title, str, ind0);
		replace(title, "\\ ", " ");
		if (title.empty())
			throw scan_err(u8"main.tex 中词条标题不能为空");
		command_arg(entry, str, ind0, 1);

		Long ind = search(entry, entries);
		if (ind < 0)
			throw scan_err(u8"main.tex 中词条文件 " + entry + u8" 未找到");
		if (in_toc[ind])
			throw scan_err(u8"目录中出现重复词条：" + entry);
		in_toc[ind] = true;
		titles[ind] = title;
		++ind0;
	}
	
	// check repeated titles
	for (Long i = 0; i < size(titles); ++i) {
		if (titles[i].empty())
			continue;
		for (Long j = i + 1; j < size(titles); ++j) {
			if (titles[i] == titles[j])
				throw scan_err(u8"目录中标题重复：" + titles[i]);
		}
	}

	// go through commented entries in main.tex
	ind0 = -1;
	while (1) {
		ind0 = find(str0, "\\entry", ++ind0);
		if (ind0 < 0)
			break;
		// get entry label
		if (command_arg(entry, str0, ind0, 1) == -1) continue;
		trim(entry);
		if (entry.empty()) continue;
		Long ind = search(entry, entries);
		if (ind < 0 || in_toc[ind])
			continue;
		// found commented \entry{}{}
		in_toc_comment[ind] = true;
	}

	Bool warned = false;
	for (Long i = 0; i < size(entries); ++i) {
		if (!in_toc[i] && !in_toc_comment[i]) {
			if (!warned) {
				cout << u8"\n\n警告: 以下词条没有被 main.tex 提及（注释中也没有），但仍会被编译" << endl;
				warned = true;
			}
			read(str, gv::path_in + "contents/" + entries[i] + ".tex");
			CRLF_to_LF(str);
			get_title(title, str);
			titles[i] = title;
			cout << std::setw(10) << std::left << entries[i]
				<< std::setw(20) << std::left << title << endl;
		}
	}

	warned = false;
	for (Long i = 0; i < size(entries); ++i) {
		if (in_toc_comment[i]) {
			if (!warned) {
				cout << u8"\n\n警告: 以下词条在 main.tex 中被注释，但仍会被编译" << endl;
				warned = true;
			}
			read(str, gv::path_in + "contents/" + entries[i] + ".tex");
			CRLF_to_LF(str);
			get_title(title, str);
			titles[i] = title;
			cout << std::setw(10) << std::left << entries[i]
				<< std::setw(20) << std::left << title << endl;
		}
	}
	cout << endl;
}

// auto add `\label{prt_xxx}` and `\label{cpt_xxx}` to main.tex
inline void auto_add_toc_labels_prt_cpt()
{
	Str str;
	read(str, gv::path_in + "main.tex");
	CRLF_to_LF(str);
	for (auto &type : {"part", "chapter"}) {
		Long ind = 0;
		while (1) {
			ind = find_command(str, type, ind);
			if (ind < 0) break;
			ind = skip_command(str, ind, 1);
			Long ind_label = find_command(str, "label", ind);
			Long ind_LF = find(str, '\n', ind);
			if (ind_label < 0 || ind_label > ind_LF) {
				stringstream ss;
				for (Long i = 16; i < 1000000; ++i) {
					ss.str("");
					ss.clear();
					ss << std::hex << i;
					if (find(str, ss.str()) < 0)
						break;
				}
				if (strcmp(type, "part") == 0)
					str.insert(ind, " \\label{prt_" + ss.str() + '}');
				else
					str.insert(ind, " \\label{cpt_" + ss.str() + '}');
			}
		}
	}
	write(str, gv::path_in + "main.tex");
}

// create table of content from main.tex
// path must end with '/'
// titles[i] is the chinese title of entries[i].tex from first line of comment
// `entry_part[i]` is the part number of `entries[i]`, 0: no info, 1: the first part, etc.
// `entry_chap[i]` is similar to `entry_part[i]`, for chapters
// if entry_part.size() == 0, `chap_names, entry_chap, entry_part, part_names` will be ignored
inline void table_of_contents(
		vecStr_O part_ids, vecStr_O part_names, vecStr_O part_chap_first, vecStr_O part_chap_last, // part info
		vecStr_O chap_ids, vecStr_O chap_names, vecLong_O chap_part, vecStr_O chap_entry_first, vecStr_O chap_entry_last, // chapter info
		vecStr_O entries, vecStr_O titles, vecBool_O is_draft, vecLong_O entry_part, vecLong_O entry_chap, // entry info
		SQLite::Database &db_rw)
{
	SQLite::Statement stmt_select(db_rw,
		R"(SELECT "caption", "draft", "type" FROM "entries" WHERE "id"=?;)");

	Long ind0 = 0, ind1 = 0, ikey, chapNo = -1, chapNo_tot = 0, partNo = 0;
	vecStr keys{ "\\part", "\\chapter", "\\entry", "\\bibli"}, type;
	Str title, entry, str, str2, html, link_class;

	part_ids.clear(); part_names.clear(); part_chap_first.clear(); part_chap_last.clear();
	chap_ids.clear(); chap_names.clear(); chap_part.clear();
	chap_entry_first.clear(); chap_entry_last.clear();
	entries.clear(); titles.clear(); is_draft.clear(); entry_part.clear(); entry_chap.clear();

	part_ids.emplace_back(""); part_names.emplace_back(u8"无"); part_chap_first.emplace_back(""); part_chap_last.emplace_back("");
	chap_ids.emplace_back(""); chap_names.emplace_back(u8"无");  chap_part.push_back(0);
	chap_entry_first.emplace_back(""); chap_entry_last.emplace_back("");

	read(str, gv::path_in + "main.tex");
	CRLF_to_LF(str);
	read(html, gv::path_out + "templates/index_template.html"); // read html template
	CRLF_to_LF(html);
	ind0 = find(html, "PhysWikiHTMLbody", ind0);
	if (ind0 < 0)
		throw internal_err(u8"index_template.html 中没有找到 PhysWikiHTMLbody");
	html.erase(ind0, 16);

	rm_comments(str); // remove comments
	if (str.empty()) str = " ";

	char last_command = 'n'; // 'p': \part, 'c': \chapter, 'e': \entry
	Str db_title, label;

	while (1) {
		ind1 = find(ikey, str, keys, ind1);
		if (ind1 < 0)
			break;
		if (ikey == 2) {
			// ============ found "\entry" ==============
			if (last_command != 'e' && last_command != 'c')
				throw scan_err(u8R"(main.tex 中 \entry{}{} 必须在 \chapter{} 或者其他 \entry{}{} 之后： )" + title);

			// parse \entry{title}{entry}
			command_arg(title, str, ind1);
			command_arg(entry, str, ind1, 1);
			replace(title, "\\ ", " ");
			if (title.empty())
				throw scan_err(u8"main.tex 中词条中文名不能为空");
			if (search(entry, entries) >= 0)
				throw scan_err(u8"main.tex 中词条重复： " + entry);
			entries.push_back(entry);
			if (last_command == 'c')
				chap_entry_first.push_back(entry);

			titles.push_back(title);

			// get db info
			stmt_select.bind(1, entry);
			if (!stmt_select.executeStep()) {
				db_log(u8"数据库中找不到 main.tex 中词条（将试图修复）： " + entry);
				vecStr titles_tmp;
				entries_titles(entries, titles_tmp);
				db_check_add_entry_simulate_editor(entries, db_rw);
				stmt_select.reset();
				stmt_select.bind(1, entry);
				if (!stmt_select.executeStep())
					throw scan_err(u8"数据库中找不到 main.tex 中词条（文件不存在）： " + entry);
			}
			db_title = stmt_select.getColumn(0).getString();
			is_draft.push_back((int)stmt_select.getColumn(1));
			type.push_back(stmt_select.getColumn(2));
			stmt_select.reset();

			// TODO: enable this when editor can update db title
			// if (db_title != title)
			//    throw scan_err(u8"目录标题 “" + title + u8"” 与数据库中词条标题 “" + db_title + u8"” 不符！");

			// insert entry into html table of contents
			link_class = type.back();
			if (is_draft.back()) {
				if (!link_class.empty())
					link_class += ' ';
				link_class += "draft";
			}
			if (!link_class.empty())
				link_class = "class=\"" + link_class + "\" ";
			clear(sb) << "<a " << link_class << "href = \"" << gv::url << entry
				<< R"(.html" target = "_blank">)" << title << u8"</a>　\n";
			ind0 = insert(html, sb, ind0);

			entry_part.push_back(partNo);
			entry_chap.push_back(chapNo_tot);
			++ind1; last_command = 'e';
		}
		else if (ikey == 1) {
			// ========== found "\chapter" =============
			// get chinese chapter name
			command_arg(title, str, ind1);
			replace(title, "\\ ", " ");
			if (last_command != 'p' && last_command != 'e') 
				throw scan_err(u8R"(main.tex 中 \chapter{} 必须在 \entry{}{} 或者 \part{} 之后， 不允许空的 \chapter{}： )" + title);
			if (chap_ids.size() > 1)
				chap_entry_last.push_back(entries.back());
			// get chapter id from label cpt_xxx, where xxx is id
			Long ind_label = find_command(str, "label", ind1);
			Long ind_LF = find(str, '\n', ind1);
			if (ind_label < 0 || ind_label > ind_LF) {
				auto_add_toc_labels_prt_cpt();
				throw scan_err(u8"每一个 \\chapter{} 后面（同一行）必须要有 \\label{cpt_XXX}， 已经自动添加， 请重新打开 main.tex");
			}
			command_arg(label, str, ind_label);
			chap_ids.push_back(label_id(label));
			if (last_command == 'p')
				part_chap_first.push_back(chap_ids.back());
			// insert chapter into html table of contents
			++chapNo; ++chapNo_tot;
			if (chapNo > 0)
				ind0 = insert(html, "</p>", ind0);
			chap_names.push_back(title);
			chap_part.push_back(partNo);
			ind0 = insert(html, u8"\n\n<h3><b>第" + num2chinese(chapNo+1) + u8"章 " + title
				+ "</b></h5>\n<div class = \"tochr\"></div><hr><div class = \"tochr\"></div>\n<p class=\"toc\">\n", ind0);
			++ind1; last_command = 'c';
		}
		else if (ikey == 0) {
			// =========== found "\part" ===============
			// get chinese part name
			chapNo = -1;
			command_arg(title, str, ind1);
			replace(title, "\\ ", " ");
			if (part_ids.size() > 1)
				part_chap_last.push_back(chap_ids.back());
			if (last_command != 'e' && last_command != 'n')
				throw scan_err(u8R"(main.tex 中 \part{} 必须在 \entry{} 之后或者目录开始， 不允许空的 \chapter{} 或 \part{}： )" + title);
			// get part id from label prt_xxx, where xxx is id
			Long ind_label = find_command(str, "label", ind1);
			Long ind_LF = find(str, '\n', ind1);
			if (ind_label < 0 || ind_label > ind_LF) {
				auto_add_toc_labels_prt_cpt();
				throw scan_err(u8"每一个 \\part{} 后面（同一行）必须要有 \\label{prt_XXX}， 已经自动添加， 请重新打开 main.tex");
			}
			command_arg(label, str, ind_label);
			if (label.substr(0, 4) != "prt_")
				throw scan_err(u8"\\label wrong format (should be \"prt_xxx\"): " + label);
			part_ids.push_back(label_id(label));
			// insert part into html table of contents
			++partNo;
			part_names.push_back(title);
			
			ind0 = insert(html,
				"</p></div>\n\n<div class = \"w3-container w3-center w3-teal w3-text-white\">\n"
				"<h2 align = \"center\" style=\"padding-top: 0px;\" id = \"part"
					+ num2str32(partNo) + u8"\">第" + num2chinese(partNo) + u8"部分 " + title + "</h3>\n"
				"</div>\n\n<div class = \"w3-container\">\n"
				, ind0);
			++ind1; last_command = 'p';
		}
		else if (ikey == 3) {
			// =========  found "\bibli" ==========
			if (last_command != 'e')
				throw scan_err(u8"\\bibli 必须放在最后, 且不允许空的 \\chapter{}");
			title = u8"参考文献";
			clear(sb) << "<a href = \"" << gv::url << R"(bibliography.html" target = "_blank">)"
					<< title << u8"</a>　\n";
			ind0 = insert(html, sb, ind0);
			++ind1;
		}
	}
	html.insert(ind0, "</p>\n</div>");

	part_chap_last.push_back(chap_ids.back());
	chap_entry_last.push_back(entry);

	// list parts
	ind0 = find(html, "PhysWikiPartList", 0);
	if (ind0 < 0)
		throw internal_err(u8"html 模板中 PhysWikiPartList 不存在！");
	html.erase(ind0, 16);
	ind0 = insert(html, "|", ind0);
	for (Long i = 1; i < size(part_names); ++i) {
		ind0 = insert(html, "   <a href = \"#part" + num2str32(i) + "\">"
						   + part_names[i] + "</a> |\n", ind0);
	}

	// write to index.html
	write(html, gv::path_out + "index.html");
	Long kk = find_repeat(chap_ids);
	if (kk >= 0)
		throw scan_err(u8"\\chapter{} label 重复定义： " + chap_ids[kk]);
	kk = find_repeat(part_ids);
	if (kk >= 0)
		throw scan_err(u8"\\part{} label 重复定义： " + part_ids[kk]);
}
