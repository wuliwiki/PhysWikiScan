-- 所有词条
CREATE TABLE "entries" (
	"entry"	TEXT NOT NULL UNIQUE, -- 词条标签
	"title"	TEXT NOT NULL UNIQUE, -- 标题
	"keys" TEXT, -- "关键词1|...|关键词N"
	
	"part"	INTEGER, -- 部分（从 1 开始）
	"chapter"	INTEGER, -- 章（从 1 开始，全局编号）
	"section"	INTEGER, -- 节，每词条一节（从 1 开始，全局编号）

	"license"	TEXT, -- [CC] CC BY-SA 3.0 [Pay] 可知识付费 [Free] 免费开放
	"discount"	INTEGER, -- [0-100] 小众内容时薪折扣%
	"type"  TEXT, -- [Wiki] 百科 [Art] 文章 [Note] 笔记

	"draft"	INTEGER NOT NULL, -- [0|1] 是否草稿（词条是否标记 \issueDraft）
	"issues"	TEXT, -- "XXX XXX XXX" 其中 XXX 是 \issueXXX 中的， 不包括 \issueDraft
	"issueOther"	TEXT, -- \issueOther{} 中的文字
	
	"pentry"	TEXT, -- "label1 label2 label3" 预备知识的
	"labels"	TEXT, -- "eq 1 2 fig 2 2 ex 3 2 code 1 2" 标签（第一个数字是显示编号， 第二个是标签编号）
	"deleted"	INTEGER DEFAULT 0, -- [0|1] 是否已删除
	"occupied"	INTEGER DEFAULT 0, -- [-1|authorID] 是否正在被占用
	PRIMARY KEY("entry"),
	FOREIGN KEY("part") REFERENCES "parts"("id"),
	FOREIGN KEY("chapter") REFERENCES "chapters"("id")
);

-- 所有部分
CREATE TABLE "parts" (
	"id"	INTEGER NOT NULL UNIQUE AUTOINCREMENT, -- 部分编号（从 1 开始）
	"name"	TEXT NOT NULL UNIQUE, -- 部分名称
	PRIMARY KEY("id")
);

-- 所有章
CREATE TABLE "chapters" (
	"id"	INTEGER NOT NULL UNIQUE AUTOINCREMENT, -- 章编号（从 1 开始）
	"name"	TEXT NOT NULL UNIQUE, -- 章名称
	"part"	INTEGER NOT NULL, -- 所在部分
	PRIMARY KEY("id"),
	FOREIGN KEY("part") REFERENCES "parts"("id")
);

-- 所有参考文献
CREATE TABLE "bibliography" (
	"bib"	TEXT NOT NULL UNIQUE AUTOINCREMENT, -- \cite{} 中的标签
	"details"	TEXT NOT NULL, -- 详细信息（待拆分成若干field）
	PRIMARY KEY("bib")
);

-- 编辑历史
CREATE TABLE "history" (
	"hash"	TEXT NOT NULL UNIQUE, -- 暂定 SHA1 的前 16 位
	"time"	TEXT NOT NULL, -- 备份时间
	"authorID"	INTEGER NOT NULL, -- 作者 ID
	"entry"	TEXT, -- 词条标签
	"add"	INTEGER, -- 新增字符数
	"del"	INTEGER, -- 减少字符数
	"discount"	INTEGER, -- 备份时时薪折扣%
	FOREIGN KEY("authorID") REFERENCES "authors"("id"),
	FOREIGN KEY("entry") REFERENCES "entries"("entry"),
	FOREIGN KEY("discount") REFERENCES "entries"("discount"),
	PRIMARY KEY("hash")
);

-- 所有作者
CREATE TABLE "authors" (
	"id"	INTEGER NOT NULL UNIQUE AUTOINCREMENT, -- ID
	"name"	TEXT NOT NULL, -- 昵称
	"official"	INTEGER DEFAULT 0, -- [0|1] 已申请
	"salary"	INTEGER, -- 时薪
	PRIMARY KEY("id" AUTOINCREMENT)
);

-- 工资历史
CREATE TABLE "salary" (
	"authorID"	TEXT NOT NULL UNIQUE, -- 作者 ID
	"begin"	TEXT NOT NULL, -- 生效时间
	"salary"	INTEGER, -- 时薪
	FOREIGN KEY("authorID") REFERENCES "authors"("id")
);
