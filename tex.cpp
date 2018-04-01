# include "tex.h"
using namespace std;

// see if an index i falls into the scopes of ind
bool IndexInRange(int i, vector<int> ind)
{
	if (ind.size() == 0)
		return false;
	for (unsigned j{}; j < ind.size() - 1; j += 2) {
		if (ind[j] <= i && i <= ind[j + 1])
			return true;
	}
	return false;
}

// see if an index ind is in any of the evironments \begin{names[j]}...\end{names[j]}
// output iname of name[iname], -1 if return false
// TODO: check if this function works.
bool IndexInEnv(int& iname, int ind, const vector<CString>& names, const CString& str)
{
	int j{}, ikey{}, ind0{ ind + 1 }, ind1{}, Nname{};
	Nname = names.size(); iname = -1;
	vector<CString> key{_T("\\begin"), _T("\\end")};
	// find next \begin{name} or \end{name}
	while (true) {
		ind0 = FindMultiple(ikey, str, key, ind0);
		if (ind0 < 0) return false;
		ind0 = ExpectKey(str, '{', ind0 + key[ikey].GetLength());
		for (j = 0; j < Nname; ++j) {
			ind1 = ExpectKey(str, names[j], ind0);
			if (ind1 >= 0) {
				iname = j;  break;
			}
		}
		if (ind1 < 0)
			return false;
		else if (ikey == 0)
			return false;
		else
			return true;
	}
}

// combine ranges ind1 and ind2
// a range can contain another range, but not partial overlap
// return total range number, or -1 if failed.
int CombineRange(vector<int>& ind, vector<int> ind1, vector<int> ind2)
{
	int i, N1 = ind1.size(), N2 = ind2.size();
	if (N1 == 0)
	{
		ind = ind2; return N2 / 2;
	}
	else if (N2 == 0)
	{
		ind = ind1; return N1 / 2;
	}

	// load start and end, and sort
	vector<int> start, end, order;
	for (i = 0; i < N1; i += 2)
		start.push_back(ind1[i]);
	for (i = 1; i < N1; i += 2)
		end.push_back(ind1[i]);
	for (i = 0; i < N2; i += 2)
		start.push_back(ind2[i]);
	for (i = 1; i < N2; i += 2)
		end.push_back(ind2[i]);
	int N = start.size();
	sort(start, order);
	vector<int> temp;
	temp.resize(N);
	for (i = 0; i < N; ++i)
		temp[i] = end[order[i]];
	end = temp;

	// load ind
	ind.resize(0);
	ind.push_back(start[0]);
	i = 0;
	while (i < N - 1)
	{
		if (end[i] > start[i + 1])
		{
			if (end[i] > end[i + 1])
			{
				end[i + 1] = end[i]; ++i;
			}
			else
			{
				wcout << L"error! range overlap!"; return -1;
			}
		}
		else if (end[i] == start[i + 1] - 1)
			++i;
		else
		{
			ind.push_back(end[i]);
			ind.push_back(start[i + 1]);
			++i;
		}
	}
	ind.push_back(end.back());
	return ind.size() / 2;
}

// invert ranges in ind0, output to ind1
int InvertRange(vector<int>& ind, const vector<int>& ind0, int Nstr)
{
	ind.resize(0);
	if (ind0.size() == 0)
	{
		ind.push_back(0); ind.push_back(Nstr - 1); return 1;
	}

	int N{}; // total num of ranges output
	if (ind0[0] > 0)
	{
		ind.push_back(0); ind.push_back(ind0[0] - 1); ++N;
	}
	for (unsigned i{ 1 }; i < ind0.size() - 1; i += 2)
	{
		ind.push_back(ind0[i] + 1);
		ind.push_back(ind0[i + 1] - 1);
		++N;
	}
	if (ind0.back() < Nstr - 1)
	{
		ind.push_back(ind0.back() + 1); ind.push_back(Nstr - 1); ++N;
	}
	return N;
}

// find comments
// output: ind is index in pairs
// return number of comments found. return -1 if failed
// range is from '%' to 'CR'
int FindComment(vector<int>& ind, const CString& str)
{
	int ind0{}, ind1{};
	int N{}; // number of comments found
	ind.resize(0);
	//TCHAR escape{'\\'};
	while (true) {
		ind1 = str.Find(_T("%"), ind0);
		if (ind1 >= 0)
			if (ind1 == 0 || (ind1 > 0 && str.GetAt(ind1 - 1) != '\\')) {
				ind.push_back(ind1); ++N;
			}
			else {
				ind0 = ind1 + 1;  continue;
			}
		else
			return N;

		ind1 = str.Find(_T("\n"), ind1 + 1);
		if (ind1 < 0) {// not found
			ind.push_back(str.GetLength() - 1);
			return N;
		}
		else
			ind.push_back(ind1);

		ind0 = ind1;
	}
}

// find a scope in str for environment named env
// output: ind is index in pairs
// return number of environments found. return -1 if failed
// if option = 'i', range starts from the next index of \begin{} and previous index of \end{}
// if option = 'o', range starts from '\' of \begin{} and '}' of \end{}
int FindEnv(vector<int>& ind, const CString& str, CString env, char option)
{
	int ind0{}, ind1{}, ind2{}, ind3{};
	vector<int> indComm{}; // result from FindComment
	int N{}; // number of environments found
	ind.resize(0);
	FindComment(indComm, str);
	while (true) {
		// find "\\begin"
		ind3 = str.Find(_T("\\begin"), ind0);
		if (IndexInRange(ind3, indComm)) { ind0 = ind3 + 6; continue; }
		if (ind3 < 0)
			return N;
		ind0 = ind3 + 6;

		// expect '{'
		ind0 = ExpectKey(str, _T("{"), ind0);
		if (ind0 < 0)
			return -1;
		// expect 'env'
		ind1 = ExpectKey(str, env, ind0);
		if (ind1 < 0)
			continue;
		ind0 = ind1;
		// expect '}'
		ind0 = ExpectKey(str, _T("}"), ind0);
		if (ind0 < 0)
			return -1;
		ind.push_back(option == 'i' ? ind0 : ind3);

		while (true)
		{
			// find "\\end"
			ind0 = str.Find(_T("\\end"), ind0);
			if (IndexInRange(ind0, indComm)) { ind0 += 4; continue; }
			if (ind0 < 0)
				return -1;
			ind2 = ind0 - 1;
			ind0 += 4;
			// expect '{'
			ind0 = ExpectKey(str, _T("{"), ind0);
			if (ind0 < 1)
				return -1;
			// expect 'env'
			ind1 = ExpectKey(str, env, ind0);
			if (ind1 < 0)
				continue;
			ind0 = ind1;
			// expect '}'
			ind0 = ExpectKey(str, _T("}"), ind0);
			if (ind0 < 0)
				return -1;
			ind.push_back(option == 'i' ? ind2 : ind0 - 1);
			++N;
			break;
		}
	}
}


// find the range of inline equations using $$
// if option = 'i', range does not include $, if 'o', it does.
// return the number of $$ environments found.
int FindInline(vector<int>& ind, const CString& str, char option)
{
	ind.resize(0);
	int N{}; // number of $$
	int ind0{};
	vector<int> indComm; // result from FindComment
	FindComment(indComm, str);
	while (true) {
		ind0 = str.Find(_T("$"), ind0);
		if (ind0 < 0) { break; } // did not find
		if (ind0 > 0 && str.GetAt(ind0 - 1) == '\\') { ++ind0; continue; } // not an escape
		if (IndexInRange(ind0, indComm)) { ++ind0; continue; } // not in comment
		ind.push_back(ind0);
		++ind0; ++N;
	}
	if (N % 2 != 0) return -1;// odd number of $ found
	N /= 2;
	if (option == 'i' && N > 0)
		for (int i{}; i < 2 * N; i += 2) {
			++ind[i]; --ind[i + 1];
		}
	return N;
}

// Find all <key>{} environment in str
// find <key>{}{} when option = '2'
// output ranges to ind
// if option = 'i', range does not include {}, if 'o', range from first character of <key> to '}'
// return number of <key>{} found, return -1 if failed.
int FindComBrace(vector<int>& ind, const CString& key, const CString& str, char option)
{
	ind.resize(0);
	int ind0{}, ind1;
	while (true)
	{
		ind1 = str.Find(key, ind0);
		if (ind1 < 0)
			return ind.size() / 2;
		ind0 = ind1 + key.GetLength();
		ind0 = ExpectKey(str, '{', ind0);
		if (ind0 < 0) {
			ind0 = ind1 + key.GetLength(); continue;
		}
		ind.push_back(option == 'i' ? ind0 : ind1);
		ind0 = PairBraceR(str, ind0 - 1);
		if (option != '2') {
			ind.push_back(option == 'i' ? ind0 - 1 : ind0);
		}
		else {
			ind0 = ExpectKey(str, '{', ind0 + 1);
			if (ind0 < 0)
				continue;
			ind0 = PairBraceR(str, ind0 - 1);
			ind.push_back(ind0);
		}
	}
}

// Find "\begin{env}" or "\begin{env}{}" (option = '2')
// output ranges to ind, from '\' to '}'
// return number found, return -1 if failed
int FindBegin(vector<int>& ind, const CString& env, const CString& str, char option)
{
	ind.resize(0);
	int N{}, ind0{}, ind1;
	while (true) {
		ind1 = str.Find(_T("\\begin"), ind0);
		if (ind1 < 0)
			return N;
		ind0 = ExpectKey(str, '{', ind1 + 6);
		if (ExpectKey(str, env, ind0) < 0)
			continue;
		++N; ind.push_back(ind1);
		ind0 = PairBraceR(str, ind0 - 1);
		if (option == '1')
			ind.push_back(ind0);
		ind0 = ExpectKey(str, '{', ind0 + 1);
		if (ind0 < 0) {
			wcout << _T("expecting {}{}!"); return -1;
		}
		ind0 = PairBraceR(str, ind0 - 1);
		ind.push_back(ind0);
	}
}

// Find "\end{env}"
// output ranges to ind, from '\' to '}'
// return number found, return -1 if failed
int FindEnd(vector<int>& ind, const CString& env, const CString& str)
{
	ind.resize(0);
	int N{}, ind0{}, ind1{};
	while (true) {
		ind1 = str.Find(_T("\\end"), ind0);
		if (ind1 < 0)
			return N;
		ind0 = ExpectKey(str, '{', ind1 + 4);
		if (ExpectKey(str, env, ind0) < 0)
			continue;
		++N; ind.push_back(ind1);
		ind0 = PairBraceR(str, ind0 - 1);
		ind.push_back(ind0);
	}
}

// Find normal text range
int FindNormalText(vector<int>& indNorm, CString& str)
{
	vector<int> ind, ind1;
	// comments
	FindComment(ind, str);
	// inline equation environments
	FindInline(ind1, str, 'o');
	CombineRange(ind, ind, ind1);
	// equation environments
	FindEnv(ind1, str, _T("equation"), 'o');
	CombineRange(ind, ind, ind1);
	// command environments
	FindEnv(ind1, str, _T("Command"), 'o');
	CombineRange(ind, ind, ind1);
	// gather environments
	FindEnv(ind1, str, _T("gather"), 'o');
	CombineRange(ind, ind, ind1);
	// align environments (not "aligned")
	FindEnv(ind1, str, _T("align"), 'o');
	CombineRange(ind, ind, ind1);
	// texttt command
	FindComBrace(ind1, _T("\\texttt"), str, 'o');
	CombineRange(ind, ind, ind1);
	// input command
	FindComBrace(ind1, _T("\\input"), str, 'o');
	CombineRange(ind, ind, ind1);
	// Figure environments
	FindEnv(ind1, str, _T("figure"), 'o');
	CombineRange(ind, ind, ind1);
	// Table environments
	FindEnv(ind1, str, _T("table"), 'o');
	CombineRange(ind, ind, ind1);
	// subsubsection command
	FindComBrace(ind1, _T("\\subsubsection"), str, 'o');
	CombineRange(ind, ind, ind1);
	//  \begin{exam}{} and \end{exam}
	FindBegin(ind1, _T("exam"), str, '2');
	CombineRange(ind, ind, ind1);
	FindEnd(ind1, _T("exam"), str);
	CombineRange(ind, ind, ind1);
	//  exer\begin{exer}{} and \end{exer}
	FindBegin(ind1, _T("exer"), str, '2');
	CombineRange(ind, ind, ind1);
	FindEnd(ind1, _T("exer"), str);
	CombineRange(ind, ind, ind1);
	// invert range
	return InvertRange(indNorm, ind, str.GetLength());
}

// detect unnecessary braces and add "删除标记"
// return the number of braces pairs removed
int RemoveBraces(vector<int>& ind_left, vector<int>& ind_right,
	vector<int>& ind_RmatchL, CString& str)
{
	unsigned i, N{};
	vector<int> ind; // redundent right brace index
	for (i = 1; i < ind_right.size(); ++i)
		// there must be no space between redundent {} and neiboring braces.
		if (ind_right[i] == ind_right[i - 1] + 1 &&
			ind_left[ind_RmatchL[i]] == ind_left[ind_RmatchL[i - 1]] - 1)
		{
			ind.push_back(ind_right[i]);
			ind.push_back(ind_left[ind_RmatchL[i]]);
			++N;
		}

	if (N > 0) {
		sort(ind.begin(), ind.end());
		for (int i = ind.size() - 1; i >= 0; --i) {
			str.Insert(ind[i], _T("删除标记"));
		}
	}
	return N;
}

// replace \nameComm{...} with strLeft...strRight
// {} cannot be omitted
// must remove comments first
int Command2Tag(CString nameComm, CString strLeft, CString strRight, CString& str)
{
	int N{}, ind0{}, ind1{}, ind2{};
	while (true) {
		ind0 = str.Find(_T("\\") + nameComm, ind0);
		if (ind0 < 0) break;
		ind1 = ind0 + nameComm.GetLength() + 1;
		ind1 = ExpectKey(str, '{', ind1); --ind1;
		if (ind1 < 0) {
			++ind0; continue;
		}
		ind2 = PairBraceR(str, ind1);
		str.Delete(ind2, 1);
		str.Insert(ind2, strRight);
		str.Delete(ind0, ind1 - ind0 + 1);
		str.Insert(ind0, strLeft);
		++N;
	}
	return N;
}

// replace nameEnv environment with strLeft...strRight
// must remove comments first
int Env2Tag(CString nameEnv, CString strLeft, CString strRight, CString& str)
{
	int i{}, N{}, Nenv;
	vector<int> indEnvOut, indEnvIn;
	Nenv = FindEnv(indEnvIn, str, nameEnv, 'i');
	Nenv = FindEnv(indEnvOut, str, nameEnv, 'o');
	for (i = 2 * Nenv - 2; i >= 0; i -= 2) {
		str.Delete(indEnvIn[i + 1] + 1, indEnvOut[i + 1] - indEnvIn[i + 1]);
		str.Insert(indEnvIn[i + 1] + 1, strRight);
		str.Delete(indEnvOut[i], indEnvIn[i] - indEnvOut[i]);
		str.Insert(indEnvOut[i], strLeft);
		++N;
	}
	return N;
}