#pragma once

// replace figure environment with html code
// copy image files to gv::path_out
inline void figure_env(
	unordered_set<Str> &img_to_delete, // [out] image files to delete
	unordered_map<Str, unordered_map<Str, Str>> &fig_ext_hash, // [append] figures.id -> (ext -> hash)
	Str_IO str, Str_I entry,
	vecStr_I fig_ids, // parsed from env_labels(), `\label{}` already deleted
	vecLong_I fig_orders, // figures.order
	SQLite::Database &db_read)
{
	Intvs intvFig;
	Str fig_fname, fname_in, fname_out, href, ext, caption, widthPt;
	Str fname_in2, str_mod, tex_fname, caption_div, image_hash;
	const char *tmp1 = "id = \"fig_";
	const Long tmp1_len = (Long)strlen(tmp1);
	SQLite::Statement stmt_update(db_read, u8R"(UPDATE "figures" SET "image_alt"='' WHERE "id"=?;)");

	Long Nfig = find_env(intvFig, str, "figure", 'o');
	if (size(fig_orders) != Nfig || size(fig_ids) != Nfig)
		throw scan_err(u8"请确保每个图片环境都有一个 \\label{} 标签");

	for (Long i_fig = Nfig - 1; i_fig >= 0; --i_fig) {
		auto &fig_id = fig_ids[i_fig];

		// verify label
		Long ind_label = rfind(str, tmp1, intvFig.L(i_fig));
		if (ind_label < 0 || (i_fig > 0 && ind_label < intvFig.R(i_fig - 1)))
			throw scan_err(u8"图片必须有标签， 请使用上传图片按钮。");
		ind_label += tmp1_len;
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
			// svg and pdf hashes not the same (new standard)
			image_hash = get_text("figures", "id", fig_id, "image_alt", db_read);
			if(find(image_hash, ' ') >= 0)
				throw internal_err(u8"目前 figures.image_alt 仅支持一张 svg");
			// if current record has non-empty "aka", then use that figure
			const Str &aka = get_text("figures", "id", fig_id, "aka", db_read);
			if (image_hash.empty()) {
				if (aka.empty())
					throw internal_err(u8"pdf 图片找不到对应的 svg 且 figures.aka 为空：" + fig_id);
				image_hash = get_text("figures", "id", aka, "image_alt", db_read);
			}
			else if (!aka.empty()) {
				clear(sb) << u8"图片环境 " << fig_id << u8" 的 figures.aka 不为空， 且 figures.image_alt 不为空（将清空）。";
				scan_warn(sb);
				stmt_update.bind(1, fig_id);
				stmt_update.exec(); stmt_update.reset();
			}
			if (size(image_hash) != 16) {
				clear(sb) << u8"图片环境 " << fig_id << u8" 的图片 images.id 长度不对：" << image_hash;
				throw internal_err(u8"发现 images.image_alt 仍然带有拓展名，将模拟编辑器删除。");
			}
			fig_ext_hash[fig_id][ext] = image_hash;
			clear(fname_in2) << gv::path_in << "figures/" << image_hash << '.' << ext;
		}
		else
			throw internal_err(u8"图片格式暂不支持：" + fig_fname);

		clear(fname_out) << gv::path_out << image_hash << "." << ext;
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
		if (!caption.empty())
			caption = u8"：" + caption;
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

// these environments are already not in a paragraph
// return number of environments processed
inline Long theorem_like_env(Str_IO str)
{
	Long N, N_tot = 0, ind0;
	Intvs intvIn, intvOut;
	Str env_title, env_num;
	vecStr envNames = {"definition", "lemma", "theorem",
		"corollary", "example", "exercise"};
	vecStr envCnNames;
	if (!gv::is_eng)
		envCnNames = {u8"定义", u8"引理", u8"定理",
			u8"推论", u8"例", u8"习题"};
	else
		envCnNames = { "Definition ", "Lemma ", "Theorem ",
			"Corollary ", "Example ", "Exercise " };
	vecStr envBorderColors = { "w3-border-red", "w3-border-red", "w3-border-red",
		"w3-border-red", "w3-border-yellow", "w3-border-green" };
	for (Long i = 0; i < size(envNames); ++i) {
		ind0 = 0; N = 0;
		while (1) {
			ind0 = find_command_spec(str, "begin", envNames[i], ind0);
			if (ind0 < 0)
				break;
			++N; num2str(env_num, N);
			++N_tot;
			command_arg(env_title, str, ind0, 1);

			// positions of the environment
			Long ind1 = skip_command(str, ind0, 2);
			Long ind2 = find_command_spec(str, "end", envNames[i], ind0);
			Long ind3 = skip_command(str, ind2, 1);

			// replace
			str.replace(ind2, ind3 - ind2, "</div>\n");
			// str.replace(ind1, ind2 - ind1, temp);
			clear(sb) << "<div class = \"w3-panel " << envBorderColors[i]
					<< " w3-leftbar\">\n <h3 style=\"margin-top: 0; padding-top: 0;\"><b>"
					<< envCnNames[i] << " "
					<< env_num << u8"</b>　" << env_title << "</h3>\n";
			str.replace(ind0, ind1 - ind0, sb);
		}
	}
	return N_tot;
}

// issue environment
inline Long issuesEnv(Str_IO str)
{
	Long Nenv = Env2Tag("issues", "<div class = \"w3-panel w3-round-large w3-sand\"><ul>", "</ul></div>", str);
	if (Nenv > 1)
		throw scan_err(u8"不支持多个 issues 环境， issues 必须放到最开始");
	else if (Nenv == 0)
		return 0;

	Long ind0 = -1, ind1 = -1, N = 0;
	Str arg;
	// issueDraft
	ind0 = find_command(str, "issueDraft");
	if (ind0 > 0) {
		ind1 = skip_command(str, ind0);
		str.replace(ind0, ind1 - ind0, u8"<li>本词条处于草稿阶段。</li>");
		++N;
	}
	// issueTODO
	ind0 = find_command(str, "issueTODO");
	if (ind0 > 0) {
		ind1 = skip_command(str, ind0);
		str.replace(ind0, ind1 - ind0, u8"<li>本词条存在未完成的内容。</li>");
		++N;
	}
	// issueMissDepend
	ind0 = find_command(str, "issueMissDepend");
	if (ind0 > 0) {
		ind1 = skip_command(str, ind0);
		str.replace(ind0, ind1 - ind0, u8"<li>本词条缺少预备知识， 初学者可能会遇到困难。</li>");
		++N;
	}
	// issueAbstract
	ind0 = find_command(str, "issueAbstract");
	if (ind0 > 0) {
		ind1 = skip_command(str, ind0);
		str.replace(ind0, ind1 - ind0, u8"<li>本词条需要更多讲解， 便于帮助理解。</li>");
		++N;
	}
	// issueNeedCite
	ind0 = find_command(str, "issueNeedCite");
	if (ind0 > 0) {
		ind1 = skip_command(str, ind0);
		str.replace(ind0, ind1 - ind0, u8"<li>本词条需要更多参考文献。</li>");
		++N;
	}
	// issueAiRaw
	ind0 = find_command(str, "issueAiRaw");
	if (ind0 > 0) {
		ind1 = skip_command(str, ind0);
		str.replace(ind0, ind1 - ind0, u8"<li> 本词条含人工智能辅助创作，待审核。</li>");
		++N;
	}
	// issueAi
	ind0 = find_command(str, "issueAi");
	if (ind0 > 0) {
		ind1 = skip_command(str, ind0);
		str.replace(ind0, ind1 - ind0, u8"<li> 本词条含人工智能辅助创作，已审核。</li>");
		++N;
	}
	// issueOther
	ind0 = 0;
	N += Command2Tag("issueOther", "<li>", "</li>", str);
	return N;
}
