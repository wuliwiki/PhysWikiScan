#ifdef _MSC_VER
#define SLS_HAS_FILESYSTEM
#endif
#include "lib/PhysWikiScan.h"

// get arguments
void get_args(vecStr_O args, Int_I argc, Char *argv[])
{
    args.clear();
    if (argc > 1) {
        // convert argv to args
        Str temp;
        for (Int i = 1; i < argc; ++i) {
            temp = argv[i];
            args.push_back(temp);
        }
    }
    else {
        // input args
        cout << "#===========================#" << endl;
        cout << "#     PhysWikiScan          #" << endl;
        cout << "#===========================#\n" << endl;

        cout << u8"请输入 arguments" << endl;
        Str temp; getline(std::cin, temp);
        Long ind0, ind1 = 0;
        ind0 = temp.find_first_not_of(' ', ind1);
        for (Int i = 0; i < 100; ++i) {
            ind1 = temp.find(' ', ind0);
            if (ind1 < 0) {
                if (size(temp) > ind0)
                    args.push_back(temp.substr(ind0));
                break;
            }

            args.push_back(temp.substr(ind0, ind1 - ind0));
            ind0 = temp.find_first_not_of(' ', ind1);
        }
    }
}

// read set_path.txt
// return paths_in.size()
Long read_path_file(vecStr_O paths_in, vecStr_O paths_out, vecStr_O paths_data, vecStr_O urls)
{
    Str temp, line;
    if (!file_exist("set_path.txt")) {
        throw internal_err(u8"set_path.txt 不存在!");
    }
    read(temp, "set_path.txt");
    CRLF_to_LF(temp);

    Long ind0 = 0;
    for (Long i = 0; i < 100; ++i) {
        // paths_in
        ind0 = skip_line(temp, ind0);
        if (ind0 < 0)
            throw internal_err(u8"path.txt 格式 (1)");
        ind0 = get_line(line, temp, ind0);
        if (ind0 < 0)
            throw internal_err(u8"path.txt 格式 (2)");
        paths_in.push_back(line); trim(paths_in.back());

        // paths_out
        ind0 = skip_line(temp, ind0);
        if (ind0 < 0)
            throw internal_err(u8"path.txt 格式 (3)");
        ind0 = get_line(line, temp, ind0);
        if (ind0 < 0)
            throw internal_err(u8"path.txt 格式 (4)");
        paths_out.push_back(line); trim(paths_out.back());

        // paths_data
        ind0 = skip_line(temp, ind0);
        if (ind0 < 0)
            throw internal_err(u8"path.txt 格式 (5)");
        ind0 = get_line(line, temp, ind0);
        if (ind0 < 0)
            throw internal_err(u8"path.txt 格式 (6)");
        paths_data.push_back(line); trim(paths_out.back());

        // urls
        ind0 = skip_line(temp, ind0);
        if (ind0 < 0)
            throw internal_err(u8"path.txt 格式 (7)");
        ind0 = get_line(line, temp, ind0);
        urls.push_back(line); trim(paths_out.back());

        if (ind0 < 0)
            break;
    }
    return paths_in.size();
}

// get path and remove --path options from args
void get_path(Str_O path_in, Str_O path_out, Str_O path_data, Str_O url, vecStr_IO args)
{
    Long N = args.size();

    // directly specify path (depricated)
    if (args.size() > 3 && args[N - 4] == "--path-in-out-data") {
        path_in = args[N - 3];
        path_out = args[N - 2];
        path_data = args[N - 1];
        url = "";
        args.erase(args.begin() + N - 4, args.end());
        return;
    }

    // directly specify paths
    if (N > 5 && args[N - 5] == "--path-in-out-data-url") {
        path_in = args[N - 4];
        path_out = args[N - 3];
        path_data = args[N - 2];
        url = args[N - 1];
        args.erase(args.begin() + N - 5, args.end());
        return;
    }

    // use path number in set_path.txt
    vecStr paths_in, paths_out, paths_data, urls;
    read_path_file(paths_in, paths_out, paths_data, urls);

    if (args.size() > 1 && args[N - 2] == "--path") {
        size_t i = str2int(args[N - 1]);
        path_in = paths_in[i];
        path_out = paths_out[i];
        path_data = paths_data[i];
        url = urls[i];
        args.pop_back(); args.pop_back();
    }
    else { // default path
        path_in = paths_in[0];
        path_out = paths_out[0];
        path_data = paths_data[0];
        url = urls[0];
    }
}

inline void replace_eng_punc_to_chinese(Str_I path_in)
{
    vecStr names, str_verb;
    Str fname, str;
    Intvs intv;
    file_list_ext(names, path_in + "contents/", "tex", false);

    //RemoveNoEntry(names);
    if (names.size() <= 0) return;
    //names.resize(0); names.push_back("Sample"));

    vecStr skip_list = { "Sample", "edTODO" };
    for (unsigned i{}; i < names.size(); ++i) {
        cout << i << " ";
        cout << names[i] << "...";
        if (search(names[i], skip_list) >= 0)
            continue;
        // main process
        fname = path_in + "contents/" + names[i] + ".tex";
        read(str, fname);
        CRLF_to_LF(str);
        // verbatim(str_verb, str);
        // TODO: hide comments then recover at the end
        // find_comments(intv, str, "%");
        Long N;
        try { N = check_normal_text_punc(str, false, true); }
        catch (...) { continue; }

        // verb_recover(str, str_verb);
        if (N > 0)
            write(str, fname);
        cout << endl;
    }
}

int main(int argc, char *argv[]) {
    using namespace slisc;
    Timer timer;
    vecStr args;
    get_args(args, argc, argv);
    timer.tic();

    get_path(gv::path_in, gv::path_out, gv::path_data, gv::url, args);
    if (gv::url == "https://wuli.wiki/online/" || gv::url == "https://wuli.wiki/changed/")
        gv::is_wiki = true;
    else
        gv::is_wiki = false;
    if (gv::path_in.substr(gv::path_in.size() - 4) == "/en/")
        gv::is_eng = true;

    // === parse arguments ===
    try {
        if (args[0] == "." && args.size() == 1)
            PhysWikiOnline();
        else if (args[0] == "--titles") {
            // update entries.txt and titles.txt
            vecStr titles, entries, isDraft;
            entries_titles(titles, entries);
            write_vec_str(titles, gv::path_data + "titles.txt");
            write_vec_str(entries, gv::path_data + "entries.txt");
        }
        else if (args[0] == "--toc" && args.size() == 1)
            arg_toc();
        else if (args[0] == "--wc" && args.size() == 1)
            word_count();
        else if (args[0] == "--inline-eq-space")
            // check format and auto correct .tex files
            add_space_around_inline_eq(gv::path_in);
        else if (args[0] == ",")
            // check format and auto correct .tex files
            replace_eng_punc_to_chinese(gv::path_in);
        else if (args[0] == "--autoref" && args.size() == 4) {
            // check a label, add one if necessary
            // args: [1]: entry, [2]: eq/fig/etc, [3]: disp_num
            Str label;
            Str fname = gv::path_data + "autoref.txt";
            file_remove(fname);
            Long ret = check_add_label(label, args[1], args[2], atoi(args[3].c_str()));
            vecStr output;
            if (ret == 0) { // added
                Str id = args[2] + args[3];
                output = {label, "added"};
            } else // ret == 1, already exist
                output = {label, "exist"};
            cout << output[0] << endl;
            cout << output[1] << endl;
            write_vec_str(output, fname);
        } else if (args[0] == "--autoref-dry" && args.size() == 4) {
            // check a label only, without adding
            Str label;
            Str fname = gv::path_data + "autoref.txt";
            file_remove(fname);
            Long ret = check_add_label(label, args[1], args[2], atoi(args[3].c_str()), true);
            vecStr output;
            if (ret == 0) // added
                output = {label, "added"};
            else // ret == 1, already exist
                output = {label, "exist"};
            cout << output[0] << endl;
            cout << output[1] << endl;
            write_vec_str(output, fname);
        } else if (args[0] == "--entry" && args.size() > 1) {
            // process specified entries
            vecStr entries;
            Str arg;
            for (Long i = 1; i < size(args); ++i) {
                arg = args[i];
                if (arg[0] == '-' && arg[1] == '-')
                    break;
                entries.push_back(arg);
            }
            PhysWikiOnlineN(entries);
        } else if (args[0] == "--bib")
            arg_bib();
        else if (args[0] == "--history" && args.size() <= 2) {
            Str path;
            if (args.size() == 2) {
                path = args[1];
                assert(path[path.size() - 1] == '/');
            } else
                path = "../PhysWiki-backup/";
            arg_history(path);
        } else if (args[0] == "--fix-db" && args.size() == 1) {
            arg_fix_db();
        } else if (args[0] == "--hide" && args.size() > 1) {
            Str str, fname = gv::path_in + "contents/" + args[1] + ".tex";
            read(str, fname);
            CRLF_to_LF(str);
            hide_eq_verb(str);
            write(str, fname);
        } else if (args[0] == "--unhide" && args.size() > 1) {
            Str str, fname = gv::path_in + "contents/" + args[1] + ".tex";
            read(str, fname);
            CRLF_to_LF(str);
            unhide_eq_verb(str);
            write(str, fname);
        } else if (args[0] == "--all-commands") {
            vecStr commands;
            all_commands(commands, gv::path_in + "contents/");
            write_vec_str(commands, gv::path_data + "commands.txt");
        } else {
            cerr << u8"内部错误： 命令不合法" << endl;
            return 0;
        }

        // PhysWikiCheck("../PhysWiki/contents/");
        cout.precision(3);
        cout << "done! time (s): " << timer.toc() << endl;
        if (argc <= 1) {
            cout << u8"按任意键退出..." << endl;
            Char c = getchar();
            ++c;
        }
    }
    catch (const std::exception &e) {
        cerr << e.what() << endl;
        return 0;
    }
    catch (...) {
        cerr << u8"内部错误： 不要 throw 除了 scan_err 和 internal_err 之外的东西！" << endl;
    }

    return 0;
}
