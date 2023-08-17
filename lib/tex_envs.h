#pragma once

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
