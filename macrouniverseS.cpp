// utilities that deal with strings

#include "macrouniverseS.h"
using namespace std;
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
	if (!fin.is_open()) {
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

// reverse find, starting from "start"
// return the location of the first character of "key", return -1 if not found
// use CString::ReverseFind instead if start from the end
int ReverseFind(const CString& key, const CString& str, int start)
{
	int i{}, k{}, end{};
	bool match{ false };
	end = key.GetLength() - 1;
	k = end;
	for (i = start; i >= 0; --i) {
		if (match == true) {
			if (str.GetAt(i) == key.GetAt(k)) {
				if (k == 0) return i;
				--k;
			}
			else {
				match = false; k = end;
			}
		}
		else {// match == false
			if (str.GetAt(i) == key.GetAt(k)) {
				match = true;
				if (k == 0) return i;
				--k;
			}
		}
	}
	return -1;
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

// Find the next number
// return -1 if not found
int FindNum(const CString& str, int start)
{
	int i{}, end{ str.GetLength() - 1 };
	unsigned char c;
	for (i = start; i <= end; ++i) {
		c = str.GetAt(i);
		if (c >= '0' && c <= '9')
			return i;
	}
}

// Fiind the next appearance of
// output the ikey of key[ikey] found
// return the first index of key[ikey], return -1 if nothing found
// TODO: temporary algorithm, can be more efficient
int FindMultiple(int& ikey, const CString& str, const vector<CString>& key, int start)
{
	int i{}, ind0{}, Nkey{}, imin;
	Nkey = key.size();
	imin = str.GetLength();
	for (i = 0; i < Nkey; ++i) {
		ind0 = str.Find(key[i], start);
		if (ind0 >= start && ind0 < imin) {
			imin = ind0; ikey = i;
		}
	}
	if (imin == str.GetLength()) imin = -1;
	return imin;
}

// convert int in CString, no negative number!
// return the index after the last digit, return -1 if failed
// CString.GetAt(start) must be a number
int CString2int(int& num, const CString& str, int start)
{
	int i{};
	unsigned char c;
	c = str.GetAt(start);
	if (c < '0' || c > '9') {
		wcout << _T("not a number!"); return -1;
	}
	num = c - '0';
	for (i = start + 1; i < str.GetLength(); ++i) {
		c = str.GetAt(i);
		if (c >= '0' && c <= '9')
			num = 10 * num + (int)(c - '0');
		else
			return i;
	}
	return i;
}

// convert double in CString, no negative number!
// return the index after the last digit, return -1 if failed
// CString.GetAt(start) must be a number
int CString2double(double& num, const CString& str, int start)
{
	int ind0{}, num1{}, num2{};
	ind0 = CString2int(num1, str, start);
	if (str.GetAt(ind0) != '.') {
		num = (double)num1;
		return ind0;
	}
	ind0 = CString2int(num2, str, ind0 + 1);
	num = num2;
	while (num >= 1)
		num /= 10;
	num += (double)num1;
	return ind0;
}

// see if a key appears followed only by only white space or '\n'
// return the next index, return -1 if nothing found.
int ExpectKey(const CString& str, CString key, int start)
{
	int ind = start;
	int ind0 = 0;
	int L = str.GetLength();
	int L0 = key.GetLength();
	TCHAR c0, c;
	while (true) {
		c0 = key.GetAt(ind0);
		c = str.GetAt(ind);
		if (c == c0) {
			++ind0;
			if (ind0 == L0)
				return ind + 1;
		}
		else if (c != ' ' && c != '\n')
			return -1;
		++ind;
		if (ind == L)
			return -1;
	}
}

// reverse version of Expect key
// return the next index, return -2 if nothing found.
int ExpectKeyReverse(const CString& str, CString key, int start)
{
	int ind = start;
	int L = str.GetLength();
	int L0 = key.GetLength();
	int ind0 = L0 - 1;
	TCHAR c0, c;
	while (true) {
		c0 = key.GetAt(ind0);
		c = str.GetAt(ind);
		if (c == c0) {
			--ind0;
			if (ind0 < 0)
				return ind - 1;
		}
		else if (c != ' ' && c != '\n')
			return -2;
		--ind;
		if (ind < 0)
			return -2;
	}
}

// delete any following ' ' or '\n' characters starting from "start" (including "start")
// return the number of characters deleted
// when option = 'r' (right), will only delete ' ' or '\n' at "start" or to the right
// when option = 'l' (left), will only delete ' ' or '\n' at "start" or to the left
// when option = 'a' (all), will delete both direction
int DeleteSpaceReturn(CString& str, int start, char option)
{
	int i{}, Nstr{}, left{start}, right{start};
	Nstr = str.GetLength();
	if (str.GetAt(start) != ' ' && str.GetAt(start) != '\n')
		return 0;
	if (option == 'r' || option == 'a') {
		for (i = start + 1; i < Nstr; ++i) {
			if (str.GetAt(i) == ' ' || str.GetAt(i) == '\n')
				continue;
			else {
				right = i - 1; break;
			}
		}
	}
	if (option == 'l' || option == 'a') {
		for (i = start - 1; i >= 0; --i) {
			if (str.GetAt(i) == ' ' || str.GetAt(i) == '\n')
				continue;
			else
				left = i + 1; break;
		}
	}
	str.Delete(left, right - left + 1);
	return right - left + 1;
}

// Pair right brace to left one (default)
// or () or [] or anying single character
// ind is inddex of left brace
// return index of right brace, -1 if failed
int PairBraceR(const CString& str, int ind, TCHAR type)
{
	TCHAR left, right;
	if (type == '{' || type == '}') {
		left = '{'; right = '}';
	}
	else if (type == '(' || type == ')') {
		left = '('; right = ')';
	}
	else if (type == '[' || type == ']') {
		left = '['; right = ']';
	}
	else {// anything else
		left = type; right = type;
	}

	TCHAR c, c0 = ' ';
	int Nleft = 1;
	for (int i{ ind + 1 }; i < str.GetLength(); i++)
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
