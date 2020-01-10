#ifdef _MSC_VER
#define SLS_HAS_FILESYSTEM
#endif
#include "lib/PhysWikiScan.h"

// get arguments
void get_args(vecStr32_O args, Int_I argc, Char *argv[])
{
    args.clear();
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
        cout << u8"#===========================#" << endl;
        cout << u8"#     PhysWikiScan          #" << endl;
        cout << u8"#===========================#\n" << endl;

        cout << u8"请输入 arguments" << endl;
        Str temp; getline(std::cin, temp);
        Long ind0, ind1 = 0;
        ind0 = temp.find_first_not_of(' ', ind1);
        for (Int i = 0; i < 100; ++i) {
            ind1 = temp.find(' ', ind0);
            if (ind1 < 0) {
                if (size(temp) > ind0)
                    args.push_back(utf8to32(temp.substr(ind0)));
                break;
            }

            args.push_back(utf8to32(temp.substr(ind0, ind1 - ind0)));
            ind0 = temp.find_first_not_of(' ', ind1);
        }
    }
}

// read set_path.txt
// return paths_in.size()
Long read_path_file(vecStr32_O paths_in, vecStr32_O paths_out, vecStr32_O paths_data)
{
	Str32 temp, line;
	if (!file_exist("set_path.txt")) {
		throw Str32(U"内部错误： set_path.txt 不存在!");
	}
	read_file(temp, "set_path.txt");
	CRLF_to_LF(temp);

	Long ind0 = 0;
	for (Long i = 0; i < 100; ++i) {
        // paths_in
		ind0 = skip_line(temp, ind0);
		if (ind0 < 0) {
			throw Str32(U"内部错误： path.txt 格式 (a)");
		}
		ind0 = get_line(line, temp, ind0);
		if (ind0 < 0) {
			throw Str32(U"内部错误： path.txt 格式 (b)");
		}
		paths_in.push_back(line); trim(paths_in.back());

        // paths_out
		ind0 = skip_line(temp, ind0);
		if (ind0 < 0) {
			throw Str32(U"内部错误： path.txt 格式 (c)");
		}
        ind0 = get_line(line, temp, ind0);
        if (ind0 < 0) {
			throw Str32(U"内部错误： path.txt 格式 (b)");
		}
        paths_out.push_back(line); trim(paths_out.back());

        // paths_data
		ind0 = skip_line(temp, ind0);
		if (ind0 < 0) {
			throw Str32(U"内部错误： path.txt 格式 (c)");
		}
        ind0 = get_line(line, temp, ind0);
        paths_data.push_back(line); trim(paths_out.back());
		if (ind0 < 0) {
			break;
		}
	}
	return paths_in.size();
}

// get path and remove --path options from args
// return 0 if successful, return -1 if failed
Long get_path(Str32_O path_in, Str32_O path_out, Str32_O path_data, vecStr32_IO args)
{
    Long N = args.size();

	// directly specify path
	if (args.size() > 3 && args[N - 4] == U"--path-in-out-data") {
		path_in = args[N - 3];
		path_out = args[N - 2];
        path_data = args[N - 1];
		args.erase(args.begin() + N - 4, args.end());
		return 0;
	}

	// use path number in set_path.txt
	vecStr32 paths_in, paths_out, paths_data;
	if (read_path_file(paths_in, paths_out, paths_data) < 0)
		return -1;
    if (args.size() > 1 && args[N - 2] == U"--path") {
        size_t i = str2int(args[N - 1]);
        path_in = paths_in[i];
        path_out = paths_out[i];
        path_data = paths_data[i];
        args.pop_back(); args.pop_back();
    }
    else { // default path
        path_in = paths_in[0];
        path_out = paths_out[0];
        path_data = paths_data[0];
    }

	return 0;
}

int main(int argc, char *argv[]) {
#ifdef _MSC_VER
    SLS_WARN("gnu source-highlight is disabled in Visual Studio! Use VS for debug only!");
	cout << endl;
	cout << "=======================================" << endl;
	cout << "= Visual Studio 测试模式， 不含代码高亮 =" << endl;
	cout << "=======================================\n" << endl;
#endif
    using namespace slisc;

    vecStr32 args;
    get_args(args, argc, argv);

    // input folder, put tex files and code files here
    // same directory structure with PhysWiki
    Str32 path_in;
	// output folder, for html and images
    Str32 path_out;
    // data folder
    Str32 path_data;
    try {get_path(path_in, path_out, path_data, args);}
	catch (Str32_I msg) {
        cerr << msg << endl;
        return 0;
    }

    // === parse arguments ===

    if (args[0] == U"." && args.size() == 1) {
        // interactive full run (ask to try again in error)
        try {PhysWikiOnline(path_in, path_out, path_data);}
		catch (Str32_I msg) {
			cerr << msg << endl;
			return 0;
		}
    }
    else if (args[0] == U"--titles") {
        // update entries.txt and titles.txt
        vecStr32 titles, entries;
        try {entries_titles(titles, entries, path_in);}
		catch (Str32_I msg) {
            cerr << msg << endl;
            return 0;
        }
        write_vec_str(titles, U"data/titles.txt");
        write_vec_str(entries, U"data/entries.txt");
    }
    else if (args[0] == U"--toc" && args.size() == 1) {
        // table of contents
        // read entries.txt and titles.txt, then generate index.html from PhysWiki.tex
        vecStr32 titles, entries;
        if (file_exist(U"data/titles.txt"))
            read_vec_str(titles, U"data/titles.txt");
        if (file_exist(U"data/entries.txt"))
            read_vec_str(entries, U"data/entries.txt");
        if (titles.size() != entries.size()) {
            cerr << U"内部错误： titles.txt 和 entries.txt 行数不同!" << endl;
            return 0;
        }
        try {table_of_contents(titles, entries, path_in, path_out);}
		catch (Str32_I msg) {
            cerr << msg << endl;
            return 0;
        }
    }
    else if (args[0] == U"--toc-changed" && args.size() == 1) {
        // table of contents
        // read entries.txt and titles.txt, then generate changed.html from changed.txt
        vecStr32 titles, entries;
        if (file_exist(U"data/titles.txt"))
            read_vec_str(titles, U"data/titles.txt");
        if (file_exist(U"data/entries.txt"))
            read_vec_str(entries, U"data/entries.txt");
        if (titles.size() != entries.size()) {
            cerr << U"内部错误： titles.txt 和 entries.txt 行数不同!" << endl;
            return 0;
        }
        try {table_of_changed(titles, entries, path_in, path_out, path_data);}
		catch (Str32_I msg) {
            cerr << msg << endl;
            return 0;
        }
    }
    else if (args[0] == U"--autoref" && args.size() == 4) {
        // check a label, add one if necessary
        vecStr32 labels, ids;
        if (file_exist(U"data/labels.txt")) {
            read_vec_str(labels, U"data/labels.txt");
            Long ind = find_repeat(labels);
            if (ind >= 0) {
                cerr << U"内部错误： labels.txt 存在重复：" + labels[ind] << endl;
                return 0;
            }
        }
        if (file_exist(U"data/ids.txt"))
            read_vec_str(ids, U"data/ids.txt");
        Str32 label;
		Long ret;
        try {ret = check_add_label(label, args[1], args[2],
            atoi(utf32to8(args[3]).c_str()), labels, ids, path_in);}
		catch (Str32_I msg) {
            cerr << msg << endl;
            return 0;
        }
        vecStr32 output;
        if (ret == 0) { // added
            Str32 id = args[2] + args[3];
            Long i = search(label, labels);
            if (i < 0) {
                labels.push_back(label);
                ids.push_back(id);
            }
            else {
                ids[i] = id;
            }
            write_vec_str(labels, U"data/labels.txt");
            write_vec_str(ids, U"data/ids.txt");
            output = { label, U"added" };
        }
        else // ret == 1, already exist
            output = {label, U"exist"};
        cout << output[0] << endl;
        cout << output[1] << endl;
        write_vec_str(output, U"data/autoref.txt");
    }
    else if (args[0] == U"--entry" && args.size() > 1) {
        // process a single entry
        vecStr32 entryN;
        Str32 temp;
        for (Int i = 1; i < args.size(); ++i) {
            temp = args[i];
            if (temp[0] == '-' && temp[1] == '-')
                break;
            entryN.push_back(temp);
        }
        try {PhysWikiOnlineN(entryN, path_in, path_out, path_data);}
		catch (Str32_I msg) {
            cerr << msg << endl;
            return 0;
        }
    }
    else if (args[0] == U"--all-commands") {
        vecStr32 commands;
        all_commands(commands, path_in + U"contents/");
        write_vec_str(commands, U"data/commands.txt");
    }
    else {
        cerr << U"内部错误： 命令不合法" << endl;
        return 0;
    }
    
    // PhysWikiCheck(U"../PhysWiki/contents/");

    cout << u8"done!" << endl;
    if (argc <= 1) {
        cout << u8"按任意键退出..." << endl;
        getchar();
    }

    return 0;
}
