#pragma once

#include <atlstr.h>
#include <vector>

std::vector<CString> GetFileNames(CString path, CString extension);
void sort(std::vector<int>& x, std::vector<int>& ind);