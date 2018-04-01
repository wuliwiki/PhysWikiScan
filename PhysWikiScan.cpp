#include <io.h> // for console unicode output
#include <fcntl.h> // for console unicode output
#include "macrouniverse.h"
#include "macrouniverseS.h"
#include "tex.h"
#include "tex2html.h"

using namespace std;

// get the title (defined as the first comment, no space after %)
// limited to 20 characters
// return -1 if error, 0 if successful
int GetTitle(CString& title, const CString& str)
{
	if (str.GetAt(0) != '%') {
		wcout << _T("Need a title!");
		return -1;
	}
	CString c;
	int ind0 = NextNoSpace(c, str, 1);
	if (ind0 < 0) {
		wcout << _T("Title is empty!");
		return -1;
	}
	int ind1 = str.Find('\n', ind0);
	if (ind1 < 0) {
		wcout << _T("Body is empty!");
	}
	title = str.Mid(ind0, ind1 - ind0);
	return 0;
}

// add <p> tags to paragraphs
// return number of tag pairs added
int ParagraphTag(CString& str)
{
	int i{}, N{}, N1{}, ind0{}, ind2{};
	vector<int> ind, ind1;
	str.TrimLeft('\n'); str.TrimRight('\n');
	// delete extra '\n' (more than two continuous)
	while (true) {
		ind0 = str.Find(_T("\n\n\n"), ind0);
		if (ind0 < 0) break;
		str.Delete(ind0, 1);
	}
	// remove "\n\n" and add <p> tags
	ind0 = 0;
	while (true) {
		ind0 = str.Find(_T("\n\n"), ind0);
		if (ind0 < 0) break;
		ind.push_back(ind0); ++N; ind0 += 2;
	}
	N = ind.size();
	for (i = N - 1; i >= 0; --i) {
		str.Insert(ind[i] + 1, _T("<p>　　"));// <p> is indented by unicode white space
		str.Insert(ind[i], _T("\n</p>"));
	}
	str.Insert(str.GetLength(), _T("\n</p>"));
	str.Insert(0, _T("<p>　　\n"));// <p> is indented by unicode white space

	// deal with ranges that should not be in <p>...</p>
	// if there is "<p>" before range, delete it, otherwise, add "</p>"
	// if there is "</p>" after range, delete it, otherwise, add "<p>　　"
	FindComBrace(ind, _T("\\subsection"), str, 'o');
	FindComBrace(ind1, _T("\\subsubsection"), str, 'o');
	CombineRange(ind, ind, ind1);
	FindComBrace(ind1, _T("\\pentry"), str, 'o');
	CombineRange(ind, ind, ind1);
	for (i = ind.size() - 2; i >= 0; i -= 2) {
		ind0 = ExpectKey(str, _T("</p>"), ind[i + 1] + 1);
		if (ind0 >= 0) {
			str.Delete(ind0 - 4, 4);
		}
		else {
			str.Insert(ind[i + 1] + 1, _T("\n<p>　　")); ++N;
		}
		ind0 = ExpectKeyReverse(str, _T("<p>　　"), ind[i] - 1);
		ind2 = ExpectKeyReverse(str, _T("<p>"), ind[i] - 1);
		if (ind0 > -2) {
			str.Delete(ind0 + 1, 5); --N;
		}
		else if (ind2 > -2) {
			str.Delete(ind2 + 1, 3); --N;
		}
		else {
			str.Insert(ind[i], _T("</p>\n\n"));
		}
	}

	// deal with ranges that should not be in <p>...</p>
	// if there is "<p>" before range, delete it, otherwise, add "</p>"
	// if there is "</p>" after range, delete it, otherwise, add "<p>"
	FindEnv(ind, str, _T("figure"), 'o');
	FindEnv(ind1, str, _T("itemize"), 'o');
	CombineRange(ind, ind, ind1);
	FindEnv(ind1, str, _T("enumerate"), 'o');
	CombineRange(ind, ind, ind1);
	FindComBrace(ind1, _T("\\code"), str, 'o');
	CombineRange(ind, ind, ind1);
	FindComBrace(ind1, _T("\\Code"), str, 'o');
	CombineRange(ind, ind, ind1);
	for (i = ind.size() - 2; i >= 0; i -= 2) {
		ind0 = ExpectKey(str, _T("</p>"), ind[i + 1] + 1);
		if (ind0 >= 0) {
			str.Delete(ind0 - 4, 4);
		}
		else {
			str.Insert(ind[i + 1] + 1, _T("\n<p>")); ++N;
		}
		ind0 = ExpectKeyReverse(str, _T("<p>　　"), ind[i] - 1);
		ind2 = ExpectKeyReverse(str, _T("<p>"), ind[i] - 1);
		if (ind0 > -2) {
			str.Delete(ind0 + 1, 5); --N;
		}
		else if (ind2 > -2) {
			str.Delete(ind2 + 1, 3); --N;
		}
		else {
			str.Insert(ind[i], _T("</p>\n\n"));
		}
	}

	// deal with equation environments alike
	// if there is "</p>\n<p>　　" before range, delete it
	// if there is "<p>　　" before range, delete "　　"
	FindEnv(ind, str, _T("equation"), 'o');
	FindEnv(ind1, str, _T("gather"), 'o');
	CombineRange(ind, ind, ind1);
	FindEnv(ind1, str, _T("align"), 'o');
	CombineRange(ind, ind, ind1);
	for (i = ind.size() - 2; i >= 0; i -= 2) {
		ind0 = ExpectKeyReverse(str, _T("</p>\n<p>　　"), ind[i] - 1);
		ind2 = ExpectKeyReverse(str, _T("<p>　　"), ind[i] - 1);
		if (ind0 > -2) {
			str.Delete(ind0 + 1, 10); --N;
		}
		else if (ind2 > -2) {
			str.Delete(ind2 + 4, 2);
		}
	}
	// deal with \noindent
	ind0 = 0;
	while (true) {
		ind0 = str.Find(_T("\\noindent"), ind0);
		if (ind0 < 0) break;
		str.Delete(ind0, 9);
		ind2 = ExpectKeyReverse(str, _T("　　"), ind0 - 1);
		if (ind2 > -2)
			str.Delete(ind2 + 1, 2);
	}

	// deal with ranges that should not be in <p>...</p>
	// if there is "<p>" before \begin{}{}, delete it, otherwise, add "</p>"
	// if there is "</p>" after \begin{}{}, delete it, otherwise, add "<p>　　"
	// if there is "<p>" before \end{}, delete it, otherwise, add "</p>"
	// if there is "</p>" after \end{}, delete it, otherwise, add "<p>　　"
	FindBegin(ind, _T("exam"), str, '2');
	FindEnd(ind1, _T("exam"), str);
	CombineRange(ind, ind, ind1);
	FindBegin(ind1, _T("exer"), str, '2');
	CombineRange(ind, ind, ind1);
	FindEnd(ind1, _T("exer"), str);
	CombineRange(ind, ind, ind1);
	for (i = ind.size() - 2; i >= 0; i -= 2) {
		ind0 = ExpectKey(str, _T("</p>"), ind[i + 1] + 1);
		if (ind0 >= 0) {
			str.Delete(ind0 - 4, 4);
		}
		else {
			str.Insert(ind[i + 1] + 1, _T("\n<p>　　")); ++N;
		}
		ind0 = ExpectKeyReverse(str, _T("<p>　　"), ind[i] - 1);
		ind2 = ExpectKeyReverse(str, _T("<p>"), ind[i] - 1);
		if (ind0 > -2) {
			str.Delete(ind0 + 1, 5); --N;
		}
		else if (ind2 > -2) {
			str.Delete(ind2 + 1, 3); --N;
		}
		else {
			str.Insert(ind[i], _T("</p>\n\n"));
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
int EnvLabel(vector<CString>& id, vector<CString>& label, const CString& entryName, CString& str)
{
	int ind0{}, ind1{}, ind2{}, ind3{}, ind4{}, ind5{}, N{}, temp{}, Ngather{}, Nalign{}, i{}, j{};
	CString idName; // "eq" or "fig" or "ex"...
	CString envName; // "equation" or "figure" or "exam"...
	CString idNum{}; // id = idName + idNum
	int idN{}; // convert to idNum
	vector<int> indEnv;
	while (true) {
		ind5 = str.Find(_T("\\label"), ind0);
		if (ind5 < 0) return N;
		// make sure label is inside an environment
		ind2 = ReverseFind(_T("\\end"), str, ind5);
		ind4 = ReverseFind(_T("\\begin"), str, ind5);
		if (ind2 >= ind4) {
			wcout << "label not in an environment!";
			return -1;
		}
		// detect environment kind
		ind1 = ExpectKey(str, '{', ind4 + 6);
		if (ExpectKey(str, _T("equation"), ind1) > 0) {
			idName = _T("eq"); envName = _T("equation");
		}
		else if (ExpectKey(str, _T("figure"), ind1) > 0) {
			idName = _T("fig"); envName = _T("figure");
		}
		else if (ExpectKey(str, _T("exam"), ind1) > 0) {
			idName = _T("ex"); envName = _T("exam");
		}
		else if (ExpectKey(str, _T("exer"), ind1) > 0) {
			idName = _T("exe"); envName = _T("exer");
		}
		else if (ExpectKey(str, _T("table"), ind1) > 0) {
			idName = _T("tab"); envName = _T("table");
		}
		else if (ExpectKey(str, _T("gather"), ind1) > 0) {
			idName = _T("eq"); envName = _T("gather");
		}
		else if (ExpectKey(str, _T("align"), ind1) > 0) {
			idName = _T("eq"); envName = _T("align");
		}
		else {
			wcout << "environment not supported for label!"; return -1;
		}
		// check label format and save label
		ind0 = ExpectKey(str, '{', ind5 + 6);
		ind3 = ExpectKey(str, entryName + _T('_') + idName, ind0);
		if (ind3 < 0) {
			wcout << "label format error!"; return -1;
		}
		ind3 = str.Find('}', ind3);
		label.push_back(str.Mid(ind0, ind3 - ind0));
		label.back().TrimLeft(' '); label.back().TrimRight(' ');
		// count idNum, insert html id tag, delete label
		if (idName != _T("eq")) {
			idN = FindEnv(indEnv, str.Left(ind4), envName) + 1;
		}
		else { // count equations
			idN = FindEnv(indEnv, str.Left(ind4), _T("equation"));
			Ngather = FindEnv(indEnv, str.Left(ind4), _T("gather"));
			if (Ngather > 0) {
				for (i = 0; i < 2 * Ngather; i += 2) {
					for (j = indEnv[i]; j < indEnv[i + 1]; ++j) {
						if (str.GetAt(j) == '\\' && str.GetAt(j+1) == '\\')
							++idN;
					}
					++idN;
				}
			}
			Nalign = FindEnv(indEnv, str.Left(ind4), _T("align"));
			if (Nalign > 0) {
				for (i = 0; i < 2 * Nalign; i += 2) {
					for (j = indEnv[i]; j < indEnv[i + 1]; ++j) {
						if (str.GetAt(j) == '\\' && str.GetAt(j + 1) == '\\')
							++idN;
					}
					++idN;
				}
			}
			if (envName == _T("gather") || envName == _T("align")) {
				for (i = ind4; i < ind5; ++i) {
					if (str.GetAt(i) == '\\' && str.GetAt(i + 1) == '\\')
						++idN;
				}
			}
			++idN;
		}
		idNum.Format(_T("%d"), idN);
		id.push_back(idName + idNum);
		str.Delete(ind5, ind3 - ind5 + 1);
		str.Insert(ind4, _T("<span id = \"") + id.back() + _T("\"></span>"));
		ind0 = ind4 + 6;
	}
}

// replace \pentry comman with html round panel
int pentry(CString& str)
{
	int i{}, N{};
	vector<int> indIn, indOut;
	FindComBrace(indIn, _T("\\pentry"), str);
	N = FindComBrace(indOut, _T("\\pentry"), str, 'o');
	for (i = 2 * N - 2; i >= 0; i -= 2) {
		str.Delete(indOut[i + 1]);
		str.Insert(indOut[i + 1], _T("</div>"));
		str.Delete(indOut[i], indIn[i] - indOut[i]);
		str.Insert(indOut[i], _T("<div class = \"w3-panel w3-round-large w3-light-blue\"><b>预备知识</b>　"));
	}
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
int MatlabCode(CString& str, const CString& path)
{
	int i{}, N{}, ind0{}, ind1{}, ind2{};
	CString name; // code file name without extension
	CString code;
	vector<int> indIn, indOut;
	// \code commands
	FindComBrace(indIn, _T("\\code"), str);
	N = FindComBrace(indOut, _T("\\code"), str, 'o');
	for (i = 2 * N - 2; i >= 0; i -= 2) {
		// get code file name
		name = str.Mid(indIn[i], indIn[i + 1] - indIn[i] + 1);
		name.TrimLeft(' '); name.TrimRight(' ');
		// read file
		if (!FileExist(path + _T("codes\\"), name + _T(".html"))) {
			wcout << _T("code file not found!"); return -1;
		}
		code = ReadUTF8(path + _T("codes\\") + name + _T(".html"));
		ind0 = code.Find(_T("<pre"), 0);
		ind1 = code.Find(_T("</pre>"), ind0);
		code = code.Mid(ind0, ind1 - ind0 + 6);
		// insert code
		str.Delete(indOut[i], indOut[i + 1] - indOut[i] + 1);
		str.Insert(indOut[i], _T("</div></div>"));
		str.Insert(indOut[i], code);
		str.Insert(indOut[i], _T("<div class = \"nospace\">"));
		str.Insert(indOut[i], _T("<span class = \"icon\"><a href = \"") + name 
			+ _T(".m\" download> <i class = \"fa fa-caret-square-o-down\"></i></a></span>"));
		str.Insert(indOut[i], _T("<div class = \"w3-code notranslate w3-pale-yellow\">"));
	}
	return N;
}

// same with MatlabCode, for \Code command
int MatlabCodeTitle(CString& str, const CString& path)
{
	int i{}, N{}, ind0{}, ind1{}, ind2{};
	CString name; // code file name without extension
	CString code;
	vector<int> indIn, indOut;
	// \code commands
	FindComBrace(indIn, _T("\\Code"), str);
	N = FindComBrace(indOut, _T("\\Code"), str, 'o');
	for (i = 2 * N - 2; i >= 0; i -= 2) {
		// get code file name
		name = str.Mid(indIn[i], indIn[i + 1] - indIn[i] + 1);
		name.TrimLeft(' '); name.TrimRight(' ');
		// read file
		if (!FileExist(path + _T("codes\\"), name + _T(".html"))) {
			wcout << _T("code file not found!"); return -1;
		}
		code = ReadUTF8(path + _T("codes\\") + name + _T(".html"));
		ind0 = code.Find(_T("<pre"), 0);
		ind1 = code.Find(_T("</pre>"), ind0);
		code = code.Mid(ind0, ind1 - ind0 + 6);
		// insert code
		str.Delete(indOut[i], indOut[i + 1] - indOut[i] + 1);
		str.Insert(indOut[i], _T("</div></div>"));
		str.Insert(indOut[i], code);
		str.Insert(indOut[i], _T("<div class = \"w3-code notranslate w3-pale-yellow\">\n<div class = \"nospace\">"));
		// insert title with download link
		if (!FileExist(path, name + _T(".m"))) {
			wcout << ".m file not found!"; return -1;
		}
		str.Insert(indOut[i], _T("<b>") + name + _T(".m</b>\n"));
		str.Insert(indOut[i], _T("<span class = \"icon\"><a href = \"") + name 
					+ _T(".m\" download> <i class = \"fa fa-caret-square-o-down\"></i></a></span>"));
	}
	return N;
}

// replace autoref with html link
// no comment allowed
// does not add link for \autoref inside eq environment (equation, align, gather)
// return number of autoref replaced, or -1 if failed
int autoref(const vector<CString>& id, const vector<CString>& label, const CString& entryName, CString& str)
{
	unsigned i{};
	int ind0{}, ind1{}, ind2{}, ind3{}, ind4{}, ind5{}, N{}, Neq{}, ienv{};
	bool inEq;
	CString entry, label0, idName, idNum, kind, newtab, file;
	vector<CString> envNames{_T("equation"), _T("align"), _T("gather")};
	while (true) {
		newtab.Empty(); file.Empty();
		ind0 = str.Find(_T("\\autoref"), ind0);
		if (ind0 < 0) return N;
		inEq = IndexInEnv(ienv, ind0, envNames, str);
		ind1 = ExpectKey(str, '{', ind0 + 8);
		ind1 = NextNoSpace(entry, str, ind1);
		ind2 = str.Find('_', ind1);
		if (ind2 < 0) {
			wcout << _T("autoref format error!"); return -1;
		}
		ind3 = FindNum(str, ind2);
		idName = str.Mid(ind2 + 1, ind3 - ind2 - 1);
		if (idName == _T("eq")) kind = _T("式");
		else if (idName == _T("fig")) kind = _T("图");
		else if (idName == _T("ex")) kind = _T("例");
		else if (idName == _T("exe")) kind = _T("练习");
		else if (idName == _T("tab")) kind = _T("表");
		else {
			wcout << _T("unknown id name!"); return -1;
		}
		ind3 = str.Find('}', ind3);
		// find id of the label
		label0 = str.Mid(ind1, ind3 - ind1); label0.TrimLeft(' '); label0.TrimRight(' ');
		for (i = 0; i < label.size(); ++i) {
			if (label0 == label[i]) break;
		}
		if (i == label.size()) {
			wcout << _T("label not found!"); return -1;
		}
		ind4 = FindNum(id[i], 0);
		idNum = id[i].Right(id[i].GetLength() - ind4);		
		entry = str.Mid(ind1, ind2 - ind1);
		if (entry != entryName) {// reference the same entry
			newtab = _T("target = \"_blank\"");
			file = entry + _T(".html");
		}
		if (!inEq) str.Insert(ind3 + 1, _T(" </a>"));
		str.Insert(ind3 + 1, kind + _T(' ') + idNum);
		if (!inEq) str.Insert(ind3 + 1, _T("<a href = \"") + file + _T("#") + id[i] + _T("\" ") + newtab + _T(">"));
		str.Delete(ind0, ind3 - ind0 + 1);
		++N;
	}
}

// process upref
// path must end with '\\'
int upref(CString& str, CString path)
{
	int i{}, N{};
	vector<int> indIn, indOut;
	CString entryName;
	FindComBrace(indIn, _T("\\upref"), str);
	N = FindComBrace(indOut, _T("\\upref"), str, 'o');
	for (i = 2 * N - 2; i >= 0; i -= 2) {
		entryName = str.Mid(indIn[i], indIn[i + 1] - indIn[i] + 1);
		entryName.TrimLeft(' '); entryName.TrimRight(' ');
		if (!FileExist(path, entryName + _T(".tex"))) {
			wcout << _T("file not found!"); return -1;
		}
		str.Delete(indOut[i], indOut[i + 1] - indOut[i] + 1);
		str.Insert(indOut[i], _T("<span class = \"icon\"><a href = \"") + entryName + _T(".html\" target = \"_blank\"><i class = \"fa fa-external-link\"></i></a></span>"));
	}
}

// replace figure environment with html image
// convert vector graph to SVG, font must set to "convert to outline"
// if svg image doesn't exist, use png, if doesn't exist, return -1
// path must end with '\\'
int FigureEnvironment(CString& str, const CString& path)
{
	int i{}, N{}, Nfig{}, ind0{}, ind1{}, indName1{}, indName2{};
	double width{}; // figure width in cm
	vector<int> indFig{};
	CString figName, format, caption, widthPt, figNo;
	Nfig = FindEnv(indFig, str, _T("figure"), 'o');
	for (i = 2*Nfig - 2; i >= 0; i-=2) {
		// get width of figure
		ind0 = str.Find(_T("width"), indFig[i]) + 5;
		ind0 = ExpectKey(str, '=', ind0);
		ind0 = FindNum(str, ind0);
		CString2double(width, str, ind0);
		// get file name of figure
		indName1 = str.Find(_T("figures/"), ind0) + 8;
		indName2 = str.Find(_T(".pdf"), indFig[i]) - 1;
		figName = str.Mid(indName1, indName2 - indName1 + 1);
		// get caption of figure
		ind0 = str.Find(_T("\\caption"), ind0);
		if (ind0 < 0) {
			wcout << _T("fig caption not found!"); return -1;
		}
		ind0 = ExpectKey(str, '{', ind0 + 8);
		ind1 = PairBraceR(str, ind0);
		caption = str.Mid(ind0, ind1 - ind0);
		str.Delete(indFig[i], indFig[i + 1] - indFig[i] + 1); // delete environment
		ind0 = ExpectKey(str, _T("\n</p>"), indFig[i]);
		// test img file existence
		if (FileExist(path, figName + _T(".svg")))
			format = _T(".svg");
		else if (FileExist(path, figName + _T(".png")))
			format = _T(".png");
		else{
			wcout << _T("figure not found!"); return -1;
		}
		// insert html code
		widthPt.Format(_T("%d"), (int)(33.4 * width));
		figNo.Format(_T("%d"), i/2 + 1);
		ind0 = Insert(str, _T("<div class = \"w3-content\" style = \"max-width:") + widthPt
			+ _T("pt;\">\n") + _T("<img src = \"") + figName + format
			+ _T("\" alt = \"图\" style = \"width:100%;\">\n</div>\n<div align = \"center\"> 图") + figNo
			+ _T("：") + caption + _T("</div>"), indFig[i]);
		++N;
	}
	return N;
}

// example is already not in a paragraph
// return number of examples processed, return -1 if failed
int ExampleEnvironment(CString& str, const CString& path0)
{
	int i{}, N{}, ind0{}, ind1{};
	vector<int> indIn, indOut;
	CString exName, exNo;
	FindEnv(indIn, str, _T("exam"));
	N = FindEnv(indOut, str, _T("exam"), 'o');
	for (i = 2 * N - 2; i >= 0; i -= 2) {
		ind0 = str.Find('{', indOut[i]);
		ind0 = PairBraceR(str, ind0);
		ind0 = ExpectKey(str, '{', ind0 + 1);
		ind1 = PairBraceR(str, ind0 - 1);
		exName = str.Mid(ind0, ind1 - ind0);
		// replace with html tags
		str.Delete(indIn[i + 1] + 1, indOut[i + 1] - indIn[i + 1]);
		str.Insert(indIn[i + 1] + 1, _T("</div>\n"));
		str.Delete(indOut[i], ind1 - indOut[i] + 1);
		exNo.Format(_T("%d"), i/2 + 1);
		str.Insert(indOut[i], _T("<div class = \"w3-panel w3-border-yellow w3-leftbar\">\n <h5><b>例")
				+ exNo + _T("</b>　") + exName + _T("</h5>"));
	}
	return N;
}

// example is already not in a paragraph
// return number of examples processed, return -1 if failed
int ExerciseEnvironment(CString& str, const CString& path0)
{
	int i{}, N{}, ind0{}, ind1{};
	vector<int> indIn, indOut;
	CString exName, exNo;
	FindEnv(indIn, str, _T("exer"));
	N = FindEnv(indOut, str, _T("exer"), 'o');
	for (i = 2 * N - 2; i >= 0; i -= 2) {
		ind0 = str.Find('{', indOut[i]);
		ind0 = PairBraceR(str, ind0);
		ind0 = ExpectKey(str, '{', ind0 + 1);
		ind1 = PairBraceR(str, ind0 - 1);
		exName = str.Mid(ind0, ind1 - ind0);
		// replace with html tags
		str.Delete(indIn[i + 1] + 1, indOut[i + 1] - indIn[i + 1]);
		str.Insert(indIn[i + 1] + 1, _T("</div>\n"));
		str.Delete(indOut[i], ind1 - indOut[i] + 1);
		exNo.Format(_T("%d"), i / 2 + 1);
		str.Insert(indOut[i], _T("<div class = \"w3-panel w3-border-green w3-leftbar\">\n <h5><b>习题")
			+ exNo + _T("</b>　") + exName + _T("</h5>"));
	}
	return N;
}

// process \Command{} to display Matlab Code
// TODO: generate one .m file to be processed by "highlight()" function in Matlab
// return the number of \Command{} processed, return -1 if failed
int Command(CString& str)
{
	int i{}, j{}, N{}, ind0{}, ind1{}, Ncolor;
	vector<int> ind, indColor;
	CString temp;
	N = FindEnv(ind, str, _T("Command"));
	for (i = 2 * N - 2; i >= 0; i -= 2) {
		temp = str.Mid(ind[i], ind[i + 1] - ind[i] + 1);
		temp.Replace(_T("\\\\"), _T(""));
		// deal with color
		Ncolor = FindComBrace(indColor, _T("\\color"), temp, 'o');
		for (j = 2 * Ncolor - 2; j >= 0; j -= 2) {
			temp.Delete(indColor[j], indColor[j + 1] - indColor[j] + 1);
			ind0 = ExpectKeyReverse(temp, '{', indColor[j] - 1) + 1;
			ind1 = PairBraceR(temp, ind0);
			temp.Delete(ind1); temp.Insert(ind1, _T("</span>"));
			temp.Delete(ind0); temp.Insert(ind0, _T("<span class = \"string\">"));
		}
		// process \par
		while (true) {
			ind0 = temp.Find(_T("\\par"));
			if (ind0 < 0) break;
			temp.Delete(ind0, 4);
			DeleteSpaceReturn(temp, ind0);
			temp.Insert(ind0, _T("\n　　"));
		}
		
		str.Delete(ind[i], ind[i + 1] - ind[i] + 1);
		str.Insert(ind[i], temp);
	}
	Env2Tag(_T("Command"), _T("<div class = \"w3-code notranslate w3-pale-yellow\">\n<div class = \"nospace\"><pre class = \"mcode\">"),
		_T("</pre></div></div>"), str);
	return N;
}

// remove extra {} from equation environment and rewrite file
// return total {} deleted
int OneFile1(CString path)
{
	CString str = ReadUTF8(path); // read file
	// get all the equation environment scopes
	vector<int> eqscope;
	if (FindEnv(eqscope, str, _T("equation")) < 0)
	{
		wcout << L"error!"; return 0;
	}
	// match  and remove braces
	vector<int> ind_left, ind_right, ind_RmatchL;
	int N{}; // total {} removed
	for (int i = eqscope.size() - 1; i > 0; i -= 2)
	{
		int Npair = MatchBraces(ind_left, ind_right, ind_RmatchL,
			str, eqscope[i - 1], eqscope[i]);
		if (Npair > 0)
			N += RemoveBraces(ind_left, ind_right, ind_RmatchL, str);
	}
	if (N > 0) {
		WriteUTF8(str, path); // write file
	}
	return N;
}

// remove extra {} from $$ environment and rewrite file
// return total number of {} pairs removed
int OneFile2(CString path)
{
	CString str = ReadUTF8(path); // read file

	// get all the equation environment scopes
	vector<int> indEq;
	if (FindEnv(indEq, str, _T("equation")) < 0) {
		wcout << L"error!"; return 0;
	}
	vector<int> ind; // indices for all the $.
	if (FindInline(ind, str) == 0)
		return 0;

	// match  and remove braces
	vector<int> ind_left, ind_right, ind_RmatchL;
	int Npair{}; // total {} pairs found
	int N{}; // total {} removed
	for (int i = ind.size() - 1; i > 0; i -= 2)
	{
		int Npair = MatchBraces(ind_left, ind_right, ind_RmatchL,
			str, ind[i - 1], ind[i]);
		if (Npair > 0)
			N += RemoveBraces(ind_left, ind_right, ind_RmatchL, str);
	}
	// write file
	if (N > 0)
		WriteUTF8(str, path);
	return N;
}

// remove any nonchinese punctuations from normal text
int OneFile3(CString path)
{
	int NNorm, N{};
	vector<int> indNorm;
	CString str = ReadUTF8(path); // read file

	NNorm = FindNormalText(indNorm, str);

	// test the result of indNorm
	/*for (int i{ NNorm*2-1 }; i >= 0; i -= 2)
	{
		str.Insert(indNorm[i] + 1, _T("正文结束"));
		str.Insert(indNorm[i-1], _T("正文开始"));
		++N;
	}*/

	// search nonchinese punctuations
	TCHAR c;
	for (int i{ NNorm * 2 - 1 }; i >= 0; i -= 2)
	{
		for (int j{ indNorm[i] }; j >= indNorm[i - 1]; --j)
		{
			c = str[j];
			if (c == '(' || c == ')' || c == ',' || c == '.' || c == '?' || c == ':' || c == '!')
				{ str.Insert(j, _T("删除标记")); ++N; }
		}
	}

	if (N > 0)
		WriteUTF8(str, path);
	return N;
}

// find \bra{}\ket{} and mark
int OneFile4(CString path)
{
	int ind0{}, ind1{}, N{};
	CString str = ReadUTF8(path); // read file
	while (true) {
		ind0 = str.Find(_T("\\bra"), ind0);
		if (ind0 < 0) break;
		ind1 = ind0 + 4;
		ind1 = ExpectKey(str, '{', ind1);
		if (ind1 < 0) {
			++ind0; continue;
		}
		ind1 = PairBraceR(str, ind1 - 1);
		ind1 = ExpectKey(str, _T("\\ket"), ind1 + 1);
		if (ind1 > 0) {
			str.Insert(ind0, _T("删除标记")); ++N;
			ind0 = ind1 + 4;
		}
		else
			++ind0;
	}
	if (N > 0)
		WriteUTF8(str, path);
	return N;
}

// remove special .tex files from a list of name
// return number of names removed
// names has ".tex" extension
int RemoveNoEntry(vector<CString>& names)
{
	int i{}, j{}, N{}, Nnames{}, Nnames0;
	vector<CString> names0; // names to remove
	names0.push_back(_T("PhysWiki1"));
	names0.push_back(_T("PhysWiki"));
	names0.push_back(_T("Sample"));
	// add other names here
	Nnames = names.size();
	Nnames0 = names0.size();
	for (i = 0; i < Nnames0; ++i) {
		names0[i] = names0[i];
	}
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

// create table of content from PhysWiki1.tex
// path must end with '\\'
// return the number of entries
int TableOfContent(const CString& path)
{
	int i{}, N{}, ind0{}, ind1{}, ind2{}, ikey{}, chapNo{ -1 }, partNo{ -1 };
	vector<CString> keys{ _T("\\part"), _T("\\chapter"), _T("\\entry"), _T("\\Entry"), _T("\\laserdog")};
	vector<CString> chineseNo{_T("一"), _T("二"), _T("三"), _T("四"), _T("五"), _T("六"), _T("七"), _T("八"), _T("九"),
							_T("十"), _T("十一"), _T("十二"), _T("十三"), _T("十四"), _T("十五") };
	//keys.push_back(_T("\\entry")); keys.push_back(_T("\\chapter")); keys.push_back(_T("\\part"));
	CString title; // chinese entry name, chapter name, or part name
	CString entryName; // entry label
	CString str = ReadUTF8(path + _T("PhysWiki.tex"));
	CString toc = ReadUTF8(_T("template.html")); // read html template
	ind0 = toc.Find(_T("PhysWikiHTMLtitle"));
	toc.Delete(ind0, 17);
	toc.Insert(ind0, _T("《小时物理百科》目录"));
	ind0 = toc.Find(_T("PhysWikiCommand"), 0);
	toc.Delete(ind0, 15);
	ind0 = toc.Find(_T("PhysWikiHTMLbody"), 0);
	toc.Delete(ind0, 16);
	ind0 = Insert(toc, _T("<h1>《小时物理百科》目录</h1>\n\n"), ind0);
	ind0 = Insert(toc, _T("<p>\n<span class=\"w3-tag w3-yellow\">公告：《小时物理百科》网页版仍在开发中，请下载<a href=\"../\">《小时物理百科》PDF 版</a>。</span>\n</p><hr>\n\n"), ind0);
	// remove comments
	vector<int> indComm;
	N = FindComment(indComm, str);
	for (i = 2 * N - 2; i >= 0; i -= 2) {
		str.Delete(indComm[i], indComm[i + 1] - indComm[i] + 1);
	}
	while (true) {
		ind1 = FindMultiple(ikey, str, keys, ind1);
		if (ind1 < 0) break;
		if (ikey >= 2) { // found "\entry"
			// get chinese title and entry label
			if (ikey == 2 || ikey == 3) ind1 += 6;
			else if (ikey == 4) ind1 += 9;
			++N;
			ind1 = ExpectKey(str, '{', ind1);
			ind2 = str.Find('}', ind1);
			title = str.Mid(ind1, ind2 - ind1);
			title.TrimLeft(' '); title.TrimRight(' ');
			title.Replace(_T("\\ "), _T(" "));
			ind1 = ExpectKey(str, '{', ind2 + 1);
			ind2 = str.Find('}', ind1);
			entryName = str.Mid(ind1, ind2 - ind1);
			ind1 = ind2;
			// insert entry into html table of contents
			ind0 = Insert(toc, _T("<a href = \"") + entryName + _T(".html") + _T("\" target = \"_blank\">")
				+ title + _T("</a>　\n"), ind0);
		}
		else if (ikey == 1) { // found "\chapter"
			// get chinese chapter name
			ind1 += 8;
			ind1 = ExpectKey(str, '{', ind1);
			ind2 = str.Find('}', ind1);
			title = str.Mid(ind1, ind2 - ind1);
			title.TrimLeft(' '); title.TrimRight(' ');
			title.Replace(_T("\\ "), _T(" "));
			ind1 = ind2;
			// insert chapter into html table of contents
			++chapNo;
			if (chapNo > 0)
				ind0 = Insert(toc, _T("</p>"), ind0);
			ind0 = Insert(toc, _T("\n\n<h5><b>第") + chineseNo[chapNo] + _T("章 ") + title
				+ _T("</b></h5>\n<div class = \"tochr\"></div><hr><div class = \"tochr\"></div>\n<p>\n"), ind0);
		}
		else if (ikey == 0){ // found "\part"
			// get chinese part name
			ind1 += 5; chapNo = -1;
			ind1 = ExpectKey(str, '{', ind1);
			ind2 = str.Find('}', ind1);
			title = str.Mid(ind1, ind2 - ind1);
			title.TrimLeft(' '); title.TrimRight(' ');
			title.Replace(_T("\\ "), _T(" "));
			ind1 = ind2;
			// insert part into html table of contents
			++partNo;
			if (partNo > 0)
				ind0 = Insert(toc, _T("</p><p>　</p>\n"), ind0);
			ind0 = Insert(toc, _T("<h3 align = \"center\">第") + chineseNo[partNo] + _T("部分 ")
				+ title + _T("</h3>\n"), ind0);
		}
	}
	toc.Insert(ind0, _T("</p>"));
	WriteUTF8(toc, path + _T("index.html"));
	return N;
}


// generate html from tex
// output the chinese title of the file, id-label pairs in the file
// return 0 if successful, -1 if failed
// entryName does not include ".tex"
// path0 is the parent folder of entryName.tex, ending with '\\'
int PhysWikiOnline1(CString& title, vector<CString>& id, vector<CString>& label, CString entryName, CString path0)
{
	// read template file from local folder
	int i{}, N{}, ind0;
	CString str = ReadUTF8(path0 + entryName + _T(".tex")); // read tex file
	//// read template from the same folder as path
	//ind0 = path.ReverseFind('\\');
	//path.Delete(ind0, str.GetLength() - ind0);
	//path += _T("\\template.html");
	CString html = ReadUTF8(_T("template.html")); // read html template
	CString newcomm = ReadUTF8(_T("newcommand.html"));
	if (GetTitle(title, str) < 0) return -1;
	// insert \newcommand
	ind0 = html.Find(_T("PhysWikiCommand"), 0);
	html.Delete(ind0, 15);
	html.Insert(ind0, newcomm);
	// insert HTML title
	ind0 = html.Find(_T("PhysWikiHTMLtitle"), 0);
	html.Delete(ind0, 17);
	html.Insert(ind0, title);
	// remove comments
	vector<int> indComm;
	N = FindComment(indComm, str);
	for (i = 2 * N - 2; i >= 0; i -= 2) {
		str.Delete(indComm[i], indComm[i + 1] - indComm[i] + 1);
	}
	// escape characters
	NormalTextEscape(str);
	// add paragraph tags
	ParagraphTag(str);
	// itemize and enumerate
	Itemize(str); Enumerate(str);
	// add html id for links
	EnvLabel(id, label, entryName, str);
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
	StarCommand(_T("comm"), str);   StarCommand(_T("pb"), str);
	StarCommand(_T("dv"), str);     StarCommand(_T("pdv"), str);
	StarCommand(_T("bra"), str);    StarCommand(_T("ket"), str);
	StarCommand(_T("braket"), str); StarCommand(_T("ev"), str);
	StarCommand(_T("mel"), str);
	// Replace \nameTwo and \nameThree commands
	VarCommand(_T("pdv"), str, 3);   VarCommand(_T("pdvStar"), str, 3);
	VarCommand(_T("dv"), str, 2);    VarCommand(_T("dvStar"), str, 2);
	VarCommand(_T("ev"), str, 2);    VarCommand(_T("evStar"), str, 2);
	// replace \name() and \name[] with \nameRound{} and \nameRound and \nameSquare
	RoundSquareCommand(_T("qty"), str);
	// replace \namd() and \name[]() with \nameRound{} and \nameRound[]{}
	MathFunction(_T("sin"), str);    MathFunction(_T("cos"), str);
	MathFunction(_T("tan"), str);    MathFunction(_T("csc"), str);
	MathFunction(_T("sec"), str);    MathFunction(_T("cot"), str);
	MathFunction(_T("sinh"), str);   MathFunction(_T("cosh"), str);
	MathFunction(_T("tanh"), str);   MathFunction(_T("arcsin"), str);
	MathFunction(_T("arccos"), str); MathFunction(_T("arctan"), str);
	MathFunction(_T("exp"), str);    MathFunction(_T("log"), str);
	MathFunction(_T("ln"), str);
	// replace \name{} with html tags
	Command2Tag(_T("subsection"), _T("<h5 class = \"w3-text-indigo\"><b>"), _T("</b></h5>"), str);
	Command2Tag(_T("subsubsection"), _T("<h5><b>"), _T("</b></h5>"), str);
	Command2Tag(_T("bb"), _T("<b>"), _T("</b>"), str);
	// replace \upref{} with link icon
	upref(str, path0);
	// replace environments with html tags
	Env2Tag(_T("equation"), _T("<div class=\"eq\"><div class = \"w3-cell\" style = \"width:710px\">\n\\begin{equation}"),
						_T("\\end{equation}\n</div></div>"), str);
	Env2Tag(_T("gather"), _T("<div class=\"eq\"><div class = \"w3-cell\" style = \"width:710px\">\n\\begin{gather}"),
		_T("\\end{gather}\n</div></div>"), str);
	Env2Tag(_T("align"), _T("<div class=\"eq\"><div class = \"w3-cell\" style = \"width:710px\">\n\\begin{align}"),
		_T("\\end{align}\n</div></div>"), str);
	// Matlab code
	MatlabCode(str, path0); MatlabCodeTitle(str, path0);
	Command(str);
	Command2Tag('x', _T("<span class = \"w3-text-teal\">"), _T("</span>"), str);
	// footnote
	footnote(str);
	// insert HTML body
	ind0 = html.Find(_T("PhysWikiHTMLbody"), ind0);
	html.Delete(ind0, 16);
	html.Insert(ind0, str);
	// insert notice
	html.Insert(ind0, _T("<p>\n<span class=\"w3-tag w3-yellow\">公告：《小时物理百科》网页版仍在开发中，请下载<a href=\"../\">《小时物理百科》PDF 版</a>。</span>\n</p>\n"));
	html.Insert(ind0, _T("<h1>") + title + _T("</h1><hr>\n")); // insert title

	// save html file
	WriteUTF8(html, path0 + entryName + _T(".html"));
	return 0;
}

// the main file
void PhysWikiOnline()
{
	int ind0{};
	CString title; // the Chinese title of an entry
	// set path
	//CString path0 = _T("C:\\Users\\addis\\Documents\\GitHub\\PhysWiki\\contents\\");
	//CString path0 = _T("C:\\Users\\addis\\Desktop\\");
	CString path0 = _T("C:\\Users\\addis\\Documents\\GitHub\\littleshi.cn\\PhysWikiOnline\\");
	vector<CString> names = GetFileNames(path0, _T("tex"), false);
	RemoveNoEntry(names);
	if (names.size() <= 0) return;
	//names.resize(0); names.push_back(_T("ITable")); // debug
	TableOfContent(path0);
	vector<CString> IdList, LabelList; // html id and corresponding tex label
	// 1st loop through tex files
	for (unsigned i{}; i < names.size(); ++i) {
		wcout << i << " ";
		wcout << names[i].GetString() << _T("...");
		// main process
		PhysWikiOnline1(title, IdList, LabelList, names[i], path0);
		wcout << endl;
	}

	// 2nd loop through tex files
	wcout << _T("\n\n\n\n\n") << endl << _T("2nd Loop:") << endl;
	CString html;
	for (unsigned i{}; i < names.size(); ++i) {
		wcout << i << ' ' << names[i].GetString() << _T("...");
		html = ReadUTF8(path0 + names[i] + _T(".html")); // read html file
		// process \autoref and \upref
		autoref(IdList, LabelList, names[i], html);
		WriteUTF8(html, path0 + names[i] + _T(".html")); // save html file
		wcout << endl;
	}
}

void PhysWikiCheck()
{
	int ind0{};
	CString path0 = _T("C:\\Users\\addis\\Documents\\GitHub\\PhysWiki\\contents1\\");
	//CString path0 = _T("C:\\Users\\addis\\Documents\\GitHub\\littleshi.cn\\root\\PhysWiki\\online\\");
	vector<CString> names = GetFileNames(path0, _T("tex"), false);
	//RemoveNoEntry(names);
	if (names.size() <= 0) return;
	//names.resize(0); names.push_back(_T("Sample"));
	for (unsigned i{}; i < names.size(); ++i) {
		wcout << i << " ";
		wcout << names[i].GetString() << _T("...");
		// main process
		wcout << OneFile4(path0 + names[i] + _T(".tex"));
		wcout << endl;
	}
}

void main()
{
	_setmode(_fileno(stdout), _O_U16TEXT); // for console unicode output
	PhysWikiOnline();
	//PhysWikiCheck();
}
