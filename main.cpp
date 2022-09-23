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
Long read_path_file(vecStr32_O paths_in, vecStr32_O paths_out, vecStr32_O paths_data, vecStr32_O urls)
{
    Str32 temp, line;
    if (!file_exist("set_path.txt")) {
        throw Str32(U"内部错误： set_path.txt 不存在!");
    }
    read(temp, "set_path.txt");
    CRLF_to_LF(temp);

    Long ind0 = 0;
    for (Long i = 0; i < 100; ++i) {
        // paths_in
        ind0 = skip_line(temp, ind0);
        if (ind0 < 0)
            throw Str32(U"内部错误： path.txt 格式 (1)");
        ind0 = get_line(line, temp, ind0);
        if (ind0 < 0)
            throw Str32(U"内部错误： path.txt 格式 (2)");
        paths_in.push_back(line); trim(paths_in.back());

        // paths_out
        ind0 = skip_line(temp, ind0);
        if (ind0 < 0)
            throw Str32(U"内部错误： path.txt 格式 (3)");
        ind0 = get_line(line, temp, ind0);
        if (ind0 < 0)
            throw Str32(U"内部错误： path.txt 格式 (4)");
        paths_out.push_back(line); trim(paths_out.back());

        // paths_data
        ind0 = skip_line(temp, ind0);
        if (ind0 < 0)
            throw Str32(U"内部错误： path.txt 格式 (5)");
        ind0 = get_line(line, temp, ind0);
        if (ind0 < 0)
            throw Str32(U"内部错误： path.txt 格式 (6)");
        paths_data.push_back(line); trim(paths_out.back());

        // urls
        ind0 = skip_line(temp, ind0);
        if (ind0 < 0)
            throw Str32(U"内部错误： path.txt 格式 (7)");
        ind0 = get_line(line, temp, ind0);
        urls.push_back(line); trim(paths_out.back());

        if (ind0 < 0)
            break;
    }
    return paths_in.size();
}

// get path and remove --path options from args
void get_path(Str32_O path_in, Str32_O path_out, Str32_O path_data, Str32_O url, vecStr32_IO args)
{
    Long N = args.size();

    // directly specify path (depricated)
    if (args.size() > 3 && args[N - 4] == U"--path-in-out-data") {
        path_in = args[N - 3];
        path_out = args[N - 2];
        path_data = args[N - 1];
        url = U"";
        args.erase(args.begin() + N - 4, args.end());
        return;
    }

    // directly specify paths
    if (N > 5 && args[N - 5] == U"--path-in-out-data-url") {
        path_in = args[N - 4];
        path_out = args[N - 3];
        path_data = args[N - 2];
        url = args[N - 1];
        args.erase(args.begin() + N - 5, args.end());
        return;
    }

    // use path number in set_path.txt
    vecStr32 paths_in, paths_out, paths_data, urls;
    read_path_file(paths_in, paths_out, paths_data, urls);

    if (args.size() > 1 && args[N - 2] == U"--path") {
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

inline void replace_eng_punc_to_chinese(Str32_I path_in)
{
    vecStr32 names, str_verb;
    Str32 fname, str;
    Intvs intv;
    file_list_ext(names, path_in + "contents/", U"tex", false);

    //RemoveNoEntry(names);
    if (names.size() <= 0) return;
    //names.resize(0); names.push_back(U"Sample"));

    vecStr32 skip_list = { U"Sample", U"edTODO" };
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
    vecStr32 args;
    get_args(args, argc, argv);
    timer.tic();

    try {get_path(gv::path_in, gv::path_out, gv::path_data, gv::url, args);}
    catch (Str32_I msg) {
        cerr << utf32to8(msg) << endl;
        return 0;
    }
    if (gv::url == U"https://wuli.wiki/online/" || gv::url == U"https://wuli.wiki/changed/")
        gv::is_wiki = true;
    else
        gv::is_wiki = false;
    if (gv::path_in.substr(gv::path_in.size() - 4) == U"/en/")
        gv::is_eng = true;

    // === parse arguments ===

    if (args[0] == U"." && args.size() == 1) {
        gv::is_entire = true;
        // remove matlab files
        vecStr fnames;
        ensure_dir(utf32to8(gv::path_out) + "code/matlab/");
        file_list_full(fnames, utf32to8(gv::path_out) + "code/matlab/");
        for (Long i = 0; i < size(fnames); ++i)
            file_remove(fnames[i]);
        // interactive full run (ask to try again in error)
        try {PhysWikiOnline();}
        catch (Str32_I msg) {
            cerr << utf32to8(msg) << endl;
            return 0;
        }
    }
    else if (args[0] == U"--titles") {
        // update entries.txt and titles.txt
        vecStr32 titles, entries, isDraft; VecLong entry_order;
        Long Ntoc; // number of entries in main.tex
        try { Ntoc = entries_titles(titles, entries, isDraft, entry_order); }
        catch (Str32_I msg) {
            cerr << utf32to8(msg) << endl;
            return 0;
        }
        write_vec_str(titles, gv::path_data + U"titles.txt");
        write_vec_str(entries, gv::path_data + U"entries.txt");
        if (gv::is_wiki)
            write_vec_str(isDraft, gv::path_data + U"isDraft.txt");
        file_remove(utf32to8(gv::path_data) + "entry_order.matt");
        Matt matt(utf32to8(gv::path_data) + "entry_order.matt", "w");
        save(entry_order, "entry_order", matt); save(Ntoc, "Ntoc", matt);
    }
    else if (args[0] == U"--toc" && args.size() == 1) {
        // table of contents
        // read entries.txt and titles.txt, then generate index.html from main.tex
        vecStr32 titles, entries, isDraft;
        if (file_exist(gv::path_data + U"titles.txt"))
            read_vec_str(titles, gv::path_data + U"titles.txt");
        if (file_exist(gv::path_data + U"entries.txt"))
            read_vec_str(entries, gv::path_data + U"entries.txt");
        if (file_exist(gv::path_data + U"is_draft.txt"))
            read_vec_str(isDraft, gv::path_data + U"is_draft.txt");
        if (titles.size() != entries.size()) {
            cerr << u8"内部错误： titles.txt 和 entries.txt 行数不同!" << endl;
            return 0;
        }
        if (entries.size() != isDraft.size()) 
            isDraft.resize(entries.size(), U"1");
        vecStr32 not_used1, not_used3;
        vecLong not_used2, not_used4;
        try {table_of_contents(not_used1, not_used2, not_used3, not_used4,
            entries, isDraft);}
        catch (Str32_I msg) {
            cerr << utf32to8(msg) << endl;
            return 0;
        }
    }
    else if (args[0] == U"--toc-changed" && args.size() == 1) {
        SLS_WARN(u8"--toc-changed 选项已被弃用， 将自动忽略该命令");
        return 0;
        // table of contents
        // read entries.txt and titles.txt, then generate changed.html from changed.txt
        /*vecStr32 titles, entries;
        if (file_exist(path_data + U"titles.txt"))
            read_vec_str(titles, path_data + U"titles.txt");
        if (file_exist(path_data + U"entries.txt"))
            read_vec_str(entries, path_data + U"entries.txt");
        if (titles.size() != entries.size()) {
            cerr << u8"内部错误： titles.txt 和 entries.txt 行数不同!" << endl;
            return 0;
        }
        try {table_of_changed(titles, entries, path_in, path_out, path_data);}
        catch (Str32_I msg) {
            cerr << utf32to8(msg) << endl;
            return 0;
        }*/
    }
    else if (args[0] == U"--clean" && args.size() == 1) {
        // clear changed record
        write(U"", gv::path_data + U"changed.txt");
        write(U"", gv::path_data + U"authors.txt");
    }
    else if (args[0] == U"--wc" && args.size() == 1) {
        // count number of Chinese characters (including punc)
        vecStr32 entries;
        Str32 str;
        Long N = 0;
        read_vec_str(entries, U"./data/entries.txt");
        for (Long i = 0; i < (Long)entries.size(); ++i) {
            read(str, gv::path_in + "contents/" + entries[i] + U".tex");
            rm_comments(str);
            for (Long j = 0; j < (Long)str.size(); ++j)
                if (is_chinese(str[j]) || is_chinese_punc(str[j]))
                    ++N;
        }
        cout << u8"中文字符数 (含标点): " << N << endl;
    }
    else if (args[0] == U"--inline-eq-space") {
        // check format and auto correct .tex files
        add_space_around_inline_eq(gv::path_in);
    }
    else if (args[0] == U",") {
        // check format and auto correct .tex files
        replace_eng_punc_to_chinese(gv::path_in);
    }
    else if (args[0] == U"--autoref" && args.size() == 4) {
        // check a label, add one if necessary
        vecStr32 labels, ids;
        if (file_exist(gv::path_data + U"labels.txt")) {
            read_vec_str(labels, gv::path_data + U"labels.txt");
            Long ind = find_repeat(labels);
            if (ind >= 0) {
                cerr << u8"内部错误： labels.txt 存在重复：" + labels[ind] << endl;
                return 0;
            }
        }
        if (file_exist(gv::path_data + U"ids.txt"))
            read_vec_str(ids, gv::path_data + U"ids.txt");
        Str32 label;
        Long ret;
        try {ret = check_add_label(label, args[1], args[2],
            atoi(utf32to8(args[3]).c_str()), labels, ids);}
        catch (Str32_I msg) {
            cerr << utf32to8(msg) << endl;
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
            write_vec_str(labels, gv::path_data + U"labels.txt");
            write_vec_str(ids, gv::path_data + U"ids.txt");
            output = { label, U"added" };
        }
        else // ret == 1, already exist
            output = {label, U"exist"};
        cout << output[0] << endl;
        cout << output[1] << endl;
        write_vec_str(output, gv::path_data + U"autoref.txt");
    }
    else if (args[0] == U"--autoref-dry" && args.size() == 4) {
        // check a label only, without adding
        vecStr32 labels, ids;
        if (file_exist(gv::path_data + U"labels.txt")) {
            read_vec_str(labels, gv::path_data + U"labels.txt");
            Long ind = find_repeat(labels);
            if (ind >= 0) {
                cerr << u8"内部错误： labels.txt 存在重复：" + labels[ind] << endl;
                return 0;
            }
        }
        if (file_exist(gv::path_data + U"ids.txt"))
            read_vec_str(ids, gv::path_data + U"ids.txt");
        Str32 label;
        Long ret;
        try {
            ret = check_add_label_dry(label, args[1], args[2],
                atoi(utf32to8(args[3]).c_str()), labels, ids);
        }
        catch (Str32_I msg) {
            cerr << utf32to8(msg) << endl;
            return 0;
        }
        vecStr32 output;
        if (ret == 0) // added
            output = { label, U"added" };
        else // ret == 1, already exist
            output = { label, U"exist" };
        cout << output[0] << endl;
        cout << output[1] << endl;
        write_vec_str(output, gv::path_data + U"autoref.txt");
    }
    else if (args[0] == U"--entry" && args.size() > 1) {
        // process a single entry
        vecStr32 entryN;
        Str32 temp;
        for (Int i = 1; i < size(args); ++i) {
            temp = args[i];
            if (temp[0] == '-' && temp[1] == '-')
                break;
            entryN.push_back(temp);
        }
        try {PhysWikiOnlineN(entryN);}
        catch (Str32_I msg) {
            cerr << utf32to8(msg) << endl;
            return 0;
        }
    }
    else if (args[0] == U"--bib") {
        // process bibliography
        vecStr32 bib_labels, bib_details;
        try { bibliography(bib_labels, bib_details); }
        catch (Str32_I msg) {
            cerr << utf32to8(msg) << endl;
            return 0;
        }
        write_vec_str(bib_labels, gv::path_data + U"bib_labels.txt");
        write_vec_str(bib_details, gv::path_data + U"bib_details.txt");
    }
    else if (args[0] == U"--hide" && args.size() > 1) {
        Str32 str, fname = gv::path_in + U"contents/" + args[1] + U".tex";
        read(str, fname); CRLF_to_LF(str);
        hide_eq_verb(str);
        write(str, fname);
    }
    else if (args[0] == U"--unhide" && args.size() > 1) {
        Str32 str, fname = gv::path_in + U"contents/" + args[1] + U".tex";
        read(str, fname); CRLF_to_LF(str);
        unhide_eq_verb(str);
        write(str, fname);
    }
    else if (args[0] == U"--all-commands") {
        vecStr32 commands;
        all_commands(commands, gv::path_in + U"contents/");
        write_vec_str(commands, gv::path_data + U"commands.txt");
    }
    else {
        cerr << u8"内部错误： 命令不合法" << endl;
        return 0;
    }
    
    // PhysWikiCheck(U"../PhysWiki/contents/");
    cout.precision(3);
    cout << u8"done! time (s): " << timer.toc() << endl;
    if (argc <= 1) {
        cout << u8"按任意键退出..." << endl;
        Char c = getchar(); ++c;
    }

    return 0;
}
