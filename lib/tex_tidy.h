#pragma once

std::unordered_set<Char32> illegal_chars;

// ensure space around a char
inline Long ensure_space_around(Str_IO str, Str_I c)
{
	Long N = 0, ind0 = 0;
	while (1) {
		ind0 = find(str, c, ind0);
		if (ind0 < 0) break;
		Long ind1 = ind0 + c.size();
		// check right
		if (ind1 == size(str)) {
			str += ' '; ++N;
		}
		else if (str[ind1] != ' ') {
			str.insert(ind0+1, " "); ++N;
		}
		// check left
		if (ind0 == 0 || str[ind0-1] != ' ') {
			str.insert(ind0, " "); ++N;
		}
		++ind0;
	}
	return N;
}

// globally forbidden characters
// assuming comments are deleted and verbose envs are not hidden
inline void global_forbid_char(Str_I str)
{
	// some of these may be fine in wuli.wiki/editor, but will not show when compiling pdf with XeLatex
	static const Str32 forbidden32 = U"αΑ∵⊥βΒ⋂◯⋃•∩∪⋯∘χΧΔδ⋄ϵ∃Ε≡⊓⊔⊏⊐□⋆ηΗ∀Γγ⩾≥≫⋙∠≈ℏ∈∫∬∭∞ιΙΚκΛλ⩽⟵⟶"
		U"⟷⟸⟹⟺≤⇐←⇇↔≪⋘↦∡∓μΜ≠∋∉⊈νΝ⊙∮ωΩ⊕⊗∥∂⟂∝ΦϕπΠ±ΨψρΡ⇉⇒σΣ∼≃⊂⊆"
		U"⊃⊇∑Ττ∴θ×Θ→⊤◁▷↕⇕⇈⇑Υ↑≐↓⇊†‡⋱⇓υε∅ϰφςϖϱϑ∨∧ΞξΖζ▽Οο⊖"
		U"⓪①②③④⑤⑥⑦⑧⑨⑩⑪⑫⑬⑭⑮⑯⑰⑱⑲⑳";
	static vecStr forbidden;

	if (forbidden.empty())
		for (auto c : forbidden32)
			forbidden.push_back(u8(c));

	// check repetition
	static bool checked = false;
	if (!checked) {
		checked = true;
		Long ind = find_repeat(forbidden);
		if (ind >= 0) {
			cout << "ind = " << ind << ", char = " << forbidden[ind] << endl;
			throw internal_err("found repeated char in `forbidden`");
		}
	}

	Long ikey, ind = find(ikey, str, forbidden);
	if (ind >= 0) {
		Long beg = max(Long(0),ind-30);
		scan_warn(u8"latex 代码中出现非法字符，建议使用公式环境和命令表示： " + str.substr(beg, ind-beg) + u8"？？？");
		// TODO: scan 应该搞一个批量替换功能
		illegal_chars.insert(char32(str, ind));
	}

	// overleaf only suppors Basic Multilingual Plane (BMP) characters
	// we should do that too (except for comments)
	u8_iter it(str), it_end(str, -1);
	for (; it != it_end; ++it) {
		if (u32(*it)[0] > 65536) {
			Long beg = max(Long(0), (Long)it-30);
			scan_warn(u8"latex 代码中出现非法字符，建议使用公式环境和命令表示： " + str.substr(beg, (Long)it-beg) + u8"？？？");
			illegal_chars.insert(str[ind]);
		}
	}
}

inline void limit_env_cmd(Str_I str)
{
	// commands not supported
	if (find_command(str, "documentclass") >= 0)
		throw scan_err(u8"\"documentclass\" 命令已经在 main.tex 中，每个词条文件是一个 section，请先阅读说明");
	if (find_command(str, "newcommand") >= 0)
		throw scan_err(u8"不支持用户 \"newcommand\" 命令， 可以尝试在设置面板中定义自动补全规则， 或者向管理员申请 newcommand");
	if (find_command(str, "renewcommand") >= 0)
		throw scan_err(u8"不支持用户 \"renewcommand\" 命令， 可以尝试在设置面板中定义自动补全规则");
	if (find_command(str, "usepackage") >= 0)
		throw scan_err(u8"不支持 \"usepackage\" 命令， 每个词条文件是一个 section，请先阅读说明");
	if (find_command(str, "newpage") >= 0)
		throw scan_err(u8"暂不支持 \"newpage\" 命令");
	if (find_command(str, "title") >= 0)
		throw scan_err(u8"\"title\" 命令已经在 main.tex 中，每个词条文件是一个 section，请先阅读说明");
	if (find_command(str, "author") >= 0)
		throw scan_err(u8"不支持 \"author\" 命令， 每个词条文件是一个 section，请先阅读说明");
	if (find_command(str, "maketitle") >= 0)
		throw scan_err(u8"不支持 \"maketitle\" 命令， 每个词条文件是一个 section，请先阅读说明");

	// allowed environments
	// there are bugs when auto inserting label into align or gather, e.g. when there are "\\" within matrices
	vecStr envs_allow = { "equation", "gather", "gather*", "gathered", "align", "align*",
		/*"alignat", "alignat*", "alignedat",*/ "aligned", "split", "figure", "itemize",
		"enumerate", "lstlisting", "example", "exercise", "lemma", "theorem", "definition",
		"corollary", "matrix", "pmatrix", "vmatrix", "table", "tabular", "cases", "array",
		"case", "Bmatrix", "bmatrix", /*"eqnarray", "eqnarray*", "multline", "multline*",
		"smallmatrix",*/ "subarray", "Vmatrix", "issues" };

	Str env;
	Long ind0 = -1;
	while (1) {
		ind0 = find_command(str, "begin", ind0+1);
		if (ind0 < 0)
			break;
		command_arg(env, str, ind0);
		if (search(env, envs_allow) < 0) {
			if (env == "document")
				throw scan_err(u8"document 环境已经在 main.tex 中，每个词条文件是一个 section，请先阅读说明。");
			else
				throw scan_err(u8"暂不支持 " + env + u8" 环境！ 如果你认为 MathJax 支持该环境， 请联系管理员。");
		}
	}
}

// warn if no space after \autoref{}
inline Long autoref_space(Str_I str, Bool_I error)
{
	Long ind0 = 0, N = 0;
	Str follow = u8" ，、．。）~”\n";
	vecStr follow2 = { "\\begin" };
	Str msg;
	while (1) {
		ind0 = find_command(str, "autoref", ind0);
		Long start = ind0;
		if (ind0 < 0)
			return N;
		try {ind0 = skip_command(str, ind0, 1);}
		catch (...) {
			scan_warn(u8"\\autoref 后面没有大括号: " + str.substr(ind0, 20));
		}
		if (ind0 >= size(str))
			return N;
		Bool continue2 = false;
		for (Long i = 0; i < size(follow2); ++i) {
			if (expect(str, follow2[i], ind0) >= 0) {
				continue2 = true; break;
			}
		}
		if (continue2)
			continue;
		if (find(follow, str[ind0]) >= 0)
			continue;
		msg = u8"\\autoref{} 后面需要空格: “" + str.substr(start, ind0 + 20 - start) + u8"”";
		if (error)
			throw scan_err(msg);
		else
			scan_warn(msg);
		++N;
	}
}

// 1. make sure there is a ~ between autoref and upref
// 2. make sure autoref and upref is for the same entry
// 3. make sure label in autoref has underscore
// 4. make sure autoref for other entry always has upref before or after
inline Long autoref_tilde_upref(Str_IO str, Str_I entry)
{
	Long ind0 = 0, N = 0;
	Str label, entry1, entry2;
	while (1) {
		ind0 = find_command(str, "autoref", ind0);
		if (ind0 < 0)
			return N;
		command_arg(label, str, ind0);
		Long ind5 = ind0;
		ind0 = skip_command(str, ind0, 1);
		if (ind0 == size(str))
			return N;
		Long ind1 = expect(str, "~", ind0);
		if (ind1 < 0) {
			if (expect(str, "\\upref", ind0) > 0)
				throw scan_err(u8"\\autoref{} 和 \\upref{} 中间应该有 ~");
			Long ind2 = find(label, '_');
			if (ind2 < 0)
				throw scan_err("\\autoref{" + label + u8"} 中必须有下划线， 请使用“内部引用”或“外部引用”按钮");
			ind5 = (Long)str.rfind("\\upref", ind5-1); entry2.clear();
			if (ind5 > 0 && ExpectKeyReverse(str, "~", ind5 - 1) < 0)
				command_arg(entry2, str, ind5);
			sb = label_entry_old(label);
			if (sb != entry && sb != entry2)
				throw scan_err("\\autoref{" + label + u8"} 引用其他词条时， 后面必须有 ~\\upref{}， 建议使用“外部引用”按钮； 也可以把 \\upref{} 放到前面");
			continue;
		}
		Long ind2 = expect(str, "\\upref", ind1);
		if (ind2 < 0)
			throw scan_err(u8"\\autoref{} 后面不应该有单独的 ~");
		command_arg(entry1, str, ind2 - 6);
		if (label_entry_old(label) != entry1) {
			clear(sb) << "\\autoref{" << label << "}~\\upref{" << entry1 << u8"} 不一致， 请使用“外部引用”按钮";
			throw scan_err(sb);
		}
		str.erase(ind1-1, 1); // delete ~
		ind0 = ind2;
	}
}

// delete spaces around chinese punctuations
inline Long rm_chinese_punc_space(Str_IO str)
{
	vecStr keys = { u8"，", u8"、", u8"．", u8"。", u8"？", u8"（", u8"）", u8"：", u8"；", u8"【", u8"】", u8"…"};
	Long N = 0, ikey;
	u8_iter it(str, 0);
	while (1) {
		Long ind = find(ikey, str, keys, it);
		if (ind < 0)
			return N;
		it = ind;
		ind = it+1;
		if (ind < size(str) && str[ind] == ' ')
			str.erase(ind, 1);
		ind = it-1;
		if (it > 0 && str[ind] == ' ') {
			str.erase(ind, 1); --it;
		}
		++N; ++it;
	}
}

// warn english punctuations , . " ? ( ) in normal text
// when replace is false, set error = true to throw error instead of console warning
// return the number replaced
inline Long check_normal_text_punc(Str_IO str, Bool_I error, Bool_I replace = false)
{
	vecStr keys = {",", ".", "?", "(", ":"};
	Str keys_rep = u8"，。？（：";
	Intvs intvNorm;
	FindNormalText(intvNorm, str);
	Long ind0 = -1, ikey = -100, N = 0;
	while (1) {
		ind0 = find(ikey, str, keys, ind0 + 1);
		if (ind0 < 0)
			break;

		if (is_in(ind0, intvNorm)) {
			Long ind1 = str.find_last_not_of(" ", ind0-1);
			if (ind1 >= 0 && is_chinese(str, ind1)) {
				str.find_first_not_of(" ", ind0+1);
				// --- exceptions ---
				if (ikey == 1 && is_alphanum(str[ind0 + 1]))
					continue; // e.g. "文件名.jpg"
				else if (ikey == 3) {
					if (is_alphanum(str[ind0+1])) {
						if (is_alphanum(str[ind0+2])) {
							if (str[ind0+3] == ')')
								continue; // e.g. "(a)", "(1)"
						}
						else if (str[ind0+2] == ')')
							continue; // e.g. "(a1)", "(24)"
					}
				}
				// --- end exceptions ---
				if (replace) {
					str[ind0] = keys_rep[ikey]; ++N;
					if (keys[ikey] == "(") {
						// replace possible matching ')' as well
						Long ind1 = str.find_first_of("()", ind0);
						if (ind1 > 0 && str[ind1] == ')' && is_in(ind1, intvNorm)) {
							str.replace(ind1, 1, u8"）"); ++N;
						}
					}
				}
				else {
					if (error)
						throw scan_err(u8"正文中使用英文标点：“" + str.substr(ind0, 40) + u8"”");
					else
						scan_warn(u8"正文中使用英文标点：“" + str.substr(ind0, 40) + u8"”\n");
				}
			}
		}
	}
	return N;
}

// check escape characters in normal text i.e. `& # _ ^`
inline Long check_normal_text_escape(Str_IO str)
{
//    vecStr keys = {"&", "#", "_", "^"};
//    Intvs intvNorm;
//    FindNormalText(intvNorm, str);
//    Long ind0 = -1, ikey, N = 0;
//    while (1) {
//        ind0 = find(ikey, str, keys, ind0 + 1);
//        if (ind0 < 0)
//            break;
//        if (is_in(ind0, intvNorm))
//            if (ind0 > 0 && str[ind0-1] != '\\')
//                throw scan_err(u8"正文中出现非法字符： " + str.substr(ind0, 20));
//    }
//    return N;
	return 0;
}

// insert space between chinese and alpha-numeric characters
inline Long chinese_alpha_num_space(Str_IO str)
{
	Long N = 0;
	u8_iter it(str, -1);
	for (; it >= 1; --it) {
		Long ind = it-1;
		if (is_chinese(str, ind) && is_alphanum(str[it])) {
			str.insert(str.begin() + it, ' ');
			++N;
		}
		if (is_alphanum(str[ind]) && is_chinese(str, it)) {
			str.insert(str.begin() + it, ' ');
			++N;
		}
	}
	return N;
}

// ensure spaces outside of chinese double quotes, if it's next to a chinese character
// return the number of spaces added
inline Long chinese_double_quote_space(Str_IO str)
{
	// chinese left and right quotes
	vecStr quotes = { u8(Char32(8220)) , u8(Char32(8221)) };
	Intvs intNorm;
	FindNormalText(intNorm, str);
	vecLong inds; // locations to insert space
	Long ind = -1, ikey;
	while (1) {
		ind = find(ikey, str, quotes, ind + 1);
		if (ind < 0)
			break;
		if (is_in(ind, intNorm)) {
			if (ikey == 0) { // left quote
				if (ind > 0) {
					Long ind10 = skip_char8(str, ind, -1);
					/*if (search(c, u8" \n，。．！？…：()（）：【】") >= 0)
						continue;*/
					if (is_chinese(str, ind10) || is_alphanum(str[ind10]))
						inds.push_back(ind);
				}
			}
			else { // right quote
				if (ind < size(str) - 1) {
					Long ind10 = skip_char8(str, ind, 1);
					/*if (search(c, u8" \n，。．！？…：()（）：【】") >= 0)
						continue;*/
					if (is_chinese(str, ind10) || is_alphanum(str[ind10]))
						inds.push_back(ind10);
				}

			}
		}
	}
	for (Long i = size(inds) - 1; i >= 0; --i)
		str.insert(str.begin() + inds[i], ' ');
	return size(inds);
}

inline Long inline_eq_space(Str_IO str)
{
	Long N = 0;
	Intvs intv;
	find_inline_eq(intv, str, 'o');
	for (Long i = intv.size() - 1; i >= 0; --i) {
		Long ind0 = intv.R(i) + 1;
		if (ind0 < size(str) && is_chinese(str, ind0)) {
			str.insert(str.begin() + ind0, ' ');
			++N;
		}
		ind0 = skip_char8(str, intv.L(i), -1);
		if (is_chinese(str, ind0)) {
			str.insert(str.begin() + intv.L(i), ' ');
			++N;
		}
	}
	return N;
}

// check format and auto correct .tex files in path0
inline void add_space_around_inline_eq(Str_I path_in)
{
	vecStr names, str_verb;
	Str fname, str;
	Intvs intv;
	file_list_ext(names, path_in + "contents/", "tex", false);
	
	//RemoveNoEntry(names);
	if (names.empty()) return;
	//names.resize(0); names.push_back("Sample"));
	
	for (unsigned i{}; i < names.size(); ++i) {
		cout << i << " ";
		cout << names[i] << "...";
		if (names[i] == "Sample" || names[i] == "edTODO")
			continue;
		// main process
		fname = path_in + "contents/" + names[i] + ".tex";
		read(str, fname);
		CRLF_to_LF(str);
		// verbatim(str_verb, str);
		// TODO: hide comments then recover at the end
		// find_comments(intv, str, "%");
		Long N;
		try{ N = inline_eq_space(str); }
		catch (...) {
			continue;
		}

		// verb_recover(str, str_verb);
		if (N > 0)
			write(str, fname);
		cout << endl;
	}
}