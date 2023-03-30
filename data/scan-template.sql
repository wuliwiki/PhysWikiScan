-- 小时百科在线编辑器数据库
-- 为方便编程， 所有数据都必须 NOT NULL

-- 所有词条
CREATE TABLE "entries" (
	"entry"	TEXT UNIQUE NOT NULL, -- 词条标签
	"title"	TEXT NOT NULL DEFAULT '', -- 标题
	"keys" TEXT NOT NULL DEFAULT '', -- "关键词1|...|关键词N"
	
	"part"	INTEGER NOT NULL DEFAULT 0, -- 部分（从 1 开始， 0 代表不在目录中）
	"chapter"	INTEGER NOT NULL DEFAULT 0, -- 章（从 1 开始，全局编号，0 代表不在目录中）
	"section"	INTEGER NOT NULL DEFAULT 0, -- 节，每词条一节（从 1 开始，全局编号，0 代表不在目录中）

	-- [CC] CC BY-SA 3.0 [Xiao] 小时科技版权 [Use] 作者版权和署名权，百科永久使用和修改权
	-- [Copy] 经作者同意转载（不可编辑） [Deri] 经作者同意转载（可编辑）
	"license"	TEXT NOT NULL DEFAULT '',

	-- [Wiki] 百科（综述） [Map] 导航 [Tutor] 教程（类似教材） [Art] 文章（类似论文） [Note] 笔记总结
	"type"  TEXT NOT NULL DEFAULT '',

	"draft"	INTEGER NOT NULL DEFAULT 2, -- [0|1|2] 是否草稿（词条是否标记 \issueDraft， 2 代表未知）
	"issues"	TEXT NOT NULL DEFAULT '', -- "XXX XXX XXX" 其中 XXX 是 \issueXXX 中的， 不包括 \issueDraft
	"issueOther"	TEXT NOT NULL DEFAULT '', -- \issueOther{} 中的文字
	
	"pentry"	TEXT NOT NULL DEFAULT '', -- "label1 label2 label3" 预备知识的
	"labels"	TEXT NOT NULL DEFAULT '', -- "eq 1 2 fig 2 2 ex 3 2 code 1 2" 标签（第一个数字是显示编号， 第二个是标签编号）

	"deleted"	INTEGER NOT NULL DEFAULT 0, -- [0|1] 是否已删除
	"occupied"	INTEGER NOT NULL DEFAULT -1, -- [-1|authorID] 是否正在被占用（审核发布后解除）

	PRIMARY KEY("entry"),
	FOREIGN KEY("part") REFERENCES "parts"("id"),
	FOREIGN KEY("chapter") REFERENCES "chapters"("id")
);

-- 所有部分
CREATE TABLE "parts" (
	"id"	INTEGER UNIQUE NOT NULL, -- 部分编号（从 1 开始）
	"name"	TEXT NOT NULL UNIQUE, -- 部分名称
	PRIMARY KEY("id" AUTOINCREMENT)
);

-- 所有章
CREATE TABLE "chapters" (
	"id"	INTEGER UNIQUE NOT NULL, -- 章编号（从 1 开始）
	"name"	TEXT NOT NULL, -- 章名称
	"part"	INTEGER NOT NULL, -- 所在部分（不能为 0）
	PRIMARY KEY("id" AUTOINCREMENT),
	FOREIGN KEY("part") REFERENCES "parts"("id")
);

-- 所有参考文献
CREATE TABLE "bibliography" (
	"bib"	TEXT UNIQUE NOT NULL, -- \cite{} 中的标签
	"details"	TEXT NOT NULL, -- 详细信息（待拆分成若干field）
	PRIMARY KEY("bib")
);

-- 编辑历史（根据时间计算贡献，一个记录 5 分钟）
CREATE TABLE "history" (
	"hash"	TEXT UNIQUE NOT NULL, -- 暂定 SHA1 的前 16 位
	"time"	TEXT NOT NULL, -- 备份时间， 格式 YYYYMMDDHHMM（下同）
	"authorID"	INTEGER NOT NULL, -- 作者 ID
	"entry"	TEXT NOT NULL, -- 词条标签
	"add"	INTEGER NOT NULL DEFAULT -1, -- 新增字符数（-1: 未知）
	"del"	INTEGER NOT NULL DEFAULT -1, -- 减少字符数（-1: 未知）
	PRIMARY KEY("hash")
	FOREIGN KEY("authorID") REFERENCES "authors"("id"),
	FOREIGN KEY("entry") REFERENCES "entries"("entry")
);

-- 审稿历史
CREATE TABLE "review" (
	"time"	TEXT NOT NULL, -- 审稿提交时间
	"refID"	INTEGER NOT NULL, -- 审稿人 ID
	"entry"	TEXT NOT NULL, -- 词条
	"authorID"	INTEGER NOT NULL, -- 作者 ID
	"action"	TEXT NOT NULL DEFAULT '', -- [Pub] 发布 [Udo] 撤回 [Fix] 继续完善
	"comment"	TEXT NOT NULL DEFAULT '', -- 意见（也可以直接修改正文或在正文中评论）
	FOREIGN KEY("refID") REFERENCES "authors"("id"),
	FOREIGN KEY("entry") REFERENCES "entries"("entry"),
	FOREIGN KEY("authorID") REFERENCES "authors"("id")
);

-- 贡献调整（history 记录之外的贡献，例如转载、画图、代码等）
CREATE TABLE "contribution" (
	"id"	INTEGER UNIQUE NOT NULL, -- 编号
	"time"	TEXT NOT NULL DEFAULT '', -- 何时做出的贡献
	"time_adj"	TEXT NOT NULL DEFAULT '', -- 何时做出的调整
	"entry"	TEXT NOT NULL, -- 词条
	"authorID"	INTEGER NOT NULL, -- 作者 ID
	"reason"	TEXT NOT NULL DEFAULT '', -- 原因
	"count"	INTEGER NOT NULL, -- 多少个 5 分钟（可以为负）
	PRIMARY KEY("id" AUTOINCREMENT),
	FOREIGN KEY("entry") REFERENCES "entries"("entry"),
	FOREIGN KEY("authorID") REFERENCES "authors"("id")
);

-- 所有作者
CREATE TABLE "authors" (
	"id"	INTEGER UNIQUE NOT NULL, -- ID
	"name"	TEXT NOT NULL, -- 昵称
	"applied"	INTEGER NOT NULL DEFAULT 0, -- [0|1] 已申请
	"salary"	INTEGER NOT NULL DEFAULT 0, -- 时薪
	PRIMARY KEY("id" AUTOINCREMENT)
);

-- 工资历史
CREATE TABLE "salary" (
	"authorID"	INTEGER NOT NULL, -- 作者 ID
	"begin"	TEXT NOT NULL, -- 生效时间
	"salary"	INTEGER NOT NULL DEFAULT 0, -- 时薪（元）
	FOREIGN KEY("authorID") REFERENCES "authors"("id")
);

-- 时薪折扣（例如用于小众词条）
CREATE TABLE "discount" (
	"entry"	TEXT NOT NULL, -- 词条
	"begin"	TEXT NOT NULL, -- 生效时间
	"percent"	INTEGER NOT NULL, -- 时薪折扣 %
	FOREIGN KEY("entry") REFERENCES "entries"("entry")
);
