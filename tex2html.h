#pragma once
#include <atlstr.h>
#include <vector>
#include "macrouniverseS.h"
#include "tex.h"

int EnsureSpace(CString name, CString& str, int start, int end);
int EqOmitTag(CString& str);
int ParagraphTag(CString& str);
int StarCommand(CString name, CString& str);
int VarCommand(CString name, CString& str, int maxVars);
int RoundSquareCommand(CString name, CString& str);
int MathFunction(CString name, CString& str);
int TextEscape(CString& str);
int NormalTextEscape(CString& str);
int Table(CString& str);