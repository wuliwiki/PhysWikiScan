#include "PhysWikiScan/PhysWikiScan.h"

int main(int argc, char *argv[]) {
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
	
	if (argc > 1) {
		if (strcmp(argv[1], ".") == 0) {
			PhysWikiOnline(path_in, path_out);
		}
		else if (strcmp(argv[1], "titles") == 0) { // update chinese titles
			// update entries.tex and titles.tex
			cout << "TODO: update entries.tex and titles.tex" << endl;
		}
		else if (strcmp(argv[1], "toc") == 0) {
			// update table of contents (index.html)
			cout << "TODO: update table of contents (index.html)" << endl;
		}
		else if (strcmp(argv[1], "toc-changed") == 0) {
			// update table of changed entries (changed.html) from data/changed.txt
			cout << "TODO: update table of changed entries (changed.html) from data/changed.txt" << endl;
		}
		else if (strcmp(argv[1], "autoref") == 0) {
			// check a label, add one if necessary
			cout << "TODO: check a label, add one if necessary" << endl;
		}
		else if (strcmp(argv[1], "entry") == 0) {
			// process a single entry
			cout << "TODO: process a single entry" << endl;
		}
		else {
			cout << "unknown command!" << endl;
		}
		return 0;
	}
	else {
		cout << "input \".\" for complete run, input [entry] for single run" << endl;
		Str entry; std::cin >> entry;
		if (entry == ".")
			PhysWikiOnline(path_in, path_out);
		else
			PhysWikiOnlineSingle(utf8to32(entry), path_in, path_out);
	}
	
	// PhysWikiCheck(U"../PhysWiki/contents/");
	cout << u8"按任意键退出..." << endl; getchar();
}
