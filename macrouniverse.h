#pragma once

#include <atlstr.h>
#include <vector>

std::vector<CString> GetFileNames(CString path, CString extension, bool ext = true);
bool FileExist(const CString& path, const CString& name);
void sort(std::vector<int>& x, std::vector<int>& ind);