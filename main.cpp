#include "SLISC/global.h"
#include "SLISC/unicode.h"
#include "SLISC/file.h"
#include "PhysWikiScan/PhysWikiScan.h"
#include "TeX/tex.h"
#include "TeX/tex2html.h"

int main() {
	using namespace slisc;
	Str32 path0 = U"../PhysWikiScanTest/PhysWikiOnline/";
	PhysWikiOnline(path0);
	// PhysWikiCheck(U"../PhysWiki/contents/");
	cout << "Program Finished!" << endl; getchar();
}
