#pragma once
#include <vector>
#include <atlstr.h>
#include <algorithm>
#include "macrouniverse.h"
#include "macrouniverseS.h"

bool IndexInRange(int i, std::vector<int> ind);
int CombineRange(std::vector<int>& ind, std::vector<int> ind1, std::vector<int> ind2);
int InvertRange(std::vector<int>& ind, const std::vector<int>& ind0, int Nstr);
int FindComment(std::vector<int>& ind, const CString& str);
int FindEnv(std::vector<int>& ind, const CString& str, CString env, char option = 'i');
int FindInline(std::vector<int>& ind, const CString& str, char option = 'i');
int FindComBrace(std::vector<int>& ind, const CString& key, const CString& str, char option = 'i');
int FindNormalText(std::vector<int>& indNorm, CString& str);
int RemoveBraces(std::vector<int>& ind_left, std::vector<int>& ind_right,
	std::vector<int>& ind_RmatchL, CString& str);
int Command2Tag(CString nameComm, CString strLeft, CString strRight, CString& str);
int Env2Tag(CString nameEnv, CString strLeft, CString strRight, CString& str);