// convert tex code to html code
// must use template.html and newcommand.html in the project folder
// designed to support physics package in tex

#include "tex2html.h"
using namespace std;

// ensure space around (CString)name
// return number of spaces added
// will only check equation env. and inline equations
int EnsureSpace(CString name, CString& str, int start, int end)
{
	int N{}, ind0{ start };
	while (true) {
		ind0 = str.Find(name, ind0);
		if (ind0 < 0 || ind0 > end) break;
		if (ind0 == 0 || str.GetAt(ind0 - 1) != ' ') {
			str.Insert(ind0, ' '); ++ind0; ++N;
		}
		ind0 += name.GetLength();
		if (ind0 == str.GetLength() || str.GetAt(ind0) != ' ') {
			str.Insert(ind0, ' '); ++N;
		}
	}
	return N;
}

// ensure space around '<' and '>' in equation env. and $$ env
// return number of spaces added
int EqOmitTag(CString& str)
{
	int i{}, N{}, Nrange{};
	vector<int> ind, indInline;
	FindEnv(ind, str, _T("equation"));
	FindInline(indInline, str);
	Nrange = CombineRange(ind, ind, indInline);
	for (i = 2 * Nrange - 2; i >= 0; i -= 2) {
		N += EnsureSpace('<', str, ind[i], ind[i + 1]);
		N += EnsureSpace('>', str, ind[i], ind[i + 1]);
	}
	return N;
}

// detect \name* command and replace with \nameStar
// return number of commmands replaced
// must remove comments first
int StarCommand(CString name, CString& str)
{
	int N{}, ind0{}, ind1{};
	while (true) {
		ind0 = str.Find(_T("\\") + name, ind0 + 1);
		if (ind0 < 0) break;
		ind0 += name.GetLength() + 1;
		ind1 = ExpectKey(str, _T("*"), ind0);
		if (ind1 < 0) continue;
		str.Delete(ind0, ind1 - ind0); // delete * and spaces
		str.Insert(ind0, _T("Star"));
		++N;
	}
	return N;
}

// detect \name{} variable parameter parameters
// replace \name{}{} with \nameTwo and \name{}{}{} with \nameThree
// return number of commmands replaced
// maxVars  = 2 or 3
// must remove comments first
int VarCommand(CString name, CString& str, int maxVars)
{
	int i{}, N{}, Nvar{}, ind0{}, ind1{};

	while (true) {
		ind0 = str.Find(_T("\\") + name, ind0 + 1);
		if (ind0 < 0) break;
		ind0 += name.GetLength() + 1;
		ind1 = ExpectKey(str, _T("{"), ind0);
		if (ind1 < 0) continue;
		ind1 = PairBraceR(str, ind1 - 1);
		ind1 = ExpectKey(str, _T("{"), ind1 + 1);
		if (ind1 < 0) continue;
		ind1 = PairBraceR(str, ind1 - 1);
		if (maxVars == 2) { str.Insert(ind0, _T("Two")); ++N; continue; }
		ind1 = ExpectKey(str, _T("{"), ind1 + 1);
		if (ind1 < 0) {
			str.Insert(ind0, _T("Two")); ++N; continue;
		}
		str.Insert(ind0, _T("Three")); ++N; continue;
	}
	return N;
}

// detect \name() commands and replace with \nameRound{}
// also replace \name[]() with \nameRound[]{}
// must remove comments first
int RoundSquareCommand(CString name, CString& str)
{
	int N{}, ind0{}, ind1{}, ind2{}, ind3;
	while (true) {
		ind0 = str.Find(_T("\\") + name, ind0);
		if (ind0 < 0) break;
		ind0 += name.GetLength() + 1;
		ind2 = ExpectKey(str, _T("("), ind0);
		if (ind2 > 0) {
			--ind2;
			ind3 = PairBraceR(str, ind2, ')');
			str.Delete(ind3, 1); str.Insert(ind3, '}');
			str.Delete(ind2, 1); str.Insert(ind2, '{');
			str.Insert(ind0, _T("Round"));
			++N;
		}
		else {
			ind2 = ExpectKey(str, _T("["), ind0);
			if (ind2 < 0) break;
			--ind2;
			ind3 = PairBraceR(str, ind2, ']');
			str.Delete(ind3, 1); str.Insert(ind3, '}');
			str.Delete(ind2, 1); str.Insert(ind2, '{');
			str.Insert(ind0, _T("Square"));
			++N;
		}
	}
	return N;
}

// detect \name() commands and replace with \nameRound{}
// also replace \name[]() with \nameRound[]{}
// must remove comments first
int MathFunction(CString name, CString& str)
{
	int N{}, ind0{}, ind1{}, ind2{}, ind3;
	while (true) {
		ind0 = str.Find(_T("\\") + name, ind0);
		if (ind0 < 0) break;
		ind0 += name.GetLength() + 1;
		ind1 = ExpectKey(str, _T("["), ind0);
		if (ind1 > 0) {
			--ind2;
			ind1 = PairBraceR(str, ind1, ']');
			ind2 = ind1 + 1;
		}
		else
			ind2 = ind0;
		ind2 = ExpectKey(str, _T("("), ind0);
		if (ind2 < 0) continue;
		--ind2;
		ind3 = PairBraceR(str, ind2, ')');
		str.Delete(ind3, 1); str.Insert(ind3, '}');
		str.Delete(ind2, 1); str.Insert(ind2, '{');
		str.Insert(ind0, _T("Round"));
		++N;
	}
	return N;
}

// deal with escape simbols in normal text
// str must be normal text
int TextEscape(CString& str)
{
	int N{};
	N += str.Replace(_T("\\ "), _T(" "));
	N += str.Replace(_T("{}"), _T(""));
	N += str.Replace(_T("\\^"), _T("^"));
	N += str.Replace(_T("\\%"), _T("%"));
	N += str.Replace(_T("\\&"), _T("&amp"));
	N += str.Replace(_T("<"), _T("&lt"));
	N += str.Replace(_T(">"), _T("&gt"));
	N += str.Replace(_T("\\{"), _T("{"));
	N += str.Replace(_T("\\}"), _T("}"));
	N += str.Replace(_T("\\#"), _T("#"));
	N += str.Replace(_T("\\~"), _T("~"));
	N += str.Replace(_T("\\_"), _T("_"));
	N += str.Replace(_T("\\,"), _T(" "));
	N += str.Replace(_T("\\;"), _T(" "));
	N += str.Replace(_T("\\textbackslash"), _T("&bsol;"));
	//N += str.Replace(_T(""), _T(""));
	return N;
}

// deal with escape simbols in normal text, \x{} commands, Command environments
// must be done before \command{} and environments are replaced with html tags
int NormalTextEscape(CString& str)
{
	int i{}, N1{}, N{}, Nnorm{};
	CString temp;
	vector<int> ind, ind1;
	FindNormalText(ind, str);
	FindComBrace(ind1, _T("\\x"), str);
	Nnorm = CombineRange(ind, ind, ind1);
	FindEnv(ind1, str, _T("Command"));
	Nnorm = CombineRange(ind, ind, ind1);
	for (i = 2 * Nnorm - 2; i >= 0; i -= 2) {
		temp = str.Mid(ind[i], ind[i + 1] - ind[i] + 1);
		N1 = TextEscape(temp); if (N1 < 0) continue;
		N += N1;
		str.Delete(ind[i], ind[i + 1] - ind[i] + 1);
		str.Insert(ind[i], temp);
	}
	return N;
}

// process table
// return number of tables processed, return -1 if failed
// must be used after EnvLabel()
int Table(CString& str)
{
	int i{}, j{}, N{}, ind0{}, ind1{}, ind2{}, ind3{}, Nline;
	vector<int> ind;
	vector<int> indLine; // stores the position of "\hline"
	CString caption;
	N = FindEnv(ind, str, _T("table"), 'o');
	if (N == 0) return 0;
	for (i = 2 * N - 2; i >= 0; i -= 2) {
		ind0 = str.Find(_T("\\caption")); 
		if (ind0 < 0 || ind0 > ind[i + 1]) {
			wcout << "table no caption!"; return -1;
		}
		ind0 += 8; ind0 = ExpectKey(str, '{', ind0);
		ind1 = PairBraceR(str, ind0 -1);
		caption = str.Mid(ind0, ind1 - ind0);
		// recognize \hline and replace with tags, also deletes '\\'
		while (true) {
			ind0 = str.Find(_T("\\hline"), ind0);
			if (ind0 < 0 || ind0 > ind[i + 1]) break;
			indLine.push_back(ind0);
			ind0 += 5;
		}
		Nline = indLine.size();
		str.Delete(indLine[Nline - 1], 6);
		str.Insert(indLine[Nline - 1], _T("</td></tr></table>"));
		ind0 = ExpectKeyReverse(str, _T("\\\\"), indLine[Nline - 1] - 1);
		str.Delete(ind0 + 1, 2);
		for (j = Nline - 2; j > 0; --j) {
			str.Delete(indLine[j], 6);
			str.Insert(indLine[j], _T("</td></tr><tr><td>"));
			ind0 = ExpectKeyReverse(str, _T("\\\\"), indLine[j] - 1);
			str.Delete(ind0 + 1, 2);
		}
		str.Delete(indLine[0], 6);
		str.Insert(indLine[0], _T("<table><tr><td>"));
	}
	// second round, replace '&' with tags
	// delete latex code
	// TODO: add title
	FindEnv(ind, str, _T("table"), 'o');
	for (i = 2 * N - 2; i >= 0; i -= 2) {
		ind0 = ind[i] + 12; ind1 = ind[i + 1];
		while (true) {
			ind0 = str.Find('&', ind0);
			if (ind0 < 0 || ind0 > ind1) break;
			str.Delete(ind0);
			str.Insert(ind0, _T("</td><td>"));
			ind1 += 8;
		}
		ind0 = ReverseFind(_T("</table>"), str, ind1) + 8;
		str.Delete(ind0, ind1 - ind0 + 1);
		ind0 = str.Find(_T("<table>"), ind[i]) - 1;
		str.Delete(ind[i], ind0 - ind[i] + 1);
	}
	return N;
}

// process Itemize environments
// return number processed, or -1 if failed
int Itemize(CString& str)
{
	int i{}, j{}, N{}, Nitem{}, ind0{};
	vector<int> indIn, indOut;
	vector<int> indItem; // positions of each "\item"
	N = FindEnv(indIn, str, "itemize");
	FindEnv(indOut, str, "itemize", 'o');
	for (i = 2 * N - 2; i >= 0; i -= 2) {
		// delete paragraph tags
		ind0 = indIn[i];
		while (true) {
			ind0 = str.Find(_T("<p>　　"), ind0);
			if (ind0 < 0 || ind0 > indIn[i + 1])
				break;
			str.Delete(ind0, 5); indIn[i + 1] -= 5; indOut[i + 1] -= 5;
		}
		ind0 = indIn[i];
		while (true) {
			ind0 = str.Find(_T("</p>"), ind0);
			if (ind0 < 0 || ind0 > indIn[i + 1])
				break;
			str.Delete(ind0, 4); indIn[i + 1] -= 4;  indOut[i + 1] -= 4;
		}
		// replace tags
		indItem.resize(0);
		str.Delete(indIn[i + 1] + 1, indOut[i + 1] - indIn[i + 1]);
		str.Insert(indIn[i + 1] + 1, _T("</li></ul>"));
		ind0 = indIn[i];
		while (true) {
			ind0 = str.Find(_T("\\item"), ind0);
			if (ind0 < 0 || ind0 > indIn[i + 1]) break;
			indItem.push_back(ind0); ind0 += 5;
		}
		Nitem = indItem.size();
		
		for (j = Nitem - 1; j > 0; --j) {
			str.Delete(indItem[j], 5);
			str.Insert(indItem[j], _T("</li><li>"));
		}
		str.Delete(indItem[0], 5);
		str.Insert(indItem[0], _T("<ul><li>"));
		str.Delete(indOut[i], indItem[0] - indOut[i]);
	}
	return N;
}

// process enumerate environments
// similar to Itemize()
int Enumerate(CString& str)
{
	int i{}, j{}, N{}, Nitem{}, ind0{};
	vector<int> indIn, indOut;
	vector<int> indItem; // positions of each "\item"
	N = FindEnv(indIn, str, "enumerate");
	FindEnv(indOut, str, "enumerate", 'o');
	for (i = 2 * N - 2; i >= 0; i -= 2) {
		// delete paragraph tags
		ind0 = indIn[i];
		while (true) {
			ind0 = str.Find(_T("<p>　　"), ind0);
			if (ind0 < 0 || ind0 > indIn[i + 1])
				break;
			str.Delete(ind0, 5); indIn[i + 1] -= 5; indOut[i + 1] -= 5;
		}
		ind0 = indIn[i];
		while (true) {
			ind0 = str.Find(_T("</p>"), ind0);
			if (ind0 < 0 || ind0 > indIn[i + 1])
				break;
			str.Delete(ind0, 4); indIn[i + 1] -= 4; indOut[i + 1] -= 4;
		}
		// replace tags
		indItem.resize(0);
		str.Delete(indIn[i + 1] + 1, indOut[i + 1] - indIn[i + 1]);
		str.Insert(indIn[i + 1] + 1, _T("</li></ol>"));
		ind0 = indIn[i];
		while (true) {
			ind0 = str.Find(_T("\\item"), ind0);
			if (ind0 < 0 || ind0 > indIn[i + 1]) break;
			indItem.push_back(ind0); ind0 += 5;
		}
		Nitem = indItem.size();

		for (j = Nitem - 1; j > 0; --j) {
			str.Delete(indItem[j], 5);
			str.Insert(indItem[j], _T("</li><li>"));
		}
		str.Delete(indItem[0], 5);
		str.Insert(indItem[0], _T("<ol><li>"));
		str.Delete(indOut[i], indItem[0] - indOut[i]);
	}
	return N;
}