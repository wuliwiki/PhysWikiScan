#pragma once

#include "str.h"
#include <nlohmann/json.hpp>

namespace slisc {

using nlohmann::json;

inline void str_diff_utf8_positions(vector<size_t> &pos, Str_I str)
{
	pos.clear();
	pos.reserve(str.size() + 1);
	pos.push_back(0);
	size_t offset = 0;
	auto it = str.begin();
	while (it != str.end()) {
		auto it0 = it;
		utf8::next(it, str.end());
		offset += size_t(it - it0);
		pos.push_back(offset);
	}
}

// calculate the difference between two strings using Myers diff
// diff: vector<(start, size, string)>
// str1.replace(start, size, string) from back to front will result in str2
inline void str_diff(vector<tuple<size_t, size_t, Str>> &diff, Str_I str1, Str_I str2, bool debug = false)
{
	diff.clear();
	SLS_ASSERT(is_valid(str1));
	SLS_ASSERT(is_valid(str2));
	if (str1 == str2)
		return;
	if (str1.empty() || str2.empty()) {
		diff.emplace_back(0, str1.size(), str2);
		if (debug) {
			Str tmp = str1;
			for (auto it = diff.rbegin(); it != diff.rend(); ++it)
				tmp.replace(get<0>(*it), get<1>(*it), get<2>(*it));
			if (tmp != str2) {
				cout<< "\n----- str1 ------------------\n"
					<< str1
					<< "\n----- str2 ------------------\n"
					<< str2
					<< "\n----- str2 reconstruct ------\n"
					<< tmp
					<< "\n-----------------------------\n" << endl;
				exit(1);
			}
		}
		return;
	}

	Str32 str1_32 = u32(str1);
	Str32 str2_32 = u32(str2);
	vector<size_t> pos1_bytes;
	vector<size_t> pos2_bytes;
	str_diff_utf8_positions(pos1_bytes, str1);
	str_diff_utf8_positions(pos2_bytes, str2);

	const size_t n = str1_32.size();
	const size_t m = str2_32.size();
	const Long maxd = Long(n + m);
	const Long offset = maxd;
	vector<Long> v(size_t(2 * maxd + 1), 0);
	vector<vector<Long>> trace;
	trace.reserve(size_t(maxd + 1));

	bool done = false;
	for (Long d = 0; d <= maxd; ++d) {
		for (Long k = -d; k <= d; k += 2) {
			const Long idx = offset + k;
			Long x;
			if (k == -d || (k != d && v[idx - 1] < v[idx + 1])) {
				x = v[idx + 1];
			}
			else {
				x = v[idx - 1] + 1;
			}
			Long y = x - k;
			while (x < Long(n) && y < Long(m) && str1_32[size_t(x)] == str2_32[size_t(y)]) {
				++x;
				++y;
			}
			v[idx] = x;
			if (x >= Long(n) && y >= Long(m)) {
				trace.push_back(v);
				done = true;
				break;
			}
		}
		if (done)
			break;
		trace.push_back(v);
	}

	vector<char> ops;
	ops.reserve(n + m);
	Long x = Long(n);
	Long y = Long(m);
	for (Long d = Long(trace.size()) - 1; d > 0; --d) {
		const Long k = x - y;
		const vector<Long> &v_prev = trace[size_t(d - 1)];
		Long k_prev;
		if (k == -d || (k != d && v_prev[offset + k - 1] < v_prev[offset + k + 1])) {
			k_prev = k + 1;
		}
		else {
			k_prev = k - 1;
		}
		const Long x_prev = v_prev[offset + k_prev];
		const Long y_prev = x_prev - k_prev;
		while (x > x_prev && y > y_prev) {
			ops.push_back('M');
			--x;
			--y;
		}
		if (x == x_prev) {
			ops.push_back('I');
			--y;
		}
		else {
			ops.push_back('D');
			--x;
		}
		x = x_prev;
		y = y_prev;
	}
	while (x > 0 && y > 0) {
		ops.push_back('M');
		--x;
		--y;
	}
	while (x > 0) {
		ops.push_back('D');
		--x;
	}
	while (y > 0) {
		ops.push_back('I');
		--y;
	}
	reverse(ops.begin(), ops.end());

	size_t pos1 = 0;
	size_t pos2 = 0;
	size_t start = 0;
	size_t del_len = 0;
	size_t insert_start = 0;
	size_t insert_len = 0;
	bool in_edit = false;
	for (char op : ops) {
		if (op == 'M') {
			if (in_edit) {
				size_t start_byte = pos1_bytes[start];
				size_t end_byte = pos1_bytes[start + del_len];
				size_t ins_start_byte = pos2_bytes[insert_start];
				size_t ins_end_byte = pos2_bytes[insert_start + insert_len];
				diff.emplace_back(start_byte, end_byte - start_byte,
					str2.substr(ins_start_byte, ins_end_byte - ins_start_byte));
				in_edit = false;
				del_len = 0;
				insert_len = 0;
			}
			++pos1;
			++pos2;
		}
		else if (op == 'D') {
			if (!in_edit) {
				start = pos1;
				in_edit = true;
				del_len = 0;
				insert_start = pos2;
				insert_len = 0;
			}
			++del_len;
			++pos1;
		}
		else {
			if (!in_edit) {
				start = pos1;
				in_edit = true;
				del_len = 0;
				insert_start = pos2;
				insert_len = 0;
			}
			++insert_len;
			++pos2;
		}
	}
	if (in_edit) {
		size_t start_byte = pos1_bytes[start];
		size_t end_byte = pos1_bytes[start + del_len];
		size_t ins_start_byte = pos2_bytes[insert_start];
		size_t ins_end_byte = pos2_bytes[insert_start + insert_len];
		diff.emplace_back(start_byte, end_byte - start_byte,
			str2.substr(ins_start_byte, ins_end_byte - ins_start_byte));
	}

	if (debug) {
		Str tmp = str1;
		for (auto it = diff.rbegin(); it != diff.rend(); ++it)
			tmp.replace(get<0>(*it), get<1>(*it), get<2>(*it));
		if (tmp != str2) {
			cout<< "\n----- str1 ------------------\n"
				<< str1
				<< "\n----- str2 ------------------\n"
				<< str2
				<< "\n----- str2 reconstruct ------\n"
				<< tmp
				<< "\n-----------------------------\n" << endl;
			exit(1);
		}
	}
}

// serialize diff to a JSON array of entries (UTF-8 payloads),
// one entry per line with no indentation:
// [
// [start, size, len, "payload"],
// ...
// ]
// where:
//   start = byte offset into original string (decimal)
//   size  = number of bytes replaced in original string (decimal)
//   len   = number of bytes in the replacement string (decimal)
// the JSON string handles escaping for the payload.
inline void str_diff_serialize(Str_O out, const vector<tuple<size_t, size_t, Str>> &diff)
{
	out.clear();
	out += "[";
	if (!diff.empty())
		out += "\n";
	for (size_t i = 0; i < diff.size(); ++i) {
		const auto &entry = diff[i];
		const Str &payload = get<2>(entry);
		SLS_ASSERT(is_valid(payload));
		json j_entry = json::array({get<0>(entry), get<1>(entry), payload.size(), payload});
		out += j_entry.dump();
		if (i + 1 < diff.size())
			out += ",\n";
		else
			out += "\n";
	}
	out += "]";
}

inline void str_diff_deserialize(vector<tuple<size_t, size_t, Str>> &diff, Str_I data)
{
	diff.clear();
	SLS_ASSERT(is_valid(data));
	if (data.empty())
		return;
	json j = json::parse(data);
	SLS_ASSERT(j.is_array());
	diff.reserve(j.size());
	for (const auto &entry : j) {
		SLS_ASSERT(entry.is_array());
		SLS_ASSERT(entry.size() == 4);
		size_t start = entry[0].get<size_t>();
		size_t size = entry[1].get<size_t>();
		size_t len = entry[2].get<size_t>();
		Str payload = entry[3].get<Str>();
		SLS_ASSERT(is_valid(payload));
		SLS_ASSERT(payload.size() == len);
		diff.emplace_back(start, size, payload);
	}
}

} // end namespace slisc
