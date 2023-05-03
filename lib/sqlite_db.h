#pragma once

// get table of content info from db "chapters", in ascending "order"
inline void db_get_chapters(vecStr_O ids, vecStr_O captions, vecStr_O parts,
                            SQLite::Database &db)
{
    SQLite::Statement stmt(db,
                           R"(SELECT "id", "order", "caption", "part" FROM "chapters" ORDER BY "order" ASC)");
    vecLong orders;
    while (stmt.executeStep()) {
        ids.push_back(stmt.getColumn(0));
        orders.push_back((int)stmt.getColumn(1));
        captions.push_back(stmt.getColumn(2));
        parts.push_back(stmt.getColumn(3));
    }
    for (Long i = 0; i < size(orders); ++i)
        if (orders[i] != i)
            SLS_ERR("something wrong!");
}

// get table of content info from db "parts", in ascending "order"
inline void db_get_parts(vecStr_O ids, vecStr_O captions, SQLite::Database &db)
{
    SQLite::Statement stmt(db,
                           R"(SELECT "id", "order", "caption" FROM "parts" ORDER BY "order" ASC)");
    vecLong orders;
    while (stmt.executeStep()) {
        ids.push_back(stmt.getColumn(0));
        orders.push_back((int)stmt.getColumn(1));
        captions.push_back(stmt.getColumn(2));
    }
    for (Long i = 0; i < size(orders); ++i)
        if (orders[i] != i)
            SLS_ERR("something wrong!");
}

// get dependency tree from database
// edge direction is the learning direction
inline void db_get_tree(vector<DGnode> &tree, vecStr_O entries, vecStr_O titles,
                        vecStr_O parts, vecStr_O chapters, SQLite::Database &db)
{
    tree.clear(); entries.clear(); titles.clear(); parts.clear(); chapters.clear();
    SQLite::Statement stmt_select(
            db,
            R"(SELECT "id", "caption", "part", "chapter", "pentry" FROM "entries" WHERE "deleted" = 0 ORDER BY "id" COLLATE NOCASE ASC;)");
    Str pentry_str;
    vector<vecStr> pentries;

    while (stmt_select.executeStep()) {
        entries.push_back(stmt_select.getColumn(0));
        titles.push_back(stmt_select.getColumn(1));
        parts.push_back(stmt_select.getColumn(2));
        chapters.push_back(stmt_select.getColumn(3));
        pentry_str = (const char*)stmt_select.getColumn(4);
        pentries.emplace_back();
        parse(pentries.back(), pentry_str);
    }
    tree.resize(entries.size());

    // construct tree
    for (Long i = 0; i < size(entries); ++i) {
        for (auto &pentry : pentries[i]) {
            Long from = search(pentry, entries);
            if (from < 0) {
                throw internal_err(u8"预备知识未找到（应该已经在 PhysWikiOnline1() 中检查了不会发生才对： " + pentry + " -> " + entries[i]);
            }
            tree[from].push_back(i);
        }
    }

    // check cyclic
    vecLong cycle;
    if (!dag_check(cycle, tree)) {
        Str msg = u8"存在循环预备知识: ";
        for (auto ind : cycle)
            msg += to_string(ind) + "." + titles[ind] + " (" + entries[ind] + ") -> ";
        msg += titles[cycle[0]] + " (" + entries[cycle[0]] + ")";
        throw scan_err(msg);
    }

    // check redundency
    vector<pair<Long,Long>> edges;
    dag_reduce(edges, tree);
    std::stringstream ss;
    if (size(edges)) {
        ss << "\n" << endl;
        ss << u8"==============  多余的预备知识  ==============" << endl;
        for (auto &edge : edges) {
            ss << titles[edge.first] << " (" << entries[edge.first] << ") -> "
                 << titles[edge.second] << " (" << entries[edge.second] << ")" << endl;
        }
        ss << "=============================================\n" << endl;
        throw scan_err(ss.str());
    }
}

// parse the string from "entries.pentry"
// 0 means the last node or the whole entry
inline void parse_pentry(Pentry_O pentry, Str_I str)
{
    Long ind0 = 0;
    Str entry; // "entry" in "entry:i_node"
    Long i_node; // "i_node" in "entry:i_node", 0 for nothing
    Bool star; // has "*" in "entry:i_node*"
    Bool tilde; // hash "~" at the end
    pentry.clear();
    while (ind0 >= 0 && ind0 < size(str)) {
        pentry.emplace_back();
        auto &pentry1 = pentry.back();
        while (1) { // get a node each loop
            i_node = 0; star = tilde = false;
            // get entry
            if (!is_letter(str[ind0]))
                throw internal_err(u8"数据库中 entries.pentry1 格式不对 (非字母): " + str);
            Long ind1 = ind0 + 1;
            while (ind1 < size(str) && is_alphanum(str[ind1]))
                ++ind1;
            entry = str.substr(ind0, ind1 - ind0);
            for (auto &ee: pentry) // check repeat
                for (auto &e : ee)
                    if (entry == e.entry)
                        throw internal_err(u8"数据库中 entries.pentry1 存在重复的节点: " + str);
            ind0 = ind1;
            if (ind0 == size(str)) break;
            // get number
            if (str[ind0] == ':')
                ind0 = str2int(i_node, str, ind0 + 1);
            if (ind0 == size(str)) break;
            // get star
            if (str[ind0] == '*') {
                star = true; ++ind0;
                if (ind0 == size(str)) break;
            }
            // check "~"
            if (str[ind0] == '~') {
                tilde = true; ++ind0;
                if (ind0 == size(str)) break;
            }
            // check "|"
            ind0 = str.find_first_not_of(' ', ind0);
            if (ind0 < 0)
                throw internal_err(u8"数据库中 entries.pentry1 格式不对 (多余的空格): " + str);
            if (str[ind0] == '|') {
                ind0 = str.find_first_not_of(' ', ind0 + 1);
                if (ind0 < 0)
                    throw internal_err(u8"数据库中 entries.pentry1 格式不对 (多余的空格): " + str);
                break;
            }
            pentry1.emplace_back(entry, i_node, star, tilde);
        }
        pentry1.emplace_back(entry, i_node, star, tilde);
    }
}

// join pentry back to a string
inline void join_pentry(Str_O str, Pentry_I v_pentries)
{
    str.clear();
    if (v_pentries.empty()) return;
    for (auto &pentries : v_pentries) {
        if (pentries.empty())
            throw scan_err("\\pentry{} empty!");
        for (auto &p : pentries) {
            str += p.entry;
            if (p.i_node > 0)
                str += ":" + num2str(p.i_node);
            if (p.star)
                str += "*";
            if (p.tilde)
                str += "~";
            str += " ";
        }
        str += "| ";
    }
    str.resize(str.size()-3); // remove " | "
}

inline void db_get_entry_info(
        pair<Str, Pentry> &info, // title, pentry
        Str_I entry, SQLite::Statement &stmt_select)
{
    auto &pentry = get<1>(info);
    stmt_select.bind(1, entry);
    if (!stmt_select.executeStep())
        throw internal_err("entry not found: " + entry + " " SLS_WHERE);
    get<0>(info) = (const char*)stmt_select.getColumn(0); // title
    parse_pentry(pentry, stmt_select.getColumn(1));
    stmt_select.reset();
}

// a node of the knowledge tree
struct Node {
    Str entry;
    Long i_node;
    Node(Str_I entry, Long_I i_node): entry(entry), i_node(i_node) {};
};

typedef const Node &Node_I;
typedef Node &Node_O, &Node_IO;

inline Bool operator==(Node_I a, Node_I b) { return a.entry == b.entry && a.i_node == b.i_node; }
inline Bool operator<(Node_I a, Node_I b) { return a.entry == b.entry ? a.i_node < b.i_node : a.entry < b.entry; }

// get dependency tree from database, for (the last node of) 1 entry0
// each node of the tree is a part of an entry0 (if there are multiple pentries)
// also check for cycle, and check for any of it's pentries are redundant
// nodes[0] will be for `entry0`
// nodes[i], tree[i] corresponds
// pentry_raw0[i][j].tilde will be updated on output
inline void db_get_tree1(vector<DGnode> &tree,
    vector<Node> &nodes, // nodes[i] is tree[i], with (entry, order)
    unordered_map<Str, pair<Str, Pentry>> &entry_info, // entry -> (title, pentry)
    Pentry_IO pentry_raw0, Str_I entry0, Str_I title0, // use the last i_node as tree root
    SQLite::Database &db_read)
{
    tree.clear(); nodes.clear(); entry_info.clear();
    SQLite::Statement stmt_select(db_read,
        R"(SELECT "caption", "pentry" FROM "entries" WHERE "id" = ?;)");

    Str pentry_str;
    Long i_node = size(pentry_raw0);
    if (i_node == 0) return; // no pentry
    nodes.emplace_back(entry0, i_node);
    deque<Node> q; // nodes to search
    q.emplace_back(entry0, i_node);
    entry_info[entry0].first = title0;
    entry_info[entry0].second = pentry_raw0;

    // broad first search (BFS) to get all nodes involved
    while (!q.empty()) {
        Str entry = std::move(q.front().entry);
        i_node = q.front().i_node;
        q.pop_front();
        auto &pentry = entry_info[entry].second;
        if (pentry.empty()) continue;
        if (i_node > size(pentry))
            throw internal_err(Str(__func__) + SLS_WHERE);
        for (auto &en : pentry[i_node-1]) {
            if (!entry_info.count(en.entry))
                db_get_entry_info(entry_info[en.entry], en.entry, stmt_select);
            if (en.i_node == 0) { // 0 is max
                en.i_node = size(entry_info[en.entry].second);
                if (en.i_node == 0) en.i_node = 1;
            }
            Node nod(en.entry, en.i_node);
            if (search(nod, nodes) < 0) {
                nodes.push_back(nod);
                if (find(q.begin(), q.end(), nod) == q.end())
                    q.push_back(nod);
            }
        }
    }

    // construct tree
    // every node has a non-zero i_node now
    tree.resize(nodes.size());
    for (Long i = 0; i < size(nodes); ++i) {
        auto &entry = nodes[i].entry;
        auto &pentry = entry_info[entry].second;
        if (pentry.empty()) continue;
        for (auto &en : pentry[nodes[i].i_node-1]) {
            Node nod {en.entry, en.i_node};
            Long from;
            from = search(nod, nodes);
            if (from < 0)
                throw internal_err(u8"预备知识未找到（应该不会发生才对）： "
                                   + nod.entry + ":" + to_string(nod.i_node));
            tree[from].push_back(i);
        }
    }

    //    // debug: print edges
    //    for (Long i = 0; i < size(tree); ++i)
    //        for (auto &j : tree[i])
    //            cout << i << "." << titles[i] << "(" << nodes[i] << ") => "
    //                 << j << "." << titles[j] << "(" << nodes[j] << ")" << endl;

    // check cyclic
    vecLong cycle;
    if (!dag_check(cycle, tree)) {
        cycle.push_back(cycle[0]);
        Str msg = u8"存在循环预备知识: ";
        for (auto ind : cycle) {
            auto &entry = nodes[ind].entry;
            auto &title = entry_info[entry].first;
            msg += to_string(ind) + "." + title + " (" + entry + ") -> ";
        }
        throw scan_err(msg);
    }

    // check redundancy

    dag_inv(tree);
    vector<vecLong> alt_paths;
        // alt_paths: path from nodes[0] to one redundant child
    dag_reduce(alt_paths, tree, 0);
    std::stringstream ss;
    if (!alt_paths.empty()) {
        for (Long j = 0; j < size(alt_paths); ++j) {
            // mark ignored (add ~ to the end)
            auto &alt_path = alt_paths[j];
            Long i_red = alt_path.back(); // node[i_red] is redundant
            auto &node_red = nodes[i_red];

            auto &pentry0 = get<1>(entry_info[entry0]);
            auto &pentry1 = pentry0[nodes[0].i_node-1];
            bool found = false;
            for (auto &e : pentry1) {
                if (e.entry == node_red.entry && e.i_node == node_red.i_node) {
                    e.tilde = found = true;
                    break;
                }
            }
            if (!found)
                throw internal_err(SLS_WHERE);
            auto &title_bak = get<0>(entry_info[node_red.entry]);
            ss << u8"\n\n存在多余的预备知识： " << title_bak << "(" << node_red.entry << ")" << endl;
            ss << u8"   已存在路径： " << endl;
            for (Long i = 0; i < size(alt_path); ++i) {
                auto &entry = nodes[alt_path[i]].entry;
                auto &title = get<0>(entry_info[entry]);
                ss << title << "(" << entry
                    << (i == size(alt_path) - 1 ? ")" : ") <-") << endl;
            }
        }
        SLS_WARN(ss.str());
    }
    dag_inv(tree);

    // add tilde to pentry_raw
    auto &pentry0 = get<1>(entry_info[entry0]);
    for (Long i = 0; i < size(pentry_raw0); ++i) {
        auto &pentry1 = pentry0[i], &pentry_raw1 = pentry_raw0[i];
        for (Long j = 0; j < size(pentry_raw1); ++j)
            pentry_raw1[j].tilde = pentry1[j].tilde;
    }
}

// calculate author list of an entry, based on "history" table counts in db_read
inline Str db_get_author_list(Str_I entry, SQLite::Database &db_read)
{
    SQLite::Statement stmt_select(db_read, R"(SELECT "authors" FROM "entries" WHERE "id"=?;)");
    stmt_select.bind(1, entry);
    if (!stmt_select.executeStep())
        throw internal_err(u8"author_list(): 数据库中不存在词条： " + entry);

    vecLong author_ids;
    Str str = stmt_select.getColumn(0);
    stmt_select.reset();
    if (str.empty())
        return u8"待更新";
    for (auto c : str)
        if ((c < '0' || c > '9') && c != ' ')
            return u8"待更新";
    parse(author_ids, str);

    vecStr authors;
    SQLite::Statement stmt_select2(db_read, R"(SELECT "name" FROM "authors" WHERE "id"=?;)");
    for (int id : author_ids) {
        stmt_select2.bind(1, id);
        if (!stmt_select2.executeStep())
            throw internal_err(u8"词条： " + entry + u8" 作者 id 不存在： " + num2str(id));
        authors.push_back(stmt_select2.getColumn(0));
        stmt_select2.reset();
    }
    join(str, authors, "; ");
    return str;
}

inline void db_check_add_entry_simulate_editor(vecStr_I entries, SQLite::Database &db_rw)
{
    SQLite::Statement stmt_insert(db_rw,
        R"(INSERT INTO "entries" ("id", "caption", "draft", "deleted") VALUES (?, ?, 1, 0);)");
    for (auto &entry : entries) {
        if (!exist("entries", "id", entry, db_rw)) {
            SLS_WARN(u8"词条不存在数据库中， 将模拟 editor 添加： " + entry);
            // 从 tex 文件获取标题
            Str str;
            read(str, gv::path_in + "contents/" + entry + ".tex"); // read tex file
            CRLF_to_LF(str);
            Str title;
            get_title(title, str);
            // 写入数据库
            stmt_insert.bind(1, entry);
            stmt_insert.bind(2, title);
            stmt_insert.exec();
            stmt_insert.reset();
        }
    }
}

// update db entries.authors, based on backup count in "history"
// TODO: use more advanced algorithm, counting other factors
inline void db_update_authors1(vecLong &author_ids, vecLong &minutes, Str_I entry, SQLite::Database &db)
{
    author_ids.clear(); minutes.clear();
    SQLite::Statement stmt_count(db,
        R"(SELECT "author", COUNT(*) as record_count FROM "history" WHERE "entry"=? GROUP BY "author" ORDER BY record_count DESC;)");
    SQLite::Statement stmt_select(db,
        R"(SELECT "hide" FROM "authors" WHERE "id"=? AND "hide"=1;)");

    stmt_count.bind(1, entry);
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
    Str str;
    join(str, author_ids);
    SQLite::Statement stmt_update(db,
        R"(UPDATE "entries" SET "authors"=? WHERE "id"=?;)");
    stmt_update.bind(1, str);
    stmt_update.bind(2, entry);
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
    Str entry; vecLong author_ids, counts;
    while (stmt_select.executeStep()) {
        entry = (const char*)stmt_select.getColumn(0);
        db_update_authors1(author_ids, counts, entry, db);
    }
    stmt_select.reset();
    cout << "done!" << endl;
}

struct BibInfo {
    Long order; // starts from 1
    Str detail;
    set<Str> ref_by; // {entry1, entry2, ...}
};

// update "bibliography" table of sqlite db
inline void db_update_bib(vecStr_I bib_labels, vecStr_I bib_details, SQLite::Database &db) {
    SQLite::Transaction transaction(db);

    // read the whole db bibliography table
    SQLite::Statement stmt_select(db,
        R"(SELECT "id", "order", "details", "ref_by" FROM "bibliography" WHERE "id" != '';)");
    SQLite::Statement stmt_delete(db,
        R"(DELETE FROM "bibliography" WHERE "id"=?;)");
    unordered_map<Str, BibInfo> db_bib_info; // id -> (order, details, ref_by)
    while (stmt_select.executeStep()) {
        const Str &id = (const char*)stmt_select.getColumn(0);
        auto &info = db_bib_info[id];
        info.order = (int)stmt_select.getColumn(1);
        info.detail = (const char*)stmt_select.getColumn(2);
        const Str &ref_by_str = (const char*)stmt_select.getColumn(3);
        parse(info.ref_by, ref_by_str);
        if (search(id, bib_labels) < 0) {
            if (!info.ref_by.empty())
                throw scan_err(u8"检测到删除的文献： " + id + u8"， 但被以下词条引用： " + ref_by_str);
            SLS_WARN(u8"检测到删除文献（将删除）： " + to_string(info.order) + ". " + id);
            stmt_delete.bind(1, id);
            stmt_delete.exec(); stmt_delete.reset();
        }
    }
    stmt_select.reset();

    SQLite::Statement stmt_update(db,
        R"(UPDATE "bibliography" SET "order"=?, "details"=? WHERE "id"=?;)");
    SQLite::Statement stmt_insert(db,
        R"(INSERT INTO "bibliography" ("id", "order", "details") VALUES (?, ?, ?);)");
    unordered_set<Str> id_flip_sign;

    for (Long i = 0; i < size(bib_labels); i++) {
        Long order = i+1;
        const Str &id = bib_labels[i], &bib_detail = bib_details[i];
        if (db_bib_info.count(id)) {
            auto &info = db_bib_info[id];
            bool changed = false;
            if (order != info.order) {
                SLS_WARN(u8"数据库中文献 " + id + " 编号改变（将更新）： " + to_string(info.order) + " -> " + to_string(order));
                changed = true;
                id_flip_sign.insert(id);
                stmt_update.bind(1, -(int)order); // to avoid unique constraint
            }
            if (bib_detail != info.detail) {
                SLS_WARN(u8"数据库中文献 " + id + " 详情改变（将更新）： " + info.detail + " -> " + bib_detail);
                changed = true;
                stmt_update.bind(1, (int)order); // to avoid unique constraint
            }
            if (changed) {
                stmt_update.bind(2, bib_detail);
                stmt_update.bind(3, id);
                stmt_update.exec(); stmt_update.reset();
            }
        }
        else {
            SLS_WARN(u8"数据库中不存在文献（将添加）： " + num2str(order) + ". " + id);
            stmt_insert.bind(1, id);
            stmt_insert.bind(2, -(int)order);
            stmt_insert.bind(3, bib_detail);
            stmt_insert.exec(); stmt_insert.reset();
            id_flip_sign.insert(id);
        }
    }

    // set the orders back
    SQLite::Statement stmt_update2(db,
        R"(UPDATE "bibliography" SET "order" = "order" * -1 WHERE "id"=?;)");
    for (auto &id : id_flip_sign) {
        stmt_update2.bind(1, id);
        stmt_update2.exec(); stmt_update2.reset();
    }
    transaction.commit();
}

// delete and rewrite "chapters" and "parts" table of sqlite db
inline void db_update_parts_chapters(
        vecStr_I part_ids, vecStr_I part_name, vecStr_I chap_first, vecStr_I chap_last,
        vecStr_I chap_ids, vecStr_I chap_name, vecLong_I chap_part,
        vecStr_I entry_first, vecStr_I entry_last)
{
    cout << "updating sqlite database (" << part_name.size() << " parts, "
         << chap_name.size() << " chapters) ..." << endl;
    cout.flush();
    SQLite::Database db_rw(gv::path_data + "scan.db", SQLite::OPEN_READWRITE);
    SQLite::Transaction transaction(db_rw);
    cout << "clear parts and chatpers tables" << endl;
    table_clear("parts", db_rw); table_clear("chapters", db_rw);

    // insert parts
    cout << "inserting parts to db_rw..." << endl;
    SQLite::Statement stmt_insert_part(db_rw,
                                       R"(INSERT INTO "parts" ("id", "order", "caption", "chap_first", "chap_last") VALUES (?, ?, ?, ?, ?);)");
    for (Long i = 0; i < size(part_name); ++i) {
        // cout << "part " << i << ". " << part_ids[i] << ": " << part_name[i] << " chapters: " << chap_first[i] << " -> " << chap_last[i] << endl;
        stmt_insert_part.bind(1, part_ids[i]);
        stmt_insert_part.bind(2, int(i));
        stmt_insert_part.bind(3, part_name[i]);
        stmt_insert_part.bind(4, chap_first[i]);
        stmt_insert_part.bind(5, chap_last[i]);
        stmt_insert_part.exec(); stmt_insert_part.reset();
    }
    cout << "\n\n\n" << endl;

    // insert chapters
    cout << "inserting chapters to db_rw..." << endl;
    SQLite::Statement stmt_insert_chap(db_rw,
                                       R"(INSERT INTO "chapters" ("id", "order", "caption", "part", "entry_first", "entry_last") VALUES (?, ?, ?, ?, ?, ?);)");

    for (Long i = 0; i < size(chap_name); ++i) {
        // cout << "chap " << i << ". " << chap_ids[i] << ": " << chap_name[i] << " chapters: " << entry_first[i] << " -> " << entry_last[i] << endl;
        stmt_insert_chap.bind(1, chap_ids[i]);
        stmt_insert_chap.bind(2, int(i));
        stmt_insert_chap.bind(3, chap_name[i]);
        stmt_insert_chap.bind(4, part_ids[chap_part[i]]);
        stmt_insert_chap.bind(5, entry_first[i]);
        stmt_insert_chap.bind(6, entry_last[i]);
        stmt_insert_chap.exec(); stmt_insert_chap.reset();
    }
    transaction.commit();
    cout << "done." << endl;
}

// update "entries" table of sqlite db, based on the info from main.tex
// `entries` are a list of entreis from main.tex
inline void db_update_entries_from_toc(
        vecStr_I entries, vecStr_I titles, vecLong_I entry_part, vecStr_I part_ids,
        vecLong_I entry_chap, vecStr_I chap_ids, SQLite::Database &db_rw)
{
    cout << "updating sqlite database (" <<
        entries.size() << " entries) with info from main.tex..." << endl; cout.flush();

    SQLite::Statement stmt_update(db_rw,
        R"(UPDATE "entries" SET "caption"=?, "part"=?, "chapter"=?, "order"=? WHERE "id"=?;)");
    SQLite::Statement stmt_select(db_rw,
        R"(SELECT "caption", "keys", "pentry", "part", "chapter", "order" FROM "entries" WHERE "id"=?;)");
    SQLite::Transaction transaction(db_rw);

    for (Long i = 0; i < size(entries); i++) {
        Long entry_order = i + 1;
        Str entry = entries[i];

        // does entry already exist (expected)?
        Bool entry_exist;
        entry_exist = exist("entries", "id", entry, db_rw);
        if (entry_exist) {
            // check if there is any change (unexpected)
            stmt_select.bind(1, entry);
            SLS_ASSERT(stmt_select.executeStep());
            Str db_title = (const char*) stmt_select.getColumn(0);
            Str db_key_str = (const char*) stmt_select.getColumn(1);
            Str db_pentry_str = (const char*) stmt_select.getColumn(2);
            Str db_part = (const char*) stmt_select.getColumn(3);
            Str db_chapter = (const char*) stmt_select.getColumn(4);
            int db_order = (int) stmt_select.getColumn(5);
            stmt_select.reset();

            bool changed = false;
            if (titles[i] != db_title) {
                SLS_WARN(entry + ": title has changed from " + db_title + " to " + titles[i]);
                changed = true;
            }
            if (part_ids[entry_part[i]] != db_part) {
                SLS_WARN(entry + ": part has changed from " + db_part + " to " + part_ids[entry_part[i]]);
                changed = true;
            }
            if (chap_ids[entry_chap[i]] != db_chapter) {
                SLS_WARN(entry + ": chapter has changed from " + db_chapter + " to " + chap_ids[entry_chap[i]]);
                changed = true;
            }
            if (entry_order != db_order) {
                SLS_WARN(entry + ": order has changed from " + to_string(db_order) + " to " + to_string(entry_order));
                changed = true;
            }
            if (changed) {
                stmt_update.bind(1, titles[i]);
                stmt_update.bind(2, part_ids[entry_part[i]]);
                stmt_update.bind(3, chap_ids[entry_chap[i]]);
                stmt_update.bind(4, (int) entry_order);
                stmt_update.bind(5, entry);
                stmt_update.exec(); stmt_update.reset();
            }
        }
        else // entry_exist == false
            throw scan_err(u8"main.tex 中的词条在数据库中未找到： " + entry);
    }
    transaction.commit();
    cout << "done." << endl;
}

// updating sqlite database "authors" and "history" table from backup files
inline void db_update_author_history(Str_I path, SQLite::Database &db_rw)
{
    vecStr fnames;
    unordered_map<Str, Long> new_authors;
    unordered_map<Long, Long> author_contrib;
    Str author;
    Str sha1, time, entry;
    file_list_ext(fnames, path, "tex", false);
    cout << "updating sqlite database \"history\" table (" << fnames.size()
        << " backup) ..." << endl; cout.flush();

    SQLite::Transaction transaction(db_rw);

    // update "history" table
    check_foreign_key(db_rw);

    SQLite::Statement stmt_select(db_rw, R"(SELECT "hash", "time", "author", "entry" FROM "history")");

    //            hash        time author entry
    unordered_map<Str,  tuple<Str, Long,  Str>> db_history;
    while (stmt_select.executeStep()) {
        Long author_id = (int) stmt_select.getColumn(2);
        db_history[stmt_select.getColumn(0)] =
            tuple<Str, Long, Str>(stmt_select.getColumn(1),
                author_id , stmt_select.getColumn(3));
    }

    cout << "there are already " << db_history.size() << " backup (history) records in database." << endl;

    Long author_id_max = -1;
    vecLong db_author_ids0;
    vecStr db_author_names0;
    SQLite::Statement stmt_select2(db_rw, R"(SELECT "id", "name" FROM "authors")");
    while (stmt_select2.executeStep()) {
        db_author_ids0.push_back((int)stmt_select2.getColumn(0));
        author_id_max = max(author_id_max, db_author_ids0.back());
        db_author_names0.push_back(stmt_select2.getColumn(1));
    }
    stmt_select2.reset();

    cout << "there are already " << db_author_ids0.size() << " author records in database." << endl;
    unordered_map<Long, Str> db_id_to_author;
    unordered_map<Str, Long> db_author_to_id;
    for (Long i = 0; i < size(db_author_ids0); ++i) {
        db_id_to_author[db_author_ids0[i]] = db_author_names0[i];
        db_author_to_id[db_author_names0[i]] = db_author_ids0[i];
    }

    SQLite::Statement stmt_select3(db_rw, R"(SELECT "id", "aka" FROM "authors" WHERE "aka" != -1;)");
    Str aka;
    unordered_map<int, int> db_author_aka;
    while (stmt_select3.executeStep()) {
        int id = stmt_select3.getColumn(0);
        int aka = stmt_select3.getColumn(1);
        db_author_aka[id] = aka;
    }

    db_author_ids0.clear(); db_author_names0.clear();
    db_author_ids0.shrink_to_fit(); db_author_names0.shrink_to_fit();

    SQLite::Statement stmt_insert(db_rw,
        R"(INSERT OR IGNORE INTO "history" ("hash", "time", "author", "entry") VALUES (?, ?, ?, ?);)");

    vecStr entries0;
    get_column(entries0, "entries", "id", db_rw);
    unordered_set<Str> entries(entries0.begin(), entries0.end()), entries_deleted_inserted;
    entries0.clear(); entries0.shrink_to_fit();

    // insert a deleted entry (to ensure FOREIGN KEY exist)
    SQLite::Statement stmt_insert_entry(db_rw,
        R"(INSERT INTO "entries" ("id", "deleted") VALUES (?, 1);)");

    // insert new_authors to "authors" table
    SQLite::Statement stmt_insert_auth(db_rw,
        R"(INSERT INTO "authors" ("id", "name") VALUES (?, ?);)");

    Str fpath;

    for (Long i = 0; i < size(fnames); ++i) {
        auto &fname = fnames[i];
        fpath = path + fname + ".tex";
        sha1 = sha1sum_f(fpath).substr(0, 16);
        bool sha1_exist = db_history.count(sha1);

        // fname = YYYYMMDDHHMM_[name]_[entry].tex
        time = fname.substr(0, 12);
        Long ind = fname.rfind('_');
        author = fname.substr(13, ind-13);
        Long authorID;
        if (db_author_to_id.count(author))
            authorID = db_author_to_id[author];
        else if (new_authors.count(author))
            authorID = new_authors[author];
        else {
            authorID = ++author_id_max;
            SLS_WARN(u8"备份文件中的作者不在数据库中（将添加）： " + author + " ID: " + to_string(author_id_max));
            new_authors[author] = author_id_max;
            stmt_insert_auth.bind(1, int(author_id_max));
            stmt_insert_auth.bind(2, author);
            stmt_insert_auth.exec(); stmt_insert_auth.reset();
        }
        author_contrib[authorID] += 5;
        entry = fname.substr(ind+1);
        if (entries.count(entry) == 0 &&
                            entries_deleted_inserted.count(entry) == 0) {
            SLS_WARN(u8"备份文件中的词条不在数据库中（将添加）： " + entry);
            stmt_insert_entry.bind(1, entry);
            stmt_insert_entry.exec(); stmt_insert_entry.reset();
            entries_deleted_inserted.insert(entry);
        }

        if (sha1_exist) {
            auto &time_author_entry = db_history[sha1];
            if (get<0>(time_author_entry) != time)
                SLS_WARN(u8"备份 " + fname + u8" 信息与数据库中的时间不同， 数据库中为（将不更新）： " + get<0>(time_author_entry));
            if (db_id_to_author[get<1>(time_author_entry)] != author)
                SLS_WARN(u8"备份 " + fname + u8" 信息与数据库中的作者不同， 数据库中为（将不更新）： " +
                         to_string(get<1>(time_author_entry)) + "." + db_id_to_author[get<1>(time_author_entry)]);
            if (get<2>(time_author_entry) != entry)
                SLS_WARN(u8"备份 " + fname + u8" 信息与数据库中的文件名不同， 数据库中为（将不更新）： " + get<2>(time_author_entry));
            db_history.erase(sha1);
        }
        else {
            SLS_WARN(u8"数据库的 history 表格中不存在备份文件（将添加）：" + sha1 + " " + fname);
            stmt_insert.bind(1, sha1);
            stmt_insert.bind(2, time);
            stmt_insert.bind(3, int(authorID));
            stmt_insert.bind(4, entry);
            stmt_insert.exec();
            if (!stmt_insert.getChanges()) {
                SLS_WARN("重复的 sha1 （将删除）: " + fname + ".tex");
                file_remove(fpath);
            }
            stmt_insert.reset();
        }
    }

    if (!db_history.empty()) {
        SLS_WARN(u8"以下数据库中记录的备份无法找到对应文件： ");
        for (auto &row : db_history) {
            cout << row.first << ", " << get<0>(row.second) << ", " <<
                get<1>(row.second) << ", " << get<2>(row.second) << endl;
        }
    }
    cout << "\ndone." << endl;

    for (auto &author : new_authors)
        cout << u8"新作者： " << author.second << ". " << author.first << endl;

    cout << "\nupdating author contribution..." << endl;
    for (auto &e : db_author_aka) {
        author_contrib[e.second] += author_contrib[e.first];
        author_contrib[e.first] = 0;
    }

    SQLite::Statement stmt_contrib(db_rw, R"(UPDATE "authors" SET "contrib"=? WHERE "id"=?;)");
    for (auto &e : author_contrib) {
        stmt_contrib.bind(1, (int)e.second);
        stmt_contrib.bind(2, (int)e.first);
        stmt_contrib.exec(); stmt_contrib.reset();
    }

    transaction.commit();
    cout << "done." << endl;
}

// db table "images"
inline void db_update_images(Str_I entry, vecStr_I fig_ids,
    const vector<unordered_map<Str,Str>> & fig_ext_hash, SQLite::Database &db_rw)
{
    SQLite::Transaction transaction(db_rw);
    SQLite::Statement stmt_select(db_rw,
        R"(SELECT "ext", "figures" FROM "images" WHERE "hash"=?;)");
    SQLite::Statement stmt_insert(db_rw,
        R"(INSERT INTO "images" ("hash", "ext", "figures") VALUES (?,?,?);)");
    SQLite::Statement stmt_update(db_rw,
        R"(UPDATE "images" SET "figures"=? WHERE "hash"=?;)");
    Str db_image_ext, tmp;
    set<Str> db_image_figures;

    // first get all "images" the entry uses
//    SQLite::Statement stmt_select2(db_rw,
//        R"(SELECT "image", "image_alt" FROM "figures" WHERE "id"=?;)");

    for (Long i = 0; i < size(fig_ids); ++i) {
        for (auto &ext_hash: fig_ext_hash[i]) {
            auto &image_ext = ext_hash.first;
            auto &image_hash = ext_hash.second;
            stmt_select.bind(1, ext_hash.second);
            if (!stmt_select.executeStep()) {
                SLS_WARN("数据库中找不到图片文件（将模拟 editor 添加）：" + image_hash + image_ext);
                stmt_insert.bind(1, image_hash);
                stmt_insert.bind(2, image_ext);
                stmt_insert.bind(3, fig_ids[i]);
                stmt_insert.exec(); stmt_insert.reset();
            }
            else {
                db_image_ext = (const char*)stmt_select.getColumn(0);
                parse(db_image_figures, stmt_select.getColumn(1));
                if (image_ext != db_image_ext)
                    throw internal_err(u8"图片文件拓展名不允许改变: " + image_hash + "."
                        + db_image_ext + " -> " + image_ext);
                if (db_image_figures.insert(fig_ids[i]).second) {
                    // inserted
                    SLS_WARN(u8"images.figures 发生改变(将模拟 editor 更新): 新增 " + fig_ids[i]);
                    join(tmp, db_image_figures);
                    stmt_update.bind(1, tmp);
                    stmt_update.bind(2, image_hash);
                    stmt_update.exec(); stmt_update.reset();
                }
            }
            stmt_select.reset();
        }
    }
    transaction.commit();
}

// db table "figures"
inline void db_update_figures(unordered_set<Str> &update_entries, vecStr_I entries,
    const vector<vecStr> &entry_figs, const vector<vecLong> &entry_fig_orders,
    const vector<vector<unordered_map<Str,Str>>> &entry_fig_ext_hash, SQLite::Database &db_rw)
{
    // cout << "updating db for figures environments..." << endl;
    update_entries.clear();  //entries that needs to rerun autoref(), since label order updated
    Str entry, id, ext;
    Long order;
    SQLite::Transaction transaction(db_rw);
    SQLite::Statement stmt_select0(db_rw,
        R"(SELECT "figures" FROM "entries" WHERE "id"=?;)");
    SQLite::Statement stmt_select1(db_rw,
        R"(SELECT "order", "ref_by", "image", "image_alt" FROM "figures" WHERE "id"=?;)");
    SQLite::Statement stmt_select2(db_rw,
        R"(SELECT "figures" FROM "entries" WHERE "id"=?;)");
    SQLite::Statement stmt_insert(db_rw,
        R"(INSERT INTO "figures" ("id", "entry", "order", "image", "image_alt") VALUES (?, ?, ?, ?, ?);)");
    SQLite::Statement stmt_update0(db_rw,
        R"(UPDATE "figures" SET "order"=? WHERE "id"=?;)");
    SQLite::Statement stmt_update(db_rw,
        R"(UPDATE "figures" SET "entry"=?, "order"=?, "image"=?, "image_alt"=? WHERE "id"=?;)");
    SQLite::Statement stmt_update2(db_rw,
        R"(UPDATE "entries" SET "figures"=? WHERE "id"=?;)");

    // get all figure envs defined in `entries`, to detect deleted figures
    //  db_xxx[i] are from the same row of "labels" table
    vecStr db_figs, db_fig_entries, db_fig_image, db_fig_image_alt;
    vecBool db_figs_used;
    vecLong db_fig_orders;
    vector<vecStr> db_fig_ref_bys;
    set<Str> db_entry_figs;
    for (Long j = 0; j < size(entries); ++j) {
        entry = entries[j];
        stmt_select0.bind(1, entry);
        if (!stmt_select0.executeStep())
            throw scan_err("词条不存在： " + entry);
        parse(db_entry_figs, stmt_select0.getColumn(0));
        stmt_select0.reset();
        for (auto &fig_id: db_entry_figs) {
            db_figs.push_back(fig_id);
            db_fig_entries.push_back(entry);
            stmt_select1.bind(1, fig_id);
            if (!stmt_select1.executeStep())
                throw scan_err("图片标签不存在： fig_" + fig_id);
            db_fig_orders.push_back((int)stmt_select1.getColumn(0));
            db_fig_ref_bys.emplace_back();
            parse(db_fig_ref_bys.back(), stmt_select1.getColumn(1));
            db_fig_image.push_back(stmt_select1.getColumn(2));
            db_fig_image_alt.push_back(stmt_select1.getColumn(3));
            stmt_select1.reset();
        }
    }
    db_figs_used.resize(db_figs.size(), false);
    Str figs_str, image, image_alt;
    set<Str> new_figs, figs;
    // fig orders set to negative to avoid conflict
    vector<pair<Str,Long>> fig_order_neg; // {id, order}

    for (Long i = 0; i < size(entries); ++i) {
        entry = entries[i];
        new_figs.clear();
        for (Long j = 0; j < size(entry_figs[i]); ++j) {
            id = entry_figs[i][j]; order = entry_fig_orders[i][j];
            Long ind = search(id, db_figs);
            const unordered_map<Str,Str> &map = entry_fig_ext_hash[i][j];
            if (map.size() == 1) { // png
                if (!map.count("png"))
                    throw internal_err("db_update_figures(): unexpected fig format!");
                image = map.at("png") + ".png";
                image_alt = "";
            }
            else if (map.size() == 2) { // svg + pdf
                if (!map.count("svg") || !map.count("pdf"))
                    throw internal_err("db_update_figures(): unexpected fig format!");
                image = map.at("pdf") + ".pdf";
                image_alt = map.at("svg") + ".svg";
            }
            if (ind < 0) {
                SLS_WARN(u8"发现数据库中没有的图片环境（将模拟 editor 添加）：" + id + ", " + entry + ", " +
                    to_string(order));
                stmt_insert.bind(1, id);
                stmt_insert.bind(2, entry);
                stmt_insert.bind(3, -(int)order);
                stmt_insert.bind(4, image);
                stmt_insert.bind(5, image_alt);
                stmt_insert.exec(); stmt_insert.reset();
                new_figs.insert(id);
                fig_order_neg.emplace_back(id, order);
                continue;
            }
            db_figs_used[ind] = true;
            bool changed = false;
            if (entry != db_fig_entries[ind]) {
                SLS_WARN(u8"发现数据库中图片 entry 改变（将更新）：" + id + ": "
                         + db_fig_entries[ind] + " -> " + entry);
                changed = true;
            }
            if (order != db_fig_orders[ind]) {
                SLS_WARN(u8"发现数据库中图片 order 改变（将更新）：" + id + ": "
                         + to_string(db_fig_orders[ind]) + " -> " + to_string(order));
                changed = true;

                // order change means other ref_by entries needs to be updated with autoref() as well.
                if (!gv::is_entire) {
                    for (auto &by_entry: db_fig_ref_bys[ind])
                        if (search(by_entry, entries) < 0)
                            update_entries.insert(by_entry);
                }
            }
            if (image != db_fig_image[ind]) {
                SLS_WARN(u8"发现数据库中图片 image 改变（将更新）：" + id + ": "
                         + db_fig_image[ind] + " -> " + image);
                changed = true;
            }
            if (image_alt != db_fig_image_alt[ind]) {
                SLS_WARN(u8"发现数据库中图片 image_alt 改变（将更新）：" + id + ": "
                         + db_fig_image_alt[ind] + " -> " + image_alt);
                changed = true;
            }
            if (changed) {
                stmt_update.bind(1, entry);
                stmt_update.bind(2, -int(order)); // -order to avoid UNIQUE constraint
                stmt_update.bind(3, image);
                stmt_update.bind(4, image_alt);
                stmt_update.bind(5, id);
                stmt_update.exec(); stmt_update.reset();
                fig_order_neg.emplace_back(id, order);
            }
        }

        // add entries.figures
        stmt_select2.bind(1, entry);
        if (!new_figs.empty()) {
            if (!stmt_select2.executeStep())
                throw internal_err("db_update_labels(): entry 不存在： " + entry);
            parse(figs, stmt_select2.getColumn(0));
            for (auto &e : new_figs)
                figs.insert(e);
            join(figs_str, figs);
            stmt_update2.bind(1, figs_str);
            stmt_update2.bind(2, entry);
            stmt_update2.exec(); stmt_update2.reset();
        }
        stmt_select2.reset();
    }

    // 检查被删除的图片（如果只被本词条引用， 就留给 autoref() 报错）
    // 这是因为入本词条的 autoref 还没有扫描不确定没有也被删除
    Str ref_by_str;
    SQLite::Statement stmt_delete(db_rw, R"(DELETE FROM "figures" WHERE "id"=?;)");
    for (Long i = 0; i < size(db_figs_used); ++i) {
        if (!db_figs_used[i]) {
            if (db_fig_ref_bys[i].empty() ||
                (db_fig_ref_bys[i].size() == 1 && db_fig_ref_bys[i][0] == entry)) {
                SLS_WARN(u8"检测到 figure 被删除（将从数据库删除）： " + db_figs[i]);
                // delete from "figures"
                stmt_delete.bind(1, db_figs[i]);
                stmt_delete.exec(); stmt_delete.reset();
                // delete from "entries.figures"
                stmt_select0.bind(1, db_fig_entries[i]);
                SLS_ASSERT(stmt_select0.executeStep());
                parse(figs, stmt_select0.getColumn(0));
                figs.erase(db_figs[i]);
                stmt_select0.reset();
                join(figs_str, figs);
                stmt_update2.bind(1, figs_str);
                stmt_update2.bind(2, db_fig_entries[i]);
                stmt_update2.exec(); stmt_update2.reset();
            }
            else {
                join(ref_by_str, db_fig_ref_bys[i], ", ");
                throw scan_err(u8"检测到 figure 被删除： " + db_figs[i] + u8"\n但是被这些词条引用： " + ref_by_str);
            }
        }
    }

    // set label orders back to positive
    for (auto &e : fig_order_neg) {
        stmt_update0.bind(1, (int)e.second); // order
        stmt_update0.bind(2, e.first); // id
        stmt_update0.exec(); stmt_update0.reset();
    }
    transaction.commit();
    // cout << "done!" << endl;
}

// update labels table of database
// order change means `update_entries` needs to be updated with autoref() as well.
inline void db_update_labels(unordered_set<Str> &update_entries, vecStr_I entries,
                 const vector<vecStr> &entry_labels, const vector<vecLong> &entry_label_orders,
                 SQLite::Database &db_rw)
{
    SQLite::Transaction transaction(db_rw);
    // cout << "updating db for labels..." << endl;
    update_entries.clear(); //entries that needs to rerun autoref(), since label order updated
    SQLite::Statement stmt_select0(db_rw,
        R"(SELECT "labels" FROM "entries" WHERE "id"=?;)");
    SQLite::Statement stmt_select1(db_rw,
        R"(SELECT "order", "ref_by" FROM "labels" WHERE "id"=?;)");
    SQLite::Statement stmt_insert(db_rw,
        R"(INSERT INTO "labels" ("id", "type", "entry", "order") VALUES (?,?,?,?);)");
    SQLite::Statement stmt_update0(db_rw,
        R"(UPDATE "labels" SET "order"=? WHERE "id"=?;)");
    SQLite::Statement stmt_update(db_rw,
        R"(UPDATE "labels" SET "entry"=?, "order"=? WHERE "id"=?;)");
    SQLite::Statement stmt_update2(db_rw,
        R"(UPDATE "entries" SET "labels"=? WHERE "id"=?;)");

    Long order;
    Str label, type, entry;

    // get all db labels defined in `entries`
    //  db_xxx[i] are from the same row of "labels" table
    vecStr db_labels, db_label_entries;
    vecBool db_labels_used;
    vecLong db_label_orders;
    vector<vecStr> db_label_ref_bys;

    // get all labels defined by `entries`
    set<Str> db_entry_labels;
    for (Long j = 0; j < size(entries); ++j) {
        entry = entries[j];
        stmt_select0.bind(1, entry);
        if (!stmt_select0.executeStep())
            throw scan_err("词条不存在： " + entry);
        parse(db_entry_labels, stmt_select0.getColumn(0));
        stmt_select0.reset();
        for (auto &label: db_entry_labels) {
            db_labels.push_back(label);
            db_label_entries.push_back(entry);
            stmt_select1.bind(1, label);
            if (!stmt_select1.executeStep())
                throw scan_err("标签不存在： " + label);
            db_label_orders.push_back((int)stmt_select1.getColumn(0));
            db_label_ref_bys.emplace_back();
            parse(db_label_ref_bys.back(), stmt_select1.getColumn(1));
            stmt_select1.reset();
        }
    }
    db_labels_used.resize(db_labels.size(), false);
    Str labels_str;
    set<Str> new_labels, labels;
    // {label, order}, negated label order， to avoid UNIQUE constraint
    vector<pair<Str,Long>> label_order_neg;
    for (Long j = 0; j < size(entries); ++j) {
        entry = entries[j];
        new_labels.clear();
        for (Long i = 0; i < size(entry_labels[j]); ++i) {
            label = entry_labels[j][i];
            order = entry_label_orders[j][i];
            type = label_type(label);
            if (type == "fig" || type == "code")
                throw internal_err("`labels` here should not contain fig or code type!");

            Long ind = search(label, db_labels);
            if (ind < 0) {
                SLS_WARN(u8"数据库中不存在 label（将模拟 editor 插入）：" + label + ", " + type + ", "
                    + entry + ", " + to_string(order));
                stmt_insert.bind(1, label);
                stmt_insert.bind(2, type);
                stmt_insert.bind(3, entry);
                stmt_insert.bind(4, -(int)order);
                stmt_insert.exec(); stmt_insert.reset();
                new_labels.insert(label);
                label_order_neg.emplace_back(label, order);
                continue;
            }
            db_labels_used[ind] = true;
            bool changed = false;
            if (entry != db_label_entries[ind]) {
                throw scan_err("label " + label + u8" 的词条发生改变（暂时不允许，请使用新的标签）："
                    + db_label_entries[ind] + " -> " + entry);
                changed = true;
            }
            if (order != db_label_orders[ind]) {
                SLS_WARN("label " + label + u8" 的 order 发生改变（将更新）："
                    + to_string(db_label_orders[ind]) + " -> " + to_string(order));
                changed = true;
                // order change means other ref_by entries needs to be updated with autoref() as well.
                if (!gv::is_entire) {
                    for (auto &by_entry: db_label_ref_bys[ind])
                        if (search(by_entry, entries) < 0)
                            update_entries.insert(by_entry);
                }
            }
            if (changed) {
                stmt_update.bind(1, entry);
                stmt_update.bind(2, -(int)order); // -order to avoid UNIQUE constraint
                stmt_update.bind(3, label);
                stmt_update.exec(); stmt_update.reset();
                label_order_neg.emplace_back(label, order);
            }
        }

        // add to entries.labels
        if (!new_labels.empty()) {
            stmt_select0.bind(1, entry);
            if (!stmt_select0.executeStep())
                throw internal_err("db_update_labels(): entry 不存在： " + entry);
            parse(labels, stmt_select0.getColumn(0));
            for (auto &e : new_labels)
                labels.insert(e);
            join(labels_str, labels);
            stmt_update2.bind(1, labels_str);
            stmt_update2.bind(2, entry);
            stmt_update2.exec(); stmt_update2.reset();
            stmt_select0.reset();
        }
    }

    // 检查被删除的标签（如果只被本词条引用， 就留给 \autoref() 报错）
    // 这是因为入本词条的 autoref 还没有扫描不确定没有也被删除
    Str ref_by_str;
    SQLite::Statement stmt_delete(db_rw, R"(DELETE FROM "labels" WHERE "id"=?;)");
    for (Long i = 0; i < size(db_labels_used); ++i) {
        if (!db_labels_used[i]) {
            auto &db_label = db_labels[i];
            if (db_label_ref_bys[i].empty() ||
                (db_label_ref_bys[i].size() == 1 && db_label_ref_bys[i][0] == entry)) {
                SLS_WARN(u8"检测到 label 被删除（将从数据库删除）： " + db_label);
                // delete from "labels"
                stmt_delete.bind(1, db_label);
                stmt_delete.exec(); stmt_delete.reset();
                // delete from "entries.labels"
                stmt_select0.bind(1, db_label_entries[i]);
                SLS_ASSERT(stmt_select0.executeStep());
                parse(labels, stmt_select0.getColumn(0));
                labels.erase(db_label);
                stmt_select0.reset();
                join(labels_str, labels);
                stmt_update2.bind(1, labels_str);
                stmt_update2.bind(2, db_label_entries[i]);
                stmt_update2.exec(); stmt_update2.reset();
            }
            else {
                join(ref_by_str, db_label_ref_bys[i], ", ");
                throw scan_err(u8"检测到 label 被删除： " + db_label + u8"\n但是被这些词条引用： " + ref_by_str);
            }
        }
    }

    // set label orders back to positive
    for (auto &e : label_order_neg) {
        stmt_update0.bind(1, int(e.second)); // order
        stmt_update0.bind(2, e.first); // label
        stmt_update0.exec(); stmt_update0.reset();
    }
    transaction.commit();
    // cout << "done!" << endl;
}

// generate json file containing dependency tree
// empty elements of 'titles' will be ignored
inline Long dep_json(SQLite::Database &db_read)
{
    vecStr entries, titles, entry_chap, entry_part, chap_ids, chap_names, chap_parts, part_ids, part_names;
    vector<DGnode> tree;
    db_get_parts(part_ids, part_names, db_read);
    db_get_chapters(chap_ids, chap_names, chap_parts, db_read);
    db_get_tree(tree, entries, titles, entry_part, entry_chap, db_read);

    Str str;
    // write part names
    str += "{\n  \"parts\": [\n";
    for (Long i = 0; i < size(part_names); ++i)
        str += R"(    {"name": ")" + part_names[i] + "\"},\n";
    str.pop_back(); str.pop_back();
    str += "\n  ],\n";
    // write chapter names
    str += "  \"chapters\": [\n";
    for (Long i = 0; i < size(chap_names); ++i)
        str += R"(    {"name": ")" + chap_names[i] + "\"},\n";
    str.pop_back(); str.pop_back();
    str += "\n  ],\n";
    // write entries
    str += "  \"nodes\": [\n";
    for (Long i = 0; i < size(titles); ++i) {
        if (titles[i].empty())
            continue;
        str += R"(    {"id": ")" + entries[i] + "\"" +
               ", \"part\": " + num2str(search(entry_part[i], part_ids)) +
               ", \"chap\": " + num2str(search(entry_chap[i], chap_ids)) +
               R"(, "title": ")" + titles[i] + "\""
            ", \"url\": \"../online/" +
            entries[i] + ".html\"},\n";
    }
    str.pop_back(); str.pop_back();
    str += "\n  ],\n";

    // write links
    str += "  \"links\": [\n";
    Long Nedge = 0;
    for (Long i = 0; i < size(tree); ++i) {
        for (auto &j : tree[i]) {
            str += R"(    {"source": ")" + entries[i] + "\", ";
            str += R"("target": ")" + entries[j] + "\", ";
            str += "\"value\": 1},\n";
            ++Nedge;
        }
    }
    if (Nedge > 0) {
        str.pop_back(); str.pop_back();
    }
    str += "\n  ]\n}\n";
    write(str, gv::path_out + "../tree/data/dep.json");
    return 0;
}

// update entries.bibs and bibliography.ref_by
inline void db_update_entry_bibs(const unordered_map<Str, unordered_map<Str, Bool>> &entry_bibs_change)
{
    SQLite::Database db_rw(gv::path_data + "scan.db", SQLite::OPEN_READWRITE);
    SQLite::Transaction transaction(db_rw);

    // convert arguments
    unordered_map<Str, unordered_map<Str, Bool>> bib_ref_bys_change; // bibliography.id -> ref_by
    for (auto &e : entry_bibs_change) {
        auto &entry = e.first;
        for (auto &bib: e.second)
            if (bib.second)
                bib_del_ref_bys[bib].insert(entry);
    }

    unordered_map<Str, unordered_set<Str>> bib_add_ref_bys; // bibliography.id -> ref_by
    for (auto &e : entry_add_bibs) {
        auto &entry = e.first;
        for (auto &bib: e.second) {
            bib_add_ref_bys[bib].insert(entry);
        }
    }

    // =========== add & delete bibliography.ref_by =================
    cout << "add & delete bibliography.ref_by..." << endl;
    SQLite::Statement stmt_update_ref_by(db_rw,
        R"(UPDATE "bibliography" SET "ref_by"=? WHERE "id"=?;)");
    Str ref_by_str;
    set<Str> ref_by;

    for (auto &e : bib_add_ref_bys) {
        auto &bib = e.first;
        auto &by_entries = e.second;
        ref_by.clear();
        ref_by_str = get_text("bibliography", "id", bib, "ref_by", db_rw);
        parse(ref_by, ref_by_str);
        for (auto &by_entry : by_entries)
            ref_by.insert(by_entry);
        if (bib_del_ref_bys.count(bib)) {
            for (auto &by_entry: bib_del_ref_bys[bib])
                ref_by.erase(by_entry);
            bib_del_ref_bys.erase(bib);
        }
        join(ref_by_str, ref_by);
        stmt_update_ref_by.bind(1, ref_by_str);
        stmt_update_ref_by.bind(2, bib);
        stmt_update_ref_by.exec(); stmt_update_ref_by.reset();
    }
    cout << "done!" << endl;

    // ========== delete from bibliography.ref_by ===========
    cout << "delete from bibliography.ref_by ..." << endl;
    for (auto &e : bib_del_ref_bys) {
        auto &bib = e.first;
        auto &by_entries = e.second;
        ref_by.clear();
        try {
            ref_by_str = get_text("bibliography", "id", bib, "ref_by", db_rw);
        }
        catch (const std::exception &e) {
            if ((Long)Str(e.what()).find("row not found") >= 0)
                continue; // bibliography.id deleted
        }
        parse(ref_by, ref_by_str);
        for (auto &by_entry: by_entries)
            ref_by.erase(by_entry);
        join(ref_by_str, ref_by);
        stmt_update_ref_by.bind(1, ref_by_str);
        stmt_update_ref_by.bind(2, bib);
        stmt_update_ref_by.exec(); stmt_update_ref_by.reset();
    }
    cout << "done!" << endl;

    // =========== add & delete entry.bibs =================
    cout << "add & delete entry.bibs ..." << endl;
    Str bibs_str;
    set<Str> bibs;
    SQLite::Statement stmt_select_entry_bibs(db_rw,
        R"(SELECT "bibs" FROM "entries" WHERE "id"=?)");
    SQLite::Statement stmt_update_entry_bibs(db_rw,
        R"(UPDATE "entries" SET "bibs"=? WHERE "id"=?)");

    for (auto &e : entry_add_bibs) {
        auto &entry = e.first;
        auto &new_bibs = e.second;
        stmt_select_entry_bibs.bind(1, entry);
        if (!stmt_select_entry_bibs.executeStep())
            throw internal_err("entry 找不到： " + entry);
        parse(bibs, stmt_select_entry_bibs.getColumn(0));
        stmt_select_entry_bibs.reset();
        bibs.insert(new_bibs.begin(), new_bibs.end());
        if (entry_del_bibs.count(entry)) {
            for (auto &bib: entry_del_bibs[entry])
                bibs.erase(bib);
            entry_del_bibs.erase(entry);
        }
        join(bibs_str, bibs);
        stmt_update_entry_bibs.bind(1, bibs_str);
        stmt_update_entry_bibs.bind(2, entry);
        stmt_update_entry_bibs.exec(); stmt_update_entry_bibs.reset();
    }

    cout << "delete entry.bibs ..." << endl;
    for (auto &e : entry_del_bibs) {
        auto &entry = e.first;
        auto &del_bibs = e.second;
        stmt_select_entry_bibs.bind(1, entry);
        if (!stmt_select_entry_bibs.executeStep())
            throw internal_err("entry 找不到： " + entry);
        parse(bibs, stmt_select_entry_bibs.getColumn(0));
        stmt_select_entry_bibs.reset();
        for (auto &bib : del_bibs)
            bibs.erase(bib);
        join(bibs_str, bibs);
        stmt_update_entry_bibs.bind(1, bibs_str);
        stmt_update_entry_bibs.bind(2, entry);
        stmt_update_entry_bibs.exec(); stmt_update_entry_bibs.reset();
    }
    cout << "done!" << endl;
    transaction.commit();
}

// update entries.uprefs, entries.ref_by, figures.ref_by
inline void db_update_uprefs(const set<Str> &uprefs, Str_I entry, SQLite::Database &db_rw)
{
    SQLite::Transaction transaction(db_rw);
    unordered_set<Str> uprefs_del;
    SQLite::Statement stmt_select(db_rw,
        R"(SELECT "uprefs" FROM "entries" WHERE "id"=?;)");
    SQLite::Statement stmt_select2(db_rw,
        R"(SELECT "ref_by" FROM "entries" WHERE "id"=?;)");
    SQLite::Statement stmt_update(db_rw,
        R"(UPDATE "entries" SET "uprefs"=? WHERE "id"=?;)");
    Str ref_by_str;
    set<Str> ref_by;
    stmt_select.bind(1, entry);
    if (!stmt_select.executeStep()) throw internal_err(SLS_WHERE);
    parse(uprefs_del, stmt_select.getColumn(0));
    for (auto &upref : uprefs) {
        if (uprefs_del.count(upref))
            uprefs_del.erase(upref);
        else {
            SLS_WARN(u8"检测到新增的 upref（将添加）：" + upref);
        }
    }
    transaction.commit();
}

// update entries.refs, labels.ref_by, figures.ref_by
inline void db_update_refs(const unordered_map<Str, unordered_set<Str>> &entry_add_refs,
    unordered_map<Str, unordered_set<Str>> &entry_del_refs)
{
    SQLite::Database db_rw(gv::path_data + "scan.db", SQLite::OPEN_READWRITE);
    SQLite::Transaction transaction(db_rw);

    // transform arguments
    unordered_map<Str, unordered_set<Str>> label_add_ref_bys, fig_add_ref_bys;
    for (auto &e : entry_add_refs) {
        auto &entry = e.first;
        for (auto &label: e.second) {
            if (label_type(label) == "fig")
                fig_add_ref_bys[label_id(label)].insert(entry);
            else
                label_add_ref_bys[label].insert(entry);
        }
    }

    unordered_map<Str, unordered_set<Str>> label_del_ref_bys, fig_del_ref_bys;
    for (auto &e : entry_del_refs) {
        auto &entry = e.first;
        for (auto &label: e.second) {
            if (label_type(label) == "fig")
                fig_del_ref_bys[label_id(label)].insert(entry);
            else
                label_del_ref_bys[label].insert(entry);
        }
    }

    // =========== add & delete labels.ref_by =================
    cout << "add & delete labels ref_by..." << endl;
    SQLite::Statement stmt_update_ref_by(db_rw,
        R"(UPDATE "labels" SET "ref_by"=? WHERE "id"=?;)");
    Str ref_by_str;
    set<Str> ref_by;

    for (auto &e : label_add_ref_bys) {
        auto &label = e.first;
        auto &by_entries = e.second;
        ref_by.clear();
        ref_by_str = get_text("labels", "id", label, "ref_by", db_rw);
        parse(ref_by, ref_by_str);
        for (auto &by_entry : by_entries)
            ref_by.insert(by_entry);
        if (label_del_ref_bys.count(label)) {
            for (auto &by_entry : label_del_ref_bys[label])
                ref_by.erase(by_entry);
            label_del_ref_bys.erase(label);
        }
        join(ref_by_str, ref_by);
        stmt_update_ref_by.bind(1, ref_by_str);
        stmt_update_ref_by.bind(2, label);
        stmt_update_ref_by.exec(); stmt_update_ref_by.reset();
    }
    cout << "done!" << endl;

    // =========== add & delete figures.ref_by =================
    cout << "add & delete figures ref_by..." << endl;
    SQLite::Statement stmt_update_ref_by_fig(db_rw,
        R"(UPDATE "figures" SET "ref_by"=? WHERE "id"=?;)");
    unordered_map<Str, set<Str>> new_entry_figs;
    // add to ref_by
    for (auto &e : fig_add_ref_bys) {
        auto &fig_id = e.first;
        auto &by_entries = e.second;
        ref_by.clear();
        ref_by_str = get_text("figures", "id", fig_id, "ref_by", db_rw);
        parse(ref_by, ref_by_str);
        for (auto &by_entry : by_entries)
            ref_by.insert(by_entry);
        if (fig_del_ref_bys.count(fig_id)) {
            for (auto &by_entry : fig_del_ref_bys[fig_id])
                ref_by.erase(by_entry);
            fig_del_ref_bys.erase(fig_id);
        }
        join(ref_by_str, ref_by);
        stmt_update_ref_by_fig.bind(1, ref_by_str);
        stmt_update_ref_by_fig.bind(2, fig_id);
        stmt_update_ref_by_fig.exec(); stmt_update_ref_by_fig.reset();
    }
    cout << "done!" << endl;

    // ========== delete from labels.ref_by ===========
    cout << "delete from labels.ref_by" << endl;
    Str type, fig_id;
    for (auto &e : label_del_ref_bys) {
        auto &label = e.first;
        auto &by_entries = e.second;
        try {
            ref_by_str = get_text("labels", "id", label, "ref_by", db_rw);
        }
        catch (const std::exception &e) {
            if ((Long)Str(e.what()).find("row not found") >= 0)
                continue; // label deleted
        }
        parse(ref_by, ref_by_str);
        for (auto &by_entry: by_entries)
            ref_by.erase(by_entry);
        join(ref_by_str, ref_by);
        stmt_update_ref_by.bind(1, ref_by_str);
        stmt_update_ref_by.bind(2, label);
        stmt_update_ref_by.exec(); stmt_update_ref_by.reset();
    }

    // ========== delete from figures.ref_by ===========
    cout << "delete from figures.ref_by" << endl;
    for (auto &e : fig_del_ref_bys) {
        auto &fig_id = e.first;
        auto &by_entries = e.second;
        try {
            ref_by_str = get_text("figures", "id", fig_id, "ref_by", db_rw);
        }
        catch (const std::exception &e) {
            if ((Long)Str(e.what()).find("row not found") >= 0)
                continue; // label deleted
        }
        parse(ref_by, ref_by_str);
        for (auto &by_entry : by_entries)
            ref_by.erase(by_entry);
        join(ref_by_str, ref_by);
        stmt_update_ref_by_fig.bind(1, ref_by_str);
        stmt_update_ref_by_fig.bind(2, fig_id);
        stmt_update_ref_by_fig.exec(); stmt_update_ref_by_fig.reset();
    }
    cout << "done!" << endl;

    // =========== add & delete entries.refs =================
    cout << "add & delete entries.refs ..." << endl;
    Str ref_str;
    set<Str> refs;
    SQLite::Statement stmt_select_entry_refs(db_rw,
        R"(SELECT "refs" FROM "entries" WHERE "id"=?)");
    SQLite::Statement stmt_update_entry_refs(db_rw,
        R"(UPDATE "entries" SET "refs"=? WHERE "id"=?)");
    for (auto &e : entry_add_refs) {
        auto &entry = e.first;
        auto &new_refs = e.second;
        stmt_select_entry_refs.bind(1, entry);
        if (!stmt_select_entry_refs.executeStep())
            throw internal_err("entry 找不到： " + entry);
        parse(refs, stmt_select_entry_refs.getColumn(0));
        stmt_select_entry_refs.reset();
        refs.insert(new_refs.begin(), new_refs.end());
        if (entry_del_refs.count(entry)) {
            for (auto &label: entry_del_refs[entry])
                refs.erase(label);
            entry_del_refs.erase(entry);
        }
        join(ref_str, refs);
        stmt_update_entry_refs.bind(1, ref_str);
        stmt_update_entry_refs.bind(2, entry);
        stmt_update_entry_refs.exec(); stmt_update_entry_refs.reset();
    }
    for (auto &e : entry_del_refs) {
        auto &entry = e.first;
        auto &del_refs = e.second;
        stmt_select_entry_refs.bind(1, entry);
        if (!stmt_select_entry_refs.executeStep())
            throw internal_err("entry 找不到： " + entry);
        parse(refs, stmt_select_entry_refs.getColumn(0));
        stmt_select_entry_refs.reset();
        for (auto &label : del_refs)
            refs.erase(label);
        join(ref_str, refs);
        stmt_update_entry_refs.bind(1, ref_str);
        stmt_update_entry_refs.bind(2, entry);
        stmt_update_entry_refs.exec(); stmt_update_entry_refs.reset();
    }
    cout << "done!" << endl;
    transaction.commit();
}

// make db consistent
// (regenerate derived fields)
inline void arg_fix_db()
{
    SQLite::Database db_rw(gv::path_data + "scan.db", SQLite::OPEN_READWRITE);
    Str refs_str, entry;
    set<Str> refs;
    unordered_map<Str, set<Str>> figs_ref_by, labels_ref_by, bib_ref_by;

    // get all labels.id and figures.id from database, to detect unused ones
    unordered_set<Str> db_figs_unused, db_labels_unused;
    {
        vecStr db_figs0, db_labels0;
        get_column(db_figs0, "figures", "id", db_rw);
        get_column(db_labels0, "labels", "id", db_rw);
        db_figs_unused.insert(db_figs0.begin(), db_figs0.end());
        db_labels_unused.insert(db_labels0.begin(), db_labels0.end());
    }

    // === regenerate "figures.entry", "labels.entry" from "entries.figures", "entries.labels" ======
    cout << R"(regenerate "figures.entry", "labels.entry" from "entries.figures", "entries.labels"...)" << endl;
    SQLite::Transaction transaction(db_rw);

    SQLite::Statement stmt_select_labels(db_rw, R"(SELECT "id", "labels" FROM "entries" WHERE "labels" != '';)");
    SQLite::Statement stmt_update_labels_entry(db_rw, R"(UPDATE "labels" SET "entry"=? WHERE "id"=?;)");
    unordered_set<Str> labels;
    while (stmt_select_labels.executeStep()) {
        entry = (const char*)stmt_select_labels.getColumn(0);
        parse(labels, stmt_select_labels.getColumn(1));
        for (auto &label : labels) {
            db_labels_unused.erase(label);
            stmt_update_labels_entry.bind(1, entry);
            stmt_update_labels_entry.bind(2, label);
            stmt_update_labels_entry.exec();
            if (!stmt_update_labels_entry.getChanges())
                throw internal_err("数据库 labels 表格中未找到： " + label);
            stmt_update_labels_entry.reset();
        }
    }

    SQLite::Statement stmt_select_figs(db_rw, R"(SELECT "id", "figures" FROM "entries" WHERE "figures" != '';)");
    SQLite::Statement stmt_update_figs_entry(db_rw, R"(UPDATE "figures" SET "entry"=? WHERE "id"=?;)");
    unordered_set<Str> figs;
    while (stmt_select_figs.executeStep()) {
        entry = (const char*)stmt_select_figs.getColumn(0);
        parse(figs, stmt_select_figs.getColumn(1));
        for (auto &fig_id : figs) {
            db_figs_unused.erase(fig_id);
            stmt_update_figs_entry.bind(1, entry);
            stmt_update_figs_entry.bind(2, fig_id);
            stmt_update_figs_entry.exec();
            if (!stmt_update_figs_entry.getChanges())
                throw internal_err("数据库 figs 表格中未找到： " + fig_id);
            stmt_update_figs_entry.reset();
        }
    }
    cout << "done!" << endl;

    // === regenerate "figures.ref_by", "labels.ref_by" from "entries.refs" ===========
    exec(R"(UPDATE "figures" SET "ref_by"='';)", db_rw);
    exec(R"(UPDATE "labels" SET "ref_by"='';)", db_rw);

    cout << R"(regenerate "figures.ref_by", "labels.ref_by" from "entries.refs...")" << endl;
    SQLite::Statement stmt_select_refs(db_rw, R"(SELECT "id", "refs" FROM "entries" WHERE "refs" != '';)");
    while (stmt_select_refs.executeStep()) {
        entry = (const char*)stmt_select_refs.getColumn(0);
        parse(refs, stmt_select_refs.getColumn(1));
        for (auto &ref : refs) {
            if (label_type(ref) == "fig")
                figs_ref_by[label_id(ref)].insert(entry);
            else
                labels_ref_by[ref].insert(entry);
        }
    }

    SQLite::Statement stmt_update_fig_ref_by(db_rw, R"(UPDATE "figures" SET "ref_by"=? WHERE "id"=?;)");
    Str ref_by_str;
    for (auto &e : figs_ref_by) {
        join(ref_by_str, e.second);
        stmt_update_fig_ref_by.bind(1, ref_by_str);
        stmt_update_fig_ref_by.bind(2, e.first);
        stmt_update_fig_ref_by.exec();
        if (!stmt_update_fig_ref_by.getChanges())
            throw internal_err("数据库 figures 表格中未找到： " + e.first);
        stmt_update_fig_ref_by.reset();
    }

    SQLite::Statement stmt_update_labels_ref_by(db_rw, R"(UPDATE "labels" SET "ref_by"=? WHERE "id"=?;)");
    for (auto &e : labels_ref_by) {
        join(ref_by_str, e.second);
        stmt_update_labels_ref_by.bind(1, ref_by_str);
        stmt_update_labels_ref_by.bind(2, e.first);
        stmt_update_labels_ref_by.exec();
        if (!stmt_update_labels_ref_by.getChanges())
            throw internal_err("数据库 labels 表格中未找到： " + e.first);
        stmt_update_labels_ref_by.reset();
    }
    cout << "done!" << endl;

    // === regenerate "bibliography.ref_by" from "entries.bibs" =====
    set<Str> bibs;
    exec(R"(UPDATE "bibliography" SET "ref_by"='';)", db_rw);
    cout << R"(regenerate "bibliography.ref_by"  from "entries.bibs ...")" << endl;
    SQLite::Statement stmt_select_bibs(db_rw, R"(SELECT "id", "bibs" FROM "entries" WHERE "bibs" != '';)");
    while (stmt_select_bibs.executeStep()) {
        entry = (const char*)stmt_select_bibs.getColumn(0);
        parse(bibs, stmt_select_bibs.getColumn(1));
        for (auto &bib : bibs) {
            bib_ref_by[bib].insert(entry);
        }
    }

    SQLite::Statement stmt_update_bib_ref_by(db_rw, R"(UPDATE "bibliography" SET "ref_by"=? WHERE "id"=?;)");
    for (auto &e : bib_ref_by) {
        join(ref_by_str, e.second);
        stmt_update_bib_ref_by.bind(1, ref_by_str);
        stmt_update_bib_ref_by.bind(2, e.first); // bib_id
        stmt_update_bib_ref_by.exec();
        if (!stmt_update_bib_ref_by.getChanges())
            throw internal_err("数据库 figures 表格中未找到： " + e.first);
        stmt_update_bib_ref_by.reset();
    }
    cout << "done!" << endl;

    transaction.commit();
}
