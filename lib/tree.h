#pragma once
#include "sqlite_db.h"

// get `Pentry` for one entry from database
inline void db_get_pentry(Pentry_O pentry, Str_I entry, SQLite::Database &db_read)
{
	SQLite::Statement stmt_select_nodes(db_read,
		R"(SELECT "id", "order" FROM "nodes" WHERE "entry"=?;)");
	SQLite::Statement stmt_select_edges(db_read,
		R"(SELECT "from", "weak", "hide" FROM "edges" WHERE "to"=?;)");
	stmt_select_nodes.bind(1, entry);
	while (stmt_select_nodes.executeStep()) {
		const Str &node_id = stmt_select_nodes.getColumn(0);
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
// also check for cycle, and check for any of it's pentries are redundant
// nodes[i] corresponds to tree[i]
// `entry0:(i+1)` will be (first few) nodes[i]
inline void db_get_tree1(
		vector<DGnode> &tree, // [out] dep tree, each node will include last node of the same entry (if any)
		vector<Node> &nodes, // [out] nodes[i] is tree[i], with (entry, order), where order > 0
		unordered_map<Str, pair<Str, Pentry>> &entry_info, // [out] entry -> (caption, Pentry)
		Pentry_IO pentry0, // pentry lists for entry0. `pentry0[i][j].tilde` will be updated
		Str_I entry0, Str_I caption0, // use the last node as tree root
		SQLite::Database &db_read)
{
	tree.clear(); nodes.clear(); entry_info.clear();
	unordered_map<Str, Long> node_id2ind; // node_id -> index to `nodes`

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

	Long Nnode0 = size(pentry0);
	if (Nnode0 == 0) { // no \pentry{}, add default node
		nodes.emplace_back(entry0, entry0, 1);
		node_id2ind[entry0] = nodes.size()-1;
		tree.emplace_back();
		return;
	}
	deque<Long> q; // nodes to search

	// set the first Nnode0 of nodes to all nodes of `entry0`
	for (Long order = 1; order <= Nnode0; ++order) {
		auto &node_id = pentry0[order-1].first;
		nodes.emplace_back(node_id, entry0, order);
		node_id2ind[node_id] = nodes.size()-1;
		q.emplace_back(nodes.size()-1);
	}
	entry_info[entry0].first = caption0;
	entry_info[entry0].second = pentry0;

	// get a "node" record from db, add to `nodes`, `node_id2ind` and `q`
	auto db_get_node = [&](Str_I node_id) {
		stmt_select_node.bind(1, node_id);
		if (!stmt_select_node.executeStep())
			throw scan_err(u8"数据库找不到 node_id: " + node_id);
		const Str &entry_from = stmt_select_node.getColumn(0);
		Long order_from = stmt_select_node.getColumn(1).getInt();
		stmt_select_node.reset();
		nodes.emplace_back(node_id, entry_from, order_from);
		node_id2ind[node_id] = nodes.size()-1;
		if (find(q.begin(), q.end(), nodes.size()-1) == q.end())
			q.push_back(nodes.size()-1);
	};

	// broad first search (BFS) to get all nodes involved
	while (!q.empty()) {
		const Str &entry = nodes[q.front()].entry;
		Long order = nodes[q.front()].order;
		q.pop_front();
		const Pentry &pentry = entry_info[entry].second;
		if (pentry.empty()) continue;
		if (order > size(pentry))
			throw internal_err(Str(__func__) + SLS_WHERE);
		if (order > 1) { // implicitly depends on the previous node
			const Str &node_id_last = pentry[order-2].first;
			if (!node_id2ind.count(node_id_last)) // last node not in `nodes`
				db_get_node(node_id_last);
		}
		if (pentry[order-1].second.empty())
			throw internal_err("empty \\pentry{} should have been ignored. " SLS_WHERE);
		for (auto &en : pentry[order-1].second) {
			if (node_id2ind.count(en.node_id) == 0) {
                db_get_node(en.node_id);
            }
			const Node &node_from = nodes[node_id2ind[en.node_id]];
			if (!entry_info.count(node_from.entry)) {
				auto &info = entry_info[node_from.entry];
				db_get_entry_caption(info.first, node_from.entry);
				db_get_pentry(info.second, entry, db_read);
			}
		}
	}

	// ---- construct tree ----
	tree.resize(nodes.size());
	for (Long i = 0; i < size(nodes); ++i) {
		auto &entry = nodes[i].entry;
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
			if (node_id2ind.count(req.node_id))
				throw internal_err(u8"节点未找到（应该不会发生才对）： " + req.node_id);
			tree[i].push_back(node_id2ind[req.node_id]);
		}
	}

	// check cyclic
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
					if (req.node_id == node_red.id) {
						req.tilde = found = true;
						break;
					}
				}
				if (!found)
					throw internal_err(SLS_WHERE);
				auto &caption_red = get<0>(entry_info[node_red.entry]);
				ss << u8"\n已标记 " << root.entry << ':' << root.order << " (" << root_caption << u8") 中多余的预备知识： "
				   << node_red.entry << ':' << node_red.order << " (" << caption_red << ')' << endl;
				ss << u8"   已存在路径： " << endl;
				for (Long i = 0; i < size(alt_path); ++i) {
					auto &nod = nodes[alt_path[i]];
					auto &caption = get<0>(entry_info[nod.entry]);
					ss << nod.entry << ':' << nod.order << " (" << caption
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
		for (Long j = 0; j < size(pentry1_info.second); ++j)
			pentry1.second[j].tilde = pentry1_info.second[j].tilde;
	}
}

// get entire dependency tree (2 ver) from database
// edge direction is the inverse of learning direction
// tree: one node for each entry
inline void db_get_tree(
		vector<DGnode> &tree, // [out] dep tree, each node will include last node of the same entry (if any)
		vector<Node> &nodes, // [out] nodes[i] is tree[i], with (entry, order). will be sorted by `entry` then `order`
		// vector<DGnode> &tree2, // [out] same with `tree`, but each tree node is one entry
		unordered_map<Str, tuple<Str, Str, Str, Pentry>> &entry_info, // [out] entry -> (caption,part,chapter,Pentry)
		SQLite::Database &db_read)
{
	tree.clear();
	SQLite::Statement stmt_select(db_read,
		R"(SELECT "id", "caption", "part", "chapter" FROM "entries" WHERE "deleted" = 0;)");

	unordered_map<Str, Long> node_id2ind; // node_id -> index of `nodes`

	// get info
	while (stmt_select.executeStep()) {
		const Str &entry = stmt_select.getColumn(0);
		auto &info = entry_info[entry];
		get<0>(info) = stmt_select.getColumn(1).getString();  // caption
		get<1>(info) = stmt_select.getColumn(2).getString();  // part
		get<2>(info) = stmt_select.getColumn(3).getString();  // chapter
		db_get_pentry(get<3>(info), entry, db_read); // pentry
	}

	// construct all nodes
	for (auto &e : entry_info) {
		auto &entry = e.first;
		auto &info = e.second;
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
			Long from = node_id2ind[pentry1.first];
			if (order > 1) { // require last node
				auto &last_node_id = pentry[order-2].first;
				if (node_id2ind.count(last_node_id) == 0) throw internal_err(SLS_WHERE);
				Long to = node_id2ind[last_node_id];
				tree[from].push_back(to);
			}
			for (auto &req: pentry1.second) {
				if (!req.tilde) {
					if (node_id2ind.count(req.node_id) == 0) throw internal_err(SLS_WHERE);
					Long to = node_id2ind[req.node_id];
					tree[from].push_back(to);
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
    SQLite::Statement stmt_select(db_rw, R"(SELECT "id" FROM "nodes" WHERE "entry"=?;)");
    SQLite::Statement stmt_select2(db_rw, R"(SELECT "from" FROM "edges" WHERE "to"=?;)");
    SQLite::Statement stmt_select3(db_rw, R"(SELECT "to" FROM "edges" WHERE "from"=?;)");
    SQLite::Statement stmt_update(db_rw,
		R"(UPDATE "nodes" SET "entry"=?, "order"=? WHERE "id"=?;)");
	SQLite::Statement stmt_insert(db_rw,
		R"(INSERT INTO "nodes" ("id", "entry", "order") VALUES (?, ?, ?);)");
    SQLite::Statement stmt_delete(db_rw, R"(DELETE FROM "edges" WHERE "from"=?;)");
    SQLite::Statement stmt_delete2(db_rw, R"(DELETE FROM "nodes" WHERE "id"=?;)");
    unordered_set<Str> nodes_deleted; // old nodes of `entry`

    // get db nodes of `entry` to see which are deleted
    // id_node == entry is not included
	stmt_select.bind(1, entry);
	while (stmt_select.executeStep()) {
		const Str &node_id = stmt_select.getColumn(0);
        if (node_id != entry)
		    nodes_deleted.insert(node_id);
    }
	stmt_select.reset();

	for (Long i = 0; i < size(pentry); ++i) {
		auto &node_id = pentry[i].first;
		// update or insert into db "nodes" table
		assert(!node_id.empty());
		nodes_deleted.erase(node_id);
        stmt_update.bind(1, entry);
		stmt_update.bind(2, (int)i+1);
		stmt_update.bind(3, node_id);
		stmt_update.exec();
		if (db_rw.getTotalChanges() == 0) {
			stmt_insert.bind(1, node_id);
			stmt_insert.bind(2, entry);
			stmt_insert.bind(3, (int)i+1);
			stmt_insert.exec(); stmt_insert.reset();
		}
		stmt_update.reset();
	}
	// deleted nodes
	for (auto &node_id : nodes_deleted) {
        // check if node_id is required by other nodes
        stmt_select2.bind(1, node_id); // edges.to
        clear(sb);
        if (stmt_select2.executeStep()) {
            clear(sb) << u8"检测到被删除的节点： " << node_id << "， 但它被以下节点引用：";
            do {
                sb << ' ' << stmt_select2.getColumn(0).getString();
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
    TODO...
    SQLite::Statement stmt_update(db_rw,
		R"(UPDATE "edges" SET "weak"=?, "hide"=? WHERE "from"=? AND "to"=?;)");
	SQLite::Statement stmt_insert(db_rw,
		R"(INSERT INTO "edges" ("from", "to", "weak", "hide") VALUES (?, ?, ?, ?);)");
	SQLite::Statement stmt_delete(db_rw, R"(DELETE FROM "edges" WHERE "from"=? AND "to"=?;)");

	unordered_set<pair<Str, Str>, hash_pair> edges_deleted; // old edges from `entry`, (from -> to)
	SQLite::Statement stmt_select(db_rw, R"(SELECT "id" FROM "nodes" WHERE "entry"=?;)");
	SQLite::Statement stmt_select2(db_rw, R"(SELECT "to" FROM "edges" WHERE "from"=?;)");
	stmt_select.bind(1, entry);
	while (stmt_select.executeStep()) {
		const Str &node_id = stmt_select.getColumn(0);
		nodes_deleted.insert(node_id);
		stmt_select2.bind(1, node_id);
		edges_deleted.insert(make_pair(node_id, stmt_select2.getColumn(0).getString()));
	}
	stmt_select.reset();

	for (Long i = 0; i < size(pentry); ++i) {
		auto &label = pentry[i].first;
		auto &pentry1 = pentry[i].second;
		// update or insert into db "nodes" table
		assert(!label.empty());
		nodes_deleted.erase(label);
		stmt_update.bind(1, (int)i+1);
		stmt_update.bind(2, label);
		stmt_update.exec();
		if (db_rw.getTotalChanges() == 0) {
			stmt_insert.bind(1, label);
			stmt_insert.bind(2, entry);
			stmt_insert.bind(3, (int)i+1);
			stmt_insert.exec(); stmt_insert.reset();
		}
		stmt_update.reset();
		// delete from "edges" table
		stmt_delete.bind(1, label);
		stmt_delete.exec(); stmt_delete.reset();
		// insert into db "edges" table
		for (const PentryRef &ref : pentry1) {
			edges_deleted.erase(make_pair(label, ref.node_id));
			stmt_insert2.bind(1, ref.node_id); // edges.to
			stmt_insert2.bind(2, label); // edges.from
			stmt_insert2.bind(3, ref.star); // edges.weak
			stmt_insert2.bind(4, ref.tilde); // edges.hide
			stmt_insert2.reset();
		}
	}

	// deleted edges and nodes
	for (auto &edge : edges_deleted) {
		stmt_delete.bind(1, edge.first);
		stmt_delete.bind(2, edge.second);
		stmt_delete.exec(); stmt_delete.reset();
	}
	// deleted nodes
	for (auto &node_id : nodes_deleted) {
		stmt_delete2.bind(1, node_id);
		stmt_delete2.exec(); stmt_delete.reset();
	}
}
