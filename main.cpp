#include "PhysWikiScan/PhysWikiScan.h"

int main() {
	using namespace slisc;
	// input folder, put tex files and code files here
	// same directory structure with PhysWiki
	Str32 path_in;
	// output folder, put png, svg in here
	// html will be output to here
	Str32 path_out;

	read_file(path_in, "set_path.txt");
	CRLF_to_LF(path_in);
	Long ind0 = path_in.find(U'\n', 0) + 1;
	Long ind1 = path_in.find(U'\n', ind0);
	Long ind2 = path_in.find(U'\n', ind1 + 1) + 1;
	path_out = path_in.substr(ind2);
	path_in = path_in.substr(ind0, ind1 - ind0);
	trim(path_in, U" \n"); trim(path_out, U" \n");

	cout << u8"#===========================#" << endl;
	cout << u8"#   欢迎使用 PhysWikiScan   #" << endl;
	cout << u8"#===========================#\n" << endl;
	
	PhysWikiOnline(path_in, path_out);
	// PhysWikiCheck(U"../PhysWiki/contents/");

	cout << u8"按任意键退出..." << endl; getchar();
}
