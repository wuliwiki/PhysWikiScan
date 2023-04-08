#pragma once

// get table of content info from db "chapters", in ascending "order"
inline void db_get_chapters(vecStr32_O ids, vecStr32_O captions, vecStr32_O parts,
                            SQLite::Database &db)
{
    SQLite::Statement stmt(db,
                           R"(SELECT "id", "order", "caption", "part" FROM "chapters" ORDER BY "order" ASC)");
    vecLong orders;
    while (stmt.executeStep()) {
        ids.push_back(u32(stmt.getColumn(0)));
        orders.push_back((int)stmt.getColumn(1));
        captions.push_back(u32(stmt.getColumn(2)));
        parts.push_back(u32(stmt.getColumn(3)));
    }
    for (Long i = 0; i < size(orders); ++i)
        if (orders[i] != i)
            SLS_ERR("something wrong!");
}

// get table of content info from db "parts", in ascending "order"
inline void db_get_parts(vecStr32_O ids, vecStr32_O captions, SQLite::Database &db)
{
    SQLite::Statement stmt(db,
                           R"(SELECT "id", "order", "caption" FROM "parts" ORDER BY "order" ASC)");
    vecLong orders;
    while (stmt.executeStep()) {
        ids.push_back(u32(stmt.getColumn(0)));
        orders.push_back((int)stmt.getColumn(1));
        captions.push_back(u32(stmt.getColumn(2)));
    }
    for (Long i = 0; i < size(orders); ++i)
        if (orders[i] != i)
            SLS_ERR("something wrong!");
}

// get dependency tree from database
// edge direction is the learning direction
inline void db_get_tree(vector<DGnode> &tree, vecStr32_O entries, vecStr32_O titles,
                        vecStr32_O parts, vecStr32_O chapters, SQLite::Database &db)
{
    tree.clear(); entries.clear(); titles.clear(); parts.clear(); chapters.clear();
    SQLite::Statement stmt_select(
            db,
            R"(SELECT "id", "caption", "part", "chapter", "pentry" FROM "entries" WHERE "deleted" = 0 ORDER BY "id" COLLATE NOCASE ASC;)");
    Str32 pentry_str;
    vector<vecStr32> pentries;

    while (stmt_select.executeStep()) {
        entries.push_back(u32(stmt_select.getColumn(0)));
        titles.push_back(u32(stmt_select.getColumn(1)));
        parts.push_back(u32(stmt_select.getColumn(2)));
        chapters.push_back(u32(stmt_select.getColumn(3)));
        pentry_str = u32(stmt_select.getColumn(4));
        pentries.emplace_back();
        parse(pentries.back(), pentry_str);
    }
    tree.resize(entries.size());

    // construct tree
    for (Long i = 0; i < size(entries); ++i) {
        for (auto &pentry : pentries[i]) {
            Long from = search(pentry, entries);
            if (from < 0) {
                throw internal_err(U"预备知识未找到（应该已经在 PhysWikiOnline1() 中检查了不会发生才对： " + pentry + U" -> " + entries[i]);
            }
            tree[from].push_back(i);
        }
    }

    // check cyclic
    vecLong cycle;
    if (!dag_check(cycle, tree)) {
        Str32 msg = U"存在循环预备知识: ";
        for (auto ind : cycle)
            msg += to_string(ind) + "." + titles[ind] + " (" + entries[ind] + ") -> ";
        msg += titles[cycle[0]] + " (" + entries[cycle[0]] + ")";
        throw msg;
    }

    // check redundency
    vector<pair<Long,Long>> edges;
    dag_reduce(edges, tree);
    std::stringstream ss;
    if (size(edges)) {
        ss << u8"\n" << endl;
        ss << u8"==============  多余的预备知识  ==============" << endl;
        for (auto &edge : edges) {
            ss << titles[edge.first] << " (" << entries[edge.first] << ") -> "
                 << titles[edge.second] << " (" << entries[edge.second] << ")" << endl;
        }
        ss << u8"=============================================\n" << endl;
        throw ss.str();
    }
}


// get dependency tree from database, for 1 entry
// also check for cycle, and check for any of it's pentries are redundant
// entries[0] (tree[0]) will be `entry`
inline void db_get_tree1(vector<DGnode> &tree, vecStr32_O entries, vecStr32_O titles,
                        vecStr32_O parts, vecStr32_O chapters, Str32_I entry, SQLite::Database &db)
{
    tree.clear(); entries.clear(); titles.clear(); parts.clear(); chapters.clear();

    SQLite::Statement stmt_select(
            db,
            R"(SELECT "caption", "part", "chapter", "pentry" FROM "entries" WHERE "id" = ?;)");

    Str pentry_str, e;
    vector<vecStr> pentries;
    deque<Str> q; q.push_back(u8(entry));

    // broad first search (BFS)
    while (!q.empty()) {
        e = q.front(); q.pop_front();
        stmt_select.bind(1, e);
        SLS_ASSERT(stmt_select.executeStep());
        entries.push_back(u32(e));
        titles.push_back(u32(stmt_select.getColumn(0)));
        parts.push_back(u32(stmt_select.getColumn(1)));
        chapters.push_back(u32(stmt_select.getColumn(2)));
        pentry_str = (const char*)stmt_select.getColumn(3);
        stmt_select.reset();
        pentries.emplace_back();
        parse(pentries.back(), pentry_str);
        for (auto &pentry : pentries.back()) {
            if (search(u32(pentry), entries) < 0
                && find(q.begin(), q.end(), pentry) == q.end())
                q.push_back(pentry);
        }
    }
    tree.resize(entries.size());

    // construct tree
    for (Long i = 0; i < size(entries); ++i) {
        for (auto &pentry : pentries[i]) {
            Long from = search(u32(pentry), entries);
            if (from < 0)
                throw internal_err(U"预备知识未找到（应该已经在 PhysWikiOnline1() 中检查了不会发生才对： "
                    + pentry + U" -> " + entries[i]);
            tree[from].push_back(i);
        }
    }

    //    // debug: print edges
    //    for (Long i = 0; i < size(tree); ++i)
    //        for (auto &j : tree[i])
    //            cout << i << "." << titles[i] << "(" << entries[i] << ") => "
    //                 << j << "." << titles[j] << "(" << entries[j] << ")" << endl;

    // check cyclic
    vecLong cycle;
    if (!dag_check(cycle, tree)) {
        Str32 msg = U"存在循环预备知识: ";
        for (auto ind : cycle)
            msg += to_string(ind) + "." + titles[ind] + " (" + entries[ind] + ") -> ";
        msg += titles[cycle[0]] + " (" + entries[cycle[0]] + ")";
        throw msg;
    }

    // check redundancy
    dag_inv(tree);
    vector<vecLong> alt_paths;
    dag_reduce(alt_paths, tree, 0);
    std::stringstream ss;
    if (size(alt_paths)) {
        for (Long j = 0; j < size(alt_paths); ++j) {
            auto &alt_path = alt_paths[j];
            Long i_beg = alt_path[0], i_bak = alt_path.back();
            ss << u8"\n\n存在多余的预备知识： " << titles[i_bak] << "(" << entries[i_bak] << ")" << endl;
            ss << "   已存在路径： " << endl;
            ss << "   " << titles[i_beg] << "(" << entries[i_beg] << ")" << endl;
            for (Long i = 1; i < size(alt_path); ++i)
                ss << "<- " << titles[alt_path[i]] << "(" << entries[alt_path[i]] << ")" << endl;
        }
        throw ss.str();
    }
    dag_inv(tree);
}

// calculate author list of an entry, based on "history" table counts in db
inline Str32 db_get_author_list(Str32_I entry, SQLite::Database &db)
{
    SQLite::Statement stmt_select(db, R"(SELECT "authors" FROM "entries" WHERE "id"=?;)");
    stmt_select.bind(1, u8(entry));
    if (!stmt_select.executeStep())
        throw internal_err(U"author_list(): 数据库中不存在词条： " + entry);

    vecLong author_ids;
    Str32 str = u32(stmt_select.getColumn(0));
    stmt_select.reset();
    if (str.empty())
        return U"待更新";
    for (auto c : str)
        if ((c < '0' || c > '9') && c != ' ')
            return U"待更新";
    parse(author_ids, str);

    vecStr32 authors;
    SQLite::Statement stmt_select2(db, R"(SELECT "name" FROM "authors" WHERE "id"=?;)");
    for (int id : author_ids) {
        stmt_select2.bind(1, id);
        if (!stmt_select2.executeStep())
            throw internal_err("词条： " + entry + " 作者 id 不存在： " + num2str(id));
        authors.push_back(u32(stmt_select2.getColumn(0)));
        stmt_select2.reset();
    }
    join(str, authors, U"; ");
    return str;
}

// update db entries.authors, based on backup count in "history"
// TODO: use more advanced algorithm, counting other factors
inline void db_update_authors1(vecLong &author_ids, vecLong &minutes, Str32_I entry, SQLite::Database &db)
{
    author_ids.clear(); minutes.clear();
    SQLite::Statement stmt_count(db,
        R"(SELECT "author", COUNT(*) as record_count FROM "history" WHERE "entry"=? GROUP BY "author" ORDER BY record_count DESC;)");
    SQLite::Statement stmt_select(db,
        R"(SELECT "hide" FROM "authors" WHERE "id"=? AND "hide"=1;)");

    try {
        stmt_count.bind(1, u8(entry));
    } catch (std::exception &e) {
        cout << "SQLiteCpp: bind: " << e.what() << endl;
        throw e;
    }
    while (stmt_count.executeStep()) {
        Long id = (int)stmt_count.getColumn(0);
        Long time = 5*(int)stmt_count.getColumn(1);
        // 十分钟以上才算作者
        if (time <= 10) continue;
        // 检查是否隐藏作者
        stmt_select.bind(1, int(id));
        bool hidden = stmt_select.executeStep();
        stmt_select.reset();
        if (hidden) continue;
        author_ids.push_back(id);
        minutes.push_back(time);
    }
    stmt_count.reset();
    Str32 str;
    join(str, author_ids);
    SQLite::Statement stmt_update(db,
        R"(UPDATE "entries" SET "authors"=? WHERE "id"=?;)");
    stmt_update.bind(1, u8(str));
    stmt_update.bind(2, u8(entry));
    stmt_update.exec();
    SLS_ASSERT(stmt_update.getChanges() > 0);
    stmt_update.reset();
}

// update all authors
inline void db_update_authors(SQLite::Database &db)
{
    cout << "updating database for author lists...." << endl;
    SQLite::Statement stmt_select( db,
        R"(SELECT "id" FROM "entries";)");
    Str32 entry; vecLong author_ids, counts;
    while (stmt_select.executeStep()) {
        entry = u32(stmt_select.getColumn(0));
        db_update_authors1(author_ids, counts, entry, db);
    }
    stmt_select.reset();
    cout << "done!" << endl;
}

// update "bibliography" table of sqlite db
inline void db_update_bib(vecStr32_I bib_labels, vecStr32_I bib_details) {
    SQLite::Database db(u8(gv::path_data + "scan.db"), SQLite::OPEN_READWRITE);
    vector<vecStr> db_data0;
    check_foreign_key(db.getHandle());
    
    get_matrix(db_data0, db.getHandle(), "bibliography", {"id", "details"});
    unordered_map<Str, Str> db_data;
    for (auto &row : db_data0)
        db_data[row[0]] = row[1];

    SQLite::Statement stmt_insert_bib(db,
        R"(INSERT INTO bibliography ("id", "order", "details") VALUES (?, ?, ?);)");

    for (Long i = 0; i < size(bib_labels); i++) {
        Str bib_label = u8(bib_labels[i]), bib_detail = u8(bib_details[i]);
        if (db_data.count(bib_label)) {
            if (db_data[bib_label] != bib_detail) {
                SLS_WARN("数据库文献详情 bib_detail 发生变化需要更新！ 该功能未实现，将不更新： " + bib_label);
                cout << "数据库的 bib_detail： " << db_data[bib_label] << endl;
                cout << "当前的   bib_detail： " << bib_detail << endl;
            }
            continue;
        }
        SLS_WARN("数据库中不存在文献（将添加）： " + num2str(i) + ". " + bib_label);
        stmt_insert_bib.bind(1, bib_label);
        stmt_insert_bib.bind(2, int(i));
        stmt_insert_bib.bind(3, bib_detail);
        stmt_insert_bib.exec(); stmt_insert_bib.reset();
    }
}

// delete and rewrite "chapters" and "parts" table of sqlite db
inline void db_update_parts_chapters(
        vecStr32_I part_ids, vecStr32_I part_name, vecStr32_I chap_first, vecStr32_I chap_last,
        vecStr32_I chap_ids, vecStr32_I chap_name, vecLong_I chap_part,
        vecStr32_I entry_first, vecStr32_I entry_last)
{
    cout << "updating sqlite database (" << part_name.size() << " parts, "
         << chap_name.size() << " chapters) ..." << endl;
    cout.flush();
    SQLite::Database db(u8(gv::path_data + "scan.db"), SQLite::OPEN_READWRITE);
    cout << "clear parts and chatpers tables" << endl;
    table_clear(db.getHandle(), "parts");
    table_clear(db.getHandle(), "chapters");

    // insert parts
    SQLite::Statement stmt_insert_part(db,
        R"(INSERT INTO "parts" ("id", "order", "caption", "chap_first", "chap_last") VALUES (?, ?, ?, ?, ?);)");
    for (Long i = 0; i < size(part_name); ++i) {
        // cout << "part " << i << ". " << part_ids[i] << ": " << part_name[i] << " chapters: " << chap_first[i] << " -> " << chap_last[i] << endl;
        stmt_insert_part.bind(1, u8(part_ids[i]));
        stmt_insert_part.bind(2, int(i));
        stmt_insert_part.bind(3, u8(part_name[i]));
        stmt_insert_part.bind(4, u8(chap_first[i]));
        stmt_insert_part.bind(5, u8(chap_last[i]));
        stmt_insert_part.exec(); stmt_insert_part.reset();
    }
    cout << "\n\n\n" << endl;

    // insert chapters
    SQLite::Statement stmt_insert_chap(db,
        R"(INSERT INTO "chapters" ("id", "order", "caption", "part", "entry_first", "entry_last") VALUES (?, ?, ?, ?, ?, ?);)");

    for (Long i = 0; i < size(chap_name); ++i) {
        // cout << "chap " << i << ". " << chap_ids[i] << ": " << chap_name[i] << " chapters: " << entry_first[i] << " -> " << entry_last[i] << endl;
        stmt_insert_chap.bind(1, u8(chap_ids[i]));
        stmt_insert_chap.bind(2, int(i));
        stmt_insert_chap.bind(3, u8(chap_name[i]));
        stmt_insert_chap.bind(4, u8(part_ids[chap_part[i]]));
        stmt_insert_chap.bind(5, u8(entry_first[i]));
        stmt_insert_chap.bind(6, u8(entry_last[i]));
        stmt_insert_chap.exec(); stmt_insert_chap.reset();
    }
    cout << "done." << endl;
}

// update "entries" table of sqlite db, based on the info from main.tex
// `entries` are a list of entreis from main.tex
inline void db_update_entries_from_toc(
        vecStr32_I entries, vecLong_I entry_part, vecStr32_I part_ids,
        vecLong_I entry_chap, vecStr32_I chap_ids)
{
    cout << "updating sqlite database (" <<
        entries.size() << " entries) with info from main.tex..." << endl; cout.flush();
    SQLite::Database db(u8(gv::path_data + "scan.db"), SQLite::OPEN_READWRITE);

    SQLite::Statement stmt_update(db,
        R"(UPDATE "entries" SET "part"=?, "chapter"=?, "order"=? WHERE "id"=?;)");
    
    for (Long i = 0; i < size(entries); i++) {
        Long entry_order = i + 1;
        Str entry = u8(entries[i]);

        // does entry already exist (expected)?
        Bool entry_exist;
        entry_exist = exist(db.getHandle(), "entries", "id", entry);
        if (entry_exist) {
            // check if there is any change (unexpected)
            vecStr row_str; vecLong row_int;
            get_row(row_str, db.getHandle(), "entries", "id", entry, {"caption", "keys", "pentry", "part", "chapter"});
            get_row(row_int, db.getHandle(), "entries", "id", entry, {"order", "draft", "deleted"});
            bool changed = false;
            if (part_ids[entry_part[i]] != u32(row_str[3])) {
                SLS_WARN(entry + ": part has changed from " + row_str[3]+ " to " + part_ids[entry_part[i]]);
                changed = true;
            }
            if (chap_ids[entry_chap[i]] != u32(row_str[4])) {
                SLS_WARN(entry + ": chapter has changed from " + row_str[4] + " to " + chap_ids[entry_chap[i]]);
                changed = true;
            }
            if (entry_order != row_int[0]) {
                SLS_WARN(entry + ": order has changed from " + to_string(row_int[0]) + " to " + to_string(entry_order));
                changed = true;
            }
            if (changed) {
                stmt_update.bind(1, u8(part_ids[entry_part[i]]));
                stmt_update.bind(2, u8(chap_ids[entry_chap[i]]));
                stmt_update.bind(3, (int) entry_order);
                stmt_update.bind(4, entry);
                stmt_update.exec(); stmt_update.reset();
            }
        }
        else // entry_exist == false
            throw scan_err(U"main.tex 中的词条在数据库中未找到： " + entry);
    }
    cout << "done." << endl;
}

// updating sqlite database "authors" and "history" table from backup files
inline void db_update_author_history(Str32_I path, SQLite::Database &db)
{
    vecStr32 fnames;
    unordered_map<Str, Long> new_authors;
    Str author;
    Str sha1, time, entry;
    file_list_ext(fnames, path, U"tex", false);
    cout << "updating sqlite database \"history\" table (" << fnames.size()
        << " backup) ..." << endl; cout.flush();

    // update "history" table
    check_foreign_key(db.getHandle());

    vector<vecStr> db_data10;
    
    vecLong db_data20;
    get_matrix(db_data10, db.getHandle(), "history", {"hash","time","entry"});
    get_column(db_data20, db.getHandle(), "history", "author");
    Long id_max = db_data20.empty() ? -1 : max(db_data20);

    cout << "there are already " << db_data10.size() << " backup (history) records in database." << endl;

    //            hash       time author entry
    unordered_map<Str, tuple<Str, Long,    Str>> db_data;
    for (Long i = 0; i < size(db_data10); ++i)
        db_data[db_data10[i][0]] = make_tuple(db_data10[i][1], db_data20[i], db_data10[i][2]);
    db_data10.clear(); db_data20.clear();
    db_data10.shrink_to_fit(); db_data20.shrink_to_fit();

    vecStr db_authors0; vecLong db_author_ids0;
    get_column(db_author_ids0, db.getHandle(), "authors", "id");
    get_column(db_authors0, db.getHandle(), "authors", "name");
    cout << "there are already " << db_author_ids0.size() << " author records in database." << endl;
    unordered_map<Long, Str> db_id_to_author;
    unordered_map<Str, Long> db_author_to_id;
    for (Long i = 0; i < size(db_author_ids0); ++i) {
        db_id_to_author[db_author_ids0[i]] = db_authors0[i];
        db_author_to_id[db_authors0[i]] = db_author_ids0[i];
    }

    db_author_ids0.clear(); db_authors0.clear();
    db_author_ids0.shrink_to_fit(); db_authors0.shrink_to_fit();

    SQLite::Statement stmt_insert(db,
        R"(INSERT INTO history ("hash", "time", "author", "entry") VALUES (?, ?, ?, ?);)");

    vecStr entries0;
    get_column(entries0, db.getHandle(), "entries", "id");
    unordered_set<Str> entries(entries0.begin(), entries0.end()), entries_deleted_inserted;
    entries0.clear(); entries0.shrink_to_fit();

    // insert a deleted entry (to ensure FOREIGN KEY exist)
    SQLite::Statement stmt_insert_entry(db,
        R"(INSERT INTO entries ("id", "deleted") VALUES (?, 1);)");

    // insert new_authors to "authors" table
    SQLite::Statement stmt_insert_auth(db,
        R"(INSERT INTO authors ("id", "name") VALUES (?, ?);)");

    for (Long i = 0; i < size(fnames); ++i) {
        auto &fname = fnames[i];
        sha1 = sha1sum_f(u8(path + fname) + ".tex").substr(0, 16);
        bool sha1_exist = db_data.count(sha1);

        time = u8(fname.substr(0, 12));
        Long ind = fname.rfind('_');
        author = u8(fname.substr(13, ind-13));
        Long authorID;
        if (db_author_to_id.count(author))
            authorID = db_author_to_id[author];
        else if (new_authors.count(author))
            authorID = new_authors[author];
        else {
            authorID = ++id_max;
            SLS_WARN("备份文件中的作者不在数据库中（将添加）： " + author + " ID: " + to_string(id_max));
            new_authors[author] = id_max;
            stmt_insert_auth.bind(1, int(id_max));
            stmt_insert_auth.bind(2, author);
            stmt_insert_auth.exec(); stmt_insert_auth.reset();
        }
        entry = u8(fname.substr(ind+1));
        if (entries.count(entry) == 0 &&
                            entries_deleted_inserted.count(entry) == 0) {
            SLS_WARN("备份文件中的词条不在数据库中（将添加）： " + entry);
            stmt_insert_entry.bind(1, entry);
            stmt_insert_entry.exec(); stmt_insert_entry.reset();

            entries_deleted_inserted.insert(entry);
        }

        if (sha1_exist) {
            auto &time_author_entry = db_data[sha1];
            if (get<0>(time_author_entry) != time)
                SLS_WARN("备份 " + fname + " 信息与数据库中的时间不同， 数据库中为（将不更新）： " + get<0>(time_author_entry));
            if (db_id_to_author[get<1>(time_author_entry)] != author)
                SLS_WARN("备份 " + fname + " 信息与数据库中的作者不同， 数据库中为（将不更新）： " +
                         to_string(get<1>(time_author_entry)) + "." + db_id_to_author[get<1>(time_author_entry)]);
            if (get<2>(time_author_entry) != entry)
                SLS_WARN("备份 " + fname + " 信息与数据库中的文件名不同， 数据库中为（将不更新）： " + get<2>(time_author_entry));
            db_data.erase(sha1);
        }
        else {
            SLS_WARN("数据库的 history 表格中不存在备份文件（将添加）：" + sha1 + " " + fname);
            stmt_insert.bind(1, sha1);
            stmt_insert.bind(2, time);
            stmt_insert.bind(3, int(authorID));
            stmt_insert.bind(4, entry);
            stmt_insert.exec(); stmt_insert.reset();
        }
    }

    if (!db_data.empty()) {
        SLS_WARN("以下数据库中记录的备份无法找到对应文件： ");
        for (auto &row : db_data) {
            cout << row.first << ", " << get<0>(row.second) << ", " <<
                get<1>(row.second) << ", " << get<2>(row.second) << endl;
        }
    }
    cout << "\ndone." << endl;

    for (auto &author : new_authors)
        cout << "新作者： " << author.second << ". " << author.first << endl;

    cout << "done." << endl;
}

// db table "figures" and "figure_store"
inline void db_update_figures(const vecStr32_I entries, const vector<vecStr32> &img_ids,
    const vector<vecLong> &img_orders, const vector<vecStr32> &img_hashes,
    SQLite::Statement &stmt_select, SQLite::Statement &stmt_insert, SQLite::Statement &stmt_update)
{
    // cout << "updating db for figures environments..." << endl;
    Str32 db_id, db_entry, db_hash;
    // vecStr32 new_ids, new_entries, new_hash;
    Long db_order;

    Str32 entry, id, hash, ext;
    Long order;
    for (Long i = 0; i < size(entries); ++i) {
        entry = entries[i];

        for (Long j = 0; j < size(img_ids[i]); ++j) {
            id = img_ids[i][j]; order = img_orders[i][j]; hash = img_hashes[i][j];
            stmt_select.bind(1, u8(id));
            if (!stmt_select.executeStep()) {
                stmt_select.reset();
                SLS_WARN("发现数据库中没有的图片环境（将添加）：" + id);
                stmt_insert.bind(1, u8(id));
                stmt_insert.bind(2, u8(entry));
                stmt_insert.bind(3, int(order));
                stmt_insert.bind(4, u8(hash));
                stmt_insert.exec(); stmt_insert.reset();
            } else { // 数据库中有 id, 检查其他信息是否改变
                db_entry = u32(stmt_select.getColumn(0));
                db_order = (int)stmt_select.getColumn(1);
                db_hash = u32(stmt_select.getColumn(2));
                stmt_select.reset();

                bool update = false;
                if (entry != db_entry) {
                    SLS_WARN("发现数据库中图片 entry 改变（将更新）：" + id + ": "
                             + db_entry + " -> " + entry);
                    update = true;
                }
                if (order != db_order) {
                    SLS_WARN("发现数据库中图片 order 改变（将更新）：" + id + ": "
                             + to_string(db_order) + " -> " + to_string(order));
                    update = true;
                }
                if (hash != db_hash) {
                    SLS_WARN("发现数据库中图片 hash 改变（将更新）：" + id + ": "
                             + db_hash + " -> " + hash);
                    update = true;
                }
                if (update) {
                    stmt_update.bind(1, u8(entry));
                    stmt_update.bind(2, int(order));
                    stmt_update.bind(3, u8(hash));
                    stmt_update.bind(4, u8(id));
                    stmt_update.exec(); stmt_update.reset();
                }
            }
        }
    }
    // cout << "done!" << endl;
}

// update labels table of database
// `ids` are xxx### where xxx is "type", and ### is "order"
inline void db_update_labels(vecStr32_I entries, const vector<vecStr32> &v_labels, const vector<vecLong> v_label_orders,
    SQLite::Statement &stmt_select, SQLite::Statement &stmt_insert, SQLite::Statement &stmt_update)
{
    // cout << "updating db for labels..." << endl;
    Str32 label, type, entry; Long order;

    // db_data[id] = {type, entry, order}
    std::unordered_map<Str32, tuple<Str32, Str32, Long>> db_data;
    while (stmt_select.executeStep()) {
        label = u32(stmt_select.getColumn(0));
        type = u32(stmt_select.getColumn(1));
        entry = u32(stmt_select.getColumn(2));
        order = (int)stmt_select.getColumn(3);
        db_data[label] = std::make_tuple(type, entry, order);
    }
    stmt_select.reset();

    for (Long j = 0; j < size(entries); ++j) {
        entry = entries[j];
        for (Long i = 0; i < size(v_labels[j]); ++i) {
            label = v_labels[j][i];
            order = v_label_orders[j][i];
            type = label_type(label);
            if (type == U"fig" || type == U"code")
                continue;
            if (type != U"eq" && type != U"sub" && type != U"tab" && type != U"def" && type != U"lem" &&
                type != U"the" && type != U"cor" && type != U"ex" && type != U"exe")
                throw scan_err(U"未知标签类型： " + type);

            if (db_data.count(label)) { // label exist in db
                auto &db_row = db_data[label];
                bool changed = false;
                if (type != get<0>(db_row)) {
                    SLS_WARN(U"type 发生改变（将更新）：" + get<0>(db_row) + " -> " + type);
                    changed = true;
                }
                if (entry != get<1>(db_row)) {
                    SLS_WARN(U"entry 发生改变（将更新）：" + get<1>(db_row) + " -> " + entry);
                    changed = true;
                }
                if (order != get<2>(db_row)) {
                    SLS_WARN(U"order 发生改变（将更新）：" + to_string(get<2>(db_row)) + " -> " + to_string(order));
                    changed = true;
                }
                if (changed) {
                    stmt_update.bind(1, u8(type));
                    stmt_update.bind(2, u8(entry));
                    stmt_update.bind(3, int(order));
                    stmt_update.bind(4, u8(label));
                    stmt_update.exec(); stmt_update.reset();
                }
            } else { // label not in db
                SLS_WARN(U"数据库中不存在 label（将插入）：" + label + ", " + type + ", " + entry + ", " +
                         to_string(order));
                stmt_insert.bind(1, u8(label));
                stmt_insert.bind(2, u8(type));
                stmt_insert.bind(3, u8(entry));
                stmt_insert.bind(4, int(order));
                stmt_insert.exec(); stmt_insert.reset();
            }
        }
    }
    // cout << "done!" << endl;
}

// generate json file containing dependency tree
// empty elements of 'titles' will be ignored
inline Long dep_json(SQLite::Database &db)
{
    vecStr32 entries, titles, entry_chap, entry_part, chap_ids, chap_names, chap_parts, part_ids, part_names;
    vector<DGnode> tree;
    db_get_parts(part_ids, part_names, db);
    db_get_chapters(chap_ids, chap_names, chap_parts, db);
    db_get_tree(tree, entries, titles,entry_part, entry_chap, db);

    Str32 str;
    // write part names
    str += U"{\n  \"parts\": [\n";
    for (Long i = 0; i < size(part_names); ++i)
        str += U"    {\"name\": \"" + part_names[i] + "\"},\n";
    str.pop_back(); str.pop_back();
    str += U"\n  ],\n";
    // write chapter names
    str += U"  \"chapters\": [\n";
    for (Long i = 0; i < size(chap_names); ++i)
        str += U"    {\"name\": \"" + chap_names[i] + "\"},\n";
    str.pop_back(); str.pop_back();
    str += U"\n  ],\n";
    // write entries
    str += U"  \"nodes\": [\n";
    for (Long i = 0; i < size(titles); ++i) {
        if (titles[i].empty())
            continue;
        str += U"    {\"id\": \"" + entries[i] + U"\"" +
               ", \"part\": " + num2str32(search(entry_part[i], part_ids)) +
               ", \"chap\": " + num2str32(search(entry_chap[i], chap_ids)) +
               ", \"title\": \"" + titles[i] + U"\""
            ", \"url\": \"../online/" +
            entries[i] + ".html\"},\n";
    }
    str.pop_back(); str.pop_back();
    str += U"\n  ],\n";

    // write links
    str += U"  \"links\": [\n";
    Long Nedge = 0;
    for (Long i = 0; i < size(tree); ++i) {
        for (auto &j : tree[i]) {
            str += U"    {\"source\": \"" + entries[i] + "\", ";
            str += U"\"target\": \"" + entries[j] + U"\", ";
            str += U"\"value\": 1},\n";
            ++Nedge;
        }
    }
    if (Nedge > 0) {
        str.pop_back(); str.pop_back();
    }
    str += U"\n  ]\n}\n";
    write(str, gv::path_out + U"../tree/data/dep.json");
    return 0;
}
