#pragma once

// search all commands
inline void all_commands(vecStr_O commands, Str_I in_path)
{
    vecStr fnames;
    Str str, name;
    file_list_ext(fnames, in_path, u8"tex");
    for (Long i = 0; i < size(fnames); ++i) {
        read(str, in_path + fnames[i]);
        Long ind0 = 0;
        while (1) {
            ind0 = str.find(u8"\\", ind0);
            if (ind0 < 0)
                break;
            command_name(name, str, ind0);
            if (!search(name, commands))
                commands.push_back(name);
            ++ind0;
        }
    }
}

inline void word_count()
{
    // count number of Chinese characters (including punc)
    vecStr entries;
    Str str;
    Long N = 0;
    SQLite::Database db(gv::path_data + "scan.db", SQLite::OPEN_READWRITE);
    get_column(entries, "entries", "id", db);
    for (Long i = 0; i < size(entries); ++i) {
        read(str, gv::path_in + "contents/" + entries[i] + u8".tex");
        rm_comments(str);
        for (Long j = 0; j < (Long) str.size(); ++j)
            if (is_chinese(str[j]) || is_chinese_punc(str[j]))
                ++N;
    }
    cout << u8"中文字符数 (含标点): " << N << endl;
}
