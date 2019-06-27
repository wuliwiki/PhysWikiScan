#include "PhysWikiScan/PhysWikiScan.h"

void get_path(Str32_O path_in, Str32_O path_out, vector_I<Str32> args)
{
	Str32 temp, line;
	read_file(temp, "set_path.txt");
	CRLF_to_LF(temp);
	vector<Str32> paths_in, paths_out;
	Long ind0;
	for (Long i = 0; i < 100; ++i) {
		ind0 = skip_line(temp, 0);
		ind0 = get_line(line, temp, ind0);
		if (line.empty() || ind0 < 0)
			break;
		paths_in.push_back(line);
		ind0 = skip_line(temp, ind0);
		ind0 = get_line(line, temp, ind0);
		paths_out.push_back(line);
	}

	Long N = args.size();
	if (args.size() > 1 && args[N - 2] == U"--path") {
		if (args[N - 1] == U"0") {
			path_in = paths_in[0];
			path_out = paths_out[0];
		}
		else if (args[N - 1] == U"1") {
			path_in = paths_in[1];
			path_out = paths_out[1];
		}
		else if (args[N - 1] == U"2") {
			path_in = paths_in[2];
			path_out = paths_out[2];
		}
		else
			SLS_ERR("illegal --path argument");
	}
	else { // default path
		path_in = paths_in[0];
		path_out = paths_out[0];
	}

	trim(path_in, U" \n"); trim(path_out, U" \n");
}

int main(int argc, char *argv[]) {
	using namespace slisc;
	// input folder, put tex files and code files here
	// same directory structure with PhysWiki
	Str32 path_in;
	// output folder, put png, svg in here
	// html will be output to here
	Str32 path_out;
	
	cout << u8"#===========================#" << endl;
	cout << u8"#     PhysWikiScan          #" << endl;
	cout << u8"#===========================#\n" << endl;

	vector<Str32> args;
	if (argc > 1) {
		// convert argv to args
		Str temp;
		for (Int i = 1; i < argc; ++i) {
			temp = argv[i];
			args.push_back(utf8to32(temp));
		}
	}
	else {
		// input args
		cout << u8"请输入 PhysWikiScan 的 arguments" << endl;
		Str temp; getline(std::cin, temp);
		Long ind0, ind1 = 0;
		ind0 = temp.find_first_not_of(' ', ind1);
		for (Int i = 0; i < 100; ++i) {
			ind1 = temp.find(' ', ind0);
			if (ind1 < 0) {
				if (temp.size() > ind0)
					args.push_back(utf8to32(temp.substr(ind0)));
				break;
			}
				
			args.push_back(utf8to32(temp.substr(ind0, ind1 - ind0)));
			ind0 = temp.find_first_not_of(' ', ind1);
		}
	}

	get_path(path_in, path_out, args);

	if (args[0] == U"." && args.size() == 1) {
		// interactive full run (ask to try again in error)
		PhysWikiOnline(path_in, path_out);
	}
	else if (args[0] == U"--titles") {
		// update entries.tex and titles.tex
		vector<Str32> titles, entries;
		if (entries_titles(titles, entries, path_in, path_out) < 0) {
			cout << err_msg << endl;
			write_file("err\n" + err_msg, "data/status.txt");
			exit(EXIT_FAILURE);
		}
		write_vec_str(titles, U"data/titles.txt");
		write_vec_str(entries, U"data/entries.txt");
	}
	else if (args[0] == U"--toc") {
		// update entries.tex and titles.tex
		vector<Str32> titles, entries;
		Long ret1 = entries_titles(titles, entries, path_in, path_out);
		write_vec_str(titles, U"data/titles.txt");
		write_vec_str(entries, U"data/entries.txt");
		Long ret2 = table_of_contents(titles, entries, path_in, path_out);
		if (ret1 < 0 || ret2 < 0) {
			cout << err_msg << endl;
			write_file("err\n" + err_msg, "data/status.txt");
			exit(EXIT_FAILURE);
		}
	}
	else if (args[0] == U"--toc-changed") {
		// update table of changed entries (changed.html) from data/changed.txt
		SLS_ERR("TODO: update table of changed entries (changed.html) from data/changed.txt");
	}
	else if (args[0] == U"--autoref") {
		// check a label, add one if necessary
		SLS_ERR("TODO: check a label, add one if necessary");
	}
	else if (args[0] == U"--entry") {
		// process a single entry
		vector<Str32> entryN;
		Str32 temp;
		for (Int i = 1; i < args.size(); ++i) {
			temp = args[i];
			if (temp[0] == '-' && temp[1] == '-')
				break;
			entryN.push_back(temp);
		}
		if (PhysWikiOnlineN(entryN, path_in, path_out) < 0) {
			cout << err_msg << endl;
			write_file("err\n" + err_msg, "data/status.txt");
			exit(EXIT_FAILURE);
		}
	}
	else {
		cout << "illegal command!" << endl;
	}
	return 0;
	
	// PhysWikiCheck(U"../PhysWiki/contents/");
	cout << u8"按任意键退出..." << endl; getchar();
}
