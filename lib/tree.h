#pragma once
#include "sqlite_db.h"

// get `Pentry` for one entry from database
// the order of `\req{}` within the same \pentry{} will be sorted (db doesn't store order)
inline void db_get_pentry(Pentry_O pentry, Str_I entry, SQLite::Database &db_read)
{
	SQLite::Statement stmt_select_nodes(db_read,
		R"(SELECT "id", "order" FROM "nodes" WHERE "entry"=?;)");
	SQLite::Statement stmt_select_edges(db_read,
		R"(SELECT "from", "weak", "hide" FROM "edges" WHERE "to"=? ORDER BY "from";)");
	stmt_select_nodes.bind(1, entry);
	while (stmt_select_nodes.executeStep()) {
		const Str &node_id = stmt_select_nodes.getColumn(0);
		if (node_id == entry) continue; // not a \pentry{}{node_id}
		vector<PentryRef> pentryN;
		stmt_select_edges.bind(1, node_id); // edges.to
		while (stmt_select_edges.executeStep()) {
			const Str &from = stmt_select_edges.getColumn(0); // edges.from
			bool weak = stmt_select_edges.getColumn(1).getInt(); // edges.weak
			bool hide = stmt_select_edges.getColumn(2).getInt(); // edges.hide
			pentryN.emplace_back(from, weak, hide);
		}
		stmt_select_edges.reset();
		pentry.emplace_back(node_id, std::move(pentryN));
	}
	stmt_select_nodes.reset();
}

// a node of the knowledge tree
struct Node {
	Str id;
	Str entry;
	Long order;
	Node(Str entry, Long_I order):
		entry(std::move(entry)), order(order) {};
	Node(Str id, Str entry, Long_I order):
		id(std::move(id)), entry(std::move(entry)), order(order) {};
};

typedef const Node &Node_I;
typedef Node &Node_O, &Node_IO;

inline bool operator==(Node_I a, Node_I b) { return a.id == b.id; }
inline bool operator<(Node_I a, Node_I b) { return a.entry == b.entry ? a.order < b.order : a.entry < b.entry; }

// get dependency tree from database, for all nodes of `entry0`
// edge direction is the inverse of learning direction
// also check for cycle, and check if any of `pentry0` refs is redundant
// nodes[i] corresponds to tree[i]
// `entry0:(i+1)` will be (first few) nodes[i]
// `nodes` will include `node_id == entry` nodes
// `tree` will resolve links to `node_id == entry` nodes if possible
inline void db_get_tree1(
		vector<DGnode> &tree, // [in/out] dep tree, each node will include last node of the same entry (if any)
		vector<Node> &nodes, // [in/out] nodes[i] is tree[i], with (entry, order), where order > 0
		unordered_map<Str, Long> &node_id2ind, // node_id -> `nodes` index
		unordered_map<Str, pair<Str, Pentry>> &entry_info, // [in/out] entry -> (caption, Pentry)
		Pentry_IO pentry0, // pentry lists for entry0. `pentry0[i][j].hide` will be updated
		Str_I entry0, // use the last node as tree root
		SQLite::Database &db_read)
{
	SQLite::Statement stmt_select_caption(db_read,
		R"(SELECT "caption" FROM "entries" WHERE "id"=?;)");
	SQLite::Statement stmt_select_node(db_read,
		R"(SELECT "entry", "order" FROM "nodes" WHERE "id"=?;)");
	SQLite::Statement stmt_select_nodes(db_read,
		R"(SELECT "id", "order" FROM "nodes" WHERE "entry"=?;)");
	SQLite::Statement stmt_select_nodes2(db_read,
		R"(SELECT "id" FROM "nodes" WHERE "entry"=? AND "order"=?;)");
	SQLite::Statement stmt_select_edges(db_read,
		R"(SELECT "from", "weak", "hide" FROM "edges" WHERE "to"=?;)");

	// get captiion for one entry from db
	auto db_get_entry_caption = [&](Str_O caption, Str_I entry) {
		stmt_select_caption.bind(1, entry);
		if (!stmt_select_caption.executeStep())
			throw internal_err("entry not found: " + entry + " " SLS_WHERE);
		caption = stmt_select_caption.getColumn(0).getString(); // entry.caption
		stmt_select_caption.reset();
	};

	// get a "node" record from db, return a `Node`
	auto db_get_node = [&](Str_I node_id) {
		stmt_select_node.bind(1, node_id);
		if (!stmt_select_node.executeStep())
			throw scan_err(u8"数据库找不到 node_id: " + node_id);
		const Str &entry_from = stmt_select_node.getColumn(0);
		Long order_from = stmt_select_node.getColumn(1).getInt();
		stmt_select_node.reset();
		return Node(node_id, entry_from, order_from);
	};

	// resolve a node with `node.id == node.entry` to the last actual node
	auto resolve_node_id = [&](Str_I node_id) {
		auto &nod = nodes[node_id2ind[node_id]];
		if (nod.id == nod.entry) {
			auto &pentry = entry_info[nod.entry].second;
			if (!pentry.empty())
				return Node(pentry.back().first, nod.entry, pentry.size());
		}
		return nod;
	};

	tree.clear();
	Long Nnode0 = size(pentry0);
	if (Nnode0 == 0) { // no \pentry{}, add default node
		nodes.emplace_back(entry0, entry0, 1);
		node_id2ind[entry0] = size(nodes)-1;
		tree.emplace_back();
		return;
	}
	deque<Long> q; // nodes to search

	// set the first Nnode0 of nodes to all nodes of `entry0`
	for (Long order = 1; order <= Nnode0; ++order) {
		auto &node_id = pentry0[order-1].first;
		nodes.emplace_back(node_id, entry0, order);
		node_id2ind[node_id] = size(nodes)-1;
		q.emplace_back(nodes.size()-1);
	}
	nodes.emplace_back(entry0, entry0, Nnode0+1);
	node_id2ind[entry0] = size(nodes)-1;
	db_get_entry_caption(entry_info[entry0].first, entry0);
	entry_info[entry0].second = pentry0;

	// broad first search (BFS) to get all nodes involved
	while (!q.empty()) {
		const Str &entry = nodes[q.front()].entry;
		Long order = nodes[q.front()].order;
		q.pop_front();
		const Pentry &pentry = entry_info[entry].second;
		if (pentry.empty()) continue;
		if (order > size(pentry)+1 || order <= 0)
			throw internal_err(Str(__func__) + SLS_WHERE);
		if (order > 1) { // implicitly depends on the previous node
			auto &pentry1_last = pentry[order-2];
			if (!node_id2ind.count(pentry1_last.first)) { // last node not in `nodes`
				nodes.emplace_back(pentry1_last.first, entry, order-1);
				node_id2ind[pentry1_last.first] = size(nodes)-1;
				q.push_back(size(nodes)-1);
			}
		}
		if (pentry[order-1].second.empty())
			throw internal_err("empty \\pentry{} should have been ignored. " SLS_WHERE);
		for (auto &req : pentry[order-1].second) {
			if (node_id2ind.count(req.node_id) == 0) {
				Node node_from = db_get_node(req.node_id);
				if (!entry_info.count(node_from.entry)) {
					auto &info = entry_info[node_from.entry];
					db_get_entry_caption(info.first, node_from.entry);
					db_get_pentry(info.second, node_from.entry, db_read);
				}
				nodes.push_back(node_from);
				node_id2ind[node_from.id] = size(nodes) - 1;
				if (node_from.id != node_from.entry)
					q.push_back(size(nodes) - 1);
				else { // is an entry node ...
					const auto &res_node = resolve_node_id(node_from.id);
					if (res_node.id != node_from.id && !node_id2ind.count(res_node.id)) {
						// ... sand resolves to the last normal node
						nodes.push_back(res_node);
						node_id2ind[res_node.id] = size(nodes) - 1;
						q.push_back(size(nodes) - 1);
					}
				}
			}
		}
	}

	// ---- construct tree ----
	tree.resize(nodes.size());
	for (Long i = 0; i < size(nodes); ++i) {
		auto &entry = nodes[i].entry;
		if (entry == nodes[i].id) continue;
		Long order = nodes[i].order;
		auto &pentry = entry_info[entry].second;
		if (pentry.empty()) continue;
		if (order > 1) { // implicitly depends on the last node
			const Str &node_id_last = pentry[order-2].first;
			if (!node_id2ind.count(node_id_last)) // last node not in `nodes`
				throw internal_err(u8"上一个节点未找到（应该不会发生才对）： " + node_id_last);
			tree[i].push_back(node_id2ind[node_id_last]);
		}
		for (auto &req : pentry[order-1].second) {
			const Str &resolved_id = resolve_node_id(req.node_id).id;
			if (!node_id2ind.count(resolved_id))
				throw internal_err(u8"节点未找到（应该不会发生才对）： " + resolved_id);
			tree[i].push_back(node_id2ind[resolved_id]);
		}
	}

	// --- check cyclic ---
	vecLong cycle;
	if (!dag_check(cycle, tree)) {
		cycle.push_back(cycle[0]);
		Str msg = u8"存在循环预备知识: ";
		for (auto ind : cycle) {
			auto &entry = nodes[ind].entry;
			auto &caption = entry_info[entry].first;
			msg << to_string(ind) << "." << caption << " (" << entry << ") -> ";
		}
		throw scan_err(msg);
	}

	// --- check redundancy ---
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
	auto &pentry0_info = entry_info[entry0].second;
	// reduce each node of entry0
	for (Long order = 1; order <= Nnode0; ++order) {
		auto &root = nodes[order-1];
		Str &root_caption = entry_info[root.entry].first;
		auto &pentry1 = pentry0_info[root.order - 1];
		dag_reduce(alt_paths, tree, order-1);
		std::stringstream ss;
		if (!alt_paths.empty()) {
			for (Long j = 0; j < size(alt_paths); ++j) {
				// mark ignored (add ~ to the end)
				auto &alt_path = alt_paths[j];
				Long i_red = alt_path.back(); // node[i_red] is redundant
				auto &node_red = nodes[i_red];
				bool found = false;
				for (auto &req: pentry1.second) {
					if (resolve_node_id(req.node_id).id == node_red.id) {
						req.hide = found = true;
						break;
					}
				}
				if (!found)
					throw internal_err(SLS_WHERE);
				// remove redundant link from `tree`
				auto &tree1 = tree[order-1];
				for (auto it = tree1.begin(); it != tree1.end(); ++it) {
					if (*it == i_red) {
						tree1.erase(it); break;
					}
				}
				assert(it != tree1.end());
				// print result
				auto &caption_red = get<0>(entry_info[node_red.entry]);
				ss << u8"\n已标记 " << root.entry << ':' << root.id << " (" << root_caption << u8") 中多余的预备知识： "
				   << node_red.entry << ':' << node_red.id << " (" << caption_red << ')' << endl;
				ss << u8"   已存在路径： " << endl;
				for (Long i = 0; i < size(alt_path); ++i) {
					auto &nod = nodes[alt_path[i]];
					auto &caption = get<0>(entry_info[nod.entry]);
					ss << nod.entry << ':' << nod.id << " (" << caption
					   << (i == size(alt_path) - 1 ? ")" : ") <-") << endl;
				}
			}
			scan_warn(ss.str());
		}
	}

	// copy `hide` to pentry0
	for (Long i = 0; i < size(pentry0_info); ++i) {
		auto &pentry1_info = pentry0_info[i];
		auto &pentry1 = pentry0[i];
		for (Long j = 0; j < size(pentry1_info.second); ++j) {
			auto &hide = pentry1_info.second[j].hide;
			if (hide < 0) hide = 0;
			pentry1.second[j].hide = hide;
		}
	}
}

// get entire dependency tree (2 ver) from database
// reference db_get_tree1()
// `nodes` order is stable: `entries` is sorted case insensitive, then for each entry, push to `nodes` in order
// node `node_id == entry_id` is the last node to each entry, and will only be linked if it's the only node of the entry
// order of edges are stable, each node will first link to the last node of the same entry, then link to other nodes sorted by their node_id
inline void db_get_tree(
		vector<DGnode> &tree, // [out] dep tree, each node will include last node of the same entry (if any)
		vector<Node> &nodes, // [out] nodes[i] is tree[i], with (entry, order). will be sorted by `entry` then `order`
		unordered_map<Str, Long> &node_id2ind, // [out] node_id -> `nodes` index
		// vector<DGnode> &tree2, // [out] same with `tree`, but each tree node is one entry
		unordered_map<Str, tuple<Str, Str, Str, Pentry>> &entry_info, // [in/out] entry -> (caption,part,chapter,Pentry)
		SQLite::Database &db_read)
{
	tree.clear(); nodes.clear(); node_id2ind.clear();
	SQLite::Statement stmt_select(db_read,
		R"(SELECT "id", "caption", "part", "chapter" FROM "entries" WHERE "deleted" = 0;)");
	vecStr entries_ordered;

	// resolve a node with `node.id == node.entry` to the last actual node
	auto resolve_node_id = [&](Str_I node_id) {
		auto &nod = nodes[node_id2ind[node_id]];
		if (nod.id == nod.entry) {
			auto &pentry = get<3>(entry_info[nod.entry]);
			if (!pentry.empty())
				return Node(pentry.back().first, nod.entry, pentry.size());
		}
		return nod;
	};

	// get info
	while (stmt_select.executeStep()) {
		const Str &entry = stmt_select.getColumn(0);
		entries_ordered.push_back(entry);
		auto &info = entry_info[entry];
		get<0>(info) = stmt_select.getColumn(1).getString();  // caption
		get<1>(info) = stmt_select.getColumn(2).getString();  // part
		get<2>(info) = stmt_select.getColumn(3).getString();  // chapter
		db_get_pentry(get<3>(info), entry, db_read); // pentry
	}

	sort_case_insens(entries_ordered);

	// construct all nodes
	for (auto &entry : entries_ordered) {
		auto &info = entry_info[entry];
		auto &pentry = get<3>(info);
		Long order = 1;
		for (; order <= size(pentry); ++order) {
			auto &pentry1 = pentry[order-1];
			nodes.emplace_back(pentry1.first, entry, order);
			node_id2ind[pentry1.first] = nodes.size()-1;
		}
		nodes.emplace_back(entry, entry, order);
		node_id2ind[entry] = nodes.size()-1;
	}

	// construct tree
	tree.resize(nodes.size());
	for (auto &e : entry_info) {
		auto &pentry = get<3>(e.second);
		if (pentry.empty()) continue;
		for (Long order = 1; order <= size(pentry); ++order) {
			auto &pentry1 = pentry[order-1];
			if (node_id2ind.count(pentry1.first) == 0) throw internal_err(SLS_WHERE);
			Long to = node_id2ind[pentry1.first];
			if (order > 1) { // require last node
				auto &last_node_id = pentry[order-2].first;
				if (node_id2ind.count(last_node_id) == 0) throw internal_err(SLS_WHERE);
				Long from = node_id2ind[last_node_id];
				tree[to].push_back(from);
			}
			for (auto &req: pentry1.second) {
				if (!req.hide) {
					if (node_id2ind.count(req.node_id) == 0) throw internal_err(SLS_WHERE);
					auto &req_nod = nodes[node_id2ind[req.node_id]];
					tree[to].push_back(node_id2ind[resolve_node_id(req_nod.id).id]);
				}
			}
		}
	}
}

// update "nondes" table in db, for 1 entry
// also deletes related db "edges" if a node is deleted
// only pentry[i].first is used
inline void db_update_nodes(Pentry_I pentry, Str_I entry, SQLite::Database &db_rw)
{
	SQLite::Statement stmt_select(db_rw, R"(SELECT "id", "order" FROM "nodes" WHERE "entry"=?;)");
	SQLite::Statement stmt_select2(db_rw, R"(SELECT "to" FROM "edges" WHERE "from"=?;)");
	SQLite::Statement stmt_insert(db_rw,
		R"(INSERT OR REPLACE INTO "nodes" ("id", "entry", "order") VALUES (?, ?, ?);)");
	SQLite::Statement stmt_update(db_rw, R"(UPDATE "nodes" SET "order"=? WHERE "id"=?;)");
	SQLite::Statement stmt_delete(db_rw, R"(DELETE FROM "edges" WHERE "from"=?;)");
	SQLite::Statement stmt_delete2(db_rw, R"(DELETE FROM "nodes" WHERE "id"=?;)");
	unordered_set<Str> nodes_deleted; // old nodes of `entry`
	unordered_map<Str, Long> db_node_order; // not includinig default node

	// get db nodes of `entry` to see which are deleted
	// id_node == entry is not included
	stmt_select.bind(1, entry);
	while (stmt_select.executeStep()) {
		const Str &node_id = stmt_select.getColumn(0);
		if (node_id != entry)
			nodes_deleted.insert(node_id);
		db_node_order[node_id] = (int)stmt_select.getColumn(1);
	}
	stmt_select.reset();

	// update or insert into db "nodes" table
	// for node_id == entry
	if (!db_node_order.count(entry)) {
		stmt_insert.bind(1, entry);
		stmt_insert.bind(2, entry);
		stmt_insert.bind(3, (int)pentry.size()+1);
		stmt_insert.exec(); stmt_insert.reset();
	}
	// \pentry{} nodes
	unordered_set<Str> check_repeat;
	for (Long i = 0; i < size(pentry); ++i) {
		auto &node_id = pentry[i].first;
		// check repeated node_id
		if (check_repeat.count(node_id))
			throw scan_err(u8"重复的 \\pentry{...}{标签}: " + node_id);
		else
			check_repeat.insert(node_id);
		// update or insert into db "nodes" table
		assert(!node_id.empty());
		int order = i+1;
		if (!db_node_order.count(node_id)) {
			stmt_insert.bind(1, node_id);
			stmt_insert.bind(2, entry);
			stmt_insert.bind(3, order);
			stmt_insert.exec(); stmt_insert.reset();
		}
		else {
			nodes_deleted.erase(node_id);
			if (db_node_order[node_id] != order) {
				stmt_update.bind(1, order);
				stmt_update.bind(2, node_id);
				stmt_update.exec();
				stmt_update.reset();
			}
		}
	}
	// deleted nodes
	for (auto &node_id : nodes_deleted) {
		// check if node_id is required by other nodes
		stmt_select2.bind(1, node_id); // edges.from
		clear(sb);
		if (stmt_select2.executeStep()) {
			clear(sb) << u8"检测到被删除的节点： " << node_id << "， 但它被以下节点引用：";
			do {
				sb << ' ' << stmt_select2.getColumn(0).getString(); // edges.to
			} while (stmt_select2.executeStep());
			throw scan_err(sb);
		}
		stmt_select2.reset();
		// delete the edges from node_id
		stmt_delete.bind(1, node_id);
		stmt_delete.exec(); stmt_delete.reset();
		// delete node_id
		stmt_delete2.bind(1, node_id);
		stmt_delete2.exec(); stmt_delete2.reset();
	}
}

// update db "edges" for 1 enetry
inline void db_update_edges(Pentry_I pentry, Str_I entry, SQLite::Database &db_rw)
{
	SQLite::Statement stmt_select(db_rw, R"(SELECT "from", "weak" FROM "edges" WHERE "to"=?;)");
	SQLite::Statement stmt_insert(db_rw,
		R"(INSERT INTO "edges" ("from", "to", "weak") VALUES (?, ?, ?);)");
	SQLite::Statement stmt_update(db_rw,
		R"(UPDATE "edges" SET "weak"=? WHERE "from"=? AND "to"=?;)");
	SQLite::Statement stmt_delete(db_rw, R"(DELETE FROM "edges" WHERE "from"=? AND "to"=?;)");

	unordered_set<pair<Str, Str>, hash_pair> edges_deleted; // old edges from `entry`, (from -> to)
	unordered_map<pair<Str, Str>, bool, hash_pair> db_edge_weak;

	// get db edges to `entry` nodes
	for (auto &pentry1 : pentry) {
		const Str &to = pentry1.first;
		stmt_select.bind(1, to);
		while (stmt_select.executeStep()) {
			const Str &from = stmt_select.getColumn(0);
			auto from_to = make_pair(from, to);
			edges_deleted.insert(from_to);
			db_edge_weak[from_to] = (int)stmt_select.getColumn(1);
		}
		stmt_select.reset();
	}

	for (Long i = 0; i < size(pentry); ++i) {
		auto &node_id = pentry[i].first;
		auto &pentry1 = pentry[i].second;
		if (!exist("nodes", "id", node_id, db_rw))
			throw scan_err(u8"edges.to 外键不存在： " + node_id);
		// update or insert into db "edges" table
		for (const PentryRef &ref : pentry1) {
			if (!exist("nodes", "id", ref.node_id, db_rw))
				throw scan_err(u8"edges.from 外键不存在： " + ref.node_id);
			auto from_to = make_pair(ref.node_id, node_id);
			if (!db_edge_weak.count(from_to)) {
				stmt_insert.bind(1, ref.node_id); // edges.from
				stmt_insert.bind(2, node_id); // edges.to
				stmt_insert.bind(3, ref.weak); // edges.weak
				stmt_insert.exec(); stmt_insert.reset();
			}
			else {
				edges_deleted.erase(from_to);
				if (db_edge_weak[from_to] != ref.weak) {
					stmt_update.bind(1, ref.weak); // edges.weak
					stmt_update.bind(2, from_to.first); // edges.from
					stmt_update.bind(3, from_to.second); // edges.to
					stmt_update.exec();
					stmt_update.reset();
				}
			}
		}
	}
	// deleted edges and nodes
	for (auto &edge : edges_deleted) {
		stmt_delete.bind(1, edge.first);
		stmt_delete.bind(2, edge.second);
		stmt_delete.exec(); stmt_delete.reset();
	}
}

// batch update edges
// this is used after round1, will not update "edges.hide", use db_update_edges_hide()
inline void db_update_edges(const vector<Pentry> &pentries, vecStr_I entries, SQLite::Database &db_rw)
{
	cout << "updating edges ..." << endl;
	for (Long i = 0; i < size(entries); ++i)
		db_update_edges(pentries[i], entries[i], db_rw);
}

// only updates db edges.hide
inline void db_update_edges_hide(Pentry_I pentry, Str_I entry, SQLite::Database &db_rw)
{
	SQLite::Statement stmt_update(db_rw, R"(UPDATE "edges" SET "hide"=? WHERE "from"=? AND "to"=?;)");
	for (Long i = 0; i < size(pentry); ++i) {
		auto &node_id = pentry[i].first;
		auto &pentry1 = pentry[i].second;
		for (const PentryRef &ref : pentry1) {
			stmt_update.bind(1, ref.hide); // edges.hide
			stmt_update.bind(2, ref.node_id); // edges.from
			stmt_update.bind(3, node_id); // edges.to
			stmt_update.exec(); stmt_update.reset();
		}
	}
}
