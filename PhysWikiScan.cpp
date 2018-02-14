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

// add html id tag before environment if it contains a label, right before "\begin{}" and delete that label
// output html id and corresponding label for future reference
// "gather" and "align" environments has id = "ga" and "ali"
// idNum is in the idNum-th environment of the same name (not necessarily equal to displayed number)
// no comment allowed
// return number of labels processed, or -1 if failed
int EnvLabel(vector<CString>& id, vector<CString>& label, const CString& entryName, CString& str)
{
	int ind0{}, ind1{}, ind2{}, ind3{}, ind4{}, ind5{}, N{}, temp{}, Ngather{}, i{}, j{};
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
			idN += FindEnv(indEnv, str.Left(ind4), _T("align"));
			Ngather = FindEnv(indEnv, str.Left(ind4), _T("gather"));
			if (Ngather > 0) {
				for (i = 0; i < 2 * Ngather; i += 2) {
					for (j = indEnv[i]; j < indEnv[i + 1]; ++j) {
						if (str.GetAt(j) == '\\')
							++idN;
					}
				}
			}
			if (envName == _T("gather")) {
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
		ind0 = str.Find(str, ind4) + 6;
	}
}

// replace \pentry comman with html round panel
int pentry(CString& str)
{
	bool moveP{ false };
	int i{}, N{}, ind0;
	vector<int> indIn, indOut;
	FindComBrace(indIn, _T("\\pentry"), str);
	N = FindComBrace(indOut, _T("\\pentry"), str, 'o');
	for (i = 2 * N - 2; i >= 0; i -= 2) {
		// if there is <p> fllowed by two unicode spaces before, move it behind
		ind0 = ExpectKeyReverse(str, _T("<p>　　\n"), indOut[i] - 1) + 1;
		if (ind0 >= 0) {
			moveP = true;
			str.Insert(indOut[i + 1] + 1, _T("\n<p>　　"));
		}
		str.Delete(indOut[i + 1]);
		str.Insert(indOut[i + 1], _T("</div>"));
		str.Delete(indOut[i], indIn[i] - indOut[i]);
		str.Insert(indOut[i], _T("<div class = \"w3-panel w3-round-large w3-light-blue\"><b>预备知识</b>　"));
		if (moveP) {
			str.Delete(ind0, indOut[i] - ind0);
		}
	}
	return N;
}

// replace autoref with html link
// no comment allowed
// return number of autoref replaced, or -1 if failed
int autoref(const vector<CString>& id, const vector<CString>& label, const CString& entryName, CString& str)
{
	int i{}, ind0{}, ind1{}, ind2{}, ind3{}, ind4{}, ind5{}, N{};
	CString entry, label0, idName, idNum, kind, X, newtab, file;
	while (true) {
		X.Empty(); newtab.Empty(); file.Empty();
		ind0 = str.Find(_T("\\autoref"), ind0);
		if (ind0 < 0) return N;
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
		// deal with \upref after \label
		ind5 = ExpectKey(str, _T("\\upref"), ind3 + 1);
		if (ind5 > 0) {
			X = 'X';
			ind5 = str.Find('}', ind5);
			str.Delete(ind3 + 1, ind5 - ind3);
		}
		entry = str.Mid(ind1, ind2 - ind1);
		if (entry != entryName) {// reference the same entry
			newtab = _T("target = \"_blank\"");
			file = entry + _T(".html");
		}
		str.Insert(ind3 + 1, kind + _T(' ') + X + idNum + _T(" </a>"));
		str.Insert(ind3 + 1, _T("<a href = \"") + file + _T("#") + id[i] + _T("\" ") + newtab + _T(">"));
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
	int i{}, N{}, Nfig{}, ind0{}, indName1{}, indName2{};
	vector<int> indFig{};
	CString figName, format;
	Nfig = FindEnv(indFig, str, _T("figure"), 'o');
	for (i = 2*Nfig - 2; i >= 0; i-=2) {
		indName1 = str.Find(_T("figures/"), indFig[i]) + 8;
		indName2 = str.Find(_T(".pdf"), indFig[i]) - 1;
		figName = str.Mid(indName1, indName2 - indName1 + 1);
		// TODO: get and set figure size and 45pt per cm
		str.Delete(indFig[i], indFig[i + 1] - indFig[i] + 1);
		// insert html code
		//TODO: set figure size in pt
		// test img file existence
		if (FileExist(path, figName + _T(".svg")))
			format = _T(".svg");
		else if (FileExist(path, figName + _T(".png")))
			format = _T(".png");
		else{
			wcout << _T("figure not found!"); return -1;
		}
			
		str.Insert(indFig[i], _T("<div class = \"w3 - cell\" style = \"width:300px\">\n"));
		ind0 = str.Find(_T("\n"), indFig[i]) + 1;
		str.Insert(ind0, _T("<img src = \"")); ind0 += 12;
		str.Insert(ind0, figName); ind0 += figName.GetLength();
		str.Insert(ind0, format + _T("\" alt=\"图\" style=\"width:100% ;\">\n</div>"));
		++N;
	}
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
		//wcout << str.GetString() << endl;
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
	if (FindEnv(indEq, str, _T("equation")) < 0)
	{
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
	// add html id for links
	EnvLabel(id, label, entryName, str);
	// process figure environment
	FigureEnvironment(str, path0);
	// ensure '<' and '>' has spaces around them
	EqOmitTag(str);
	// add paragraph tags
	ParagraphTag(str);
	// process \pentry{}
	wcout << pentry(str);
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
	Command2Tag(_T("subsection"), _T("</p><h4>"), _T("</h4><hr>\n<p>　　"), str); // <p> is indented by unicode white space
	Command2Tag(_T("bb"), _T("<b>"), _T("</b>"), str);
	// replace \upref{} with "[x]" link
	upref(str, path0);
	// replace environments with html tags
	Env2Tag(_T("exam"), _T("</p><h5> 例：</h5>\n<p>"), _T(""), str);
	Env2Tag(_T("equation"), _T("<div class=\"eq\"><div class = \"w3-cell\" style = \"width:730px\">\n\\begin{equation}"), _T("\\end{equation}\n</div></div>"), str);
	// insert HTML body
	ind0 = html.Find(_T("PhysWikiHTMLbody"), ind0);
	html.Delete(ind0, 16);
	
	html.Insert(ind0, str);// insert notice
	html.Insert(ind0, _T("<p><span class=\"w3-tag w3-yellow\">公告：《小时物理百科》网页版仍在开发中，请下载<a href=\"../\">《小时物理百科》PDF 版</a>。</span></p>\n"));
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
	CString path0 = _T("C:\\Users\\addis\\Documents\\GitHub\\littleshi.cn\\root\\PhysWiki\\online\\");
	vector<CString> names = GetFileNames(path0, _T("tex"));
	if (names.size() < 0) return;

	vector<CString> IdList, LabelList; // html id and corresponding tex label
	// table of content
	CString toc = ReadUTF8(_T("template.html")); // read html template
	ind0 = toc.Find(_T("PhysWikiCommand"), 0);
	toc.Delete(ind0, 15);
	ind0 = toc.Find(_T("PhysWikiHTMLbody"), 0); --ind0;
	toc.Insert(ind0, _T("<h1>《小时物理百科》网页版目录</h1>\n\n"));
	ind0 = toc.Find(_T("PhysWikiHTMLbody"), ind0); --ind0;
	toc.Insert(ind0, _T("<p><span class=\"w3-tag w3-yellow\">公告：《小时物理百科》网页版仍在开发中，请下载<a href=\"../\">《小时物理百科》PDF 版</a>。</span></p><hr>\n\n<p>\n"));
	ind0 = toc.Find(_T("PhysWikiHTMLbody"), ind0); --ind0;
	// 1st loop through tex files
	for (unsigned i{}; i < names.size(); ++i) {
		wcout << i << " ";
		wcout << names[i].GetString() << _T("...");
		names[i].Delete(names[i].GetLength() - 4, 4);
		// main process
		PhysWikiOnline1(title, IdList, LabelList, names[i], path0);
		wcout << endl;
		// add to table of content
		toc.Insert(ind0, _T("<a href=\""));// insert notice
		ind0 = toc.Find(_T("PhysWikiHTMLbody"), ind0); --ind0;
		toc.Insert(ind0, names[i] + _T(".html\" target = \"_blank\""));
		ind0 = toc.Find(_T("PhysWikiHTMLbody"), ind0); --ind0;
		toc.Insert(ind0, _T("\">") + title + _T("</a><br>\n"));
		ind0 = toc.Find(_T("PhysWikiHTMLbody"), ind0); --ind0;
		//wcout << toc.GetString() << _T("\n\n\n\n\n") << endl;
	}
	toc.Insert(ind0, _T("</p>"));
	++ind0; toc.Delete(ind0, 16);
	WriteUTF8(toc, path0 + _T("index.html"));

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
	// insert OneFile1 etc.
}

void main()
{
	_setmode(_fileno(stdout), _O_U16TEXT); // for console unicode output
	PhysWikiOnline();
	// PhysWikiCheck
}
