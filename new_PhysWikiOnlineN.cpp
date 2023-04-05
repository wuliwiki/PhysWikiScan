// like PhysWikiOnline, but convert only specified files
// requires ids.txt and labels.txt output from `PhysWikiOnline()`
inline Long PhysWikiOnlineN(vecStr32_I entryN)
{
    SQLite::Database db(u8(gv::path_data + "scan.db"), SQLite::OPEN_READWRITE);
    SQLite::Statement stmt_select(db,
        R"(SELECT "caption", "keys", "pentry", "draft", "issues", "issueOther" from "entries" WHERE "id"=?;)");
    SQLite::Statement stmt_insert(db,
        R"(INSERT INTO "entries" ("id", "caption", "keys", "pentry", "draft", "issues", "issueOther") )"
        R"(VALUES (?, ?, ?, ?, ?, ?, ?);)");
    SQLite::Statement stmt_update(db,
        R"(UPDATE "entries" SET "caption"=?, "keys"=?, "pentry"=?, "draft"=?, "issues"=?, "issueOther"=? WHERE "id"=?;)");

    vecStr32 rules;  // for newcommand()
    define_newcommands(rules);

    // 1st loop through tex files
    // files are processed independently
    // `IdList` and `LabelList` are recorded for 2nd loop
    cout << u8"\n\n======  第 1 轮转换 ======\n" << endl;

    // main process
    vecLong links; Str32 db_title;
    for (Long i = 0; i < size(entryN); ++i) {
        stmt_select.bind(1, u8(entryN[i]));
        db_title = u32(stmt_select.getColumn(0));
        cout << std::left << entryN[i]
            << std::setw(20) << std::left << db_title << endl;
        VecChar not_used1(0); vecStr32 not_used2;
        Bool isdraft; vecStr32 keywords;
        vecStr32 img_ids, img_hashes; vecLong img_orders;
        PhysWikiOnline1(img_ids, img_orders, img_hashes, isdraft, keywords, ids, labels, links, entries, entry_order, titles, Ntoc, ind, rules, not_used1, not_used2);
    }

    // 2nd loop through tex files
    // deal with autoref
    // need `IdList` and `LabelList` from 1st loop
    cout << "\n\n\n" << u8"====== 第 2 轮转换 ======\n" << endl;

    Str32 html, fname;
    vecStr32 ids, labels;
    vector<vecStr32> not_used;
    for (Long i = 0; i < size(entryN); ++i) {
        cout << std::setw(10) << std::left << entryN[i]
            << std::setw(20) << std::left << db_title << endl;
        fname = gv::path_out + entryN[i] + ".html";
        read(html, fname + ".tmp"); // read html file
        // process \autoref and \upref
        autoref(not_used, ids, labels, entryN[i], html);
        write(html, fname); // save html file
        file_remove(u8(fname) + ".tmp");
    }
    cout << "\n\n" << endl;
    return 0;
}