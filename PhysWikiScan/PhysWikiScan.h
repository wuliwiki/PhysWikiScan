#pragma once
#include "../SLISC/file.h"
#include "../SLISC/input.h"
#include "../TeX/tex2html.h"
#include "../Matlab/matlab2html.h"

using namespace slisc;

// get the title (defined as the first comment, no space after %)
// limited to 20 characters
// return -1 if error, 0 if successful
inline Long GetTitle(Str32_O title, Str32_I str)
{
	if (str.at(0) != U'%') {
		SLS_WARN("Need a title!"); // break point here
		return -1;
	}
	Str32 c;
	Long ind0 = NextNoSpace(c, str, 1);
	if (ind0 < 0) {
		SLS_WARN("Title is empty!"); // break point here
		return -1;
	}
	Long ind1 = str.find(U'\n', ind0);
	if (ind1 < 0) {
		SLS_WARN("Body is empty!"); // break point here
		return -1;
	}
	title = str.substr(ind0, ind1 - ind0);
	return 0;
}

// add <p> tags to paragraphs
// return number of tag pairs added
// return -1 if failed
inline Long paragraph_tag1_old(Str32_IO str)
{
	Long ind0 = 0, N = 0;
	trim(str, U"\n");
	// delete extra '\n' (more than two continuous)
	while (true) {
		ind0 = str.find(U"\n\n\n", ind0);
		if (ind0 < 0)
			break;
		eatR(str, ind0 + 2, U"\n");
	}

	// replace "\n\n" with "\n</p>\n<p>　　\n"
	N = replace(str, U"\n\n", U"\n</p>\n<p>　　\n");
	str.insert(str.size(), U"\n</p>");
	str.insert(0, U"<p>　　\n");// <p> is indented by unicode white space
	return N;
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

TODO: test this!
inline Long ParagraphTag(Str32_IO str)
{
	Long N = 0, ind0 = 0, left = 0, length, ikey;
	Intvs intv;
	Str32 temp, env, end, begin;

	// "begin", and commands that cannot be in a paragraph
	vector<Str32> commands = {U"\\begin",
		U"\\ubsection", U"\\subsubsection", U"\\pentry", U"\\code", U"\\Code"};
	// equation envs
	vector<Str32> envs_eq = {U"equation", U"align", U"gather"};
	// environments that needs <p></p> inside
	vector<Str32> envs_p = { U"exam", U"exer"};

	// 'n' (for normal); 'e' (for env_eq); 'p' (for env_p)
	char mode;

	// handle normal text intervals separated by
	// commands in "commands" and environments
	while (true) {
		ind0 = find(ikey, str, commands, ind0);
		if (ind0 < 0)
			return N;
		
		// decide mode
		mode = 'n';
		if (ikey == 0) { // found environment
			command_arg(env, str, ind0);
			if (is_in(env, envs_eq)) {
				mode = 'e';
			}
			else if (is_in(env, envs_p)) {
				mode = 'p';
			}
		}
		
		// decide ending tag
		if (mode == 'e' &&
			// equations can be inside paragraph
			ExpectKeyReverse(str, U"\n\n", ind0 - 1) < 0) {
			end.clear();
		}
		else
			end = U"\n<\p>\n";

		// add tags
		length = ind0 - left;
		temp = str.substr(left, length);
		N += paragraph_tag1(temp);
		str.replace(left, length, begin + temp + end);
		ind0 += temp.size() - length;
		
		// handle env_p environments or skip
		if (ikey == 0) {
			if (mode == 'p') {
				; //"TODO: handle 'env_p' environments!";
			}
			ind0 = skip_env(str, ind0);
		}
		else
			ind0 = skip_command(str, ind0, 1);
		
		// beginning tag for next interval
		if (mode == 'e' &&
			expect(str, U"\n\n", ind0) < 0) {
			end.clear();
		}
		else
			end = U"\n<p>　　\n";
	}
}

// add <p> tags to paragraphs
// return number of tag pairs added
// return -1 if failed
inline Long ParagraphTag_old(Str32_IO str)
{
	Long i{}, N{}, N1{}, ind0{}, ind2{};
	N = paragraph_tag1_old(str);

	// deal with ranges that should not be in <p>...</p>
	// if there is "<p>" before range, delete it, otherwise, add "</p>"
	// if there is "</p>" after range, delete it, otherwise, add "<p>　　"
	Intvs intv, intv1;
	find_all_command_intv(intv, U"subsection", str);
	find_all_command_intv(intv1, U"subsubsection", str);
	if (combine(intv, intv1) < 0)
		return -1;
	find_all_command_intv(intv1, U"pentry", str);
	if (combine(intv, intv1) < 0)
		return -1;
	find_all_command_intv(intv1, U"code", str);
	if (combine(intv, intv1) < 0)
		return -1;
	find_all_command_intv(intv1, U"Code", str);
	if (combine(intv, intv1) < 0)
		return -1;

	find_env(intv1, str, U"figure", 'o');
	if (combine(intv, intv1) < 0)
		return -1;
	find_env(intv1, str, U"itemize", 'o');
	if (combine(intv, intv1) < 0)
		return -1;
	find_env(intv1, str, U"enumerate", 'o');
	if (combine(intv, intv1) < 0)
		return -1;

	FindAllBegin(intv1, U"exam", str, '2');
	if (combine(intv, intv1) < 0)
		return -1;
	FindEnd(intv1, U"exam", str);
	if (combine(intv, intv1) < 0)
		return -1;
	FindAllBegin(intv1, U"exer", str, '2');
	if (combine(intv, intv1) < 0)
		return -1;
	FindEnd(intv1, U"exer", str);
	if (combine(intv, intv1) < 0)
		return -1;
	
	for (i = intv.size() - 1; i >= 0; --i) {
		ind0 = expect(str, U"</p>", intv.R(i) + 1);
		if (ind0 >= 0) {
			str.erase(ind0 - 4, 4);
		}
		else {
			str.insert(intv.R(i) + 1, U"\n<p>　　"); ++N;
		}
		ind0 = ExpectKeyReverse(str, U"<p>　　", intv.L(i) - 1);
		ind2 = ExpectKeyReverse(str, U"<p>", intv.L(i) - 1);
		if (ind0 > -2) {
			str.erase(ind0 + 1, 5); --N;
		}
		else if (ind2 > -2) {
			str.erase(ind2 + 1, 3); --N;
		}
		else {
			str.insert(intv.L(i), U"</p>\n\n");
		}
	}

	// deal with equation environments alike
	// if there is "</p>\n<p>　　" before range, delete it
	// if there is "<p>　　" before range, delete "　　"
	find_env(intv, str, U"equation", 'o');
	find_env(intv1, str, U"gather", 'o');
	if (combine(intv, intv1) < 0) return -1;
	find_env(intv1, str, U"align", 'o');
	if (combine(intv, intv1) < 0) return -1;
	for (i = intv.size() - 1; i >= 0; --i) {
		ind0 = ExpectKeyReverse(str, U"</p>\n<p>　　", intv.L(i) - 1);
		ind2 = ExpectKeyReverse(str, U"<p>　　", intv.L(i) - 1);
		if (ind0 > -2) {
			str.erase(ind0 + 1, 10); --N;
		}
		else if (ind2 > -2) {
			str.erase(ind2 + 4, 2);
		}
	}

	// deal with \noindent
	ind0 = 0;
	while (true) {
		ind0 = str.find(U"\\noindent", ind0);
		if (ind0 < 0) break;
		str.erase(ind0, 9);
		ind2 = ExpectKeyReverse(str, U"　　", ind0 - 1);
		if (ind2 > -2)
			str.erase(ind2 + 1, 2);
	}
	return N + 1;
}

// add html id tag before environment if it contains a label, right before "\begin{}" and delete that label
// output html id and corresponding label for future reference
// "gather" and "align" environments has id = "ga" and "ali"
// idNum is in the idNum-th environment of the same name (not necessarily equal to displayed number)
// no comment allowed
// return number of labels processed, or -1 if failed
inline Long EnvLabel(vector<Str32>& id, vector<Str32>& label, Str32_I entryName, Str32_IO str)
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
			SLS_WARN("label not in an environment!"); // break point here
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
			SLS_WARN("environment not supported for label!");
			return -1; // break point here
		}
		// check label format and save label
		ind0 = expect(str, U"{", ind5 + 6);
		ind3 = expect(str, entryName + U'_' + idName, ind0);
		if (ind3 < 0) {
			SLS_WARN("label format error! expecting \"" + utf32to8(entryName + U'_' + idName) + "\"");
			return -1; // break point here
		}
		ind3 = str.find(U"}", ind3);
		label.push_back(str.substr(ind0, ind3 - ind0));
		trim(label.back());
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
		id.push_back(idName + idNum);
		str.erase(ind5, ind3 - ind5 + 1);
		str.insert(ind4, U"<span id = \"" + id.back() + U"\"></span>");
		ind0 = ind4 + 6;
	}
}

// replace figure environment with html image
// convert vector graph to SVG, font must set to "convert to outline"
// if svg image doesn't exist, use png, if doesn't exist, return -1
// path must end with '\\'
inline Long FigureEnvironment(Str32_IO str, Str32_I path)
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
			SLS_WARN("error when reading figure name!"); // breakpoint here
			return -1;
		}
		figName = str.substr(indName1, indName2 - indName1 + 1);
		// get caption of figure
		ind0 = str.find(U"\\caption", ind0);
		if (ind0 < 0) {
			SLS_WARN("fig caption not found!");
			return -1; // break point here
		}
		ind0 = expect(str, U"{", ind0 + 8);
		ind1 = pair_brace(str, ind0);
		caption = str.substr(ind0, ind1 - ind0);
		str.erase(intvFig.L(i), intvFig.R(i) - intvFig.L(i) + 1); // delete environment
		ind0 = expect(str, U"\n</p>", intvFig.L(i));
		// test img file existence
		if (file_exist(path + figName + ".svg"))
			format = U".svg";
		else if (file_exist(path + figName + ".png"))
			format = U".png";
		else {
			SLS_WARN("figure \"" + figName + "\" not found!");
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

// replace \pentry comman with html round panel
inline Long pentry(Str32_IO str)
{
	return Command2Tag(U"pentry", U"<div class = \"w3-panel w3-round-large w3-light-blue\"><b>预备知识</b>　", U"</div>", str);
}

// remove special .tex files from a list of name
// return number of names removed
// names has ".tex" extension
inline Long RemoveNoEntry(vector<Str32> &names)
{
	Long i{}, j{}, N{}, Nnames{}, Nnames0;
	vector<Str32> names0; // names to remove
	names0.push_back(U"PhysWiki");
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
inline Long ExampleEnvironment(Str32_IO str, Str32_I path0)
{
	Long i{}, N{}, ind0{}, ind1{};
	Intvs intvIn, intvOut;
	Str32 exName, exNo;
	find_env(intvIn, str, U"exam");
	N = find_env(intvOut, str, U"exam", 'o');
	for (i = N - 1; i >= 0; --i) {
		ind0 = str.find('{', intvOut.L(i));
		ind0 = pair_brace(str, ind0);
		ind0 = expect(str, U"{", ind0 + 1);
		ind1 = pair_brace(str, ind0 - 1);
		exName = str.substr(ind0, ind1 - ind0);
		// replace with html tags
		str.erase(intvIn.R(i) + 1, intvOut.R(i) - intvIn.R(i));
		str.insert(intvIn.R(i) + 1, U"</div>\n");
		str.erase(intvOut.L(i), ind1 - intvOut.L(i) + 1);
		num2str(exNo, i + 1);
		str.insert(intvOut.L(i), U"<div class = \"w3-panel w3-border-yellow w3-leftbar\">\n <h5><b>例"
				+ exNo + U"</b>　" + exName + U"</h5>");
	}
	return N;
}

// example is already not in a paragraph
// return number of examples processed, return -1 if failed
inline Long ExerciseEnvironment(Str32_IO str, Str32_I path0)
{
	Long i{}, N{}, ind0{}, ind1{};
	Intvs intvIn, intvOut;
	Str32 exName, exNo;
	find_env(intvIn, str, U"exer");
	N = find_env(intvOut, str, U"exer", 'o');
	for (i = N - 1; i >= 0; --i) {
		ind0 = str.find(U"{", intvOut.L(i));
		ind0 = pair_brace(str, ind0);
		ind0 = expect(str, U"{", ind0 + 1);
		ind1 = pair_brace(str, ind0 - 1);
		exName = str.substr(ind0, ind1 - ind0);
		// replace with html tags
		str.erase(intvIn.R(i) + 1, intvOut.R(i) - intvIn.R(i));
		str.insert(intvIn.R(i) + 1, U"</div>\n");
		str.erase(intvOut.L(i), ind1 - intvOut.L(i) + 1);
		num2str(exNo, i + 1);
		str.insert(intvOut.L(i), U"<div class = \"w3-panel w3-border-green w3-leftbar\">\n <h5><b>习题"
			+ exNo + U"</b>　" + exName + U"</h5>");
	}
	return N;
}

// replace autoref with html link
// no comment allowed
// does not add link for \autoref inside eq environment (equation, align, gather)
// return number of autoref replaced, or -1 if failed
inline Long autoref(const vector<Str32> &id, const vector<Str32> &label, Str32_I entryName, Str32_IO str)
{
	unsigned i{};
	Long ind0{}, ind1{}, ind2{}, ind3{}, ind4{}, ind5{}, N{}, Neq{}, ienv{};
	Bool inEq;
	Str32 entry, label0, idName, idNum, kind, newtab, file;
	vector<Str32> envNames{U"equation", U"align", U"gather"};
	while (true) {
		newtab.clear(); file.clear();
		ind0 = str.find(U"\\autoref", ind0);
		if (ind0 < 0) return N;
		inEq = index_in_env(ienv, ind0, envNames, str);
		ind1 = expect(str, U"{", ind0 + 8);
		ind1 = NextNoSpace(entry, str, ind1);
		ind2 = str.find('_', ind1);
		if (ind2 < 0) {
			SLS_WARN("autoref format error!");
			return -1; // break point here
		}
		ind3 = find_num(str, ind2);
		idName = str.substr(ind2 + 1, ind3 - ind2 - 1);
		if (idName == U"eq") kind = U"式";
		else if (idName == U"fig") kind = U"图";
		else if (idName == U"ex") kind = U"例";
		else if (idName == U"exe") kind = U"练习";
		else if (idName == U"tab") kind = U"表";
		else {
			SLS_WARN("unknown id name!");
			return -1; // break point here
		}
		ind3 = str.find('}', ind3);
		// find id of the label
		label0 = str.substr(ind1, ind3 - ind1); trim(label0);
		for (i = 0; i < label.size(); ++i) {
			if (label0 == label[i]) break;
		}
		if (i == label.size()) {
			SLS_WARN("label \"" + label0 +"\" not found!");
			return -1; // break point here
		}
		ind4 = find_num(id[i], 0);
		idNum = id[i].substr(ind4);		
		entry = str.substr(ind1, ind2 - ind1);
		if (entry != entryName) {// reference the same entry
			newtab = U"target = \"_blank\"";
			file = entry + U".html";
		}
		if (!inEq)
			str.insert(ind3 + 1, U" </a>");
		str.insert(ind3 + 1, kind + U' ' + idNum);
		if (!inEq)
			str.insert(ind3 + 1, U"<a href = \"" + file + U"#" + id[i] + U"\" " + newtab + U">");
		str.erase(ind0, ind3 - ind0 + 1);
		++N;
	}
}

// process upref
// path must end with '\\'
inline Long upref(Str32_IO str, Str32_I path)
{
	Long ind0 = 0, right, N = 0;
	Str32 entryName;
	while (true) {
		ind0 = find_command(str, U"upref", ind0);
		if (ind0 < 0)
			return N;
		command_arg(entryName, str, ind0);
		trim(entryName);
		if (!file_exist(path + utf32to8(entryName) + ".tex")) {
			SLS_WARN("upref file not found!");
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

// create table of content from PhysWiki1.tex
// path must end with '\\'
// return the number of entries
// names is a list of filenames
// output chinese titles,  titles[i] is the chinese title of names[i]
inline Long TableOfContent(vector<Str32> &titles, const vector<Str32> &names, Str32_I path)
{
	Long i{}, N{}, ind0{}, ind1{}, ind2{}, ikey{}, chapNo{ -1 }, partNo{ -1 };
	vector<Str32> keys{ U"\\part", U"\\chapter", U"\\entry", U"\\Entry", U"\\laserdog"};
	vector<Str32> chineseNo{U"一", U"二", U"三", U"四", U"五", U"六", U"七", U"八", U"九",
							U"十", U"十一", U"十二", U"十三", U"十四", U"十五"};
	//keys.push_back(U"\\entry"); keys.push_back(U"\\chapter"); keys.push_back(U"\\part");
	Str32 newcomm;
	read_file(newcomm, "PhysWikiScan/newcommand.html");
	CRLF_to_LF(newcomm);
	Str32 title; // chinese entry name, chapter name, or part name
	Str32 entryName; // entry label
	Str32 str; read_file(str, path + "PhysWiki.tex");
	CRLF_to_LF(str);
	Str32 toc; read_file(toc, "PhysWikiScan/index_template.html"); // read html template
	CRLF_to_LF(str);
	titles.resize(names.size());
	ind0 = toc.find(U"PhysWikiHTMLtitle");
	toc.erase(ind0, 17);
	toc.insert(ind0, U"小时物理百科在线");
	ind0 = toc.find(U"PhysWikiCommand", ind0);
	toc.erase(ind0, 15); toc.insert(ind0, newcomm);
	ind0 = toc.find(U"PhysWikiHTMLbody", ind0);
	toc.erase(ind0, 16);

	ind0 = insert(toc,
		U"<img src = \"../title.png\" alt = \"图\" style = \"width:100%;\">\n"
		U"<div class = \"w3-container w3-center w3-blue w3-text-white\">\n"
		U"<h1>小时物理百科在线</h1>\n</div>\n\n"
		U"<div class = \"w3-container\"><p>\n"
		U"<a href = \"license.html\" target = \"_blank\">版权声明</a>　\n"
		U"<a href = \"about.html\" target = \"_blank\">项目介绍</a>　\n"
		U"<a href = \"readme.html\" target = \"_blank\">使用说明</a>　\n"
		U"<a href = \"../\">返回主页</a>\n"
		, ind0);

	// remove comments
	Intvs intvComm;
	find_comment(intvComm, str);
	for (i = intvComm.size() - 1; i >= 0; --i)
		str.erase(intvComm.L(i), intvComm.R(i) - intvComm.L(i) + 1);
	while (true) {
		ind1 = find(ikey, str, keys, ind1);
		if (ind1 < 0) break;
		if (ikey >= 2) { // found "\entry"
			// get chinese title and entry label
			if (ikey == 2 || ikey == 3) ind1 += 6;
			else if (ikey == 4) ind1 += 9;
			++N;
			ind1 = expect(str, U"{", ind1);
			ind2 = str.find(U"}", ind1);
			title = str.substr(ind1, ind2 - ind1);
			trim(title);
			replace(title, U"\\ ", U" ");
			if (title.empty()) {
				SLS_WARN("Chinese title is empty in PhysWiki.tex!");
				return -1;  // break point here
			}
			ind1 = expect(str, U"{", ind2 + 1);
			ind2 = str.find(U"}", ind1);
			entryName = str.substr(ind1, ind2 - ind1);
			ind1 = ind2;
			// insert entry into html table of contents
			ind0 = insert(toc, U"<a href = \"" + entryName + ".html" + "\" target = \"_blank\">"
				+ title + U"</a>　\n", ind0);
			// record Chinese title
			for (i = 0; i < Size(names); ++i) {
				if (entryName == names[i]) break;
			}
			if (i == names.size()) {
				SLS_WARN("File not found for an entry in PhysWiki.tex!");
				return -1; // break point here
			}
			titles[i] = title;
		}
		else if (ikey == 1) { // found "\chapter"
			// get chinese chapter name
			ind1 += 8;
			ind1 = expect(str, U"{", ind1);
			ind2 = str.find('}', ind1);
			title = str.substr(ind1, ind2 - ind1);
			trim(title);

	 		replace(title, U"\\ ", U" ");
	 		ind1 = ind2;
	 		// insert chapter into html table of contents
	 		++chapNo;
	 		if (chapNo > 0)
	 			ind0 = insert(toc, U"</p>", ind0);
	 		ind0 = insert(toc, U"\n\n<h5><b>第" + chineseNo[chapNo] + U"章 " + title
	 			+ U"</b></h5>\n<div class = \"tochr\"></div><hr><div class = \"tochr\"></div>\n<p>\n", ind0);
	 	}
	 	else if (ikey == 0){ // found "\part"
	 		// get chinese part name
	 		ind1 += 5; chapNo = -1;
	 		ind1 = expect(str, U"{", ind1);
	 		ind2 = str.find('}', ind1);
	 		title = str.substr(ind1, ind2 - ind1);
			trim(title);
	 		replace(title, U"\\ ", U" ");
	 		ind1 = ind2;
	 		// insert part into html table of contents
	 		++partNo;
			
	 		ind0 = insert(toc,
				U"</p></div>\n\n<div class = \"w3-container w3-center w3-teal w3-text-white\">\n"
				U"<h3 align = \"center\">第" + chineseNo[partNo] + U"部分 " + title + U"</h3>\n"
				U"</div>\n\n<div class = \"w3-container\">\n"
				, ind0);
	 	}
	}
	toc.insert(ind0, U"</p>\n</div>");
	write_file(toc, path + "index.html");
	cout << u8"\n\n警告: 以下词条没有被 PhysWiki.tex 收录" << endl;
	for (i = 0; i < Size(titles); ++i) {
		if (titles[i].empty())
			cout << names[i] << endl;
	}
	cout << endl;
	return N;
}

// process Matlab code (\code command)
inline Long MatlabCode(Str32_IO str, Str32_I path, Bool_I show_title)
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
		if (!file_exist(path + utf32to8(name) + ".m")) {
			SLS_WARN("code file \"" + utf32to8(name) + ".m\" not found!");
			return -1; // break point here
		}
		read_file(code, path + "codes/" + utf32to8(name) + ".m");
		CRLF_to_LF(code);
		Matlab_highlight(code);

		// insert code
		// for download button, use
		// U"<span class = \"icon\"><a href = \"" + name + U".m\" download> <i class = \"fa fa-caret-square-o-down\"></i></a></span>"
		Str32 title;
		if (show_title)
			title = U"<b>" + name + U".m</b>\n";
		str.replace(ind0, skip_command(str, ind0, 1) - ind0,
			title +
			U"<div class = \"w3-code notranslate w3-pale-yellow\">\n"
			U"<div class = \"nospace\"><pre class = \"mcode\">\n"
			+ code +
			U"</pre></div></div>"
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
// return 0 if successful, -1 if failed
// entryName does not include ".tex"
// path0 is the parent folder of entryName.tex, ending with '\\'
inline Long PhysWikiOnline1(vector<Str32>& id, vector<Str32>& label, Str32_I entryName,
	Str32 path0, const vector<Str32>& names, const vector<Str32>& titles)
{
	Long i{}, j{};
	Str32 str;
	read_file(str, path0 + entryName + ".tex"); // read tex file
	CRLF_to_LF(str);
	Str32 title;
	// read html template and \newcommand{}
	Str32 html;
	read_file(html, "PhysWikiScan/entry_template.html");
	CRLF_to_LF(html);
	Str32 newcomm;
	read_file(newcomm, "PhysWikiScan/newcommand.html");
	CRLF_to_LF(newcomm);
	// read title from first comment
	if (GetTitle(title, str) < 0)
		return -1;
	for (j = 0; j < Size(names); ++j) {
		if (entryName == names[j])
			break;
	}
	if (!titles[j].empty() && title != titles[j]) {
		SLS_WARN("title inconsistent!");
		return -1;
	}
	// insert \newcommand{}
	if (replace(html, U"PhysWikiCommand", newcomm) != 1) {
		SLS_WARN("wrong format in newcommand.html!");
		return -1;
	}
	// insert HTML title
	if (replace(html, U"PhysWikiHTMLtitle", title) != 1) {
		SLS_WARN("wrong format in newcommand.html!");
		return -1;
	}
	// remove comments
	Intvs intvComm;
	find_comment(intvComm, str);
	for (i = intvComm.size() - 1; i >= 0; --i) {
		str.erase(intvComm.L(i), intvComm.R(i) - intvComm.L(i) + 1);
	}
	// escape characters
	if (NormalTextEscape(str) < 0)
		return -1;
	// add paragraph tags
	if (ParagraphTag_old(str) < 0)
		return -1;
	// itemize and enumerate
	if (Itemize(str) < 0)
		return -1;
	if (Enumerate(str) < 0)
		return -1;
	// add html id for links
	if (EnvLabel(id, label, entryName, str) < 0)
		return -1;
	// process table environments
	if (Table(str) < 0)
		return -1;
	// ensure '<' and '>' has spaces around them
	if (EqOmitTag(str) < 0)
		return -1;
	// process figure environments
	if (FigureEnvironment(str, path0) < 0)
		return -1;
	// process example and exercise environments
	if (ExampleEnvironment(str, path0) < 0)
		return -1;
	if (ExerciseEnvironment(str, path0) < 0)
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
	VarCommand(U"braket", str, 2);
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
	if (upref(str, path0) < 0)
		return -1;
	// replace environments with html tags
	Env2Tag(U"equation", U"<div class=\"eq\"><div class = \"w3-cell\" style = \"width:710px\">\n\\begin{equation}",
						U"\\end{equation}\n</div></div>", str);
	Env2Tag(U"gather", U"<div class=\"eq\"><div class = \"w3-cell\" style = \"width:710px\">\n\\begin{gather}",
		U"\\end{gather}\n</div></div>", str);
	Env2Tag(U"align", U"<div class=\"eq\"><div class = \"w3-cell\" style = \"width:710px\">\n\\begin{align}",
		U"\\end{align}\n</div></div>", str);
	// Matlab code
	if (MatlabCode(str, path0, false) < 0)
		return -1;
	if (MatlabCode(str, path0, true) < 0)
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
		SLS_WARN("\"PhysWikiTitle\" not found in entry_template.html!");
		return -1;
	}
	// insert HTML body
	if (replace(html, U"PhysWikiHTMLbody", str) != 1) {
		SLS_WARN("\"PhysWikiHTMLbody\" not found in entry_template.html!");
		return -1;
	}
	// save html file
	write_file(html, path0 + entryName + ".html");
	return 0;
}

// generate html for littleshi.cn/online
inline void PhysWikiOnline(Str32_I path0)
{
	vector<Str32> names;
	
	file_list_ext(names, path0, Str32(U"tex"), false);
	vector<Str32> titles;
	RemoveNoEntry(names);

	if (names.size() <= 0) return;
	cout << u8"正在从 PhysWiki.tex 生成目录 index.html ..." << endl;

	while (TableOfContent(titles, names, path0) < 0) {
		if (!Input().Bool("重试?"))
			exit(EXIT_FAILURE);
	}

	vector<Str32> IdList, LabelList; // html id and corresponding tex label
	// 1st loop through tex files
	cout << u8"======  第 1 轮转换 ======\n" << endl;
	for (Long i = 0; i < Size(names); ++i) {
		cout    << std::setw(5)  << std::left << i
				<< std::setw(10)  << std::left << names[i]
				<< std::setw(20) << std::left << titles[i] << endl;
		if (names[i] == U"ITable")
			cout << "one file debug" << endl;
		// main process
		while (PhysWikiOnline1(IdList, LabelList, names[i], path0, names, titles) < 0) {
			if (!Input().Bool("try again?"))
				exit(EXIT_FAILURE);
		}
	}

	// 2nd loop through tex files
	cout << "\n\n\n\n" << u8"====== 第 2 轮转换 ======\n" << endl;
	Str32 html;
	for (unsigned i{}; i < names.size(); ++i) {
		cout    << std::setw(5)  << std::left << i
				<< std::setw(10)  << std::left << names[i]
				<< std::setw(20) << std::left << titles[i] << endl;
		read_file(html, path0 + names[i] + ".html"); // read html file
		if (names[i] == U"ITable")
			cout << "one file debug" << endl;
		// process \autoref and \upref
		if (autoref(IdList, LabelList, names[i], html) < 0) {
			if (Input().Bool("try again?")) {
				--i; continue;
			}
			else
				exit(EXIT_FAILURE);
		}
		write_file(html, path0 + names[i] + ".html"); // save html file
	}
	cout << endl;
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
