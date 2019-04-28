#include "PhysWikiScan/PhysWikiScan.h"
#include "SLISC/inverv.h"

int main() {
	using namespace slisc;
	Str32 path0 = U"../PhysWikiScanTest/PhysWikiOnline/";

	cout << u8"#===========================#" << endl;
	cout << u8"#   欢迎使用 PhysWikiScan   #" << endl;
	cout << u8"#===========================#\n" << endl;
	
	PhysWikiOnline(path0);
	// PhysWikiCheck(U"../PhysWiki/contents/");
	cout << u8"按任意键退出..." << endl; getchar();
}
