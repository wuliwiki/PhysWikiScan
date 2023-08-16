#pragma once
#include "sqlite_db.h"


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
				throw internal_err(u8"数据库中 entries.pentry 格式不对 (非字母): " + str + SLS_WHERE);
			Long ind1 = ind0 + 1;
			while (ind1 < size(str) && is_alphanum(str[ind1]))
				++ind1;
			entry = str.substr(ind0, ind1 - ind0);
			for (auto &ee: pentry) // check repeat
				for (auto &e : ee)
					if (entry == e.entry)
						throw internal_err(u8"数据库中 entries.pentry1 存在重复的节点: " + str + SLS_WHERE);
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
				throw internal_err(u8"数据库中 entries.pentry1 格式不对 (多余的空格): " + str + SLS_WHERE);
			if (str[ind0] == '|') {
				ind0 = str.find_first_not_of(' ', ind0 + 1);
				if (ind0 < 0)
					throw internal_err(u8"数据库中 entries.pentry1 格式不对 (多余的空格): " + str + SLS_WHERE);
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
			throw scan_err("\\pentry{} empty!" SLS_WHERE);
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
			scan_warn(ss.str());
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
inline void db_get_tree(
		vector<DGnode> &tree, // [out] dep tree, each node will include last node of the same entry (if any)
		vector<Node> &nodes, // [out] nodes[i] is tree[i], with (entry, order). will be sorted by `entry` then `i_node`
		// vector<DGnode> &tree2, // [out] same with `tree`, but each tree node is one entry
		unordered_map<Str, tuple<Str, Str, Str, Pentry>> &entry_info, // [out] entry -> (title,part,chapter,Pentry)
		SQLite::Database &db_read)
{
	tree.clear();
	SQLite::Statement stmt_select(db_read,
		R"(SELECT "id", "caption", "part", "chapter", "pentry" FROM "entries" )"
		R"(WHERE "deleted" = 0 ORDER BY "id" COLLATE NOCASE ASC;)");

	// get info
	while (stmt_select.executeStep()) {
		const Str &entry = stmt_select.getColumn(0);
		auto &info = entry_info[entry];
		get<0>(info) = (const char*)stmt_select.getColumn(1); // titles
		get<1>(info) = (const char*)stmt_select.getColumn(2); // parts
		get<2>(info) = (const char*)stmt_select.getColumn(3); // chapter
		parse_pentry(get<3>(info), stmt_select.getColumn(4)); // pentry
	}

	// sort entries
	vecStr entries;
	for (auto &e : entry_info)
		entries.push_back(e.first);
	sort(entries);

	// construct nodes
	// all `i_node` in `entry_info` will be non-zero
	for (auto &entry : entries) {
		auto &info = entry_info[entry];
		auto &pentry = get<3>(info);
		if (pentry.empty()) {
			nodes.emplace_back(entry, 1);
			continue;
		}
		for (Long i_node = 1; i_node <= size(pentry); ++i_node) {
			auto &pentry1 = pentry[i_node-1];
			nodes.emplace_back(entry, i_node);
			for (auto &ee : pentry1) {
				// convert node `0` to the actual node
				if (ee.i_node == 0) {
					ee.i_node = size(get<3>(entry_info[ee.entry]));
					if (ee.i_node == 0) ee.i_node = 1;
				}
			}
		}
	}

	tree.resize(nodes.size());

	for (auto &e : entry_info) {
		auto &entry = e.first;
		auto &pentry = get<3>(e.second);
		if (pentry.empty()) continue;
		for (Long i_node = 1; i_node <= size(pentry); ++i_node) {
			auto &pentry1 = pentry[i_node-1];
			Long from = search(Node(entry, i_node), nodes);
			if (from < 0) throw internal_err(SLS_WHERE);
			if (i_node > 1) {
				Long to = search(Node(entry, i_node-1), nodes);
				if (to < 0) throw internal_err(SLS_WHERE);
				tree[from].push_back(to);
			}
			for (auto &ee: pentry1) {
				// convert node `0` to the actual node
				if (!ee.tilde) {
					Long to = search(Node(ee.entry, ee.i_node), nodes);
					if (to < 0)
						throw internal_err(SLS_WHERE);
					tree[from].push_back(to);
				}
			}
		}
	}
}
