#pragma once

// replace figure environment with html image
// convert vector graph to SVG, font must set to "convert to outline"
// if svg image doesn't exist, use png
// path must end with '\\'
// `imgs` is the list of image names, `mark[i]` will be set to 1 when `imgs[i]` is used
// if `imgs` is empty, `imgs` and `mark` will be ignored
// fig_ext_hash[i] maps the file extension to file hash
inline Long figure_env(unordered_set<Str> &img_to_delete, vector<unordered_map<Str, Str>> &fig_ext_hash, Str_IO str, Str_I entry,
					vecStr_I fig_ids, vecLong_I fig_orders, SQLite::Database &db_read)
{
	fig_ext_hash.clear();
	Long N = 0;
	Intvs intvFig;
	Str figName, fname_in, fname_out, href, format, caption, widthPt, figNo;
	Str fname_in2, str_mod, tex_fname, svg_file;
	Long Nfig = find_env(intvFig, str, "figure", 'o');
	if (size(fig_orders) != Nfig || size(fig_ids) != Nfig)
		throw scan_err(u8"请确保每个图片环境都有一个 \\label{} 标签");
	fig_ext_hash.resize(Nfig);
	Str tmp, fig_hash;

	for (Long i = Nfig - 1; i >= 0; --i) {
		num2str(figNo, i + 1);
		// get label and id
		Str tmp1 = "id = \"fig_";
		Long ind_label = str.rfind(tmp1, intvFig.L(i));
		if (ind_label < 0 || (i-1 >= 0 && ind_label < intvFig.R(i-1)))
			throw scan_err(u8"图片必须有标签， 请使用上传图片按钮。");
		ind_label += size(tmp1);
		Long ind_label_end = find(str, '\"', ind_label);
		SLS_ASSERT(ind_label_end > 0);
		Str id = str.substr(ind_label, ind_label_end - ind_label);
		// get width of figure
		Long ind0 = find(str, "width", intvFig.L(i)) + 5;
		ind0 = expect(str, "=", ind0);
		ind0 = find_num(str, ind0);
		Doub width; // figure width in cm
		width = str2double(str, ind0);
		if (width > 14.25)
			throw scan_err(u8"图" + figNo + u8"尺寸不能超过 14.25cm");

		// get file name of figure
		Long indName1 = find(str, "figures/", ind0) + 8;
		Long indName2 = find(str, '}', ind0) - 1;
		if (indName1 < 0 || indName2 < 0) {
			throw scan_err(u8"读取图片名错误");
		}
		figName = str.substr(indName1, indName2 - indName1 + 1);
		trim(figName);

		// get format (png or svg)
		Long Nname = size(figName);
		if (figName.substr(Nname - 4, 4) == ".png") {
			format = "png";
			figName = figName.substr(0, Nname - 4);
			fname_in.clear(); fname_in << gv::path_in << "figures/" << figName << "." << format;
			if (!file_exist(fname_in))
				throw internal_err(u8"图片 \"" + fname_in + u8"\" 未找到");
			fig_hash = sha1sum_f(fname_in).substr(0, 16);
			fig_ext_hash[i][format] = fig_hash;
			// rename figure files with hash
			fname_in2.clear(); fname_in2 << gv::path_in << "figures/" << fig_hash << "." << format;
			if (figName != fig_hash) {
				file_copy(fname_in2, fname_in, true);
				img_to_delete.insert(fname_in);
				// modify .tex file
				tex_fname.clear(); tex_fname << gv::path_in << "contents/" << entry << ".tex";
				read(str_mod, tex_fname);
				replace(str_mod, figName + "." + format, fig_hash + "." + format);
				write(str_mod, tex_fname);
			}
		}
		else if (figName.substr(Nname - 4, 4) == ".pdf") {
			figName = figName.substr(0, Nname - 4);
			// ==== pdf ====
			format = "pdf";
			fname_in.clear(); fname_in << gv::path_in << "figures/" << figName << "." << format;
			if (!file_exist(fname_in))
				throw internal_err(u8"图片 \"" + fname_in + u8"\" 未找到");
			fig_hash = sha1sum_f(fname_in).substr(0, 16);
			fig_ext_hash[i][format] = fig_hash;
			// rename figure files with hash
			fname_in2.clear(); fname_in2 << gv::path_in << "figures/" << fig_hash << "." << format;
			if (figName != fig_hash) {
				file_copy(fname_in2, fname_in, true);
				img_to_delete.insert(fname_in);
				// modify .tex file
				tex_fname.clear(); tex_fname << gv::path_in << "contents/" << entry << ".tex";
				read(str_mod, tex_fname);
				replace(str_mod, figName + "." + format, fig_hash + "." + format);
				write(str_mod, tex_fname);
			}

			// ==== svg =====
			format = "svg";
			fname_in.clear(); fname_in << gv::path_in << "figures/" << figName << "." << format;
			if (file_exist(fname_in)) {
				// svg and pdf hashes (or old file name rule) are still the same (old standard)
				read(tmp, fname_in);
				CRLF_to_LF(tmp);
				fig_hash = sha1sum(tmp).substr(0, 16);
				fig_ext_hash[i][format] = fig_hash;
				fname_in2.clear(); fname_in2 << gv::path_in << "figures/" << fig_hash << "." << format;
				// rename figure files with hash
				if (figName != fig_hash) {
					file_copy(fname_in2, fname_in, true);
					img_to_delete.insert(fname_in);
				}
			}
			else {
				// svg and pdf hashes not the same (new standard)
				fig_hash = get_text("figures", "id", fig_ids[i], "image_alt", db_read);
				if (size(fig_hash) != 16) {
					SLS_WARN(u8"发现 figures.image_alt 仍然带有拓展名，将模拟编辑器删除。");
					fig_hash.resize(16);
				}
				fig_ext_hash[i][format] = fig_hash;
				fname_in2.clear(); fname_in2 << gv::path_in << "figures/" << svg_file;
			}
		}
		else
			throw internal_err(u8"图片格式暂不支持：" + figName);

		fname_out.clear(); fname_out << gv::path_out << fig_hash << "." << format;
		file_copy(fname_out, fname_in2, true);

		// get caption of figure
		ind0 = find_command(str, "caption", ind0);
		if (ind0 < 0) {
			throw scan_err(u8"图片标题未找到， 请使用上传图片按钮！");
		}
		command_arg(caption, str, ind0);
		if (find(caption, "\\footnote") >= 0)
			throw scan_err(u8"图片标题中不能添加 \\footnote{}");
		// insert html code
		widthPt = num2str((33 / 14.25 * width * 100)/100.0, 4);
		href.clear(); href << gv::url << fig_hash << "." << format;
		tmp.clear(); tmp << R"(<div class = "w3-content" style = "max-width:)" << widthPt << "em;\">\n"
				"<a href=\"" << href << R"(" target = "_blank"><img src = ")" << href
				<< "\" alt = \"" << (gv::is_eng? "Fig" : u8"图")
				<< "\" style = \"width:100%;\"></a>\n</div>\n<div align = \"center\"> "
				<< (gv::is_eng ? "Fig. " : u8"图 ") << figNo
				<< u8"：" << caption << "</div>";
		str.replace(intvFig.L(i), intvFig.R(i) - intvFig.L(i) + 1, tmp);
		++N;
	}
	return N;
}

// these environments are already not in a paragraph
// return number of environments processed
inline Long theorem_like_env(Str_IO str)
{
	Long N, N_tot = 0, ind0;
	Intvs intvIn, intvOut;
	Str env_title, env_num, tmp;
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
			tmp.clear(); tmp << "<div class = \"w3-panel " << envBorderColors[i]
					<< " w3-leftbar\">\n <h3 style=\"margin-top: 0; padding-top: 0;\"><b>"
					<< envCnNames[i] << " "
					<< env_num << u8"</b>　" << env_title << "</h3>\n";
			str.replace(ind0, ind1 - ind0, tmp);
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
