#pragma once
#include "global.h"
#include <sstream>
#include <fstream>
#include <codecvt>
#include <filesystem>
#include "string.h"

namespace slisc {

inline Bool file_exist(Str_I fname) {
	ifstream f(fname.c_str());
	return f.good();
}

inline void file_rm(Str_I wildcard_name) {
	system(("rm " + wildcard_name).c_str());
}

// list all files in current directory
// only works for linux
#ifdef __GNUC__
inline void file_list(vector<Str> &fnames, Str_I path)
{
	Str temp_fname, temp_fname_pref = "SLISC_temporary_file";

	// create unused temporary file name
	for (Long i = 0; i < 1000; ++i) {
		if (i == 999) error("too many temporary files!");
		temp_fname = temp_fname_pref + to_string(i);
		if (!file_exist(temp_fname)) break;
	}
	
	// save a list of all files (no folder) to temporary file
	system(("ls -p " + path + " | grep -v / > " + temp_fname).c_str());

	// read the temporary file
	ifstream fin(temp_fname);
	for (Long i = 0; i < 10000; ++i) {
		Str name;
		std::getline(fin, name);
		if (fin.eof())
			break;
		fnames.push_back(name);
	}
	fin.close();

	// remove temporary file
	std::remove(temp_fname.c_str());
}
#endif

// std::filesystem implementation of file_list()
// works in Visual Studio, not gcc 8
// directory example: "C:/Users/addis/Documents/GitHub/SLISC/"
#ifdef _MSC_VER
inline void file_list(vector<Str> &names, Str_I path)
{
	for (const auto & entry : std::filesystem::directory_iterator(path)) {
		std::stringstream ss;
		if (entry.is_directory())
			continue;
		auto temp = entry.path().filename();
		ss << temp;
		string str = ss.str();
		str = str.substr(1, str.size() - 2);
		// str = str.substr(path.size(), str.size()); // remove " " and path
		names.push_back(str);
	}
}
#endif

// choose files with a given extension from a list of files
inline void file_ext(vector<Str> &fnames_ext, const vector<Str> &fnames, Str_I ext, Bool_I keep_ext = true)
{
	fnames_ext.resize(0);
	Long N_ext = ext.size();
	for (Long i = 0; i < fnames.size(); ++i) {
		const Str & str = fnames[i];
		// check position of '.'
		Long ind = fnames[i].size() - N_ext - 1;
		if (ind < 0 || str[ind] != '.')
			continue;
		// match extension
		if (str.rfind(ext) != str.size() - ext.size())
			continue;
		if (keep_ext)
			fnames_ext.push_back(str);
		else
			fnames_ext.push_back(str.substr(0, str.size() - N_ext - 1));
	}
}

// list all files in current directory, with a given extension
inline void file_list_ext(vector<Str> &fnames, Str_I path, Str_I ext, Bool_I keep_ext = true)
{
	vector<Str> fnames0;
	file_list(fnames0, path);
	file_ext(fnames, fnames0, ext, keep_ext);
}

} // namespace slisc