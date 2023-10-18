#pragma once
// AST nodes
#include "../SLISC/str/unicode.h"

struct TexNode
{
	int type; // one of type_val
	int line; // line in source code
	int col; // column in source code
	enum type_val {
		Cmd = 0,  // CmdNode: command
		Env = 1,  // EnvNode: environment
		Txt = 2,  // TxtNode: text
		Num = 3,  // NumNode: number
		Sub = 4,  // SubNode: subsection
		Ssub = 5, // SsubNode: subsubsection
		Par = 6,  // ParNode: paragraph
		EqIn = 7, // EqInNode: inline equation $ $ and \( \)
		Ent = 8,  // EntNode: entry
		Fig = 9,  // FigNode: figure
	};
};

typedef vector<TexNode*> vecNode;
typedef const vecNode &vecNode_I;
typedef vecNode &vecNode_IO;

// generic TeX command.
struct CmdNode : public TexNode
{
	Str name; // name of command, not including '\'
	TexNode *opt_arg; // optional argument
	vector<TexNode*> args; // arguments

	CmdNode(Str_I name, vecNode_I args, TexNode *opt_arg = nullptr)
		: name(name), args(args), opt_arg(opt_arg) {}
};

struct EnvNode : public TexNode
{
	Str name; // name of the environment, including '*'
	TexNode *opt_arg; // optional argument
	vector<TexNode*> args; // arguments
	vector<TexNode*> body; // environment body

	EnvNode(Str_I name, vecNode_I body, vecNode_I args = {}, TexNode *opt_arg = nullptr)
		: name(name), args(args), opt_arg(opt_arg) {}
}

struct SubNode : public TexNode
{
	TexNode *caption; // caption of the subsection
	TexNode *opt_arg; // optional argument
	vector<TexNode*> args; // arguments
	vector<TexNode*> body; // environment body

	EnvNode(Str_I name, vecNode_I body, vecNode_I args = {}, TexNode *opt_arg = nullptr)
		: name(name), args(args), opt_arg(opt_arg) {}
}

struct EntryTree
{
	// pool for all nodes
	vector<TexNode> TexNodes;
	vector<CmdNode> CmdNodes;
	vector<EnvNode> EnvNodes;
	// children
	vector<TexNode*> nodes;
}

// parser

inline TexNode tex_ast_parser(Str_I str)
{
	u8iter it;
	for (Long it = 0; ?; ++it) {
		if (*it == '\\') {
			
		}
		else if (*it == '$') {
			if (*(++it) == '$') {

			}
		}
	}
	find()
}
