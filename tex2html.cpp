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

// add <p> tags to paragraphs
// return number of tag pairs added
int ParagraphTag(CString& str)
{
	int i{}, N{}, ind0{};
	vector<int> ind;
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
	for (i = N - 2; i > 0; --i) {
		str.Insert(ind[i] + 1, _T("<p>"));
		str.Insert(ind[i], _T("\n</p>"));
	}
	str.Insert(str.GetLength(), _T("</p>"));
	str.Insert(0, _T("<p>"));
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