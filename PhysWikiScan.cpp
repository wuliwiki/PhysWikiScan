#include <atlstr.h>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <iostream>
#include <io.h> // for console unicode output
#include <fcntl.h> // for console unicode output

using namespace std;

// search all file names of a certain file extension
// path must end with "\\"
vector<CString> GetFileNames(CString path, CString extension)
{
	vector<CString> names;
	CString file, name;
	WIN32_FIND_DATA data;
	HANDLE hfile;
	file = path + _T("*.") + extension;
	hfile = FindFirstFile(file, &data);
	if (data.cFileName[0] == 52428)
		return names;
	names.push_back(data.cFileName);
	//wcout << names.back().GetString() << endl;
	while (true)
	{
		FindNextFile(hfile, &data);
		name = data.cFileName;
		if (!name.Compare(names.back())) //if name == names.back()
			break;
		else
		{
			names.push_back(name);
			//wcout << name.GetString() << endl;
		}
	}
	return names;
}

// convert CString to UTF-8
string to_utf8(CString cstr)
{
	char *buffer = new char[1024 * 1024];
	WideCharToMultiByte(CP_UTF8, NULL, cstr, -1, buffer, 1024 * 1024, NULL, FALSE);

	string str = buffer;
	delete[]buffer;
	return str;
}

// convert UTF-8 to CString
CString from_utf8(string str)
{
	wchar_t *buffer = new wchar_t[1024 * 1024];
	// Convert headers from ASCII to Unicode.
	MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, buffer, 1024 * 1024);

	CString cstr = buffer;
	delete[]buffer;
	return cstr;
}

// read a UTF8 file to CString
char buffer[1024 * 1024] = {};
CString ReadUTF8(CString path)
{
	ifstream fin;
	fin.open(path, ios::in);
	if (!fin.is_open())
	{
		wcout << _T("open input file error");
		return _T("error");
	}
	memset(buffer, 0, 1024 * 1024);
	fin.read(buffer, 1024 * 1024);
	fin.close();

	return from_utf8(buffer);
}

// write a CString to a UTF8 file
void WriteUTF8(CString text, CString path)
{
	string out_text = to_utf8(text);
	ofstream fout;
	fout.open(path, ios::out | ios::trunc | ios::binary);
	if (!fout.is_open())
		printf("open output file error\n");
	fout.write(out_text.c_str(), out_text.size());
	fout.close();
}

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

// find next character that is not a space
// output single character c, return the position of the c
// return -1 if not found
int NextNoSpace(CString& c, const CString& str, int start)
{
	for (int i{ start }; i < (int)str.GetLength(); ++i) {
		c = str.GetAt(i);
		if (c == ' ')
			continue;
		else
			return i;
	}
	return -1;
}

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

// see if a key appears followed only with only white space
// return the next index, return -1 if nothing found.
int ExpectKey(const CString& str, CString key, int start)
{
	int ind = start;
	int ind0 = 0;
	int L = str.GetLength();
	int L0 = key.GetLength();
	TCHAR c0, c;
	while (true)
	{
		c0 = key.GetAt(ind0);
		c = str.GetAt(ind);
		if (c == c0)
		{
			++ind0;
			if (ind0 == L0)
				return ind + 1;
		}
		else if (c != ' ')
			return -1;
		++ind;
		if (ind == L)
			return -1;
	}
}

// find a scope in str for environment named env
// output: ind is index in pairs
// return number of environments found. return -1 if failed
// if option = 'i', range starts from the next index of \begin{} and previous index of \end{}
// if option = 'o', range starts from '\' of \begin{} and '}' of \end{}
int FindEnv(vector<int>& ind, const CString& str, CString env, char option = 'i')
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
		ind.push_back(option == 'i'? ind0 : ind3);

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
			ind.push_back(option == 'i'? ind2 : ind0 - 1);
			++N;
			break;
		}
	}
}

// find the range of inline equations using $$
// if option = 'i', range does not include $, if 'o', it does.
// return the number of $$ environments found.
int FindInline(vector<int>& ind, const CString& str, char option = 'i')
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

// Pair right brace to left one (default)
// or () or [] or anying single character
// ind is inddex of left brace
// return index of right brace, -1 if failed
int PairBraceR(const CString& str, int ind, TCHAR type = '{')
{
	TCHAR left, right;
	if (type == '{' || type == '}') {
		left = '{'; right = '}';
	}
	else if (type == '(' || type == ')') {
		left = '('; right = ')';
	}
	else if (ind == '[' || type == ']') {
		left = '['; right = ']';
	}
	else {// anything else
		left = type; right = type;
	}

	TCHAR c, c0 = ' ';
	int Nleft = 1;
	for (int i{ ind+1 }; i < str.GetLength(); i++)
	{
		c = str.GetAt(i);
		if (c == left && c0 != '\\')
			++Nleft;
		else if (c == right && c0 != '\\')
		{
			--Nleft;
			if (Nleft == 0)
				return i;
		}
		c0 = c;
	}
	return -1;
}

// Find all <key>{} environment in str
// output ranges to ind
// if option = 'i', range does not include {}, if 'o', range from first letter of <key> to '}'
// return number of <key>{} found, return -1 if failed.
int FindComBrace(vector<int>& ind, const CString& key, const CString& str, char option = 'i')
{
	ind.resize(0);
	int ind0{}, ind1;
	while (true)
	{
		ind1 = str.Find(key, ind0);
		if (ind1 < 0)
			return ind.size() / 2;
		ind0 = ind1 + key.GetLength();
		ind0 = ExpectKey(str, _T("{"), ind0);
		if (ind0 < 0)
			return -1;
		ind.push_back(option == 'i'? ind0 : ind1);
		ind0 = PairBraceR(str, ind0 - 1);
		if (ind0 < 0)
			{ wcout << L"error!"; return -1; }
		ind.push_back(option == 'i'? ind0 - 1 : ind0);
	}
}

// sort x in descending order while tracking the original index
void sort(vector<int>& x, vector<int>& ind)
{
	int i, temp;
	int N = x.size();
	// initialize ind
	ind.resize(0);
	for (i = 0; i < N; ++i)
		ind.push_back(i);

	bool changed{ true };
	while (changed == true)
	{
		changed = false;
		for (i = 0; i < N - 1; i++)
		{
			if (x[i] > x[i + 1])
			{
				temp = x[i];
				x[i] = x[i + 1];
				x[i + 1] = temp;
				temp = ind[i];
				ind[i] = ind[i + 1];
				ind[i + 1] = temp;
				changed = true;
			}
		}
	}
}

// combine ranges ind1 and ind2
// a range can contain another range, but not partial overlap
// return total range number, or -1 if failed.
int CombineRange(vector<int>& ind, vector<int> ind1, vector<int> ind2)
{
	int i, N1 = ind1.size(), N2 = ind2.size();
	if (N1 == 0)
		{ ind = ind2; return N2/2; }
	else if (N2 == 0)
		{ ind = ind1; return N1/2; }

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
	while(i < N - 1)
	{
		if (end[i] > start[i + 1])
		{
			if (end[i] > end[i + 1])
				{ end[i + 1] = end[i]; ++i; }
			else
				{ wcout << L"error! range overlap!"; return -1; }
		}
		else if (end[i] == start[i+1] - 1)
			++i;
		else
		{
			ind.push_back(end[i]);
			ind.push_back(start[i+1]);
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
		{ ind.push_back(0); ind.push_back(Nstr-1); return 1; }

	int N{}; // total num of ranges output
	if (ind0[0] > 0)
		{ ind.push_back(0); ind.push_back(ind0[0] - 1); ++N; }
	for (unsigned i{ 1 }; i < ind0.size() - 1; i += 2)
	{
		ind.push_back(ind0[i] + 1);
		ind.push_back(ind0[i + 1] - 1);
		++N;
	}
	if (ind0.back() < Nstr - 1)
		{ ind.push_back(ind0.back() + 1); ind.push_back(Nstr - 1); ++N; }
	return N;
}

// Find normal text range
int FindNormalText(vector<int>& indNorm, CString& str)
{
	vector<int> ind, indComm, indEq, indInline, indCom, indGath, indAli, 
		indTT, indFig, indInp, indTable, indSSub;
	int NComm, NEq, NInline, NCom, NGath, NAli, NTT, NFig, NInp, NTable, NSSub;
	// comments
	NComm = FindComment(indComm, str);
	ind = indComm;
	// inline equation environments
	NInline = FindInline(indInline, str, 'o');
	if (NInline > 0)
		CombineRange(ind, ind, indInline);
	// equation environments
	NEq = FindEnv(indEq, str, _T("equation"), 'o');
	if (NEq > 0)
		CombineRange(ind, ind, indEq);
	// command environments
	NCom = FindEnv(indCom, str, _T("Command"), 'o');
	if (NCom > 0)
		CombineRange(ind, ind, indCom);
	// gather environments
	NGath = FindEnv(indGath, str, _T("gather"), 'o');
	if (NGath > 0)
		CombineRange(ind, ind, indGath);
	// align environments (not "aligned")
	NAli = FindEnv(indAli, str, _T("align"), 'o');
	if (NAli > 0)
		CombineRange(ind, ind, indAli);
	// texttt command
	NTT = FindComBrace(indTT, _T("\\texttt"), str, 'o');
	if (NTT > 0)
		CombineRange(ind, ind, indTT);
	// input command
	NInp = FindComBrace(indInp, _T("\\input"), str, 'o');
	if (NInp > 0)
		CombineRange(ind, ind, indInp);
	// Figure environments
	NFig = FindEnv(indFig, str, _T("figure"), 'o');
	if (NFig > 0)
		CombineRange(ind, ind, indFig);
	// Table environments
	NTable = FindEnv(indTable, str, _T("table"), 'o');
	if (NTable > 0)
		CombineRange(ind, ind, indTable);
	// subsubsection command
	NSSub = FindComBrace(indSSub, _T("\\subsubsection"), str, 'o');
	if (NSSub > 0)
		CombineRange(ind, ind, indSSub);
	// invert range
	return InvertRange(indNorm, ind, str.GetLength());
}

// match braces
// return -1 means failure, otherwise return number of {} paired
// output ind_left, ind_right, ind_RmatchL
int MatchBraces(vector<int>& ind_left, vector<int>& ind_right,
	vector<int>& ind_RmatchL, CString& str, int start, int end)
{
	ind_left.resize(0); ind_right.resize(0); ind_RmatchL.resize(0);
	TCHAR c, c_last = ' ';
	bool continuous{ false };
	int Nleft = 0, Nright = 0;
	vector<bool> Lmatched;
	bool matched;
	for (int i = start; i <= end; ++i)
	{
		c = str.GetAt(i);
		if (c == '{' && c_last != '\\')
		{
			++Nleft;
			ind_left.push_back(i);
			Lmatched.push_back(false);
		}
		else if (c == '}' && c_last != '\\')
		{
			++Nright;
			ind_right.push_back(i);
			matched = false;
			for (int j = Nleft - 1; j >= 0; --j)
				if (!Lmatched[j])
				{
					ind_RmatchL.push_back(j);
					Lmatched[j] = true;
					matched = true;
					break;
				}
			if (!matched)
				return -1; // unbalanced braces
		}
		c_last = c;
	}
	if (Nleft != Nright)
		return -1;
	return Nleft;
}

// detect unnecessary braces and add "删除标记"
// return the number of braces pairs removed
int RemoveBraces(vector<int>& ind_left, vector<int>& ind_right,
	vector<int>& ind_RmatchL, CString& str)
{
	unsigned i, N{};
	//bool continuous{ false };
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
	str.Insert(str.GetLength() - 1, _T("</p>"));
	str.Insert(0, _T("<p>"));
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
		str.Delete(indEnvIn[i+1] + 1, indEnvOut[i+1] - indEnvIn[i+1]);
		str.Insert(indEnvIn[i + 1] + 1, strRight);
		str.Delete(indEnvOut[i], indEnvIn[i] - indEnvOut[i]);
		str.Insert(indEnvOut[i], strLeft);
		++N;
	}
	return N;
}

// ensure space around (CString)name
// return number of spaces added
// will only check equation env. and inline equations
int EnsureSpace(CString name, CString& str, int start, int end)
{
	int N{}, ind0{start};
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

// replace figure environment with html image
// convert vector graph to SVG, font must set to "convert to outline"
int FigureEnvironment(CString& str)
{
	int i{}, N{}, Nfig{}, ind0{}, indName1{}, indName2{};
	vector<int> indFig{};
	CString figName;
	Nfig = FindEnv(indFig, str, _T("figure"), 'o');
	for (i = 2*Nfig - 2; i >= 0; i-=2) {
		indName1 = str.Find(_T("figures/"), indFig[i]) + 8;
		indName2 = str.Find(_T(".pdf"), indFig[i]) - 1;
		figName = str.Mid(indName1, indName2 - indName1 + 1);
		// TODO: get and set figure size and 45pt per cm
		str.Delete(indFig[i], indFig[i + 1] - indFig[i] + 1);
		// insert html code
		//TODO: set figure size in pt
		str.Insert(indFig[i], _T("<div class = \"w3 - cell\" style = \"width:300px\">\n"));
		ind0 = str.Find(_T("\n"), indFig[i]) + 1;
		str.Insert(ind0, _T("<img src = \"")); ind0 += 12;
		str.Insert(ind0, figName); ind0 += figName.GetLength();
		str.Insert(ind0, _T(".svg\" alt=\"图\" style=\"width:100% ;\">\n</div>"));
		++N;
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
// output the chinese title of the file
// return 0 if successful, -1 if failed
int tex2html1(CString& title, CString path)
{
	// read template file from local folder
	int i{}, N{}, ind0;
	CString str = ReadUTF8(path); // read tex file
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
	// process figure environment
	wcout << FigureEnvironment(str);
	// ensure '<' and '>' has spaces around them
	EqOmitTag(str);
	// add paragraph tags
	ParagraphTag(str);
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
	Command2Tag(_T("subsection"), _T("</p><h4>"), _T("</h4><hr>\n<p>"), str);
	Command2Tag(_T("bb"), _T("<b>"), _T("</b>"), str);
	// replace environments with html tags
	Env2Tag(_T("exam"), _T("</p><h5> 例：</h5>\n<p>"), _T(""), str);

	//wcout << endl << str.GetString() << endl;
	// insert HTML body
	ind0 = html.Find(_T("PhysWikiHTMLbody"), ind0);
	html.Delete(ind0, 16);
	
	html.Insert(ind0, str);// insert notice
	html.Insert(ind0, _T("<p><span class=\"w3-tag\">公告</span><span class=\"w3-tag w3-yellow\">《小时物理百科》网页版仍在开发中，请下载<a href=\".. / \">《小时物理百科》PDF 版</a>。</span></p>\n"));
	html.Insert(ind0, _T("<h1>") + title + _T("</h1><hr>\n")); // insert title

	// save html file
	path.Delete(path.GetLength() - 3, 3);
	path.Insert(path.GetLength(), _T("html"));
	WriteUTF8(html, path);
	return 0;
}


void main()
{
	_setmode(_fileno(stdout), _O_U16TEXT); // for console unicode output

	int ind0{};
	//CString path0 = _T("C:\\Users\\addis\\Documents\\GitHub\\PhysWiki\\contents\\");
	//CString path0 = _T("C:\\Users\\addis\\Desktop\\");
	CString path0 = _T("C:\\Users\\addis\\Documents\\GitHub\\littleshi.cn\\root\\PhysWiki\\online\\");

	vector<CString> names = GetFileNames(path0, _T("tex"));
	CString title;
	if (names.size() < 0) return;
	//int N;

	CString toc = ReadUTF8(_T("template.html")); // read html template
	ind0 = toc.Find(_T("PhysWikiCommand"), 0);
	toc.Delete(ind0, 15);
	ind0 = toc.Find(_T("PhysWikiHTMLbody"), 0); --ind0;
	toc.Insert(ind0, _T("<h1>《小时物理百科》网页版目录</h1>\n\n"));// insert notice
	ind0 = toc.Find(_T("PhysWikiHTMLbody"), ind0); --ind0;
	toc.Insert(ind0, _T("<p><span class=\"w3-tag\">公告</span><span class=\"w3-tag w3-yellow\">《小时物理百科》网页版仍在开发中，请下载<a href=\"../\">《小时物理百科》PDF 版</a>。</span></p><hr>\n\n<p>\n"));
	ind0 = toc.Find(_T("PhysWikiHTMLbody"), ind0); --ind0;

	for (unsigned i{}; i < names.size(); ++i)
	{
		wcout << i << " ";
		wcout << names[i].GetString() << _T("...");
		//N = OneFile3(path0 + _T("BEng.tex"));
		//N = OneFile3(path0 + names[i]);
		//wcout << N << endl;

		tex2html1(title, path0 + names[i]);
		wcout << endl;
		// add to table of content
		toc.Insert(ind0, _T("<a href=\""));// insert notice
		ind0 = toc.Find(_T("PhysWikiHTMLbody"), ind0); --ind0;
		names[i].Delete(names[i].GetLength() - 3, 3);
		toc.Insert(ind0, names[i] + _T("html"));
		ind0 = toc.Find(_T("PhysWikiHTMLbody"), ind0); --ind0;
		toc.Insert(ind0, _T("\">") + title + _T("</a><br>\n"));
		ind0 = toc.Find(_T("PhysWikiHTMLbody"), ind0); --ind0;
		//wcout << toc.GetString() << _T("\n\n\n\n\n") << endl;
	}
	toc.Insert(ind0, _T("</p>"));
	++ind0; toc.Delete(ind0, 16);
	WriteUTF8(toc, path0 + _T("index.html"));
}
