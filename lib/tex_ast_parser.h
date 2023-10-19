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
	Ent  // EntNode:  entry
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

typedef vector<TexNode*> vecNode;
typedef const vecNode &vecNode_I;
typedef vecNode &vecNode_IO;

// a whole entry
// child: sub1 sub2...
struct EntNode : public TexNode
{
	TexNode *caption() { return child; }
	TexNode *body() { return child.next; }
};

// generic TeX command.
// child: name [arg_opt] arg1 arg2...
struct CmdNode : public TexNode
{
	Str name;
	bool has_opt; // has 1 optional argument
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

// equation with a (optional) single label
// child: body1 body2...
struct Eq1Node : public TexNode { Str label; }

// figure
// child: caption
struct FigNode : public TexNode
{
	Str label;
	Str file;  // path
	Str width; // unit cm
	TexNode *caption() { return child; }
};

// subsection
struct SubNode : public TexNode
{
	Str label;
	TexNode *caption() { return child; }
	TexNode *body() { return child.next; }
};

// subsubsection
struct SsubNode : public TexNode
{
	TexNode *caption() { return child; }
	TexNode *body() { return child.next; }
};

inline void traverse_DFS(TexNode *node)
{
	if (is(node, TexNode::Cmd)) {

	}
	else if (is(node, TexNode::Env)) {

	}
	else if (is(node, TexNode::EqIn)) {
		
	}
}

// parser

inline TexNode tex_ast_parser(Str_I str)
{
	Str tmp;
	u8iter it(str);
	while((Long)it < size(str)) {
		if (*it == '\\') {
			if (is_alpha(*(it+1))) { // command
				command_name(tmp, it, str);
				if (tmp == "begin") { // \begin{}
					
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
			// ignore otherwise leave it for later
		}
		else if (*it == '\n') {
			if (*(it+1) == '\n') { // paragraph

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
