#pragma once

// for google translation: hide code to protect them from translation
// replace with `\verb|编号|`
inline Long hide_verbatim(vecStr_O str_verb, Str_IO str)
{
	Long ind0 = 0, ind1, ind2;
	char dlm;
	Str tmp;

	// verb
	while (1) {
		ind0 = find_command(str, "verb", ind0);
		if (ind0 < 0)
			break;
		ind1 = str.find_first_not_of(U' ', ind0 + 5);
		if (ind1 < 0)
			throw scan_err(u8"\\verb 没有开始");
		dlm = str[ind1];
		if (dlm == U'{')
			throw scan_err(u8"\\verb 不支持 {...}， 请使用任何其他符号如 \\verb|...|， \\verb@...@");
		ind2 = find(str, dlm, ind1 + 1);
		if (ind2 < 0)
			throw scan_err(u8"\\verb 没有闭合");
		if (ind2 - ind1 == 1)
			throw scan_err(u8"\\verb 不能为空");
		tmp = str.substr(ind0, ind2 + 1 - ind0);
		replace(tmp, "\n", "PhysWikiScanLF");
		str_verb.push_back(tmp);
		str.replace(ind0, ind2 + 1 - ind0, "\\verb|" + num2str(size(str_verb)-1, 4) + "|");
		ind0 += 3;
	}

	// lstinline
	ind0 = 0;
	while (1) {
		ind0 = find_command(str, "lstinline", ind0);
		if (ind0 < 0)
			break;
		ind1 = str.find_first_not_of(U' ', ind0 + 10);
		if (ind1 < 0)
			throw scan_err(u8"\\lstinline 没有开始");
		dlm = str[ind1];
		if (dlm == U'{')
			throw scan_err(u8"lstinline 不支持 {...}， 请使用任何其他符号如 \\lstinline|...|， \\lstinline@...@");
		ind2 = find(str, dlm, ind1 + 1);
		if (ind2 < 0)
			throw scan_err(u8"\\lstinline 没有闭合");
		if (ind2 - ind1 == 1)
			throw scan_err(u8"\\lstinline 不能为空");
		tmp = str.substr(ind0, ind2 + 1 - ind0);
		replace(tmp, "\n", "PhysWikiScanLF");
		str_verb.push_back(tmp);

		str.replace(ind0, ind2 + 1 - ind0, "\\verb|" + num2str(size(str_verb)-1, 4) + "|");
		ind0 += 3;
	}

	// lstlisting
	vecStr str_verb1;
	ind0 = 0;
	Intvs intv;
	Str code;
	Long N = find_env(intv, str, "lstlisting", 'o');
	Str lang; // language
	for (Long i = N - 1; i >= 0; --i) {
		tmp = str.substr(intv.L(i), intv.R(i) + 1 - intv.L(i));
		replace(tmp, "\n", "PhysWikiScanLF");
		str_verb1.push_back(tmp);
		str.replace(intv.L(i), intv.R(i) + 1 - intv.L(i), "\\verb|" + num2str(size(str_verb)+i, 4) + "|");
	}
	for (Long i = 0; i < N; ++i) {
		str_verb.push_back(str_verb1.back());
		str_verb1.pop_back();
	}
	return str_verb.size();
}

// for google translation: hide equations and verbatims
inline void hide_eq_verb(Str_IO str)
{
	Str tmp;
	vecStr eq_list, verb_list;
	Intvs intv, intv1;
	find_inline_eq(intv, str, 'o');
	find_display_eq(intv1, str, 'o');
	combine(intv, intv1);

	for (Long i = intv.size() - 1; i >= 0; --i) {
		tmp = str.substr(intv.L(i), intv.R(i) - intv.L(i) + 1);
		replace(tmp, "\n", "PhysWikiScanLF");
		eq_list.push_back(tmp);
		str.replace(intv.L(i), intv.R(i) - intv.L(i) + 1, "$" + num2str(size(eq_list)-1, 4) + "$");
	}
	hide_verbatim(verb_list, str);
	// save to files
	write_vec_str(eq_list, gv::path_data + "eq_list.txt");
	write_vec_str(verb_list, gv::path_data + "verb_list.txt");
}

inline void unhide_eq_verb(Str_IO str)
{
	Str label;
	vecStr eq_list, verb_list;
	read_vec_str(eq_list, gv::path_data + "eq_list.txt");
	read_vec_str(verb_list, gv::path_data + "verb_list.txt");
	for (Long i = 0; i < size(eq_list); ++i) {
		label = "$" + num2str(i, 4) + "$";
		Long ind = find(str, label);
		sb = eq_list[i];
		replace(sb, "PhysWikiScanLF", "\n");
		if (ind < 0) {
			clear(sb1) << label << u8" 没有找到，替换： \n" << sb << "\n";
			scan_warn(sb1);
		}
		else
			str.replace(ind, label.size(), sb);
	}

	for (Long i = 0; i < size(verb_list); ++i) {
		label = "\\verb|" + num2str(i, 4) + "|";
		Long ind = find(str, label);
		sb = verb_list[i];
		replace(sb, "PhysWikiScanLF", "\n");
		if (ind < 0) {
			clear(sb1) << label << u8" 没有找到，替换： \n" << sb << "\n";
			scan_warn(sb1);
		}
		else
			str.replace(ind, label.size(), sb);
	}
}
