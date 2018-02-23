#include "macrouniverse.h"
using namespace std;

// search all file names of a certain file extension
// path must end with "\\"
// if ext == false, does not include extension in return
vector<CString> GetFileNames(CString path, CString extension, bool ext)
{
	int extLen{};
	vector<CString> names;
	CString file, name;
	WIN32_FIND_DATA data;
	HANDLE hfile;
	file = path + _T("*.") + extension;
	hfile = FindFirstFile(file, &data);
	if (data.cFileName[0] == 52428) // not found
		return names;
	name = data.cFileName;
	if (!ext) {
		extLen = extension.GetLength();
		name.Delete(name.GetLength() - extLen - 1, extLen + 1);
	}
	names.push_back(name);
	while (true) {
		FindNextFile(hfile, &data);
		name = data.cFileName;
		if (!ext) {
			name.Delete(name.GetLength() - extLen - 1, extLen + 1);
		}
		if (!name.Compare(names.back())) //if name == names.back()
			break;
		else {
			names.push_back(name);
		}
	}
	return names;
}

// check if a file exist in a path
// path must end with "\\"
// name must include extension
bool FileExist(const CString& path, const CString& name)
{
	CString file;
	WIN32_FIND_DATA data;
	HANDLE hfile;
	file = path + name;
	hfile = FindFirstFile(file, &data);
	if (data.cFileName[0] == 52428) // not found
		return false;
	else
		return true;
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