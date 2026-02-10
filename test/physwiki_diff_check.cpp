#include "SLISC/str/str_diff_patch2.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <vector>

namespace fs = std::filesystem;
using namespace slisc;

static bool read_file(const fs::path &path, Str &out)
{
	std::ifstream in(path, std::ios::binary);
	if (!in)
		return false;
	out.assign((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
	return true;
}

static void write_file(const fs::path &path, Str_I data)
{
	std::ofstream out(path, std::ios::binary);
	SLS_ASSERT(out.good());
	out.write(data.data(), static_cast<std::streamsize>(data.size()));
}

static Str apply_diff(Str_I src, const vector<tuple<size_t, size_t, Str>> &diff)
{
	Str out = src;
	for (auto it = diff.rbegin(); it != diff.rend(); ++it)
		out.replace(get<0>(*it), get<1>(*it), get<2>(*it));
	return out;
}

static bool extract_suffix(const std::string &name, std::string &suffix)
{
	if (name.size() < 5 || name.substr(name.size() - 4) != ".tex")
		return false;
	const size_t pos = name.rfind('_');
	if (pos == std::string::npos || pos + 1 >= name.size() - 4)
		return false;
	suffix = name.substr(pos + 1, name.size() - 4 - (pos + 1));
	return true;
}

struct GroupInfo {
	std::string suffix;
	std::vector<fs::path> files;
	size_t max_size = 0;
};

static int run_for_group(const fs::path &dir, const GroupInfo &group)
{
	std::cout << "Processing _" << group.suffix << ".tex (max " << group.max_size << " bytes)" << std::endl;
	const auto &files = group.files;
	Str current;
	if (!read_file(files.front(), current)) {
		std::cerr << "Failed to read " << files.front() << "\n";
		return 1;
	}
	if (!is_valid(current)) {
		std::cerr << "Invalid UTF-8 in " << files.front() << "\n";
		return 1;
	}

	for (size_t i = 0; i + 1 < files.size(); ++i) {
		Str target;
		if (!read_file(files[i + 1], target)) {
			std::cerr << "Failed to read " << files[i + 1] << "\n";
			return 1;
		}
		if (!is_valid(target)) {
			std::cerr << "Invalid UTF-8 in " << files[i + 1] << "\n";
			return 1;
		}
		vector<tuple<size_t, size_t, Str>> diff;
		str_diff(diff, current, target);
		Str serialized;
		str_diff_serialize(serialized, diff);
		fs::path diff_path = dir / (files[i].stem().string() + "-" + files[i + 1].stem().string() + ".json");
		write_file(diff_path, serialized);
		vector<tuple<size_t, size_t, Str>> decoded;
		str_diff_deserialize(decoded, serialized);
		Str patched = apply_diff(current, decoded);
		if (patched != target) {
			std::cerr << "Mismatch after applying diff for group " << group.suffix << ":\n";
			std::cerr << files[i] << "\n" << files[i + 1] << "\n";
			return 1;
		}
		current.swap(patched);
	}

	Str last;
	if (!read_file(files.back(), last)) {
		std::cerr << "Failed to read " << files.back() << "\n";
		return 1;
	}
	if (current != last) {
		std::cerr << "Final reconstruction mismatch for group " << group.suffix << "\n";
		return 1;
	}

	std::cout << "Verified " << files.size() << " files for _" << group.suffix << ".tex" << std::endl;
	return 0;
}

int main()
{
	fs::path dir = "/mnt/g/github/PhysWiki-backup";
	std::map<std::string, GroupInfo> groups;
	for (const auto &entry : fs::directory_iterator(dir)) {
		if (!entry.is_regular_file())
			continue;
		const std::string name = entry.path().filename().string();
		std::string suffix;
		if (!extract_suffix(name, suffix))
			continue;
		if (suffix == "AU" || suffix == "llvmIR")
			continue;
		auto &group = groups[suffix];
		group.suffix = suffix;
		group.files.push_back(entry.path());
		const auto size = static_cast<size_t>(entry.file_size());
		if (size > group.max_size)
			group.max_size = size;
	}

	std::vector<GroupInfo> candidates;
	for (auto &kv : groups) {
		auto &group = kv.second;
		if (group.files.size() < 2)
			continue;
		candidates.push_back(group);
	}
	std::sort(candidates.begin(), candidates.end(), [](const GroupInfo &a, const GroupInfo &b) {
		if (a.max_size != b.max_size)
			return a.max_size < b.max_size;
		return a.suffix < b.suffix;
	});

	if (candidates.empty()) {
		std::cerr << "No groups with at least two files found." << std::endl;
		return 1;
	}
	if (candidates.size() > 100)
		candidates.resize(100);

	for (auto &group : candidates) {
		auto &files = group.files;
		std::sort(files.begin(), files.end());
		int rc = run_for_group(dir, group);
		if (rc != 0)
			return rc;
	}

	std::cout << "Processed " << candidates.size() << " groups." << std::endl;
	return 0;
}
