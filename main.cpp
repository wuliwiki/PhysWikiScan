#include "PhysWikiScan/PhysWikiScan.h"

int main() {
	using namespace slisc;
	// input folder, put tex files and code files here
	// same directory structure with PhysWiki
	Str32 path_in;
	// output folder, put png, svg in here
	// html will be output to here
	Str32 path_out;
	// secondary paths
	Str32 path_in2, path_out2;

	Str32 temp;
	read_file(temp, "set_path.txt");
	CRLF_to_LF(temp);
	Long ind0 = skip_line(temp, 0);
	ind0 = get_line(path_in, temp, ind0);
	ind0 = skip_line(temp, ind0);
	ind0 = get_line(path_out, temp, ind0);
	ind0 = skip_line(temp, ind0);
	ind0 = get_line(path_in2, temp, ind0);
	ind0 = skip_line(temp, ind0);
	ind0 = get_line(path_out2, temp, ind0);
	
	trim(path_in, U" \n"); trim(path_out, U" \n");

	cout << u8"#===========================#" << endl;
	cout << u8"#   欢迎使用 PhysWikiScan   #" << endl;
	cout << u8"#===========================#\n" << endl;
	
	PhysWikiOnline(path_in, path_out);
	// PhysWikiCheck(U"../PhysWiki/contents/");

	cout << u8"按任意键退出..." << endl; getchar();
}
