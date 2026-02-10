#include "str_diff_patch2.h"

#include <cassert>

namespace slisc {

static Str apply_diff(Str_I src, const vector<tuple<size_t, size_t, Str>> &diff)
{
	Str out = src;
	for (auto it = diff.rbegin(); it != diff.rend(); ++it)
		out.replace(get<0>(*it), get<1>(*it), get<2>(*it));
	return out;
}

static void check_diff(Str_I src, Str_I dst)
{
	vector<tuple<size_t, size_t, Str>> diff;
	str_diff(diff, src, dst);
	for (size_t i = 1; i < diff.size(); ++i) {
		assert(get<0>(diff[i-1]) <= get<0>(diff[i]));
	}
	for (auto &entry : diff) {
		assert(get<0>(entry) + get<1>(entry) <= src.size());
	}
	Str patched = apply_diff(src, diff);
	if (patched != dst) {
		cout << "Patch failed\n";
		cout << "src: " << src << "\n";
		cout << "dst: " << dst << "\n";
		cout << "got: " << patched << "\n";
		assert(false);
	}
}

static void check_serialize(Str_I src, Str_I dst)
{
	vector<tuple<size_t, size_t, Str>> diff;
	str_diff(diff, src, dst);
	Str encoded;
	str_diff_serialize(encoded, diff);
	vector<tuple<size_t, size_t, Str>> decoded;
	str_diff_deserialize(decoded, encoded);
	assert(diff == decoded);
	Str patched = apply_diff(src, decoded);
	assert(patched == dst);
}

} // namespace slisc

int main()
{
	using namespace slisc;
	check_diff("hello", "hello");
	check_diff("abc", "abXc");
	check_diff("abc", "ac");
	check_diff("abc", "axc");
	check_diff("abc", "zabc");
	check_diff("abc", "ab");
	check_diff("", "abc");
	check_diff("abc", "");
	check_diff("The quick brown fox", "The fast red fox");
	check_diff("aaaaab", "aaaab");
	check_diff("kitten", "sitting");
	check_diff("abababab", "ababaXbab");
	check_diff("start middle end", "START middle END");
	check_diff(u8"héllo", u8"héllö");
	check_diff(u8"😀😃😄", u8"😀😄");
	check_serialize("a\nb\nc", "a\nB\nc");
	check_serialize("alpha", "alphabet");
	check_serialize("remove all", "");
	check_serialize("", "add all");
	check_serialize(u8"Καλημέρα", u8"Καλό βράδυ");
	return 0;
}
