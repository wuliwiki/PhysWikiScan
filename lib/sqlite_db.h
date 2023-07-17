#pragma once
#include <regex>
#include "../SLISC/str/str.h"
#include "../SLISC/util/time.h"

// add or delete elements from a set
template <class T>
inline void change_set(set<T> &s, const unordered_map<T, bool> &change)
{
	for (auto &e : change)
		if (e.second)
			s.insert(e.first);
		else
			s.erase(e.first);
}

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
	if (str.empty())
		return;
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

// get title and Pentry obj for one entry
// stmt_select should be defined as follows
// SQLite::Statement stmt_select(db_read,
//		R"(SELECT "caption", "pentry" FROM "entries" WHERE "id" = ?;)");
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

// get dependency tree from database, for (the last node of) 1 entry0 (last node as root)
// edge direction is the inverse of learning direction
// each node of the tree is a part of an entry0 (if there are multiple pentries)
// also check for cycle, and check for any of it's pentries are redundant
// `entry0:(i+1)` will be in nodes[i]
// nodes[i] corresponds to tree[i]
inline void db_get_tree1(
	vector<DGnode> &tree, // [out] dep tree, each node will include last node of the same entry (if any)
	vector<Node> &nodes, // [out] nodes[i] is tree[i], with (entry, order), where order > 0
	unordered_map<Str, pair<Str, Pentry>> &entry_info, // [out] entry -> (title, Pentry)
	Str_I entry0, Str_I title0, // use the last node as tree root
	Pentry_IO pentry0, // pentry lists for entry0. `pentry0[i][j].tilde` will be updated
	SQLite::Database &db_read)
{
	tree.clear(); nodes.clear(); entry_info.clear();
	SQLite::Statement stmt_select(db_read,
		R"(SELECT "caption", "pentry" FROM "entries" WHERE "id" = ?;)");

	Long Nnode0 = size(pentry0);
	if (Nnode0 == 0) { // no \pentry
		nodes.emplace_back(entry0, 1);
		tree.emplace_back();
		return;
	}
	deque<Node> q; // nodes to search

	// set the first Nnode0 of nodes to all nodes of `entry0`
	for (Long i_node = 1; i_node <= Nnode0; ++i_node) {
		nodes.emplace_back(entry0, i_node);
		q.emplace_back(entry0, i_node);
	}
	entry_info[entry0].first = title0;
	entry_info[entry0].second = pentry0;

	// broad first search (BFS) to get all nodes involved
	while (!q.empty()) {
		Str entry = std::move(q.front().entry);
		Long i_node = q.front().i_node;
		q.pop_front();
		auto &pentry = entry_info[entry].second;
		if (pentry.empty()) continue;
		if (i_node > size(pentry))
			throw internal_err(Str(__func__) + SLS_WHERE);
		if (i_node > 1) { // implicitly depends on the previous node
            Node nod(entry, i_node-1);
            if (search(nod, nodes) < 0) {
                nodes.push_back(nod);
                if (find(q.begin(), q.end(), nod) == q.end())
                    q.push_back(nod);
            }
        }
		if (pentry[i_node-1].empty())
			throw internal_err("empty \\pentry{} should have been ignored. " SLS_WHERE);
		for (auto &en : pentry[i_node-1]) {
			if (!entry_info.count(en.entry))
				db_get_entry_info(entry_info[en.entry], en.entry, stmt_select);
			if (en.i_node == 0) { // 0 means the last node
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

	// ---- construct tree ----
	// every node in `entry_info` & `nodes` has a non-zero i_node now
	tree.resize(nodes.size());
	for (Long i = 0; i < size(nodes); ++i) {
		auto &entry = nodes[i].entry;
        Long i_node = nodes[i].i_node;
		auto &pentry = entry_info[entry].second;

		if (pentry.empty()) continue;
		if (i_node > 1) { // implicitly depends on the last node
			Node nod(entry, i_node-1);
			Long from = search(nod, nodes);
			if (from < 0)
				throw internal_err(u8"上一个节点未找到（应该不会发生才对）： "
					+ nod.entry + ":" + to_string(nod.i_node));
			tree[i].push_back(from);
		}
		for (auto &en : pentry[i_node-1]) {
			Node nod(en.entry, en.i_node);
			Long from = search(nod, nodes);
			if (from < 0)
				throw internal_err(u8"节点未找到（应该不会发生才对）： "
								+ nod.entry + ":" + to_string(nod.i_node));
			tree[i].push_back(from);
		}
	}

	// check cyclic
	vecLong cycle;
	if (!dag_check(cycle, tree)) {
		cycle.push_back(cycle[0]);
		Str msg = u8"存在循环预备知识: ";
		for (auto ind : cycle) {
			auto &entry = nodes[ind].entry;
			auto &title = entry_info[entry].first;
			msg << to_string(ind) << "." << title << " (" << entry << ") -> ";
		}
		throw scan_err(msg);
	}

	// check redundancy
	if (0) { // debug: print edges
		for (Long i = 0; i < size(tree); ++i) {
			auto &entry_i = nodes[i].entry;
			cout << i << "." << entry_i << " " << entry_info[entry_i].first << endl;
			for (auto &j: tree[i]) {
				auto &entry_j = nodes[j].entry;
					cout << "  << " << j << "." << entry_j << " " << entry_info[entry_j].first << " " << endl;
			}
		}
	}

	vector<vecLong> alt_paths; // path from nodes[0] to one redundant child
	auto &pentry0_info = get<1>(entry_info[entry0]);
	// reduce each node of entry0
	for (Long i_node = 1; i_node <= Nnode0; ++i_node) {
		auto &root = nodes[i_node-1];
		Str &root_title = entry_info[root.entry].first;
		auto &pentry1 = pentry0_info[root.i_node - 1];
		dag_reduce(alt_paths, tree, i_node-1);
		std::stringstream ss;
		if (!alt_paths.empty()) {
			for (Long j = 0; j < size(alt_paths); ++j) {
				// mark ignored (add ~ to the end)
				auto &alt_path = alt_paths[j];
				Long i_red = alt_path.back(); // node[i_red] is redundant
				auto &node_red = nodes[i_red];
				bool found = false;
				for (auto &e: pentry1) {
					if (e.entry == node_red.entry && e.i_node == node_red.i_node) {
						e.tilde = found = true;
						break;
					}
				}
				if (!found)
					throw internal_err(SLS_WHERE);
				auto &title_red = get<0>(entry_info[node_red.entry]);
				ss << u8"\n已标记 " << root.entry << ':' << root.i_node << " (" << root_title << u8") 中多余的预备知识： "
				   << node_red.entry << ':' << node_red.i_node << " (" << title_red << ')' << endl;
				ss << u8"   已存在路径： " << endl;
				for (Long i = 0; i < size(alt_path); ++i) {
					auto &nod = nodes[alt_path[i]];
					auto &title = get<0>(entry_info[nod.entry]);
					ss << nod.entry << ':' << nod.i_node << " (" << title
					   << (i == size(alt_path) - 1 ? ")" : ") <-") << endl;
				}
			}
			SLS_WARN(ss.str());
		}
	}

	// copy tilde to pentry0
	for (Long i = 0; i < size(pentry0_info); ++i) {
		auto &pentry1_info = pentry0_info[i];
		auto &pentry1 = pentry0[i];
		for (Long j = 0; j < size(pentry1_info); ++j)
			pentry1[j].tilde = pentry1_info[j].tilde;
	}
}

// get entire dependency tree (2 ver) from database
// edge direction is the inverse of learning direction
// tree: one node for each entry
// tree2: one node for each \pentry{}
inline void db_get_tree(
		vector<DGnode> &tree, // [out] dep tree, each node will include last node of the same entry (if any)
		vector<Node> &nodes, // [out] nodes[i] is tree[i], with (entry, order)
		vector<DGnode> &tree2, // [out] same with `tree`, but each tree node is one entry
		unordered_map<Str, tuple<Str, Str, Str, Pentry>> &entry_info, // [out] entry -> (title,part,chapter,Pentry)
		SQLite::Database &db_read)
{
	tree.clear();
	SQLite::Statement stmt_select(db_read,
		R"(SELECT "id", "caption", "part", "chapter", "pentry" FROM "entries ")"
		R"(WHERE "deleted" = 0 ORDER BY "id" COLLATE NOCASE ASC;)");
	Str tmp;

	// get info
	while (stmt_select.executeStep()) {
		const Str &entry = stmt_select.getColumn(0);
		auto &info = entry_info[entry];
		auto &pentry = get<3>(info);
		get<0>(info) = (const char*)stmt_select.getColumn(0); // titles
		get<1>(info) = (const char*)stmt_select.getColumn(1); // parts
		get<2>(info) = (const char*)stmt_select.getColumn(2); // chapter
		parse_pentry(pentry, stmt_select.getColumn(3)); // pentry
	}

	// construct tree
	for (auto &e : entry_info) {
		auto &entry = e.first;
		auto &pentry = get<3>(e.second);
		if (pentry.empty()) continue;
		for (Long i_node = 1; i_node <= size(pentry); ++i_node) {
			auto &pentry1 = pentry[i_node-1];
			nodes.emplace_back(entry, i_node);
			tree.emplace_back();
			for (auto &ee: pentry1) {
				// convert node `0` to the actual node
				if (ee.i_node == 0) {
					ee.i_node = size(get<3>(entry_info[ee.entry]));
					if (ee.i_node == 0) ee.i_node = 1;
				}
				if (!ee.tilde) {
					Long to = search(Node(ee.entry, ee.i_node), nodes);
					if (to < 0) throw internal_err(SLS_WHERE);
					tree.back().push_back(to);
				}
			}
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
	Str tmp;
	for (auto &entry : entries) {
		if (!exist("entries", "id", entry, db_rw)) {
			SLS_WARN(u8"词条不存在数据库中， 将模拟 editor 添加： " + entry);
			// 从 tex 文件获取标题
			Str str;
			tmp = gv::path_in; tmp << "contents/" << entry << ".tex";
			read(str, tmp); // read tex file
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
	Str tmp;
	while (stmt_select.executeStep()) {
		const Str &id = (const char*)stmt_select.getColumn(0);
		auto &info = db_bib_info[id];
		info.order = (int)stmt_select.getColumn(1);
		info.detail = (const char*)stmt_select.getColumn(2);
		const Str &ref_by_str = (const char*)stmt_select.getColumn(3);
		parse(info.ref_by, ref_by_str);
		if (search(id, bib_labels) < 0) {
			if (!info.ref_by.empty()) {
				tmp = u8"检测到删除的文献： "; tmp << id << u8"， 但被以下词条引用： " << ref_by_str;
				throw scan_err(tmp);
			}
			tmp = u8"检测到删除文献（将删除）： "; tmp << to_string(info.order) << ". " << id;
			SLS_WARN(tmp);
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
				tmp = u8"数据库中文献 "; tmp << id << " 编号改变（将更新）： " << to_string(info.order)
						<< " -> " << to_string(order);
				SLS_WARN(tmp);
				changed = true;
				id_flip_sign.insert(id);
				stmt_update.bind(1, -(int)order); // to avoid unique constraint
			}
			if (bib_detail != info.detail) {
				tmp = u8"数据库中文献 ";
				tmp << id << " 详情改变（将更新）： " << info.detail << " -> " << bib_detail;
				SLS_WARN(tmp);
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
		vecStr_I entry_first, vecStr_I entry_last, SQLite::Database &db_rw)
{
	cout << "updating sqlite database (" << part_name.size() << " parts, "
		 << chap_name.size() << " chapters) ..." << endl;
	cout.flush();
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
		R"(UPDATE "entries" SET "caption"=?, "part"=?, "chapter"=?, "last"=?, "next"=? WHERE "id"=?;)");
	SQLite::Statement stmt_select(db_rw,
		R"(SELECT "caption", "keys", "pentry", "part", "chapter", "last", "next" FROM "entries" WHERE "id"=?;)");
	SQLite::Transaction transaction(db_rw);
	Str tmp, empty;
	Long N = size(entries);

	for (Long i = 0; i < N; i++) {
		auto &entry = entries[i];
		auto &entry_last = (i == 0 ? empty : entries[i-1]);
		auto &entry_next = (i == N-1 ? empty : entries[i+1]);

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
			Str db_last = (const char*) stmt_select.getColumn(5);
			Str db_next = (const char*) stmt_select.getColumn(6);
			stmt_select.reset();

			bool changed = false;
			if (titles[i] != db_title) {
				tmp = entry; tmp << ": title has changed from " << db_title << " to " << titles[i];
				SLS_WARN(tmp);
				changed = true;
			}
			if (part_ids[entry_part[i]] != db_part) {
				tmp = entry; tmp << ": part has changed from " << db_part << " to " << part_ids[entry_part[i]];
				SLS_WARN(tmp);
				changed = true;
			}
			if (chap_ids[entry_chap[i]] != db_chapter) {
				tmp = entry; tmp << ": chapter has changed from " << db_chapter << " to " << chap_ids[entry_chap[i]];
				SLS_WARN(tmp);
				changed = true;
			}
			if (entry_last != db_last) {
				tmp = entry; tmp << ": 'last' has changed from " << db_last << " to " << entry_last;
				SLS_WARN(tmp);
				changed = true;
			}
			if (entry_next != db_next) {
				tmp = entry; tmp << ": 'next' has changed from " << db_next << " to " << entry_next;
				SLS_WARN(tmp);
				changed = true;
			}
			if (changed) {
				stmt_update.bind(1, titles[i]);
				stmt_update.bind(2, part_ids[entry_part[i]]);
				stmt_update.bind(3, chap_ids[entry_chap[i]]);
				stmt_update.bind(4, entry_last);
				stmt_update.bind(5, entry_next);
				stmt_update.bind(6, entry);
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

	//            hash        time author entry  file-exist
	unordered_map<Str,  tuple<Str, Long,  Str,   bool>> db_history;
	while (stmt_select.executeStep()) {
		Long author_id = (int) stmt_select.getColumn(2);
		db_history[stmt_select.getColumn(0)] =
			make_tuple(stmt_select.getColumn(1).getString(),
				author_id , stmt_select.getColumn(3).getString(), false);
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

	SQLite::Statement stmt_select3(db_rw,
		R"(SELECT "id", "aka" FROM "authors" WHERE "aka" != -1;)");
	unordered_map<int, int> db_author_aka;
	while (stmt_select3.executeStep()) {
		int id = stmt_select3.getColumn(0);
		int aka = stmt_select3.getColumn(1);
		db_author_aka[id] = aka;
	}

	db_author_ids0.clear(); db_author_names0.clear();
	db_author_ids0.shrink_to_fit(); db_author_names0.shrink_to_fit();

	SQLite::Statement stmt_insert(db_rw,
		R"(INSERT INTO "history" ("hash", "time", "author", "entry") VALUES (?, ?, ?, ?);)");

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

	Str fpath, tmp;

	for (Long i = 0; i < size(fnames); ++i) {
		auto &fname = fnames[i];
		fpath = path + fname + ".tex";
		read(tmp, fpath); CRLF_to_LF(tmp);
		sha1 = sha1sum(tmp).substr(0, 16);
		bool sha1_exist = db_history.count(sha1);

		// fname = YYYYMMDDHHMM_authorID_entry
		time = fname.substr(0, 12);
		Long ind = fname.rfind('_');
		entry = fname.substr(ind+1);
		author = fname.substr(13, ind-13);
		Long authorID;

		// check if editor is still saving using old `YYYYMMDDHHMM_author_entry` format
		Bool use_author_name = true;
		if (str2int(authorID, author) == size(author)) {
			// SQLite::Statement stmt_select4(db_rw, R"(SELECT 1 FROM "authors" WHERE "id"=)" + author);
			// if (stmt_select4.executeStep())
				use_author_name = false;
		}

		if (use_author_name) {
			// editor is still saving using old `YYYYMMDDHHMM_author_entry` format for now
			// TODO: delete this block after editor use `YYYYMMDDHHMM_authorID_entry` format
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
				stmt_insert_auth.exec();
				stmt_insert_auth.reset();
			}
			// rename to new format
			fname = time; fname << '_' << to_string(authorID) << '_' << entry;
			tmp = path; tmp << fname << ".tex";
			SLS_WARN("moving " + fpath + " -> " + tmp);
			file_move(tmp, fpath);
		}

		author_contrib[authorID] += 5;
		if (entries.count(entry) == 0 &&
							entries_deleted_inserted.count(entry) == 0) {
			SLS_WARN(u8"备份文件中的词条不在数据库中（将模拟编辑器添加）： " + entry);
			stmt_insert_entry.bind(1, entry);
			stmt_insert_entry.exec(); stmt_insert_entry.reset();
			entries_deleted_inserted.insert(entry);
		}

		if (sha1_exist) {
			auto &time_author_entry_fexist = db_history[sha1];
			if (get<0>(time_author_entry_fexist) != time)
				SLS_WARN(u8"备份 " + fname + u8" 信息与数据库中的时间不同， 数据库中为（将不更新）： " + get<0>(time_author_entry_fexist));
			if (get<1>(time_author_entry_fexist) != authorID)
				SLS_WARN(u8"备份 " + fname + u8" 信息与数据库中的作者不同， 数据库中为（将不更新）： " +
						 to_string(get<1>(time_author_entry_fexist)) + "." + db_id_to_author[get<1>(time_author_entry_fexist)]);
			if (get<2>(time_author_entry_fexist) != entry)
				SLS_WARN(u8"备份 " + fname + u8" 信息与数据库中的文件名不同， 数据库中为（将不更新）： " + get<2>(time_author_entry_fexist));
			get<3>(time_author_entry_fexist) = true;
		}
		else {
			SLS_WARN(u8"数据库的 history 表格中不存在备份文件（将添加）：" + sha1 + " " + fname);
			stmt_insert.bind(1, sha1);
			stmt_insert.bind(2, time);
			stmt_insert.bind(3, int(authorID));
			stmt_insert.bind(4, entry);
			try { stmt_insert.exec(); }
			catch (std::exception &e) { throw internal_err(Str(e.what()) + SLS_WHERE); }
			stmt_insert.reset();
			db_history[sha1] = make_tuple(time, authorID, entry, true);
		}
	}

	for (auto &row : db_history) {
		if (!get<3>(row.second)) {
			cout << u8"数据库 history 中文件不存在：" << row.first << ", " << get<0>(row.second) << ", " <<
				 get<1>(row.second) << ", " << get<2>(row.second) << endl;
		}
	}
	cout << "\ndone." << endl;

	for (auto &new_author : new_authors)
		cout << u8"新作者： " << new_author.second << ". " << new_author.first << endl;

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

// update "history.last" for all table
inline void db_update_history_last(SQLite::Database &db_read, SQLite::Database &db_rw)
{
	cout << "updating history.last..." << endl;
	SQLite::Statement stmt_select(db_read,
		R"(SELECT "hash", "time", "entry" FROM "history";)");
	unordered_map<Str, map<Str, Str>> entry2time_hash; // entry -> (time -> hash)
	while (stmt_select.executeStep()) {
		entry2time_hash[stmt_select.getColumn(2)]
			[stmt_select.getColumn(1).getString()] =
				stmt_select.getColumn(0).getString();
	}

	// update db
	SQLite::Transaction transaction(db_rw);
	SQLite::Statement stmt_update(db_rw,
		R"(UPDATE "history" SET "last"=? WHERE "hash"=?;)");
	Str last_hash;
	for (auto &e : entry2time_hash) {
		last_hash.clear();
		for (auto &time_hash : e.second) {
			auto &hash = time_hash.second;
			stmt_update.bind(1, last_hash);
			stmt_update.bind(2, hash);
			stmt_update.exec(); stmt_update.reset();
			last_hash = hash;
		}
	}
	transaction.commit();
	cout << "done." << endl;
}

// sum history.add and history.del for an author for a given time period
// time_start and time_end are in yyyymmddmmss format
inline void author_char_stat(Str_I time_start, Str_I time_end, Str author, SQLite::Database &db_read)
{
	if (time_start.size() != 12 || time_end.size() != 12)
		throw scan_err(u8"日期格式不正确" SLS_WHERE);
	Long authorID = -1;
	SQLite::Statement stmt_author0(db_read,
		R"(SELECT "id", "name" FROM "authors" WHERE "name"=?;)");
	stmt_author0.bind(1, author);
	Long Nfound = 0;
	while (stmt_author0.executeStep()) {
		++Nfound;
		authorID = (int)stmt_author0.getColumn(0);
		cout << "作者： " << stmt_author0.getColumn(1).getString() << endl;
		cout << "id: " << authorID << endl;
	}
	if (Nfound > 1)
		throw scan_err(u8"数据库中找到多于一个作者名（精确匹配）： " + author);
	else if (Nfound == 0) {
		cout << "精确匹配失败， 尝试模糊匹配……" << endl;
		SQLite::Statement stmt_author(db_read,
			R"(SELECT "id", "name" FROM "authors" WHERE "name" LIKE ? ESCAPE '\';)");
		replace(author, "_", "\\_"); replace(author, "%", "\\%");
		stmt_author.bind(1, '%' + author + '%');
		Long Nfound = 0;
		while (stmt_author.executeStep()) {
			++Nfound;
			authorID = (int)stmt_author.getColumn(0);
			cout << "作者： " << stmt_author.getColumn(1).getString() << endl;
			cout << "id: " << authorID << endl;
		}
		if (Nfound == 0)
			throw scan_err(u8"数据库中找不到作者名（模糊匹配）： " + author);
		if (Nfound > 1)
			throw scan_err(u8"数据库中找到多于一个作者名（模糊匹配）： " + author);
	}

	Str tmp = R"(SELECT SUM("add"), SUM("del") FROM "history" WHERE "author"=)";
	tmp << authorID << R"( AND "time" >= ')" << time_start << R"(' AND "time" <= ')" << time_end << "';";
	cout << "SQL 命令:\n" << tmp << endl;
	SQLite::Statement stmt_select(db_read, tmp);
	if (!stmt_select.executeStep())
		throw internal_err(SLS_WHERE);

	auto data = stmt_select.getColumn(0);
	Long tot_add = data.isNull() ? 0 : data.getInt64();

	data = stmt_select.getColumn(1);
	Long tot_del = data.isNull() ? 0 : data.getInt64();

	cout << "新增字符：" << tot_add << endl;
	cout << "删除字符：" << tot_del << endl;
	cout << "新增减删除：" << tot_add - tot_del << endl;
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
				if (image_ext != db_image_ext) {
					tmp = u8"图片文件拓展名不允许改变: "; tmp << image_hash << '.'
						  << db_image_ext << " -> " << image_ext;
					throw internal_err(tmp);
				}
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
	Str id, tmp;
	Long order;
	SQLite::Transaction transaction(db_rw);
	SQLite::Statement stmt_select0(db_rw,
		R"(SELECT "figures" FROM "entries" WHERE "id"=?;)");
	SQLite::Statement stmt_select1(db_rw,
		R"(SELECT "order", "ref_by", "image", "image_alt", "deleted" FROM "figures" WHERE "id"=?;)");
	SQLite::Statement stmt_insert(db_rw,
		R"(INSERT INTO "figures" ("id", "entry", "order", "image", "image_alt") VALUES (?, ?, ?, ?, ?);)");
	SQLite::Statement stmt_update(db_rw,
		R"(UPDATE "figures" SET "entry"=?, "order"=?, "image"=?, "image_alt"=?, "deleted"=0 WHERE "id"=?;)");
	SQLite::Statement stmt_update2(db_rw,
		R"(UPDATE "entries" SET "figures"=? WHERE "id"=?;)");

	// get all figure envs defined in `entries`, to detect deleted figures
	// db_xxx[i] are from the same row of "labels" table
	vecStr db_figs, db_fig_entries, db_fig_image, db_fig_image_alt;
	vecBool db_figs_used, figs_used;
	vecLong db_fig_orders;
	vector<vecStr> db_fig_ref_bys;
	set<Str> db_entry_figs;
	for (Long j = 0; j < size(entries); ++j) {
		auto &entry = entries[j];
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
			db_figs_used.push_back(!(bool)stmt_select1.getColumn(4).getInt());
			stmt_select1.reset();
		}
	}
	figs_used.resize(db_figs_used.size(), false);
	Str figs_str, image, image_alt;
	set<Str> new_figs, figs;

	for (Long i = 0; i < size(entries); ++i) {
		auto &entry = entries[i];
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
			image = image.substr(0, 16); // keep only hash
			if (ind < 0) { // 图片 label 不在 entries.figures 中
				tmp = u8"发现数据库中没有的图片环境（将模拟 editor 添加）："; tmp << id << ", " << entry
					<< ", " << to_string(order);
				SLS_WARN(tmp);
				stmt_insert.bind(1, id);
				stmt_insert.bind(2, entry);
				stmt_insert.bind(3, (int)order);
				stmt_insert.bind(4, image);
				stmt_insert.bind(5, image_alt);
				stmt_insert.exec(); stmt_insert.reset();
				new_figs.insert(id);
				continue;
			}
			figs_used[ind] = true;
			bool changed = false;
			// 图片 label 在 entries.figures 中，检查是否未被使用
			if (!db_figs_used[ind]) {
				SLS_WARN(u8"发现数据库中未使用的图片被重新使用：" + id);
				changed = true;
			}
			if (entry != db_fig_entries[ind]) {
				tmp = u8"发现数据库中图片 entry 改变（将更新）："; tmp << id << ": "
					  << db_fig_entries[ind] << " -> " << entry;
				SLS_WARN(tmp);
				changed = true;
			}
			if (order != db_fig_orders[ind]) {
				tmp = u8"发现数据库中图片 order 改变（将更新）："; tmp << id << ": "
						<< to_string(db_fig_orders[ind]) << " -> " << to_string(order);
				SLS_WARN(tmp);
				changed = true;

				// order change means other ref_by entries needs to be updated with autoref() as well.
				if (!gv::is_entire) {
					for (auto &by_entry: db_fig_ref_bys[ind])
						if (search(by_entry, entries) < 0)
							update_entries.insert(by_entry);
				}
			}
			if (image != db_fig_image[ind]) {
				tmp = u8"发现数据库中图片 image 改变（将更新）："; tmp << id << ": "
					  << db_fig_image[ind] << " -> " << image;
				SLS_WARN(tmp);
				changed = true;
			}
			if (image_alt != db_fig_image_alt[ind]) {
				tmp = u8"发现数据库中图片 image_alt 改变（将更新）："; tmp << id << ": "
					  << db_fig_image_alt[ind] << " -> " << image_alt;
				SLS_WARN(tmp);
				changed = true;
			}
			if (changed) {
				stmt_update.bind(1, entry);
				stmt_update.bind(2, int(order)); // -order to avoid UNIQUE constraint
				stmt_update.bind(3, image);
				stmt_update.bind(4, image_alt);
				stmt_update.bind(5, id);
				stmt_update.exec(); stmt_update.reset();
			}
		}

		// add entries.figures
		stmt_select0.bind(1, entry);
		if (!new_figs.empty()) {
			if (!stmt_select0.executeStep())
				throw internal_err("db_update_labels(): entry 不存在： " + entry);
			parse(figs, stmt_select0.getColumn(0));
			for (auto &e : new_figs)
				figs.insert(e);
			join(figs_str, figs);
			stmt_update2.bind(1, figs_str);
			stmt_update2.bind(2, entry);
			stmt_update2.exec(); stmt_update2.reset();
		}
		stmt_select0.reset();
	}

	// 检查被删除的图片（如果只被本词条引用， 就留给 autoref() 报错）
	// 这是因为入本词条的 autoref 还没有扫描不确定没有也被删除
	Str ref_by_str;
	SQLite::Statement stmt_update3(db_rw, R"(UPDATE "figures" SET "deleted"=1, "order"=0 WHERE "id"=?;)");
	for (Long i = 0; i < size(figs_used); ++i) {
		if (!figs_used[i] && db_figs_used[i]) {
			if (db_fig_ref_bys[i].empty() ||
				(db_fig_ref_bys[i].size() == 1 && db_fig_ref_bys[i][0] == db_fig_entries[i])) {
				tmp = u8"检测到 \\label{fig_";
				tmp << db_figs[i] << u8"}  被删除（图 " << db_fig_orders[i] << u8"）， 将标记未使用";
				SLS_WARN(tmp);
				// set "figures.entry" = ''
				stmt_update3.bind(1, db_figs[i]);
				stmt_update3.exec(); stmt_update3.reset();
//				// delete from "entries.figures"
//				// (update: "entries.figures" now also includes deleted figs)
//				stmt_select0.bind(1, db_fig_entries[i]);
//				SLS_ASSERT(stmt_select0.executeStep());
//				parse(figs, stmt_select0.getColumn(0));
//				figs.erase(db_figs[i]);
//				stmt_select0.reset();
//				join(figs_str, figs);
//				stmt_update2.bind(1, figs_str);
//				stmt_update2.bind(2, db_fig_entries[i]);
//				stmt_update2.exec(); stmt_update2.reset();
			}
			else {
				join(ref_by_str, db_fig_ref_bys[i], ", ");
				throw scan_err(u8"检测到 \\label{fig_" + db_figs[i] + u8"}  被删除， 但是被这些词条引用（请撤销删除）： " + ref_by_str);
			}
		}
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
	Str type, tmp;

	// get all db labels defined in `entries`
	//  db_xxx[i] are from the same row of "labels" table
	vecStr db_labels, db_label_entries;
	vecBool db_labels_used;
	vecLong db_label_orders;
	vector<vecStr> db_label_ref_bys;

	// get all labels defined by `entries`
	set<Str> db_entry_labels;
	for (Long j = 0; j < size(entries); ++j) {
		auto &entry = entries[j];
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
		auto &entry = entries[j];
		new_labels.clear();
		for (Long i = 0; i < size(entry_labels[j]); ++i) {
			auto &label = entry_labels[j][i];
			order = entry_label_orders[j][i];
			type = label_type(label);
			if (type == "fig" || type == "code")
				throw internal_err("`labels` here should not contain fig or code type!");

			Long ind = search(label, db_labels);
			if (ind < 0) {
				tmp = u8"数据库中不存在 label（将模拟 editor 插入）："; tmp << label << ", " << type << ", "
						<< entry << ", " << to_string(order);
				SLS_WARN(tmp);
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
				tmp = "label "; tmp << label << u8" 的词条发生改变（暂时不允许，请使用新的标签）："
						<< db_label_entries[ind] << " -> " << entry;
				throw scan_err(tmp);
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
			auto &entry = db_label_entries[i];
			if (db_label_ref_bys[i].empty() ||
				(db_label_ref_bys[i].size() == 1 && db_label_ref_bys[i][0] == entry)) {
				SLS_WARN(u8"检测到 label 被删除（将从数据库删除）： " + db_label);
				// delete from "labels"
				stmt_delete.bind(1, db_label);
				stmt_delete.exec(); stmt_delete.reset();
				// delete from "entries.labels"
				stmt_select0.bind(1, entry);
				SLS_ASSERT(stmt_select0.executeStep());
				parse(labels, stmt_select0.getColumn(0));
				labels.erase(db_label);
				stmt_select0.reset();
				join(labels_str, labels);
				stmt_update2.bind(1, labels_str);
				stmt_update2.bind(2, entry);
				stmt_update2.exec(); stmt_update2.reset();
			}
			else {
				join(ref_by_str, db_label_ref_bys[i], ", ");
				tmp = u8"检测到 label 被删除： "; tmp << db_label << u8"\n但是被这些词条引用： " << ref_by_str;
				throw scan_err(tmp);
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

// update entries.bibs and bibliography.ref_by
inline void db_update_entry_bibs(const unordered_map<Str, unordered_map<Str, Bool>> &entry_bibs_change, SQLite::Database &db_rw)
{
	SQLite::Transaction transaction(db_rw);

	// convert arguments
	unordered_map<Str, unordered_map<Str, Bool>> bib_ref_bys_change; // bib -> (entry -> [1]add/[0]del)
	for (auto &e : entry_bibs_change) {
		auto &entry = e.first;
		for (auto &bib: e.second)
			bib_ref_bys_change[bib.first][entry] = bib.second;
	}

	// =========== change bibliography.ref_by =================
	cout << "add & delete bibliography.ref_by..." << endl;
	SQLite::Statement stmt_update_ref_by(db_rw,
		R"(UPDATE "bibliography" SET "ref_by"=? WHERE "id"=?;)");
	Str ref_by_str;
	set<Str> ref_by;

	for (auto &e : bib_ref_bys_change) {
		auto &bib = e.first;
		auto &by_entries = e.second;
		ref_by.clear();
		ref_by_str = get_text("bibliography", "id", bib, "ref_by", db_rw);
		parse(ref_by, ref_by_str);
		change_set(ref_by, by_entries);
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

	for (auto &e : entry_bibs_change) {
		auto &entry = e.first;
		auto &bibs_changed = e.second;
		stmt_select_entry_bibs.bind(1, entry);
		if (!stmt_select_entry_bibs.executeStep())
			throw internal_err("entry 找不到： " + entry);
		parse(bibs, stmt_select_entry_bibs.getColumn(0));
		stmt_select_entry_bibs.reset();
		for (auto &bib : bibs_changed) {
			if (bib.second)
				bibs.insert(bib.first);
			else
				bibs.erase(bib.first);
		}
		join(bibs_str, bibs);
		stmt_update_entry_bibs.bind(1, bibs_str);
		stmt_update_entry_bibs.bind(2, entry);
		stmt_update_entry_bibs.exec(); stmt_update_entry_bibs.reset();
	}
	cout << "done!" << endl;
	transaction.commit();
}

// update entries.uprefs, entries.ref_by
inline void db_update_uprefs(
		const unordered_map<Str, unordered_map<Str, Bool>> &entry_uprefs_change,
		SQLite::Database &db_rw)
{
	SQLite::Transaction transaction(db_rw);

	cout << "updating entries.uprefs ..." << endl;
	SQLite::Statement stmt_select(db_rw,
		R"(SELECT "uprefs" FROM "entries" WHERE "id"=?;)");
	SQLite::Statement stmt_update(db_rw,
		R"(UPDATE "entries" SET "uprefs"=? WHERE "id"=?;)");
	Str str;
	set<Str> uprefs, ref_by;
	for (auto &e : entry_uprefs_change) {
		stmt_select.bind(1, e.first);
		if (!stmt_select.executeStep()) throw internal_err(SLS_WHERE);
		parse(uprefs, stmt_select.getColumn(0));
		stmt_select.reset();
		change_set(uprefs, e.second);
		join(str, uprefs);
		stmt_update.bind(1, str);
		stmt_update.bind(2, e.first);
		stmt_update.exec(); stmt_update.reset();
	}

	cout << "updating entries.ref_by ..." << endl;
	unordered_map<Str, unordered_map<Str, Bool>> entry_ref_bys_change;
	for (auto &e : entry_uprefs_change)
		for (auto &ref : e.second)
			entry_ref_bys_change[ref.first][e.first] = ref.second;

	SQLite::Statement stmt_select2(db_rw,
		R"(SELECT "ref_by" FROM "entries" WHERE "id"=?;)");
	SQLite::Statement stmt_update2(db_rw,
		R"(UPDATE "entries" SET "ref_by"=? WHERE "id"=?;)");
	for (auto &e : entry_ref_bys_change) {
		stmt_select2.bind(1, e.first);
		if (!stmt_select2.executeStep()) throw internal_err(SLS_WHERE);
		parse(ref_by, stmt_select2.getColumn(0));
		stmt_select2.reset();
		change_set(ref_by, e.second);
		join(str, ref_by);
		stmt_update2.bind(1, str);
		stmt_update2.bind(2, e.first);
		stmt_update2.exec(); stmt_update2.reset();
	}

	transaction.commit();
}

// update entries.refs, labels.ref_by, figures.ref_by
inline void db_update_refs(const unordered_map<Str, unordered_set<Str>> &entry_add_refs,
	unordered_map<Str, unordered_set<Str>> &entry_del_refs,
	SQLite::Database &db_rw)
{
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
	Str type;
	for (auto &e : label_del_ref_bys) {
		auto &label = e.first;
		auto &by_entries = e.second;
		try {
			ref_by_str = get_text("labels", "id", label, "ref_by", db_rw);
		}
		catch (const std::exception &e) {
			if (find(e.what(), "row not found") >= 0)
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
			if (find(e.what(), "row not found") >= 0)
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
// TODO: fix other generated field, i.e. entries.ref_by
inline void arg_fix_db(SQLite::Database &db_rw)
{
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

// upgrade `user-notes/*/cmd_data/scan.db` with the schema of `note-template/cmd_data/scan.db`
// data of `note-template/cmd_data/scan.db` has no effect
inline void migrate_user_db() {
	Str file_old, file;
	vecStr folders;
	folder_list_full(folders, "../user-notes/");
	for (auto &folder: folders) {
		if (!file_exist(folder + "main.tex") || folder == "../user-notes/note-template/")
			continue;
		file_old = folder + "cmd_data/scan-old.db";
		file = folder + "cmd_data/scan.db";
		if (!file_exist(file))
			SLS_ERR("file not found:" + file);
		cout << "migrating " << file << endl;
		file_move(file_old, file, true);
		file_copy(file, "../user-notes/note-template/cmd_data/scan.db", true);
		migrate_db(file, file_old);
		file_remove(file_old);
	}
}

// `git diff --no-index --word-diff-regex=. file1.tex file2.tex`
// then search for each `{+..+}` and `{-..-}`
inline void file_add_del(Long_O add, Long_O del, Str str1, Str str2)
{
	Str tmp;
	std::regex rm_s(u8"\\s");
	add = del = 0;

	// remove all space, tab, return etc
	str1 = std::regex_replace(str1, rm_s, "");
	str2 = std::regex_replace(str2, rm_s, "");
	SLS_ASSERT(utf8::is_valid(str1));
	SLS_ASSERT(utf8::is_valid(str2));

	if (str1 == str2) {
		add = del = 0; return;
	}
	Str str;
	// check if git is installed (only once)
	static bool checked = false;
	if (!checked) {
		exec_str(str, "which git");
		trim(str, " \n");
		if (str.empty())
			throw std::runtime_error("git binary not found!");
	}

	// replace "{+", "+}", "[-", "-]" to avoid conflict
	static const Str file1 = "file_add_del_tmp_file1.txt",
		file2 = "file_add_del_tmp_file2.txt",
		file_diff = "file_add_del_git_diff_output.txt";
	replace(str1, "{+", u8"{➕"); replace(str1, "+}", u8"➕}");
	replace(str1, "[-", u8"[➖"); replace(str1, "-]", u8"➖]");
	write(str1, file1);

	replace(str2, "{+", u8"{➕"); replace(str2, "+}", u8"➕}");
	replace(str2, "[-", u8"[➖"); replace(str2, "-]", u8"➖]");
	write(str2, file2);

	tmp = "git diff --no-index --word-diff-regex=. ";
	tmp << '"' << file1 << "\" \"" << file2 << '"';
	// git diff will return 0 iff there is no difference
	if (exec_str(str, tmp) == 0)
		return;
#ifndef NDEBUG
	write(str, file_diff);
#endif
	if (str.empty()) return;
	// parse git output, find addition
	Long ind = -1;
	while (1) {
		ind = find(str, "{+", ind+1);
		if (ind < 0) break;
		ind += 2;
		Long ind1 = find(str, "+}", ind);
		if (ind1 < 0) {
			file_rm(file1); file_rm(file2);
#ifndef NDEBUG
			file_rm(file_diff);
#endif
			throw std::runtime_error("matching +} not found!");
		}
		add += u8count(str, ind, ind1);
		ind = ind1 + 1;
	}
	// parse git output, find deletion
	ind = -1;
	while (1) {
		ind = find(str, "[-", ind+1);
		if (ind < 0) break;
		ind += 2;
		Long ind1 = find(str, "-]", ind);
		if (ind1 < 0) {
			file_rm(file1); file_rm(file2);
#ifndef NDEBUG
			file_rm(file_diff);
#endif
			throw std::runtime_error("matching -] not found!");
		}
		del += u8count(str, ind, ind1);
		ind = ind1 + 1;
	}

	// check
	Long Nchar1 = u8count(str1), Nchar2 = u8count(str2);
	Long net_add = Nchar2 - Nchar1;
	if (net_add != add - del) {
		Str tmp = "something wrong";
		tmp << to_string(net_add) <<
			", add = " << to_string(add) << ", del = " << to_string(del) <<
			", add-del = " << to_string(add - del);
		throw std::runtime_error(tmp);
	}

	file_rm(file1); file_rm(file2);
#ifndef NDEBUG
	file_rm(file_diff);
#endif
}

// calculate "history.add" and "history.del", with output of
inline void history_add_del(SQLite::Database &db_read, SQLite::Database &db_rw, Bool_I redo_all = false) {
	cout << "calculating history.add/del..." << endl;
	SQLite::Statement stmt_select(db_read, R"(SELECT "id" FROM "entries";)");
	vecStr entries;
	while (stmt_select.executeStep())
		entries.push_back(stmt_select.getColumn(0));

	SQLite::Statement stmt_select2(db_read,
		R"(SELECT "hash", "time", "author", "add", "del" FROM "history"
			WHERE "entry"=? ORDER BY "time";)");
	Str fname_old, fname, str, str_old;
	unordered_map<Str, pair<Long, Long>> hist_add_del; // backup hash -> (add, del)
	for (auto &entry : entries) {
		fname_old.clear(); fname.clear();
		stmt_select2.bind(1, entry);
		bool fname_exist = true;
		while (stmt_select2.executeStep()) {
			if (fname_exist) fname_old = fname;
			fname = "../PhysWiki-backup/";
			fname << stmt_select2.getColumn(1).getString() << '_';
			fname << to_string((int)stmt_select2.getColumn(2)) << '_';
			fname << entry << ".tex";
			if (!file_exist(fname)) {
				fname_exist = false; continue;
			}
			fname_exist = true;
			const char *hash = stmt_select2.getColumn(0);
			read(str, fname); CRLF_to_LF(str);
			if (fname_old.empty()) // first backup
				hist_add_del[hash] = make_pair(u8count(str), 0);
			else {
				int db_add = stmt_select2.getColumn(3);
				int db_del = stmt_select2.getColumn(4);
				if (!redo_all && db_add != -1 && db_del != -1)
					continue;
				auto &e = hist_add_del[hash];
				Long &add = e.first, &del = e.second;
				cout << fname << endl;
				read(str_old, fname_old);
				// compare str and str_old
				file_add_del(add, del, str_old, str);
			}
		}
		stmt_select2.reset();
	}

	// update db "history.add/del"
	cout << "\n\nupdating db history.add/del..." << endl;
	SQLite::Transaction transaction(db_rw);
	SQLite::Statement stmt_update(db_rw,
		R"(UPDATE "history" SET "add"=?, "del"=? WHERE "hash"=?;)");
	for (auto &e : hist_add_del) {
		auto &hash = e.first;
		auto &add_del = e.second;
		stmt_update.bind(1, (int)add_del.first);
		stmt_update.bind(2, (int)add_del.second);
		stmt_update.bind(3, hash);
		stmt_update.exec(); stmt_update.reset();
	}
	cout << "committing transaction..." << endl;
	transaction.commit();
	cout << "done." << endl;
}

// simulate 5min backup rule, by renaming backup files
inline void history_normalize(SQLite::Database &db_read, SQLite::Database &db_rw)
{
	SQLite::Statement stmt_select(db_read,
		R"(SELECT "entry", "author", "time", "hash" FROM "history")");
	//            entry                author     time         hash  time2 (new time, or "" for nothing, "d" to delete)
	unordered_map<Str,   unordered_map<Str,    map<Str,   pair<Str,  Str>>>> entry_author_time_hash_time2;
	while (stmt_select.executeStep()) {
		entry_author_time_hash_time2[stmt_select.getColumn(0)]
			[stmt_select.getColumn(1)] [stmt_select.getColumn(2)].first
			= stmt_select.getColumn(3).getString();
	}

	time_t t1, t2, t;
	Str *time2_last;
	for (auto &e5 : entry_author_time_hash_time2) {
		t1 = t2 = 0;
		for (auto &e4 : e5.second) {
			t1 = t2 = 0;
			time2_last = nullptr;
			for (auto &time_hash_time2 : e4.second) {
				// debug
				// if (time_hash_time2.first == "202303231039")
				t = str2time_t(time_hash_time2.first);
				if (t <= t1) // t1 is a resume of a session
					*time2_last = "d";
				else if (!t1) // t is 1st backup in a session
					t1 = t;
				else if (!t2) { // t is 2nd backup
					here:
					if (t - t1 < 300)
						t2 = t;
					else if (t - t1 > 300) {
						if (t - t1 < 1800) {
							if ((t - t1) % 300) {
								t1 += ((t - t1) / 300 + 1) * 300;
								time_t2yyyymmddhhmm(time_hash_time2.second.second, t1);
							}
						}
						else // end of 30min session
							t1 = t;
					}
					else // t - t1 == 300
						t1 = t;
				}
				else { // t2 > 0, t is 3rd backup
					if (t - t1 < 300) {
						t2 = t;
						*time2_last = "d";
					}
					else if (t - t1 > 300) {
						t1 += 300;
						time_t2yyyymmddhhmm(*time2_last, t1);
						t2 = 0;
						goto here; // I know, but this is actually cleaner
					}
					else { // t - t1 == 300
						t1 = t; t2 = 0;
						*time2_last = "d";
					}
				}
				time2_last = &time_hash_time2.second.second;
			}
			if (t2 > 0)
				time_t2yyyymmddhhmm(*time2_last, t1+300);
		}
	}

	// remove or rename files, and update db
	SQLite::Transaction transaction(db_rw);
	SQLite::Statement stmt_select2(db_rw,
		R"(SELECT "author", "entry" FROM "history" WHERE "hash"=?;)");
	SQLite::Statement stmt_update(db_rw,
		R"(UPDATE "history" SET "time"=? WHERE "hash"=?;)");
	SQLite::Statement stmt_delete(db_rw,
		R"(DELETE FROM "history" WHERE "hash"=?;)");
	Str fname, fname_new;
	for (auto &e5 : entry_author_time_hash_time2) {
		for (auto &e4: e5.second) {
			for (auto &time_hash_time2: e4.second) {
				auto &time2 = time_hash_time2.second.second;
				if (time2.empty())
					continue;
				auto &time = time_hash_time2.first;
				auto &hash = time_hash_time2.second.first;

				// rename or rename backup file, update db
				stmt_select2.bind(1, hash);
				if (!stmt_select2.executeStep())
					throw internal_err(SLS_WHERE);
				Long authorID = stmt_select2.getColumn(0).getInt64();
				const Str &entry = stmt_select2.getColumn(1).getString();
				stmt_select2.reset();
				fname = "../PhysWiki-backup/" + time;
				fname << '_' << authorID << '_' << entry << ".tex";
				if (time2 == "d") {
					cout << "rm " << fname << endl;
					file_remove(fname);
					// db
					stmt_delete.bind(1, hash);
					stmt_delete.exec(); stmt_delete.reset();
				}
				else {
					fname_new = "../PhysWiki-backup/" + time2;
					fname_new << '_' << authorID << '_' << entry << ".tex";
					cout << "mv " << fname << ' ' << fname_new << endl;
					file_move(fname_new, fname);
					// db
					stmt_update.bind(1, time2);
					stmt_update.bind(2, hash);
					stmt_update.exec(); stmt_update.reset();
				}
			}
		}
	}
	transaction.commit();
}
