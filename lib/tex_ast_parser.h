#pragma once
// AST nodes
#include "../SLISC/str/unicode.h"

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
	Ent,  // EntNode:  entry
	Brac, // BracNode: {...}
	// Fig,  // FigNode:  figure
	// Exm,  // ExmNode:  example
	// Exe   // ExeNode:  exercise
};

enum class Env
{
	Ent,  // EntNode:  entry
	Sub,  // SubNode:  subsection
	Ssub, // SsubNode: subsubsection
	Par,  // ParNode:  paragraph
	EqIn, // EqInNode: inline equation $...$ and \(...\)
	Fig,  // FigNode:  figure
	Exm,  // ExmNode:  example
	Exe   // ExeNode:  exercise
};

// the base of all nodes
struct TexNode
{
	node_type type;
	u8iter begin, end; // range in source string
	TexNode *parent;
	TexNode *last, *next; // null if doesn't exist
	TexNode *child; // 1st child (null if doesn't exist)

	TexNode(u8iter begin) :
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
	EntNode(u8iter begin, u8iter end) : TexNode(begin, end) {};
	TexNode *caption() { return child; }
	TexNode *body() { return child.next; }
};

// generic TeX command.
// child: name [arg_opt] arg1 arg2...
struct CmdNode : public TexNode
{
	Str name;
	bool has_opt; // has 1 optional argument
	CmdNode(u8iter begin, u8iter end, Str_I name) : TexNode(begin, end), name(name) {};
	TexNode *name() { return child; } // name of command, not including '\'
	TexNode *arg_opt() { return has_opt ? child.next : nullptr; }
	TexNode *arg() { return has_opt ? child.next.next : child.next; }
};

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
	Str label;
	int order; // displayed number
	Eq1Node(u8iter begin, u8iter end) : TexNode(begin, end) {};
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
	FigNode(u8iter begin, u8iter end) : TexNode(begin, end) {};
	TexNode *caption() { return child; }
};

// subsection
struct SubNode : public TexNode
{
	Str label;
	SubNode(u8iter begin, u8iter end) : TexNode(begin, end) {};
	TexNode *caption() { return child; }
	TexNode *body() { return child.next; }
};

// subsubsection
struct SsubNode : public TexNode
{
	SsubNode(u8iter begin, u8iter end) : TexNode(begin, end) {};
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

// first time parser of source code
inline TexNode tex_ast_parser(Str_I str)
{
	Str tmp;
	u8iter it(str);
	stack<node_type> where;
	while((Long)it < size(str)) {
		if (*it == '\\') {
			++it;
			if (is_alpha(*it)) { // command
				Long ind = (Long)it-1;
				command_name(tmp, ind, str);
				if (tmp == "begin") { // \begin{}
					command_arg(tmp, ind, str);
					if (tmp == "equation") {
						Eq1Node(it);
					}
					else if (tmp == "figure") {
						
					}
					else {

					}
				}
				else if (tmp == "end") { // \end{}

				}
				else if (tmp == "subsection") {

				}
				else if (tmp == "subsubsection") {

				}
				else { // generic command

				}
			}
			else if (*it == '[') { // display equation
				
			}
			else if (*it == ']') { // end of display equation

			}
			else if (*it == '\\')
			// ignore otherwise leave it for later
		}
		else if (*it == '\n') {
			if (*(++it) == '\n') { // paragraph
				while (*(++it) == '\n') ;

			}
		}
		else if (*it == '$') {
			++it;
			if (*it == '$') { // equation environment

			}
		}
		else if (*it == '}') { // end of command?

		}
	}
	find()
}
