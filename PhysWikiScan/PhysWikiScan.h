﻿#pragma once
#include "../SLISC/global.h"
#include "../SLISC/file.h"
#include "../TeX/tex2html.h"

using namespace slisc;

// get the title (defined as the first comment, no space after %)
// limited to 20 characters
// return -1 if error, 0 if successful
Long GetTitle(Str32_O title, Str32_I str)
{
	if (str.at(0) != U'%') {
		SLS_ERR("Need a title!"); // break point here
		return -1;
	}
	Str32 c;
	Long ind0 = NextNoSpace(c, str, 1);
	if (ind0 < 0) {
		SLS_ERR("Title is empty!"); // break point here
		return -1;
	}
	Long ind1 = str.find(U'\n', ind0);
	if (ind1 < 0) {
		SLS_ERR("Body is empty!"); // break point here
	}
	title = str.substr(ind0, ind1 - ind0);
	return 0;
}

// add <p> tags to paragraphs
// return number of tag pairs added
Long ParagraphTag(Str32_IO str)
{
	Long i{}, N{}, N1{}, ind0{}, ind2{};
	vector<Long> ind, ind1;
	TrimLeft(str, U'\n'); TrimRight(str, U'\n');
	// delete extra '\n' (more than two continuous)
	while (true) {
		ind0 = str.find(U"\n\n\n", ind0);
		if (ind0 < 0) break;
		str.erase(ind0, 1);
	}
	// remove "\n\n" and add <p> tags
	ind0 = 0;
	while (true) {
		ind0 = str.find(U"\n\n", ind0);
		if (ind0 < 0) break;
		ind.push_back(ind0); ++N; ind0 += 2;
	}
	N = ind.size();
	for (i = N - 1; i >= 0; --i) {
		str.insert(ind[i] + 1, U"<p>　　");// <p> is indented by unicode white space
		str.insert(ind[i], U"\n</p>");
	}
	str.insert(str.size(), U"\n</p>");
	str.insert(0, U"<p>　　\n");// <p> is indented by unicode white space

	// deal with ranges that should not be in <p>...</p>
	// if there is "<p>" before range, delete it, otherwise, add "</p>"
	// if there is "</p>" after range, delete it, otherwise, add "<p>　　"
	FindComBrace(ind, U"\\subsection", str, 'o');
	FindComBrace(ind1, U"\\subsubsection", str, 'o');
	CombineRange(ind, ind, ind1);
	FindComBrace(ind1, U"\\pentry", str, 'o');
	CombineRange(ind, ind, ind1);
	for (i = ind.size() - 2; i >= 0; i -= 2) {
		ind0 = ExpectKey(str, U"</p>", ind[i + 1] + 1);
		if (ind0 >= 0) {
			str.erase(ind0 - 4, 4);
		}
		else {
			str.insert(ind[i + 1] + 1, U"\n<p>　　"); ++N;
		}
		ind0 = ExpectKeyReverse(str, U"<p>　　", ind[i] - 1);
		ind2 = ExpectKeyReverse(str, U"<p>", ind[i] - 1);
		if (ind0 > -2) {
			str.erase(ind0 + 1, 5); --N;
		}
		else if (ind2 > -2) {
			str.erase(ind2 + 1, 3); --N;
		}
		else {
			str.insert(ind[i], U"</p>\n\n");
		}
	}

	// deal with ranges that should not be in <p>...</p>
	// if there is "<p>" before range, delete it, otherwise, add "</p>"
	// if there is "</p>" after range, delete it, otherwise, add "<p>"
	FindEnv(ind, str, U"figure", 'o');
	FindEnv(ind1, str, U"itemize", 'o');
	CombineRange(ind, ind, ind1);
	FindEnv(ind1, str, U"enumerate", 'o');
	CombineRange(ind, ind, ind1);
	FindComBrace(ind1, U"\\code", str, 'o');
	CombineRange(ind, ind, ind1);
	FindComBrace(ind1, U"\\Code", str, 'o');
	CombineRange(ind, ind, ind1);
	for (i = ind.size() - 2; i >= 0; i -= 2) {
		ind0 = ExpectKey(str, U"</p>", ind[i + 1] + 1);
		if (ind0 >= 0) {
			str.erase(ind0 - 4, 4);
		}
		else {
			str.insert(ind[i + 1] + 1, U"\n<p>"); ++N;
		}
		ind0 = ExpectKeyReverse(str, U"<p>　　", ind[i] - 1);
		ind2 = ExpectKeyReverse(str, U"<p>", ind[i] - 1);
		if (ind0 > -2) {
			str.erase(ind0 + 1, 5); --N;
		}
		else if (ind2 > -2) {
			str.erase(ind2 + 1, 3); --N;
		}
		else {
			str.insert(ind[i], U"</p>\n\n");
		}
	}

	// deal with equation environments alike
	// if there is "</p>\n<p>　　" before range, delete it
	// if there is "<p>　　" before range, delete "　　"
	FindEnv(ind, str, U"equation", 'o');
	FindEnv(ind1, str, U"gather", 'o');
	CombineRange(ind, ind, ind1);
	FindEnv(ind1, str, U"align", 'o');
	CombineRange(ind, ind, ind1);
	for (i = ind.size() - 2; i >= 0; i -= 2) {
		ind0 = ExpectKeyReverse(str, U"</p>\n<p>　　", ind[i] - 1);
		ind2 = ExpectKeyReverse(str, U"<p>　　", ind[i] - 1);
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

	// deal with ranges that should not be in <p>...</p>
	// if there is "<p>" before \begin{}{}, delete it, otherwise, add "</p>"
	// if there is "</p>" after \begin{}{}, delete it, otherwise, add "<p>　　"
	// if there is "<p>" before \end{}, delete it, otherwise, add "</p>"
	// if there is "</p>" after \end{}, delete it, otherwise, add "<p>　　"
	FindBegin(ind, U"exam", str, '2');
	FindEnd(ind1, U"exam", str);
	CombineRange(ind, ind, ind1);
	FindBegin(ind1, U"exer", str, '2');
	CombineRange(ind, ind, ind1);
	FindEnd(ind1, U"exer", str);
	CombineRange(ind, ind, ind1);
	for (i = ind.size() - 2; i >= 0; i -= 2) {
		ind0 = ExpectKey(str, U"</p>", ind[i + 1] + 1);
		if (ind0 >= 0) {
			str.erase(ind0 - 4, 4);
		}
		else {
			str.insert(ind[i + 1] + 1, U"\n<p>　　"); ++N;
		}
		ind0 = ExpectKeyReverse(str, U"<p>　　", ind[i] - 1);
		ind2 = ExpectKeyReverse(str, U"<p>", ind[i] - 1);
		if (ind0 > -2) {
			str.erase(ind0 + 1, 5); --N;
		}
		else if (ind2 > -2) {
			str.erase(ind2 + 1, 3); --N;
		}
		else {
			str.insert(ind[i], U"</p>\n\n");
		}
	}
	return N;
}

// add html id tag before environment if it contains a label, right before "\begin{}" and delete that label
// output html id and corresponding label for future reference
// "gather" and "align" environments has id = "ga" and "ali"
// idNum is in the idNum-th environment of the same name (not necessarily equal to displayed number)
// no comment allowed
// return number of labels processed, or -1 if failed
Long EnvLabel(vector<Str32>& id, vector<Str32>& label, Str32_I entryName, Str32_IO str)
{
	Long ind0{}, ind1{}, ind2{}, ind3{}, ind4{}, ind5{}, N{}, temp{},
		Ngather{}, Nalign{}, i{}, j{};
	Str32 idName; // "eq" or "fig" or "ex"...
	Str32 envName; // "equation" or "figure" or "exam"...
	Str32 idNum{}; // id = idName + idNum
	Long idN{}; // convert to idNum
	vector<Long> indEnv;
	while (true) {
		ind5 = str.find(U"\\label", ind0);
		if (ind5 < 0) return N;
		// make sure label is inside an environment
		ind2 = str.rfind(U"\\end", ind5);
		ind4 = str.rfind(U"\\begin", ind5);
		if (ind2 >= ind4) {
			SLS_ERR("label not in an environment!"); // break point here
			return -1;
		}
		// detect environment kind
		ind1 = ExpectKey(str, U"{", ind4 + 6);
		if (ExpectKey(str, U"equation", ind1) > 0) {
			idName = U"eq"; envName = U"equation";
		}
		else if (ExpectKey(str, U"figure", ind1) > 0) {
			idName = U"fig"; envName = U"figure";
		}
		else if (ExpectKey(str, U"exam", ind1) > 0) {
			idName = U"ex"; envName = U"exam";
		}
		else if (ExpectKey(str, U"exer", ind1) > 0) {
			idName = U"exe"; envName = U"exer";
		}
		else if (ExpectKey(str, U"table", ind1) > 0) {
			idName = U"tab"; envName = U"table";
		}
		else if (ExpectKey(str, U"gather", ind1) > 0) {
			idName = U"eq"; envName = U"gather";
		}
		else if (ExpectKey(str, U"align", ind1) > 0) {
			idName = U"eq"; envName = U"align";
		}
		else {
			SLS_ERR("environment not supported for label!"); return -1; // break point here
		}
		// check label format and save label
		ind0 = ExpectKey(str, U"{", ind5 + 6);
		ind3 = ExpectKey(str, entryName + U'_' + idName, ind0);
		if (ind3 < 0) {
			SLS_ERR("label format error!"); return -1; // break point here
		}
		ind3 = str.find(U"}", ind3);
		label.push_back(str.substr(ind0, ind3 - ind0));
		TrimLeft(label.back(), U' '); TrimRight(label.back(), U' ');
		// count idNum, insert html id tag, delete label
		if (idName != U"eq") {
			idN = FindEnv(indEnv, str.substr(0,ind4), envName) + 1;
		}
		else { // count equations
			idN = FindEnv(indEnv, str.substr(0,ind4), U"equation");
			Ngather = FindEnv(indEnv, str.substr(0,ind4), U"gather");
			if (Ngather > 0) {
				for (i = 0; i < 2 * Ngather; i += 2) {
					for (j = indEnv[i]; j < indEnv[i + 1]; ++j) {
						if (str.at(j) == '\\' && str.at(j+1) == '\\')
							++idN;
					}
					++idN;
				}
			}
			Nalign = FindEnv(indEnv, str.substr(0,ind4), U"align");
			if (Nalign > 0) {
				for (i = 0; i < 2 * Nalign; i += 2) {
					for (j = indEnv[i]; j < indEnv[i + 1]; ++j) {
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
Long FigureEnvironment(Str32_IO str, Str_I path)
{
	Long i{}, N{}, Nfig{}, ind0{}, ind1{}, indName1{}, indName2{};
	double width{}; // figure width in cm
	vector<Long> indFig{};
	Str figName;
	Str32 format, caption, widthPt, figNo;
	Nfig = FindEnv(indFig, str, U"figure", 'o');
	for (i = 2 * Nfig - 2; i >= 0; i -= 2) {
		// get width of figure
		ind0 = str.find(U"width", indFig[i]) + 5;
		ind0 = ExpectKey(str, U"=", ind0);
		ind0 = FindNum(str, ind0);
		str2double(width, str, ind0);
		// get file name of figure
		indName1 = str.find(U"figures/", ind0) + 8;
		indName2 = str.find(U".pdf", indFig[i]) - 1;
		if (indName2 < 0)
			indName2 = str.find(U".png", indFig[i]) - 1;
		if (indName2 < 0)
			SLS_ERR("error when reading figure name!"); // breakpoint here
		figName = UTF32_to_UTF8(str.substr(indName1, indName2 - indName1 + 1));
		// get caption of figure
		ind0 = str.find(U"\\caption", ind0);
		if (ind0 < 0) {
			SLS_ERR("fig caption not found!"); return -1; // break point here
		}
		ind0 = ExpectKey(str, U"{", ind0 + 8);
		ind1 = PairBraceR(str, ind0);
		caption = str.substr(ind0, ind1 - ind0);
		str.erase(indFig[i], indFig[i + 1] - indFig[i] + 1); // delete environment
		ind0 = ExpectKey(str, U"\n</p>", indFig[i]);
		// test img file existence
		if (file_exist(path + figName + ".svg"))
			format = U".svg";
		else if (file_exist(path + figName + ".png"))
			format = U".png";
		else {
			SLS_ERR("figure not found!"); return -1; // break point here
		}
		// insert html code
		num2str(widthPt, (Long)(33.4 * width));
		num2str(figNo, i / 2 + 1);
		ind0 = insert(str, U"<div class = \"w3-content\" style = \"max-width:" + widthPt
			+ U"pt;\">\n" + U"<img src = \"" + UTF8_to_UTF32(figName) + format
			+ U"\" alt = \"图\" style = \"width:100%;\">\n</div>\n<div align = \"center\"> 图" + figNo
			+ U"：" + caption + U"</div>", indFig[i]);
		++N;
	}
	return N;
}

// replace \pentry comman with html round panel
Long pentry(Str32_IO str)
{
	Long i{}, N{};
	vector<Long> indIn, indOut;
	FindComBrace(indIn, U"\\pentry", str);
	N = FindComBrace(indOut, U"\\pentry", str, 'o');
	for (i = 2 * N - 2; i >= 0; i -= 2) {
		str.erase(indOut[i + 1], 1);
		str.insert(indOut[i + 1], U"</div>");
		str.erase(indOut[i], indIn[i] - indOut[i]);
		str.insert(indOut[i], U"<div class = \"w3-panel w3-round-large w3-light-blue\"><b>预备知识</b>　");
	}
	return N;
}

// remove special .tex files from a list of name
// return number of names removed
// names has ".tex" extension
Long RemoveNoEntry(vector<Str> &names)
{
	Long i{}, j{}, N{}, Nnames{}, Nnames0;
	vector<Str> names0; // names to remove
	names0.push_back("PhysWiki");
	names0.push_back("Sample");
	names0.push_back("FrontMatters");
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
Long ExampleEnvironment(Str32_IO str, Str_I path0)
{
	Long i{}, N{}, ind0{}, ind1{};
	vector<Long> indIn, indOut;
	Str32 exName, exNo;
	FindEnv(indIn, str, U"exam");
	N = FindEnv(indOut, str, U"exam", 'o');
	for (i = 2 * N - 2; i >= 0; i -= 2) {
		ind0 = str.find('{', indOut[i]);
		ind0 = PairBraceR(str, ind0);
		ind0 = ExpectKey(str, U"{", ind0 + 1);
		ind1 = PairBraceR(str, ind0 - 1);
		exName = str.substr(ind0, ind1 - ind0);
		// replace with html tags
		str.erase(indIn[i + 1] + 1, indOut[i + 1] - indIn[i + 1]);
		str.insert(indIn[i + 1] + 1, U"</div>\n");
		str.erase(indOut[i], ind1 - indOut[i] + 1);
		num2str(exNo, i/2 + 1);
		str.insert(indOut[i], U"<div class = \"w3-panel w3-border-yellow w3-leftbar\">\n <h5><b>例"
				+ exNo + U"</b>　" + exName + U"</h5>");
	}
	return N;
}

// example is already not in a paragraph
// return number of examples processed, return -1 if failed
Long ExerciseEnvironment(Str32_IO str, Str_I path0)
{
	Long i{}, N{}, ind0{}, ind1{};
	vector<Long> indIn, indOut;
	Str32 exName, exNo;
	FindEnv(indIn, str, U"exer");
	N = FindEnv(indOut, str, U"exer", 'o');
	for (i = 2 * N - 2; i >= 0; i -= 2) {
		ind0 = str.find(U"{", indOut[i]);
		ind0 = PairBraceR(str, ind0);
		ind0 = ExpectKey(str, U"{", ind0 + 1);
		ind1 = PairBraceR(str, ind0 - 1);
		exName = str.substr(ind0, ind1 - ind0);
		// replace with html tags
		str.erase(indIn[i + 1] + 1, indOut[i + 1] - indIn[i + 1]);
		str.insert(indIn[i + 1] + 1, U"</div>\n");
		str.erase(indOut[i], ind1 - indOut[i] + 1);
		num2str(exNo, i / 2 + 1);
		str.insert(indOut[i], U"<div class = \"w3-panel w3-border-green w3-leftbar\">\n <h5><b>习题"
			+ exNo + U"</b>　" + exName + U"</h5>");
	}
	return N;
}

// replace autoref with html link
// no comment allowed
// does not add link for \autoref inside eq environment (equation, align, gather)
// return number of autoref replaced, or -1 if failed
Long autoref(const vector<Str32> &id, const vector<Str32> &label, Str32_I entryName, Str32_IO str)
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
		inEq = IndexInEnv(ienv, ind0, envNames, str);
		ind1 = ExpectKey(str, U"{", ind0 + 8);
		ind1 = NextNoSpace(entry, str, ind1);
		ind2 = str.find('_', ind1);
		if (ind2 < 0) {
			SLS_ERR("autoref format error!"); return -1; // break point here
		}
		ind3 = FindNum(str, ind2);
		idName = str.substr(ind2 + 1, ind3 - ind2 - 1);
		if (idName == U"eq") kind = U"式";
		else if (idName == U"fig") kind = U"图";
		else if (idName == U"ex") kind = U"例";
		else if (idName == U"exe") kind = U"练习";
		else if (idName == U"tab") kind = U"表";
		else {
			SLS_ERR("unknown id name!"); return -1; // break point here
		}
		ind3 = str.find('}', ind3);
		// find id of the label
		label0 = str.substr(ind1, ind3 - ind1); TrimLeft(label0, U' '); TrimRight(label0, U' ');
		for (i = 0; i < label.size(); ++i) {
			if (label0 == label[i]) break;
		}
		if (i == label.size()) {
			SLS_ERR("label not found!"); return -1; // break point here
		}
		ind4 = FindNum(id[i], 0);
		idNum = id[i].substr(ind4);		
		entry = str.substr(ind1, ind2 - ind1);
		if (entry != entryName) {// reference the same entry
			newtab = U"target = \"_blank\"";
			file = entry + U".html";
		}
		if (!inEq) str.insert(ind3 + 1, U" </a>");
		str.insert(ind3 + 1, kind + U' ' + idNum);
		if (!inEq) str.insert(ind3 + 1, U"<a href = \"" + file + U"#" + id[i] + U"\" " + newtab + U">");
		str.erase(ind0, ind3 - ind0 + 1);
		++N;
	}
}

// process upref
// path must end with '\\'
Long upref(Str32_IO str, Str_I path)
{
	Long i{}, N{};
	vector<Long> indIn, indOut;
	Str32 entryName;
	FindComBrace(indIn, U"\\upref", str);
	N = FindComBrace(indOut, U"\\upref", str, 'o');
	for (i = 2 * N - 2; i >= 0; i -= 2) {
		entryName = str.substr(indIn[i], indIn[i + 1] - indIn[i] + 1);
		TrimLeft(entryName, U' '); TrimRight(entryName, U' ');
		if (!file_exist(path + UTF32_to_UTF8(entryName) + ".tex")) {
			SLS_ERR("upref file not found!"); return -1; // break point here
		}
		str.erase(indOut[i], indOut[i + 1] - indOut[i] + 1);
		str.insert(indOut[i], U"<span class = \"icon\"><a href = \"" +
			entryName + U".html\" target = \"_blank\"><i class = \"fa fa-external-link\"></i></a></span>");
	}
}

// create table of content from PhysWiki1.tex
// path must end with '\\'
// return the number of entries
// names is a list of filenames
// output chinese titles,  titles[i] is the chinese title of names[i]
Long TableOfContent(vector<Str32> &titles, const vector<Str> &names, Str_I path)
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
	Str entryName; // entry label
	Str32 str; read_file(str, path + "PhysWiki.tex");
	CRLF_to_LF(str);
	Str32 toc; read_file(toc, "PhysWikiScan/template.html"); // read html template
	CRLF_to_LF(str);
	titles.resize(names.size());
	ind0 = toc.find(U"PhysWikiHTMLtitle");
	toc.erase(ind0, 17);
	toc.insert(ind0, U"小时物理百科在线");
	ind0 = toc.find(U"PhysWikiCommand", ind0);
	toc.erase(ind0, 15); toc.insert(ind0, newcomm);
	ind0 = toc.find(U"PhysWikiHTMLbody", ind0);
	toc.erase(ind0, 16);

	ind0 = insert(toc, U"<img src = \"../title.png\" alt = \"图\" style = \"width:100%;\">\n", ind0);
	ind0 = insert(toc, U"<div class = \"w3-container w3-center w3-blue w3-text-white\">\n", ind0);
	ind0 = insert(toc, U"<h1>小时物理百科在线</h1>\n</div>\n\n", ind0);

	ind0 = insert(toc, U"<div class = \"w3-container\"><p>\n", ind0);
	ind0 = insert(toc, U"<a href = \"license.html\" target = \"_blank\">版权声明</a>　\n", ind0);
	ind0 = insert(toc, U"<a href = \"about.html\" target = \"_blank\">项目介绍</a>　\n", ind0);
	ind0 = insert(toc, U"<a href = \"readme.html\" target = \"_blank\">使用说明</a>　\n", ind0);
	ind0 = insert(toc, U"<a href = \"../\">返回主页</a>\n", ind0);

	// remove comments
	vector<Long> indComm;
	for (i = 2*FindComment(indComm, str)-2; i >= 0; i -= 2)
		str.erase(indComm[i], indComm[i + 1] - indComm[i] + 1);
	while (true) {
		ind1 = FindMultiple(ikey, str, keys, ind1);
		if (ind1 < 0) break;
		if (ikey >= 2) { // found "\entry"
			// get chinese title and entry label
			if (ikey == 2 || ikey == 3) ind1 += 6;
			else if (ikey == 4) ind1 += 9;
			++N;
			ind1 = ExpectKey(str, U"{", ind1);
			ind2 = str.find(U"}", ind1);
			title = str.substr(ind1, ind2 - ind1);
			TrimLeft(title, U' '); TrimRight(title, U' ');
			replace(title, U"\\ ", U" ");
			if (title.empty()) {
				SLS_ERR("Chinese title is empty in PhysWiki.tex!"); return -1;  // break point here
			}
			ind1 = ExpectKey(str, U"{", ind2 + 1);
			ind2 = str.find(U"}", ind1);
			entryName = UTF32_to_UTF8(str.substr(ind1, ind2 - ind1));
			ind1 = ind2;
			// insert entry into html table of contents
			ind0 = insert(toc, U"<a href = \"" + UTF8_to_UTF32(entryName) + U".html" + U"\" target = \"_blank\">"
				+ title + U"</a>　\n", ind0);
			// record Chinese title
			for (i = 0; i < names.size(); ++i) {
				if (entryName == names[i]) break;
			}
			if (i == names.size()) {
				SLS_ERR("File not found for an entry in PhysWiki.tex!"); return -1; // break point here
			}
			titles[i] = title;
		}
		else if (ikey == 1) { // found "\chapter"
			// get chinese chapter name
			ind1 += 8;
			ind1 = ExpectKey(str, U"{", ind1);
			ind2 = str.find('}', ind1);
			title = str.substr(ind1, ind2 - ind1);
			TrimLeft(title, U' '); TrimRight(title, U' ');

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
	 		ind1 = ExpectKey(str, U"{", ind1);
	 		ind2 = str.find('}', ind1);
	 		title = str.substr(ind1, ind2 - ind1);
			TrimLeft(title, U' '); TrimRight(title, U' ');
	 		replace(title, U"\\ ", U" ");
	 		ind1 = ind2;
	 		// insert part into html table of contents
	 		++partNo;
			
	 		ind0 = insert(toc, U"</p></div>\n\n<div class = \"w3-container w3-center w3-teal w3-text-white\">\n", ind0);
	 		ind0 = insert(toc, U"<h3 align = \"center\">第" + chineseNo[partNo] + U"部分 "
	 			+ title + U"</h3>\n", ind0);
	 		ind0 = insert(toc, U"</div>\n\n<div class = \"w3-container\">\n", ind0);
	 	}
	}
	toc.insert(ind0, U"</p>\n</div>");
	write_file(toc, path + "index.html");
	cout << "\n\nWarning: the following files are not used by PhysWiki.tex: " << endl;
	for (i = 0; i < titles.size(); ++i) {
		if (titles[i].empty())
			cout << names[i] + ".tex" << endl;
	}
	cout << endl;
	return N;
}

// process Matlab code (\code command)
// https://www.artefact.tk/software/matlab/highlight/
// to allow unicode:
// first save .tex code to .m code in ANSI encoding
// then use the program in this website to convert to html
// then save the html files as UTF-8 encoding
// path must end with '\\', code files (html) must be in "<path>\codes\" folder
// no comment allowed, must use after ParagraphTag()
Long MatlabCode(Str32_IO str, Str_I path)
{
	Long i{}, N{}, ind0{}, ind1{}, ind2{};
	Str32 name; // code file name without extension
	Str32 code;
	vector<Long> indIn, indOut;
	// \code commands
	FindComBrace(indIn, U"\\code", str);
	N = FindComBrace(indOut, U"\\code", str, 'o');
	for (i = 2 * N - 2; i >= 0; i -= 2) {
		// get code file name
		name = str.substr(indIn[i], indIn[i + 1] - indIn[i] + 1);
		TrimLeft(name, U' '); TrimRight(name, U' ');
		// read file
		if (!file_exist(path + "codes\\" + UTF32_to_UTF8(name) + ".html")) {
			SLS_ERR("code file not found!"); return -1; // break point here
		}
		read_file(code, path + "codes\\" + UTF32_to_UTF8(name) + ".html");
		CRLF_to_LF(code);
		ind0 = code.find(U"<pre", 0);
		ind1 = code.find(U"</pre>", ind0);
		code = code.substr(ind0, ind1 - ind0 + 6);
		// insert code
		str.erase(indOut[i], indOut[i + 1] - indOut[i] + 1);
		str.insert(indOut[i], U"</div></div>");
		str.insert(indOut[i], code);
		str.insert(indOut[i], U"<div class = \"nospace\">");
		str.insert(indOut[i], U"<span class = \"icon\"><a href = \"" + name
			+ U".m\" download> <i class = \"fa fa-caret-square-o-down\"></i></a></span>");
		str.insert(indOut[i], U"<div class = \"w3-code notranslate w3-pale-yellow\">");
	}
	return N;
}

// same with MatlabCode, for \Code command
Long MatlabCodeTitle(Str32_IO str, Str_I path)
{
	Long i{}, N{}, ind0{}, ind1{}, ind2{};
	Str32 name; // code file name without extension
	Str32 code;
	vector<Long> indIn, indOut;
	// \code commands
	FindComBrace(indIn, U"\\Code", str);
	N = FindComBrace(indOut, U"\\Code", str, 'o');
	for (i = 2 * N - 2; i >= 0; i -= 2) {
		// get code file name
		name = str.substr(indIn[i], indIn[i + 1] - indIn[i] + 1);
		TrimLeft(name, U' '); TrimRight(name, U' ');
		// read file
		if (!file_exist(path + "codes\\" + UTF32_to_UTF8(name) + ".html")) {
			SLS_ERR("code file not found!"); return -1; // break point here
		}
		read_file(code, path + "codes\\" + UTF32_to_UTF8(name) + ".html");
		CRLF_to_LF(code);
		ind0 = code.find(U"<pre", 0);
		ind1 = code.find(U"</pre>", ind0);
		code = code.substr(ind0, ind1 - ind0 + 6);
		// insert code
		str.erase(indOut[i], indOut[i + 1] - indOut[i] + 1);
		str.insert(indOut[i], U"</div></div>");
		str.insert(indOut[i], code);
		str.insert(indOut[i], U"<div class = \"w3-code notranslate w3-pale-yellow\">\n<div class = \"nospace\">");
		// insert title with download link
		if (!file_exist(path + UTF32_to_UTF8(name) + ".m")) {
			SLS_ERR(".m file not found!"); return -1; // break point here
		}
		str.insert(indOut[i], U"<b>" + name + U".m</b>\n");
		str.insert(indOut[i], U"<span class = \"icon\"><a href = \"" + name
			+ U".m\" download> <i class = \"fa fa-caret-square-o-down\"></i></a></span>");
	}
	return N;
}

// find \bra{}\ket{} and mark
Long OneFile4(Str_I path)
{
	Long ind0{}, ind1{}, N{};
	Str32 str;
	read_file(str, path); // read file
	CRLF_to_LF(str);
	while (true) {
		ind0 = str.find(U"\\bra", ind0);
		if (ind0 < 0) break;
		ind1 = ind0 + 4;
		ind1 = ExpectKey(str, U"{", ind1);
		if (ind1 < 0) {
			++ind0; continue;
		}
		ind1 = PairBraceR(str, ind1 - 1);
		ind1 = ExpectKey(str, U"\\ket", ind1 + 1);
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

// process \Command{} to display Matlab Code
// TODO: generate one .m file to be processed by "highlight()" function in Matlab
// return the number of \Command{} processed, return -1 if failed
Long Command(Str32_IO str)
{
	Long i{}, j{}, N{}, ind0{}, ind1{}, Ncolor;
	vector<Long> ind, indColor;
	Str32 temp;
	N = FindEnv(ind, str, U"Command");
	for (i = 2 * N - 2; i >= 0; i -= 2) {
		temp = str.substr(ind[i], ind[i + 1] - ind[i] + 1);
		replace(temp, U"\\\\", U"");
		// deal with color
		Ncolor = FindComBrace(indColor, U"\\color", temp, 'o');
		for (j = 2 * Ncolor - 2; j >= 0; j -= 2) {
			temp.erase(indColor[j], indColor[j + 1] - indColor[j] + 1);
			ind0 = ExpectKeyReverse(temp, U"{", indColor[j] - 1) + 1;
			ind1 = PairBraceR(temp, ind0);
			temp.erase(ind1, 1); temp.insert(ind1, U"</span>");
			temp.erase(ind0, 1); temp.insert(ind0, U"<span class = \"string\">");
		}
		// process \par
		while (true) {
			ind0 = temp.find(U"\\par");
			if (ind0 < 0) break;
			temp.erase(ind0, 4);
			DeleteSpaceReturn(temp, ind0);
			temp.insert(ind0, U"\n　　");
		}

		str.erase(ind[i], ind[i + 1] - ind[i] + 1);
		str.insert(ind[i], temp);
	}
	Env2Tag(U"Command", U"<div class = \"w3-code notranslate w3-pale-yellow\">\n<div class = \"nospace\"><pre class = \"mcode\">",
		U"</pre></div></div>", str);
	return N;
}

// generate html from tex
// output the chinese title of the file, id-label pairs in the file
// return 0 if successful, -1 if failed
// entryName does not include ".tex"
// path0 is the parent folder of entryName.tex, ending with '\\'
Long PhysWikiOnline1(vector<Str32>& id, vector<Str32>& label, Str_I entryName,
	Str path0, const vector<Str>& names, const vector<Str32>& titles)
{
	// read template file from local folder
	Long i{}, j{}, N{}, ind0;
	Str32 str;
	read_file(str, path0 + entryName + ".tex"); // read tex file
	CRLF_to_LF(str);
	Str32 title;
	//// read template from the same folder as path
	//ind0 = path.ReverseFind('\\');
	//path.erase(ind0, str.GetLength() - ind0);
	//path += U"\\template.html";
	Str32 html; read_file(html, "PhysWikiScan/template.html"); // read html template
	CRLF_to_LF(html);
	Str32 newcomm; read_file(newcomm, "PhysWikiScan/newcommand.html");
	CRLF_to_LF(newcomm);
	if (GetTitle(title, str) < 0) return -1;
	for (j = 0; j < names.size(); ++j) {
		if (entryName == names[j])
			break;
	}
	if (!titles[j].empty() && title != titles[j]) {
		SLS_ERR("Chinese title different from toc!"); return -1; // break point here
	}
	// insert \newcommand
	ind0 = html.find(U"PhysWikiCommand", 0);
	html.erase(ind0, 15);
	html.insert(ind0, newcomm);
	// insert HTML title
	ind0 = html.find(U"PhysWikiHTMLtitle", 0);
	html.erase(ind0, 17);
	html.insert(ind0, title);
	// remove comments
	vector<Long> indComm;
	N = FindComment(indComm, str);
	for (i = 2 * N - 2; i >= 0; i -= 2) {
		str.erase(indComm[i], indComm[i + 1] - indComm[i] + 1);
	}
	// escape characters
	NormalTextEscape(str);
	// add paragraph tags
	ParagraphTag(str);
	// itemize and enumerate
	Itemize(str); Enumerate(str);
	// add html id for links
	EnvLabel(id, label, UTF8_to_UTF32(entryName), str);
	// process table environments
	Table(str);
	// ensure '<' and '>' has spaces around them
	EqOmitTag(str);
	// process figure environments
	FigureEnvironment(str, path0);
	// process example and exercise environments
	ExampleEnvironment(str, path0);
	ExerciseEnvironment(str, path0);
	// process \pentry{}
	pentry(str);
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
	upref(str, path0);
	// replace environments with html tags
	Env2Tag(U"equation", U"<div class=\"eq\"><div class = \"w3-cell\" style = \"width:710px\">\n\\begin{equation}",
						U"\\end{equation}\n</div></div>", str);
	Env2Tag(U"gather", U"<div class=\"eq\"><div class = \"w3-cell\" style = \"width:710px\">\n\\begin{gather}",
		U"\\end{gather}\n</div></div>", str);
	Env2Tag(U"align", U"<div class=\"eq\"><div class = \"w3-cell\" style = \"width:710px\">\n\\begin{align}",
		U"\\end{align}\n</div></div>", str);
	// Matlab code
	MatlabCode(str, path0); MatlabCodeTitle(str, path0);
	Command(str);
	Command2Tag(U"x", U"<span class = \"w3-text-teal\">", U"</span>", str);
	// footnote
	footnote(str);
	// delete redundent commands
	replace(str, U"\\dfracH", U"");
	// insert title and notice
	ind0 = html.find(U"PhysWikiHTMLbody", ind0); html.erase(ind0, 16);
	ind0 = insert(html, U"<img src = \"../title.png\" alt = \"图\" style = \"width:100%;\">\n", ind0);
	ind0 = insert(html, U"<div class = \"w3-container w3-center w3-blue w3-text-white\">\n", ind0);
	ind0 = insert(html, U"<h3>" + title + U"</h3>\n</div>\n\n<div class = \"w3-container\">\n", ind0);
	ind0 = insert(html, U"<p>\n<span class=\"w3-tag w3-yellow\">公告：《小时物理百科》网页版仍在开发中，", ind0);
	ind0 = insert(html, U"请下载<a href=\"../\">《小时物理百科》PDF 版</a>。</span>\n</p>\n", ind0);
	// insert HTML body
	ind0 = insert(html, str, ind0);
	ind0 = insert(html, U"\n</div>\n", ind0);
	ind0 = insert(html, U"<div class = \"w3-container w3-gray\">", ind0);
	ind0 = insert(html, U"<p>\n<a href = \"../online\">返回目录</a>　", ind0);
	ind0 = insert(html, U"<a href = \"../\">返回主页</a>　", ind0);
	ind0 = insert(html, U"<a href = \"../#donation\">捐助项目</a>　</p>\n", ind0);
	ind0 = insert(html, U"</div>", ind0);
	// save html file
	write_file(html, path0 + entryName + ".html");
	return 0;
}

inline void PhysWikiOnline()
{
	Str path0 = "C:/Users/Addis/Documents/GitHub/temp/PhysWikiOnline/";
	vector<Str> names;
	
	file_list_ext(names, path0, "tex", false);
	vector<Str32> titles;
	RemoveNoEntry(names);

	if (names.size() <= 0) return;
	cout << "Creating table of contents from PhysWiki.tex...\n";

	TableOfContent(titles, names, path0);
	cout << endl;
	vector<Str32> IdList, LabelList; // html id and corresponding tex label
	// 1st loop through tex files
	for (Long i = 0; i < names.size(); ++i) {
		cout << i << " ";
		cout << names[i] << "...";
		if (names[i] == "MatFun")
			Long Set_Break_Point_Here = 1000; // one file debug
		// main process
		PhysWikiOnline1(IdList, LabelList, names[i], path0, names, titles);
		cout << endl;
	}

	// 2nd loop through tex files
	cout << "\n\n\n\n\n" << endl << "2nd Loop:" << endl;
	Str32 html;
	for (unsigned i{}; i < names.size(); ++i) {
		cout << i << ' ' << names[i] << "...";
		read_file(html, path0 + names[i] + ".html"); // read html file
		if (names[i] == "ITable")
			Long Set_Break_Point_Here = 1000; // one file debug
		// process \autoref and \upref
		autoref(IdList, LabelList, UTF8_to_UTF32(names[i]), html);
		write_file(html, path0 + names[i] + ".html"); // save html file
		cout << endl;
	}
}

void PhysWikiCheck()
{
	Long ind0{};
	Str path0 = "C:/Users/addis/Documents/GitHub/PhysWiki/contents/";
	//CString path0 = U"C:\\Users\\addis\\Documents\\GitHub\\littleshi.cn\\root\\PhysWiki\\online\\");
	vector<Str> names;
	file_list_ext(names, path0, "tex", false);
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