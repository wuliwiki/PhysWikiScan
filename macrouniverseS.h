#pragma once
#include <vector>
#include <fstream>
#include <string>
#include <atlstr.h>
#include <iostream>

std::string to_utf8(CString cstr);
CString from_utf8(std::string str);
CString ReadUTF8(CString path);
void WriteUTF8(CString text, CString path);
int ReverseFind(const CString& key, const CString& str, int start);
int NextNoSpace(CString& c, const CString& str, int start);
int FindNum(const CString& str, int start);
int CString2int(int& num, const CString& str, int start);
int ExpectKey(const CString& str, CString key, int start);
int PairBraceR(const CString& str, int ind, TCHAR type = '{');
int MatchBraces(std::vector<int>& ind_left, std::vector<int>& ind_right,
	std::vector<int>& ind_RmatchL, CString& str, int start, int end);