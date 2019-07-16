﻿#pragma once
#include "../SLISC/file.h"
#include "../SLISC/input.h"
#include "../TeX/tex2html.h"
#include "../Matlab/matlab2html.h"

using namespace slisc;
Str32 err_msg;

// get the title (defined in the first comment, can have space after %)
// limited to 20 characters
// return -1 if error, 0 if successful
inline Long get_title(Str32_O title, Str32_I str)
{
	if (str.at(0) != U'%') {
		err_msg = U"请在第一行注释标题!"; // break point here
		return -1;
	}
	Str32 c;
	Long ind0 = NextNoSpace(c, str, 1);
	if (ind0 < 0) {
		err_msg = U"标题不能为空!"; // break point here
		return -1;
	}
	Long ind1 = str.find(U'\n', ind0);
	if (ind1 < 0) {
		err_msg = U"正文不能为空!"; // break point here
		return -1;
	}
	title = str.substr(ind0, ind1 - ind0);
	return 0;
}

// trim “\n" and " " on both sides
// remove unnecessary "\n"
// replace “\n\n" with "\n</p>\n<p>　　\n"
inline Long paragraph_tag1(Str32_IO str)
{
	Long ind0 = 0, N = 0;
	trim(str, U" \n");
	// delete extra '\n' (more than two continuous)
	while (true) {
		ind0 = str.find(U"\n\n\n", ind0);
		if (ind0 < 0)
			break;
		eatR(str, ind0 + 2, U"\n");
	}

	// replace "\n\n" with "\n</p>\n<p>　　\n"
	N = replace(str, U"\n\n", U"\n</p>\n<p>　　\n");
	return N;
}

inline Long paragraph_tag(Str32_IO str)
{
	Long N = 0, ind0 = 0, left = 0, length, ikey;
	Intvs intv;
	Str32 temp, env, end, begin = U"<p>　　\n";

	// "begin", and commands that cannot be in a paragraph
	vector<Str32> commands = {U"\\begin",
		U"\\subsection", U"\\subsubsection", U"\\pentry", U"\\code", U"\\Code"};

	// environments that must be in a paragraph (use "<p>" instead of "<p>　　" when at the start of the paragraph)
	vector<Str32> envs_eq = {U"equation", U"align", U"gather", U"lstlisting"};

	// environments that needs paragraph tags inside
	vector<Str32> envs_p = { U"exam", U"exer"};

	// 'n' (for normal); 'e' (for env_eq); 'p' (for env_p); 'f' (end of file)
	char next, last = 'n';

	Intvs intvInline;
	find_inline_eq(intvInline, str);

	// handle normal text intervals separated by
	// commands in "commands" and environments
	while (true) {
		// decide mode
		if (ind0 == str.size()) {
			next = 'f';
		}
		else {
			ind0 = find(ikey, str, commands, ind0);
			if (ind0 < 0)
				next = 'f';
			else {
				next = 'n';
				if (ikey == 0) { // found environment
					if (is_in(ind0, intvInline)) {
						++ind0; continue; // ignore in inline equation
					}
					command_arg(env, str, ind0);
					if (search(env, envs_eq) >= 0) {
						next = 'e';
					}
					else if (search(env, envs_p) >= 0) {
						next = 'p';
					}
				}
			}
		}

		// decide ending tag
		if (next == 'n' || next == 'p') {
			end = U"\n</p>\n";
		}
		else if (next == 'e') {
			// equations can be inside paragraph
			if (ExpectKeyReverse(str, U"\n\n", ind0 - 1) >= 0) {
				end = U"\n</p>\n<p>\n";
			}
			else
				end = U"\n";
		}
		else if (next == 'f') {
			end = U"\n</p>\n";
		}

		// add tags
		length = ind0 - left;
		temp = str.substr(left, length);
		N += paragraph_tag1(temp);
		if (temp.empty()) {
			if (next == 'e' && last != 'e') {
				temp = U"\n<p>\n";
			}
			else if (last == 'e' && next != 'e') {
				temp = U"\n</p>\n";
			}
			else {
				temp = U"\n";
			}
		}
		else {
			temp = begin + temp + end;
		}
		str.replace(left, length, temp);
		find_inline_eq(intvInline, str);
		if (next == 'f')
			return N;
		ind0 += temp.size() - length;
		
		// skip
		if (ikey == 0) {
			if (next == 'p') {
				Long ind1 = skip_env(str, ind0);
				Long left, right;
				left = inside_env(right, str, ind0, 2);
				length = right - left;
				temp = str.substr(left, length);
				paragraph_tag(temp);
				str.replace(left, length, temp);
				find_inline_eq(intvInline, str);
				ind0 = ind1 + temp.size() - length;
			}
			else
				ind0 = skip_env(str, ind0);
		}
		else
			ind0 = skip_command(str, ind0, 1);

		left = ind0;
		last = next;
		if (ind0 == str.size()) {
			continue;
		}
		
		// beginning tag for next interval
		
		if (last == 'n' || last == 'p')
			begin = U"\n<p>　　\n";
		else if (last == 'e') {
			if (expect(str, U"\n\n", ind0) >= 0) {
				begin = U"\n</p>\n<p>　　\n";
			}
			else
				begin = U"\n";
		}	
	}
}

// add html id tag before environment if it contains a label, right before "\begin{}" and delete that label
// output html id and corresponding label for future reference
// `gather` and `align` environments has id starting wth `ga` and `ali`
// `idNum` is in the idNum-th environment of the same name (not necessarily equal to displayed number)
// no comment allowed
// return number of labels processed, or -1 if failed
inline Long EnvLabel(vector_IO<Str32> ids, vector_IO<Str32> labels, Str32_I entryName, Str32_IO str)
{
	Long ind0{}, ind1{}, ind2{}, ind3{}, ind4{}, ind5{}, N{}, temp{},
		Ngather{}, Nalign{}, i{}, j{};
	Str32 idName; // "eq" or "fig" or "ex"...
	Str32 envName; // "equation" or "figure" or "exam"...
	Str32 idNum{}; // id = idName + idNum
	Long idN{}; // convert to idNum
	while (true) {
		ind5 = str.find(U"\\label", ind0);
		if (ind5 < 0) return N;
		// make sure label is inside an environment
		ind2 = str.rfind(U"\\end", ind5);
		ind4 = str.rfind(U"\\begin", ind5);
		if (ind2 >= ind4) {
			err_msg = U"label 不在环境内!"; // break point here
			return -1;
		}
		// detect environment kind
		ind1 = expect(str, U"{", ind4 + 6);
		if (expect(str, U"equation", ind1) > 0) {
			idName = U"eq"; envName = U"equation";
		}
		else if (expect(str, U"figure", ind1) > 0) {
			idName = U"fig"; envName = U"figure";
		}
		else if (expect(str, U"exam", ind1) > 0) {
			idName = U"ex"; envName = U"exam";
		}
		else if (expect(str, U"exer", ind1) > 0) {
			idName = U"exe"; envName = U"exer";
		}
		else if (expect(str, U"table", ind1) > 0) {
			idName = U"tab"; envName = U"table";
		}
		else if (expect(str, U"gather", ind1) > 0) {
			idName = U"eq"; envName = U"gather";
		}
		else if (expect(str, U"align", ind1) > 0) {
			idName = U"eq"; envName = U"align";
		}
		else {
			err_msg = U"该环境不支持 label!";
			return -1; // break point here
		}
		// check label format and save label
		ind0 = expect(str, U"{", ind5 + 6);
		ind3 = expect(str, entryName + U'_' + idName, ind0);
		if (ind3 < 0) {
			err_msg = U"label 格式错误， 是否为 \"" + entryName + U'_' + idName + U"\"？";
			return -1; // break point here
		}
		ind3 = str.find(U"}", ind3);
		labels.push_back(str.substr(ind0, ind3 - ind0));
		trim(labels.back());
		// count idNum, insert html id tag, delete label
		Intvs intvEnv;
		if (idName != U"eq") {
			idN = find_env(intvEnv, str.substr(0,ind4), envName) + 1;
		}
		else { // count equations
			idN = find_env(intvEnv, str.substr(0,ind4), U"equation");
			Ngather = find_env(intvEnv, str.substr(0,ind4), U"gather");
			if (Ngather > 0) {
				for (i = 0; i < Ngather; ++i) {
					for (j = intvEnv.L(i); j < intvEnv.R(i); ++j) {
						if (str.at(j) == '\\' && str.at(j+1) == '\\')
							++idN;
					}
					++idN;
				}
			}
			Nalign = find_env(intvEnv, str.substr(0,ind4), U"align");
			if (Nalign > 0) {
				for (i = 0; i < Nalign; ++i) {
					for (j = intvEnv.L(i); j < intvEnv.R(i); ++j) {
						if (str.at(j) == '\\' && str.at(j + 1) == '\\')
							++idN;
					}
					++idN;
				}
			}
			if (envName == U"gather" || envName == U"align") {
				for (i = ind4; i < ind5; ++i) {
					if (str.at(i) == U'\\' && str.at(i + 1) == U'\\')
						++idN;
				}
			}
			++idN;
		}
		num2str(idNum, idN);
		ids.push_back(idName + idNum);
		str.erase(ind5, ind3 - ind5 + 1);
		str.insert(ind4, U"<span id = \"" + ids.back() + U"\"></span>");
		ind0 = ind4 + 6;
	}
}

// replace figure environment with html image
// convert vector graph to SVG, font must set to "convert to outline"
// if svg image doesn't exist, use png, if doesn't exist, return -1
// path must end with '\\'
inline Long FigureEnvironment(Str32_IO str, Str32_I path_out)
{
	Long i{}, N{}, Nfig{}, ind0{}, ind1{}, indName1{}, indName2{};
	double width{}; // figure width in cm
	Intvs intvFig;
	Str32 figName;
	Str32 format, caption, widthPt, figNo;
	Nfig = find_env(intvFig, str, U"figure", 'o');
	for (i = Nfig - 1; i >= 0; --i) {
		// get width of figure
		ind0 = str.find(U"width", intvFig.L(i)) + 5;
		ind0 = expect(str, U"=", ind0);
		ind0 = find_num(str, ind0);
		str2double(width, str, ind0);
		// get file name of figure
		indName1 = str.find(U"figures/", ind0) + 8;
		indName2 = str.find(U".pdf", intvFig.L(i)) - 1;
		if (indName2 < 0)
			indName2 = str.find(U".png", intvFig.L(i)) - 1;
		if (indName2 < 0) {
			err_msg = U"读取图片名错误!"; // breakpoint here
			return -1;
		}
		figName = str.substr(indName1, indName2 - indName1 + 1);
		// get caption of figure
		ind0 = str.find(U"\\caption", ind0);
		if (ind0 < 0) {
			err_msg = U"图片标题未找到!";
			return -1; // break point here
		}
		ind0 = expect(str, U"{", ind0 + 8);
		ind1 = pair_brace(str, ind0);
		caption = str.substr(ind0, ind1 - ind0);
		str.erase(intvFig.L(i), intvFig.R(i) - intvFig.L(i) + 1); // delete environment

		// test img file existence
		if (file_exist(path_out + figName + ".svg"))
			format = U".svg";
		else if (file_exist(path_out + figName + ".png"))
			format = U".png";
		else {
			err_msg = U"图片 \"" + figName + "\" 未找到!";
			return -1; // break point here
		}
		// insert html code
		num2str(widthPt, (Long)(33.4 * width));
		num2str(figNo, i + 1);
		str.insert(intvFig.L(i), U"<div class = \"w3-content\" style = \"max-width:" + widthPt
			+ U"pt;\">\n" + U"<img src = \"" + figName + format
			+ U"\" alt = \"图\" style = \"width:100%;\">\n</div>\n<div align = \"center\"> 图" + figNo
			+ U"：" + caption + U"</div>");
		++N;
	}
	return N;
}

// get dependent entries from \pentry{}
// links are file names, not chinese titles
// links[i][0] --> links[i][1]
inline Long depend_entry(vector_IO<Long> links, Str32_I str, vector_I<Str32> entryNames, Long_I ind)
{
	Long ind0 = 0, N = 0;
	Str32 temp;
	Str32 depEntry;
	Str32 link[2];
	while (true) {
		ind0 = find_command(str, U"pentry", ind0);
		if (ind0 < 0)
			return N;
		command_arg(temp, str, ind0, 0, 't');
		Long ind1 = 0;
		while (true) {
			ind1 = find_command(temp, U"upref", ind1);
			if (ind1 < 0)
				return N;
			command_arg(depEntry, temp, ind1, 0, 't');
			Long i; Bool flag = false;
			for (i = 0; i < Size(entryNames); ++i) {
				if (depEntry == entryNames[i]) {
					flag = true; break;
				}
			}
			if (!flag) {
				err_msg = U"\\upref 引用的文件未找到: " + depEntry + ".tex";
				return -1;
			}
			links.push_back(i);
			links.push_back(ind);
			++N; ++ind1;
		}
	}
}

// replace \pentry comman with html round panel
inline Long pentry(Str32_IO str)
{
	return Command2Tag(U"pentry", U"<div class = \"w3-panel w3-round-large w3-light-blue\"><b>预备知识</b>　", U"</div>", str);
}

// remove special .tex files from a list of name
// return number of names removed
// names has ".tex" extension
inline Long RemoveNoEntry(vector_IO<Str32> names)
{
	Long i{}, j{}, N{}, Nnames{}, Nnames0;
	vector<Str32> names0; // names to remove
	names0.push_back(U"Sample");
	names0.push_back(U"FrontMatters");
	// add other names here
	Nnames = names.size();
	Nnames0 = names0.size();
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

// example is already not in a paragraph
// return number of examples processed, return -1 if failed
inline Long ExampleEnvironment(Str32_IO str)
{
	Long i{}, N{}, ind0{}, ind1{};
	Intvs intvIn, intvOut;
	Str32 exName, exNo;
	while (true) {
		ind0 = find_command_spec(str, U"begin", U"exam", ind0);
		if (ind0 < 0)
			return N;
		++N; num2str(exNo, N);
		command_arg(exName, str, ind0, 1);

		// positions of the environment
		Long ind1 = skip_command(str, ind0, 2);
		Long ind2 = find_command_spec(str, U"end", U"exam", ind0);
		Long ind3 = skip_command(str, ind2, 1);

		//// add <p> </p>
		//Str32 temp = str.substr(ind1, ind2 - ind1);
		//ParagraphTag(temp);
		
		// replace
		str.replace(ind2, ind3 - ind2, U"</div>\n");
		// str.replace(ind1, ind2 - ind1, temp);
		str.replace(ind0, ind1 - ind0, U"<div class = \"w3-panel w3-border-yellow w3-leftbar\">\n <h5><b>例"
			+ exNo + U"</b>　" + exName + U"</h5>\n");
	}
	return N;
}

// example is already not in a paragraph
// return number of examples processed, return -1 if failed
inline Long ExerciseEnvironment(Str32_IO str)
{
	Long i{}, N{}, ind0{}, ind1{};
	Intvs intvIn, intvOut;
	Str32 exName, exNo;
	while (true) {
		ind0 = find_command_spec(str, U"begin", U"exer", ind0);
		if (ind0 < 0)
			return N;
		++N; num2str(exNo, N);
		command_arg(exName, str, ind0, 1);

		// positions of the environment
		Long ind1 = skip_command(str, ind0, 2);
		Long ind2 = find_command_spec(str, U"end", U"exer", ind0);
		Long ind3 = skip_command(str, ind2, 1);

		//// add <p> </p>
		//Str32 temp = str.substr(ind1, ind2 - ind1);
		//ParagraphTag(temp);

		// replace
		str.replace(ind2, ind3 - ind2, U"</div>\n");
		// str.replace(ind1, ind2 - ind1, temp);
		str.replace(ind0, ind1 - ind0, U"<div class = \"w3-panel w3-border-green w3-leftbar\">\n <h5><b>习题"
			+ exNo + U"</b>　" + exName + U"</h5>\n");
	}
	return N;
}

// replace autoref with html link
// no comment allowed
// does not add link for \autoref inside eq environment (equation, align, gather)
// return number of autoref replaced, or -1 if failed
inline Long autoref(vector_I<Str32> ids, vector_I<Str32> labels, Str32_I entryName, Str32_IO str)
{
	unsigned i{};
	Long ind0{}, ind1{}, ind2{}, ind3{}, ind4{}, ind5{}, N{}, Neq{}, ienv{};
	Bool inEq;
	Str32 entry, label0, idName, idNum, kind, newtab, file;
	vector<Str32> envNames{U"equation", U"align", U"gather"};
	while (true) {
		newtab.clear(); file.clear();
		ind0 = str.find(U"\\autoref", ind0);
		if (ind0 < 0)
			return N;
		inEq = index_in_env(ienv, ind0, envNames, str);
		ind1 = expect(str, U"{", ind0 + 8);
		if (ind1 < 0) {
			err_msg = U"\\autoref 变量不能为空!";
			return -1;
		}
		ind1 = NextNoSpace(entry, str, ind1);
		ind2 = str.find('_', ind1);
		if (ind2 < 0) {
			err_msg = U"\\autoref 格式错误!";
			return -1;
		}
		ind3 = find_num(str, ind2);
		if (ind3 < 0) {
			err_msg = U"autoref 格式错误!";
			return -1;
		}
		idName = str.substr(ind2 + 1, ind3 - ind2 - 1);
		if (idName == U"eq") kind = U"式";
		else if (idName == U"fig") kind = U"图";
		else if (idName == U"ex") kind = U"例";
		else if (idName == U"exe") kind = U"练习";
		else if (idName == U"tab") kind = U"表";
		else {
			err_msg = U"\\label 类型错误， 必须为 eq/fig/ex/exe/tab 之一!";
			return -1; // break point here
		}
		ind3 = str.find('}', ind3);
		// find id of the label
		label0 = str.substr(ind1, ind3 - ind1); trim(label0);
		for (i = 0; i < labels.size(); ++i) {
			if (label0 == labels[i]) break;
		}
		if (i == labels.size()) {
			err_msg = U"label \"" + label0 + U"\" 未找到!";
			return -1; // break point here
		}
		ind4 = find_num(ids[i], 0);
		idNum = ids[i].substr(ind4);		
		entry = str.substr(ind1, ind2 - ind1);
		if (entry != entryName) {// reference the same entry
			newtab = U"target = \"_blank\"";
			file = entry + U".html";
		}
		if (!inEq)
			str.insert(ind3 + 1, U" </a>");
		str.insert(ind3 + 1, kind + U' ' + idNum);
		if (!inEq)
			str.insert(ind3 + 1, U"<a href = \"" + file + U"#" + ids[i] + U"\" " + newtab + U">");
		str.erase(ind0, ind3 - ind0 + 1);
		++N;
	}
}

// get a new label name for an environment for an entry
void new_label_name(Str32_O label, Str32_I envName, Str32_I entry, Str32_I str)
{
	Str32 label0;
	for (Long num = 1; ; ++num) {
		Long ind0 = 0;
		while (true) {
			label = entry + "_" + envName + num2str(num);
			ind0 = find_command(str, U"label", ind0);
			if (ind0 < 0)
				return; // label is unique
			command_arg(label0, str, ind0, 0);
			if (label == label0)
				break;
			++ind0;
		}
	}
}

// check if a label exist, if not, add it
// ind is not the label number, but the displayed number
// if exist, 1, output label
// if doesn't exist, return 0
// if failed, return -1
Long check_add_label(Str32_O label, Str32_I entry, Str32_I idName, Long ind,
	vector_I<Str32> labels, vector_I<Str32> ids, Str32_I path_in)
{
	Long ind0 = 0;
	Str32 label0, newtab;

	while (true) {
		ind0 = search(idName + num2str(ind), ids, ind0);
		if (ind0 < 0)
			break;
		if (labels[ind0].substr(0, entry.size()) != entry) {
			++ind0; continue;
		}
		label = labels[ind0];
		return 1;
	}

	// label does not exist
	Str32 full_name = path_in + "/contents/" + entry + ".tex";
	if (!file_exist(full_name)) {
		err_msg = U"文件不存在： " + entry + ".tex";
		return -1;
	}
	Str32 str;
	read_file(str, full_name);

	// find comments
	Intvs intvComm;
	find_comment(intvComm, str);

	vector<Str32> idNames = { U"eq", U"fig", U"ex", U"exe", U"tab" };
	vector<Str32> envNames = { U"equation", U"figure", U"exam", U"exer", U"table"};

	Long idNum = search(idName, idNames);
	if (idNum < 0) {
		err_msg = U"\\label 类型错误， 必须为 eq/fig/ex/exe/tab 之一!";
		return -1; // break point here
	}
	
	// count environment display number starting at ind4
	Intvs intvEnv;
	if (idName != U"eq") {
		Long Nid = find_env(intvEnv, str, envNames[idNum], 'i');
		if (ind > Nid) {
			err_msg = U"被引用对象不存在!";
			return -1;
		}

		new_label_name(label, idName, entry, str);
		str.insert(intvEnv.L(ind - 1), U"\\label{" + label + "}");
		write_file(str, full_name);
		return 0;
	}
	else { // count equations
		ind0 = 0;
		Long idN = 0;
		vector<Str32> eq_envs = { U"equation", U"gather", U"align" };
		Str32 env0;
		while (true) {
			ind0 = find_command(str, U"begin", ind0);
			if (ind0 < 0) {
				err_msg = U"被引用公式不存在!";
				return -1;
			}
			if (is_in(ind0, intvComm)) {
				++ind0; continue;
			}
			command_arg(env0, str, ind0);
			Long ienv = search(env0, eq_envs);
			if (ienv < 0) {
				++ind0; continue;
			}
			// found one of eq_envs
			++idN;
			if (idN == ind) {
				new_label_name(label, idName, entry, str);
				ind0 = skip_command(str, ind0, 1);
				str.insert(ind0, U"\\label{" + label + "}");
				write_file(str, full_name);
				return 0;
			}
			if (ienv > 0) {// found gather or align
				Long ind1 = skip_env(str, ind0);
				for (Long i = ind0; i < ind1; ++i) {
					if (str.substr(i, 2) == U"\\\\") {
						++idN;
						if (idN == ind) {
							new_label_name(label, idName, entry, str);
							ind0 = skip_command(str, ind0, 1);
							str.insert(i + 2, U"\n\\label{" + label + "}");
							write_file(str, full_name);
							return 0;
						}
					}		
				}
			}
			++ind0;
		}
	}
}

// replace "link{name}{http://example.com}"
// to <a href="http://example.com">name</a>
inline Long link(Str32_IO str)
{
	Long ind0 = 0, N = 0;
	while (true) {
		ind0 = find_command(str, U"link", ind0);
		if (ind0 < 0)
			return N;
		Str32 name, url;
		command_arg(name, str, ind0, 0);
		command_arg(url, str, ind0, 1);
		if (url.substr(0, 7) != U"http://" &&
			url.substr(0, 8) != U"https://") {
			err_msg = U"链接格式错误: " + url;
			return -1;
		}

		Long ind1 = skip_command(str, ind0, 2);
		str.replace(ind0, ind1 - ind0,
			"<a href=\"" + url + "\">" + name + "</a>");
		++N; ++ind0;
	}
}

// process upref
// path must end with '\\'
inline Long upref(Str32_IO str, Str32_I path_in)
{
	Long ind0 = 0, right, N = 0;
	Str32 entryName;
	while (true) {
		ind0 = find_command(str, U"upref", ind0);
		if (ind0 < 0)
			return N;
		command_arg(entryName, str, ind0);
		trim(entryName);
		if (!file_exist(path_in + U"contents/" + entryName + U".tex")) {
			err_msg = U"\\upref 引用的文件未找到： " + entryName + U".tex";
			return -1; // break point here
		}
		right = skip_command(str, ind0, 1);
		str.replace(ind0, right - ind0,
			U"<span class = \"icon\"><a href = \""
			+ entryName +
			U".html\" target = \"_blank\"><i class = \"fa fa-external-link\"></i></a></span>");
		++N;
	}
	return N;
}

// update entries.txt and titles.txt
// return the number of entries in PhysWiki.tex
// return -1 if failed
inline Long entries_titles(vector_O<Str32> titles, vector_O<Str32> entries, Str32_I path_in)
{
	entries.clear(); titles.clear();
	file_list_ext(entries, path_in + "contents/", Str32(U"tex"), false);
	RemoveNoEntry(entries);
	if (entries.size() == 0) {
		err_msg = U"未找到任何词条";
		return -1;
	}

	Long N = 0, ind0 = 0;

	//keys.push_back(U"\\entry"); keys.push_back(U"\\chapter"); keys.push_back(U"\\part");
	Str32 title; // chinese entry name, chapter name, or part name
	Str32 entryName; // entry label
	Str32 str; read_file(str, path_in + "PhysWiki.tex");
	CRLF_to_LF(str);
	titles.resize(entries.size());

	// remove comments
	Intvs intvComm;
	find_comment(intvComm, str);
	for (Long i = intvComm.size() - 1; i >= 0; --i)
		str.erase(intvComm.L(i), intvComm.R(i) - intvComm.L(i) + 1);

	while (true) {
		ind0 = str.find(U"\\entry", ind0);
		if (ind0 < 0)
			break;
		// get chinese title and entry label
		++N;
		command_arg(title, str, ind0);
		replace(title, U"\\ ", U" ");
		if (title.empty()) {
			err_msg = U"PhysWiki.tex 中词条中文名不能为空!";
			return -1;  // break point here
		}
		command_arg(entryName, str, ind0, 1);

		// record Chinese title
		Long ind = search(entryName, entries);
		if (ind < 0) {
			err_msg = U"PhysWiki.tex 中词条文件未找到!";
			return -1; // break point here
		}
		titles[ind] = title;
		++ind0;
	}

	cout << u8"\n\n警告: 以下词条没有被 PhysWiki.tex 收录，将被忽略" << endl;
	for (Long i = 0; i < Size(titles); ++i) {
		if (titles[i].empty()) {
			cout << entries[i] << endl;
			entries.erase(entries.begin() + i, entries.begin() + i + 1);
			titles.erase(titles.begin() + i, titles.begin() + i + 1);
			--i;
		}
	}
	cout << endl;
	return N;
}

// create table of content from PhysWiki.tex
// path must end with '\\'
// return the number of entries, return -1 if failed
// names is a list of filenames
// titles[i] is the chinese title of entries[i]
inline Long table_of_contents(vector_I<Str32> titles, vector_I<Str32> entries, Str32_I path_in, Str32_I path_out)
{
	Long i{}, N{}, ind0{}, ind1{}, ind2{}, ikey{}, chapNo{ -1 }, partNo{ -1 };
	vector<Str32> keys{ U"\\part", U"\\chapter", U"\\entry", U"\\Entry", U"\\laserdog"};
	vector<Str32> chineseNo{U"一", U"二", U"三", U"四", U"五", U"六", U"七", U"八", U"九",
							U"十", U"十一", U"十二", U"十三", U"十四", U"十五"};
	//keys.push_back(U"\\entry"); keys.push_back(U"\\chapter"); keys.push_back(U"\\part");
	
	Str32 title; // chinese entry name, chapter name, or part name
	Str32 entryName; // entry label
	Str32 newcomm;
	Str32 str, toc;

	read_file(newcomm, "lib/newcommand.html");
	CRLF_to_LF(newcomm);
	read_file(str, path_in + "PhysWiki.tex");
	CRLF_to_LF(str);
	read_file(toc, "lib/index_template.html"); // read html template
	CRLF_to_LF(toc);

	ind0 = toc.find(U"PhysWikiHTMLtitle");
	toc.replace(ind0, 17, U"小时物理");
	ind0 = toc.find(U"PhysWikiCommand", ind0);
	toc.erase(ind0, 15); toc.insert(ind0, newcomm);
	ind0 = toc.find(U"PhysWikiHTMLbody", ind0);
	toc.erase(ind0, 16);

	ind0 = insert(toc,
		U"<img src = \"../title.png\" alt = \"图\" style = \"width:100%;\">\n"
		U"<div class = \"w3-container w3-center w3-blue w3-text-white\">\n"
		U"<h1>小时物理</h1>\n</div>\n\n"
		U"<div class = \"w3-container\"><p>\n"
		U"<a href = \"license.html\" target = \"_blank\">版权声明</a>　\n"
		U"<a href = \"about.html\" target = \"_blank\">项目介绍</a>　\n"
		U"<a href = \"readme.html\" target = \"_blank\">使用说明</a>　\n"
		U"<a href = \"../\">返回主页</a>\n\n"
		U"<div class=\"w3-content\" style=\"max-width:400px;\">\n"
		U"	<a href=\"../tree\"><img src=\"../tree.png\" style=\"width:100%;\" alt=\"目录树\"></a><br>\n"
		U"</div>\n<br>\n\n"
		, ind0);

	// remove comments
	Intvs intvComm;
	find_comment(intvComm, str);
	for (i = intvComm.size() - 1; i >= 0; --i)
		str.erase(intvComm.L(i), intvComm.R(i) - intvComm.L(i) + 1);

	while (true) {
		ind1 = find(ikey, str, keys, ind1);
		if (ind1 < 0)
			break;
		if (ikey >= 2) { // found "\entry"
			// get chinese title and entry label
			++N;
			command_arg(title, str, ind1);
			replace(title, U"\\ ", U" ");
			if (title.empty()) {
				err_msg = U"PhysWiki.tex 中词条中文名不能为空!";
				return -1;  // break point here
			}
			command_arg(entryName, str, ind1, 1);
			// insert entry into html table of contents
			ind0 = insert(toc, U"<a href = \"" + entryName + ".html" + "\" target = \"_blank\">"
				+ title + U"</a>　\n", ind0);
			// record Chinese title
			Long ind = search(entryName, entries);
			if (ind < 0) {
				err_msg = U"PhysWiki.tex 中词条文件未找到!";
				return -1; // break point here
			}
			++ind1;
		}
		else if (ikey == 1) { // found "\chapter"
			// get chinese chapter name
			command_arg(title, str, ind1);
	 		replace(title, U"\\ ", U" ");
	 		// insert chapter into html table of contents
	 		++chapNo;
	 		if (chapNo > 0)
	 			ind0 = insert(toc, U"</p>", ind0);
	 		ind0 = insert(toc, U"\n\n<h5><b>第" + chineseNo[chapNo] + U"章 " + title
	 			+ U"</b></h5>\n<div class = \"tochr\"></div><hr><div class = \"tochr\"></div>\n<p>\n", ind0);
			++ind1;
	 	}
	 	else if (ikey == 0){ // found "\part"
	 		// get chinese part name
	 		chapNo = -1;
			command_arg(title, str, ind1);
	 		replace(title, U"\\ ", U" ");
	 		// insert part into html table of contents
	 		++partNo;
			
	 		ind0 = insert(toc,
				U"</p></div>\n\n<div class = \"w3-container w3-center w3-teal w3-text-white\">\n"
				U"<h3 align = \"center\">第" + chineseNo[partNo] + U"部分 " + title + U"</h3>\n"
				U"</div>\n\n<div class = \"w3-container\">\n"
				, ind0);
			++ind1;
	 	}
	}
	toc.insert(ind0, U"</p>\n</div>");
	write_file(toc, path_out + "index.html");
	return N;
}

// create table of content from PhysWiki.tex
// path must end with '\\'
// return the number of entries, return -1 if failed
// names is a list of filenames
// titles[i] is the chinese title of entries[i]
inline Long table_of_changed(vector_I<Str32> titles, vector_I<Str32> entries, Str32_I path_in, Str32_I path_out)
{
	Long i{}, N{}, ind0{};
	//keys.push_back(U"\\entry"); keys.push_back(U"\\chapter"); keys.push_back(U"\\part");
	
	Str32 entryName; // entry label
	Str32 newcomm;
	Str32 toc;
	vector<Str32> changed, authors;

	read_file(newcomm, "lib/newcommand.html");
	CRLF_to_LF(newcomm);
	if (!file_exist(U"data/changed.txt")) {
		err_msg = U"内部错误： data/changed.txt 不存在!";
		return -1;
	}
	read_vec_str(changed, U"data/changed.txt");
	if (!file_exist(U"data/authors.txt")) {
		err_msg = U"内部错误： data/authors.txt 不存在!";
		return -1;
	}
	read_vec_str(authors, U"data/authors.txt");
	if (changed.size() != authors.size()) {
		err_msg = U"内部错误： changed.txt 和 authors.txt 行数不同!";
		return -1;
	}

	read_file(toc, "lib/index_template.html"); // read html template
	CRLF_to_LF(toc);

	ind0 = toc.find(U"PhysWikiHTMLtitle");
	toc.replace(ind0, 17, U"小时物理草稿");
	ind0 = toc.find(U"PhysWikiCommand", ind0);
	toc.erase(ind0, 15); toc.insert(ind0, newcomm);
	ind0 = toc.find(U"PhysWikiHTMLbody", ind0);
	toc.erase(ind0, 16);

	ind0 = insert(toc,
		U"<img src = \"../title.png\" alt = \"图\" style = \"width:100%;\">\n"
		U"<div class = \"w3-container w3-center w3-blue w3-text-white\">\n"
		U"<h1>小时物理草稿</h1>\n</div>\n\n"
		U"<div class = \"w3-container\"><p>\n"
		U"<a href = \"../\">返回主页</a>\n\n"
		U"</p></div>\n\n<div class = \"w3-container w3-center w3-teal w3-text-white\">\n"
		U"<h3 align = \"center\">草稿</h3>\n"
		U"</div>\n\n<div class = \"w3-container\">\n"
		, ind0);

	for (Long i = 0; i < Size(changed); ++i) {
		Long ind = changed[i].rfind('.');
		if (ind < 0) {
			err_msg = U"内部错误： changed.txt 中文件必须有后缀名!";
			return -1; // break point here
		}
		if (changed[i].substr(ind + 1) != U"tex")
			continue;
		entryName = changed[i].substr(0, ind);
		// get chinese title and entry label
		++N;
		// get Chinese title
		ind = search(entryName, entries);
		if (ind < 0) {
			err_msg = U"内部错误： changed.txt 中词条文件未找到!";
			return -1; // break point here
		}
		// insert entry into html table of contents
		ind0 = insert(toc, U"<a href = \"" + entryName + ".html" + "\" target = \"_blank\">"
			+ titles[ind] + U"（" + authors[i] + U"）" + U"</a>　\n", ind0);
	}
	toc.insert(ind0, U"</p>\n</div>");
	write_file(toc, path_out + "changed.html");
	return N;
}

// process Matlab code (\code command)
inline Long MatlabCode(Str32_IO str, Str32_I path_in, Bool_I show_title)
{
	Long N = 0, ind0 = 0;
	Str32 name; // code file name without extension
	Str32 code;
	Intvs intvIn, intvOut;
	// \code commands
	while (true) {
		if (show_title)
			ind0 = find_command(str, U"Code", ind0);
		else
			ind0 = find_command(str, U"code", ind0);
		if (ind0 < 0)
			return N;
		command_arg(name, str, ind0);
		// read file
		if (!file_exist(path_in + "codes/" + utf32to8(name) + ".m")) {
			err_msg = U"代码文件 \"" + utf32to8(name) + ".m\" 未找到!";
			return -1; // break point here
		}
		read_file(code, path_in + "codes/" + utf32to8(name) + ".m");
		CRLF_to_LF(code);
		Matlab_highlight(code);

		// insert code
		// for download button, use
		// U"<span class = \"icon\"><a href = \"" + name + U".m\" download> <i class = \"fa fa-caret-square-o-down\"></i></a></span>"
		Str32 title, line_nums, num;
		Long ind1 = 0, i = 0;
		while (true) {
			ind1 = code.find(U'\n', ind1);
			if (ind1 < 0) break;
			num2str(num, ++i);
			line_nums += num + U"<br>";
			++ind1;
		}
		line_nums.erase(line_nums.size() - 4, 4);

		if (show_title)
			title = U"<h6><b>　　" + name + U".m</b></h6>\n";
		str.replace(ind0, skip_command(str, ind0, 1) - ind0,
			title +
			U"<table class=\"code\"><tr class=\"code\"><td class=\"linenum\">\n"
			+ line_nums +
			U"\n</td><td class=\"code\">\n"
			U"<div class = \"w3-code notranslate w3-pale-yellow\">\n"
			U"<div class = \"nospace\"><pre class = \"mcode\">\n"
			+ code +
			U"</pre></div></div>\n"
			U"</td></tr></table>"
		);
		++N;
	}
}

// process lstlisting[language=MatlabCom] environments to display Matlab Command Line
// return the number of \Command{} processed, return -1 if failed
// TODO: make sure it's \begin{lstlisting}[language=MatlabCom], not any lstlisting
inline Long MatlabComLine(Str32_IO str)
{
	Long i{}, j{}, N{}, ind0{};
	Intvs intvIn, intvOut;
	Str32 code;
	N = find_env(intvIn, str, U"lstlisting");
	N = find_env(intvOut, str, U"lstlisting", 'o');
	for (i = N - 1; i >= 0; --i) {
		ind0 = expect(str, U"[", intvIn.L(i));
		ind0 = pair_brace(str, ind0, U'[');
		code = str.substr(ind0+1, intvIn.R(i)-ind0);
		if (code[0] != U'\n' || code.back() != U'\n') {
			cout << "wrong format of Matlab command line environment!" << endl;
			return -1;
		}
		code = code.substr(1, code.size() - 2);
		Matlab_highlight(code);

		str.erase(intvOut.L(i), intvOut.R(i) - intvOut.L(i) + 1);
		ind0 = intvOut.L(i);
		ind0 = insert(str,
			U"<div class = \"w3-code notranslate w3-pale-yellow\">\n"
			"<div class = \"nospace\"><pre class = \"mcode\">\n"
			+ code +
			U"\n</pre></div></div>"
			, ind0);
	}
	return N;
}

inline Long cppComLine(Str32_IO str)
{
	Long N = 0, ind0 = 0;
	Intvs intvIn, intvOut;
	Str32 code;
	N = find_env(intvIn, str, U"lstlisting");
	N = find_env(intvOut, str, U"lstlisting", 'o');
	for (Long i = N - 1; i >= 0; --i) {
		ind0 = expect(str, U"[", intvIn.L(i));
		ind0 = pair_brace(str, ind0, U'[');
		code = str.substr(ind0 + 1, intvIn.R(i) - ind0);
		if (code[0] != U'\n' || code.back() != U'\n') {
			cout << "wrong format of Matlab command line environment!" << endl;
			return -1;
		}
		code = code.substr(1, code.size() - 2);
		Matlab_highlight(code);

		str.erase(intvOut.L(i), intvOut.R(i) - intvOut.L(i) + 1);
		ind0 = intvOut.L(i);
		ind0 = insert(str,
			U"<div class = \"w3-code notranslate w3-pale-yellow\">\n"
			"<div class = \"nospace\"><pre class = \"mcode\">\n"
			+ code +
			U"\n</pre></div></div>"
			, ind0);
	}
	return N;
}

// find \bra{}\ket{} and mark
inline Long OneFile4(Str32_I path)
{
	Long ind0{}, ind1{}, N{};
	Str32 str;
	read_file(str, path); // read file
	CRLF_to_LF(str);
	while (true) {
		ind0 = str.find(U"\\bra", ind0);
		if (ind0 < 0) break;
		ind1 = ind0 + 4;
		ind1 = expect(str, U"{", ind1);
		if (ind1 < 0) {
			++ind0; continue;
		}
		ind1 = pair_brace(str, ind1 - 1);
		ind1 = expect(str, U"\\ket", ind1 + 1);
		if (ind1 > 0) {
			str.insert(ind0, U"删除标记"); ++N;
			ind0 = ind1 + 4;
		}
		else
			++ind0;
	}
	if (N > 0)
		write_file(str, path);
	return N;
}

// generate html from tex
// output the chinese title of the file, id-label pairs in the file
// output dependency info from \pentry{}, links[i][0] --> links[i][1]
// return 0 if successful, -1 if failed
// entryName does not include ".tex"
// path0 is the parent folder of entryName.tex, ending with '\\'
inline Long PhysWikiOnline1(vector_IO<Str32> ids, vector_IO<Str32> labels, vector_IO<Long> links,
	Str32_I path_in, Str32_I path_out, vector_I<Str32> entries, vector_I<Str32> titles, Long_I ind)
{
	Str32 str;
	read_file(str, path_in + "contents/" + entries[ind] + ".tex"); // read tex file
	CRLF_to_LF(str);
	Str32 title;
	// read html template and \newcommand{}
	Str32 html;
	read_file(html, "lib/entry_template.html");
	CRLF_to_LF(html);
	Str32 newcomm;
	read_file(newcomm, "lib/newcommand.html");
	CRLF_to_LF(newcomm);
	// read title from first comment
	if (get_title(title, str) < 0)
		return -1;
	if (!titles[ind].empty() && title != titles[ind]) {
		err_msg = U"中文标题不符: " + title + " | " + titles[ind];
		return -1;
	}
	// insert \newcommand{}
	if (replace(html, U"PhysWikiCommand", newcomm) != 1) {
		err_msg = U"newcommand.html 格式错误!";
		return -1;
	}
	// insert HTML title
	if (replace(html, U"PhysWikiHTMLtitle", title) != 1) {
		err_msg = U"newcommand.html 格式错误!";
		return -1;
	}
	// remove comments
	Intvs intvComm;
	find_comment(intvComm, str);
	for (Long i = intvComm.size() - 1; i >= 0; --i) {
		str.erase(intvComm.L(i), intvComm.R(i) - intvComm.L(i) + 1);
	}
	// escape characters
	if (NormalTextEscape(str) < 0)
		return -1;
	// add paragraph tags
	if (paragraph_tag(str) < 0)
		return -1;
	// itemize and enumerate
	if (Itemize(str) < 0)
		return -1;
	if (Enumerate(str) < 0)
		return -1;
	// add html id for links
	if (EnvLabel(ids, labels, entries[ind], str) < 0)
		return -1;
	// process table environments
	if (Table(str) < 0)
		return -1;
	// ensure '<' and '>' has spaces around them
	if (EqOmitTag(str) < 0)
		return -1;
	// process example and exercise environments
	if (ExampleEnvironment(str) < 0)
		return -1;
	if (ExerciseEnvironment(str) < 0)
		return -1;
	// process figure environments
	if (FigureEnvironment(str, path_out) < 0)
		return -1;
	// get dependent entries from \pentry{}
	if (depend_entry(links, str, entries, ind) < 0)
		return -1;
	// process \pentry{}
	if (pentry(str) < 0)
		return -1;
	// Replace \nameStar commands
	StarCommand(U"comm", str);   StarCommand(U"pb", str);
	StarCommand(U"dv", str);     StarCommand(U"pdv", str);
	StarCommand(U"bra", str);    StarCommand(U"ket", str);
	StarCommand(U"braket", str); StarCommand(U"ev", str);
	StarCommand(U"mel", str);
	// Replace \nameTwo and \nameThree commands
	VarCommand(U"pdv", str, 3);   VarCommand(U"pdvStar", str, 3);
	VarCommand(U"dv", str, 2);    VarCommand(U"dvStar", str, 2);
	VarCommand(U"ev", str, 2);    VarCommand(U"evStar", str, 2);
	VarCommand(U"braket", str, 2); VarCommand(U"braketStar", str, 2);
	// replace \name() and \name[] with \nameRound{} and \nameRound and \nameSquare
	RoundSquareCommand(U"qty", str);
	// replace \namd() and \name[]() with \nameRound{} and \nameRound[]{}
	MathFunction(U"sin", str);    MathFunction(U"cos", str);
	MathFunction(U"tan", str);    MathFunction(U"csc", str);
	MathFunction(U"sec", str);    MathFunction(U"cot", str);
	MathFunction(U"sinh", str);   MathFunction(U"cosh", str);
	MathFunction(U"tanh", str);   MathFunction(U"arcsin", str);
	MathFunction(U"arccos", str); MathFunction(U"arctan", str);
	MathFunction(U"exp", str);    MathFunction(U"log", str);
	MathFunction(U"ln", str);
	// replace \name{} with html tags
	Command2Tag(U"subsection", U"<h5 class = \"w3-text-indigo\"><b>", U"</b></h5>", str);
	Command2Tag(U"subsubsection", U"<h5><b>", U"</b></h5>", str);
	Command2Tag(U"bb", U"<b>", U"</b>", str);
	// replace \upref{} with link icon
	if (upref(str, path_in) < 0)
		return -1;
	// replace \link{}{} with <a href="">...</a>
	if (link(str) < 0)
		return -1;
	// replace environments with html tags
	Env2Tag(U"equation", U"<div class=\"eq\"><div class = \"w3-cell\" style = \"width:710px\">\n\\begin{equation}",
						U"\\end{equation}\n</div></div>", str);
	Env2Tag(U"gather", U"<div class=\"eq\"><div class = \"w3-cell\" style = \"width:710px\">\n\\begin{gather}",
		U"\\end{gather}\n</div></div>", str);
	Env2Tag(U"align", U"<div class=\"eq\"><div class = \"w3-cell\" style = \"width:710px\">\n\\begin{align}",
		U"\\end{align}\n</div></div>", str);
	// Matlab code
	if (MatlabCode(str, path_in, false) < 0)
		return -1;
	if (MatlabCode(str, path_in, true) < 0)
		return -1;
	if (MatlabComLine(str) < 0)
		return -1;
	Command2Tag(U"x", U"<span class = \"w3-text-teal\">", U"</span>", str);
	// footnote
	footnote(str);
	// delete redundent commands
	replace(str, U"\\dfracH", U"");
	// insert body Title
	if (replace(html, U"PhysWikiTitle", title) != 1) {
		err_msg = U"\"PhysWikiTitle\" 在 entry_template.html 中未找到!";
		return -1;
	}
	// insert HTML body
	if (replace(html, U"PhysWikiHTMLbody", str) != 1) {
		err_msg = U"\"PhysWikiHTMLbody\" 在 entry_template.html 中未找到!";
		return -1;
	}
	// save html file
	write_file(html, path_out + entries[ind] + ".html");
	return 0;
}

// generate json file containing dependency tree
// empty elements of 'titles' will be ignored
inline void dep_json(vector_I<Str32> entries, vector_I<Str32> titles, vector_I<Long> links, Str32_I path_out)
{
	Str32 str;
	// write entries
	str += U"{\n  \"nodes\": [\n";
	for (Long i = 0; i < Size(titles); ++i) {
		if (titles[i].empty())
			continue;
		str += U"    {\"id\": \"" + titles[i] + U"\", \"group\": 1, \"url\": \"../online/" +
			entries[i] + ".html\"},\n";
	}
	str.pop_back(); str.pop_back();
	str += U"\n  ],\n";

	// write links
	str += U"  \"links\": [\n";
	for (Long i = 0; i < Size(links)/2; ++i) {
		if (titles[links[2*i]].empty() || titles[links[2*i+1]].empty())
			continue;
		str += U"  {\"source\": \"" + titles[links[2*i]] + "\", ";
		str += U"\"target\": \"" + titles[links[2*i+1]] + U"\", ";
		str += U"\"value\": 1},\n";
	}
	str.pop_back(); str.pop_back();
	str += U"\n  ]\n}\n";
	write_file(str, path_out + "../tree/data/dep.json");
}

// convert PhysWiki/ folder to littleshi.cn/online folder
inline void PhysWikiOnline(Str32_I path_in, Str32_I path_out)
{
	vector<Str32> entries; // name in \entry{}, also .tex file name
	vector<Str32> titles; // Chinese titles in \entry{}
	entries_titles(titles, entries, path_in);
	write_vec_str(titles, U"data/titles.txt");
	write_vec_str(entries, U"data/entries.txt");

	cout << u8"正在从 PhysWiki.tex 生成目录 index.html ...\n" << endl;

	while (table_of_contents(titles, entries, path_in, path_out) < 0) {
		if (!Input().getBool("重试?")) {
			cout << err_msg << endl;
			exit(EXIT_FAILURE);
		}
	}

	// dependency info from \pentry, entries[link[2*i]] is in \pentry{} of entries[link[2*i+1]]
	vector<Long> links;
	// html tag id and corresponding latex label (e.g. Idlist[i]: "eq5", "fig3")
	// the number in id is the n-th occurrence of the same type of environment
	vector<Str32> labels, ids;

	// 1st loop through tex files
	// files are processed independently
	// `IdList` and `LabelList` are recorded for 2nd loop
	cout << u8"======  第 1 轮转换 ======\n\n" << endl;
	for (Long i = 0; i < Size(entries); ++i) {
		cout    << std::setw(5)  << std::left << i
				<< std::setw(10)  << std::left << entries[i]
				<< std::setw(20) << std::left << titles[i] << endl;
		current_entry = entries[i];
		if (debug(U"BECSup"))
			cout << "one file debug" << endl;
		// main process
		while (PhysWikiOnline1(ids, labels, links,
			path_in, path_out, entries, titles, i) < 0) {
			cout << err_msg << endl;
			if (!Input().getBool("重试?"))
				exit(EXIT_FAILURE);
		}
	}

	// save id and label data
	write_vec_str(labels, U"data/labels.txt");
	write_vec_str(ids, U"data/ids.txt");

	// generate dep.json
	dep_json(entries, titles, links, path_out);

	// 2nd loop through tex files
	// deal with autoref
	// need `IdList` and `LabelList` from 1st loop
	cout << "\n\n\n\n" << u8"====== 第 2 轮转换 ======\n" << endl;
	Str32 html;
	for (Long i = 0; i < Size(entries); ++i) {
		cout    << std::setw(5)  << std::left << i
				<< std::setw(10)  << std::left << entries[i]
				<< std::setw(20) << std::left << titles[i] << endl;
		read_file(html, path_out + entries[i] + ".html"); // read html file
		current_entry = entries[i];
		if (debug(U"QSHOnr"))
			cout << "one file debug" << endl;
		// process \autoref and \upref
		if (autoref(ids, labels, entries[i], html) < 0) {
			cout << err_msg << endl;
			Input().getBool("already in second scan, please rerun the program!");
			exit(EXIT_FAILURE);
		}
		write_file(html, path_out + entries[i] + ".html"); // save html file
	}
	cout << endl;
}

// like PhysWikiOnline, but convert only one file
// requires ids.txt and labels.txt output from `PhysWikiOnline()`
inline Long PhysWikiOnlineN(vector_I<Str32> entryN, Str32_I path_in, Str32_I path_out)
{
	// html tag id and corresponding latex label (e.g. Idlist[i]: "eq5", "fig3")
	// the number in id is the n-th occurrence of the same type of environment
	vector<Str32> labels, ids, entries, titles;
	if (!file_exist(U"data/labels.txt")) {
		err_msg = U"内部错误： data/labels.txt 不存在!";
		return -1;
	}
	read_vec_str(labels, U"data/ids.txt");
	if (!file_exist(U"data/ids.txt")) {
		err_msg = U"内部错误： data/ids.txt 不存在!";
		return -1;
	}
	read_vec_str(ids, U"data/ids.txt");
	if (!file_exist(U"data/entries.txt")) {
		err_msg = U"内部错误： data/entries.txt 不存在!";
		return -1;
	}
	read_vec_str(entries, U"data/entries.txt");
	if (!file_exist(U"data/titles.txt")) {
		err_msg = U"内部错误： data/titles.txt 不存在!";
		return -1;
	}
	read_vec_str(titles, U"data/titles.txt");
	if (labels.size() != ids.size()) {
		err_msg = U"内部错误： labels.txt 与 ids.txt 长度不符";
		return -1;
	}
	if (entries.size() != titles.size()) {
		err_msg = U"内部错误： entries.txt 与 titles.txt 长度不符";
		return -1;
	}

	// 1st loop through tex files
	// files are processed independently
	// `IdList` and `LabelList` are recorded for 2nd loop
	cout << u8"======  第 1 轮转换 ======\n\n" << endl;

	// main process
	vector<Long> links;
	for (Long i = 0; i < Size(entryN); ++i) {
		current_entry = entryN[i];
		Long ind = search(entryN[i], entries);
		if (ind < 0) {
			err_msg = U"entries.txt 中未找到该词条!";
			return -1;
		}

		if (PhysWikiOnline1(ids, labels, links,
			path_in, path_out, entries, titles, ind) < 0) {
			return -1;
		}
	}
	
	write_vec_str(labels, U"data/labels.txt");
	write_vec_str(ids, U"data/ids.txt");

	// 2nd loop through tex files
	// deal with autoref
	// need `IdList` and `LabelList` from 1st loop
	cout << "\n\n\n\n" << u8"====== 第 2 轮转换 ======\n" << endl;

	Str32 html;
	for (Long i = 0; i < Size(entryN); ++i) {
		current_entry = entryN[i];
		Long ind = search(entryN[i], entries);
		read_file(html, path_out + entries[ind] + ".html"); // read html file
		// process \autoref and \upref
		if (autoref(ids, labels, entries[ind], html) < 0) {
			return -1;
		}
		write_file(html, path_out + entries[ind] + ".html"); // save html file
	}

	return 0;
}

// check format error of .tex files in path0
inline void PhysWikiCheck(Str32_I path0)
{
	Long ind0{};
	vector<Str32> names;
	file_list_ext(names, path0, U"tex", false);
	//RemoveNoEntry(names);
	if (names.size() <= 0) return;
	//names.resize(0); names.push_back(U"Sample"));
	for (unsigned i{}; i < names.size(); ++i) {
		cout << i << " ";
		cout << names[i] << "...";
		// main process
		cout << OneFile4(path0 + names[i] + ".tex");
		cout << endl;
	}
}