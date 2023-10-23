#pragma once
// AST nodes
#include "tex.h"
#include "../SLISC/algo/ObjPool.h"

enum class node_type
{
	Tex,  // TexNode:  unparsed
	Cmd,  // CmdNode:  command
	Env,  // EnvNode:  environment
	Txt,  // TxtNode:  text
	// Num,  // NumNode:  number
	Sub,  // SubNode:  subsection
	Ssub, // SsubNode: subsubsection
	Par,  // ParNode:  paragraph
	EqIn, // EqInNode: inline equation $...$ and \(...\)
	Eq1,  // Eq1Node: equation with (optional) label and number
	Ent,  // EntNode:  entry
	Brac, // BracNode: {...}
	Fig  // FigNode:  figure
	// Exm,  // ExmNode:  example
	// Exe   // ExeNode:  exercise
};

// the base of all nodes
struct TexNode
{
	node_type type;
	Long begin, end; // range in source string
	TexNode *parent;
	TexNode *last, *next; // null if doesn't exist
	TexNode *child; // 1st child (null if doesn't exist)

	TexNode(u8_iter begin) :
		type(node_type::Tex), begin(begin), end(-1), parent(0), last(0), next(0), child(0) {}

	Long nchild() { // # of child
		if (!child) return 0;
		else {
			Long n = 1;
			auto p = child;
			while (p.next) { p = p.next; ++n; }
		}
	}

	TexNode *next(Long n) {
		auto p = this;
		while (n--) { p = p.next; }
		return p;
	}

	TexNode *last(Long n) {
		auto p = this;
		while (n--) { p = p.last; }
		return p;
	}
};

struct BracNode : TexNode {};

typedef vector<TexNode*> vecNode;
typedef const vecNode &vecNode_I;
typedef vecNode &vecNode_IO;

// a whole entry
// child: sub1 sub2...
struct EntNode : public TexNode
{
	EntNode(Long begin) : TexNode(begin) {};
	TexNode *caption() { return child; }
	TexNode *body() { return child.next; }
};

// generic TeX command.
// child: name [arg_opt] arg1 arg2...
struct CmdNode : public TexNode
{
	Str name;
	bool has_opt, has_star; // has 1 optional argument
	CmdNode(Long begin, Str_I name) : TexNode(begin), name(name) {};
	TexNode *name() { return child; } // name of command, not including '\'
	TexNode *arg_opt() { return has_opt ? child.next : nullptr; }
	TexNode *arg() { return has_opt ? child.next.next : child.next; }
};

struct ParNode : public TexNode {}

// generic environment
// child: name [arg_opt] arg1 arg2... body1 body2...
// struct EnvNode : public TexNode
// {
// 	Str label;
// 	bool has_opt; // has 1 optional argument
// 	Long Nargs; // arguments (not including optional)
// 	TexNode *name() { return child; } // name of command, not including '\'
// 	TexNode *arg_opt() { return has_opt ? child.next : nullptr; }
// 	TexNode *arg() { return has_opt ? child.next.next : child.next; }
// 	TexNode *body() { return has_opt ? child.child.next(2+Nargs) : child.next(1+Nargs); }
// 	Long nbody() { return nchild()-1-has_opt-Nargs; }
// };

// equation with a (optional) single label and a number
// child: body1 body2...
struct Eq1Node : public TexNode {
	char dlm;  // one of '$' (for $$...$$), '[' (for \[...\]), 'e' (for equation env);
	Str label;
	int order; // displayed number
	Eq1Node(Long begin) : TexNode(begin) {};
}

// align environment, with multiple Eq1Node
struct AliNode : public TexNode {
	Eq1Node *eq() { return dynamic_cast<Eq1Node>(child); }
}

// figure
// child: caption
struct FigNode : public TexNode
{
	Str label;
	Str file;  // path
	Str width; // unit cm
	FigNode(Long begin) : TexNode(begin) {};
	TexNode *caption() { return child; }
};

// subsection
struct SubNode : public TexNode
{
	Str label;
	SubNode(Long begin) : TexNode(begin) {};
	TexNode *caption() { return child; }
	TexNode *body() { return child.next; }
};

// subsubsection
struct SsubNode : public TexNode
{
	SsubNode(Long begin) : TexNode(begin) {};
	TexNode *caption() { return child; }
	TexNode *body() { return child.next; }
};

// deep first search traverse the whole AST
inline void traverse_DFS(TexNode *node)
{
	assert(node);
	TexNode *p = node.child;
	while (!p) {
		; // do something
		traverse_DFS(p);
		p = p.next;
	}
}

ObjPool<TexNode> pool_Tex;
ObjPool<CmdNode> pool_Cmd;
ObjPool<Eq1Node> pool_Eq1;

// first time parser of source code
inline TexNode tex_ast_parser(Str_I str)
{
	Str tmp;
	u8_iter it(str);
	stack<node_type> st;
	while((Long)it < size(str)) {
		if (*it == '\\') {
			++it;
			if (is_alpha(*it)) { // command
				st.push(node_type::Cmd);
				Long ind = (Long)it-1;
				auot cmd = pool_Cmd.get();
				cmd->name = cmd_name(str, ind);
				cmd->has_star = command_star(str, ind);
				cmd->has_opt = command_has_opt(str, ind);
				cmd->arg_opt = // get the range of opt arg as node
				cmd->arg = // get the range of every args as node
			}
			else if (*it == '[') { // display equation
				auto eq = pool_Eq1.get();
				st.push(node_type::Eq1);
			}
			else if (*it == ']') { // end of display equation
				if (st.top != node_type::Eq1) SLS_ERR("\\] doesn't match \\[");
				st.pop();
			}
			else if (*it == '\\') { // line break
				;
			}
			else { // escaped character (*it not a letter)
				; // do nothing for now
			}
		}
		else if (*it == '\n') {
			if (*(++it) == '\n') { // paragraph
				while (*(++it) == '\n') ;
				if (st.top == node_type::Par)
					st.pop();
				p.child == new ParNode;
			}
		}
		else if (*it == '$') {
			++it;
			if (*it == '$') { // equation environment
				if (st.top == node_type::Eq1)
					st.pop();
				st.push(node_type::Eq1);
			}
		}
		else if (*it == '}') { // end of command?

		}
	}
	find()
}


inline void second_parse(TexNode_IO root)
{
	if (tmp == "begin") { // \begin{}
		command_arg(tmp, ind, str);
		if (tmp == "equation") {
			st.push(node_type::Eq1);
			Eq1Node(it);
		}
		else if (tmp == "figure") {
			st.push(node_type::Fig);
		}
		else {
			;
		}
	}
	else if (tmp == "end") { // \end{}
		if (tmp == "equation") {
			if (st.top != node_type::Eq1) SLS_ERR("\\end{} doesn't match \\begin{}");
			st.pop();
		}
		else if (tmp == "figure") {
			if (st.top != node_type::Fig) SLS_ERR("\\end{} doesn't match \\begin{}");
			st.pop();
		}
		else {
			;
		}
	}
	else if (tmp == "subsection") {
		st.push(node_type::Sub);
	}
	else if (tmp == "subsubsection") {
		st.push(node_type::Ssub);
	}
}

} // namespace ast
