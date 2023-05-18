-- 小时百科在线编辑器数据库
-- 为方便编程， 所有数据都必须 NOT NULL

-- 所有词条
-- 目录中的 \entry{标题}{xxx} 中 xxx 为 "id"
CREATE TABLE "entries" (
	"id"	TEXT UNIQUE NOT NULL,
	"caption"	TEXT NOT NULL DEFAULT '', -- 标题（以 main.tex 中为准， 若不在目录中则以首行注释为准）
	"authors"	TEXT NOT NULL DEFAULT '', -- 【生成】"id1 id2 id3" 作者 ID
	
	"part"	TEXT NOT NULL DEFAULT '', -- 部分
	"chapter"	TEXT NOT NULL DEFAULT '', -- 章
	"order"	INTEGER NOT NULL DEFAULT 0, -- 目录中出现的顺序（从 1 开始，全局编号，0 代表不在目录中）

	-- [Xiao] 小时科技版权，作者署名权（见 licens.tex）
	-- [Use] 作者版权，百科永久使用和修改权（见 licens.tex）， 若转载需注明来源如 "Use https://xxxx.com/xxx"
	-- [CCBYSA3] CC BY-SA 3.0 开源协议（和维基百科相同）
	-- [Copy] 经作者同意或根据协议转载（不可修改）需注明来源如 "Copy https://xxxx.com/xxx"
	"license"	TEXT NOT NULL DEFAULT '',

	-- [Wiki] 百科（综述） [Tutor] 教程（类似教材） [Art] 文章（类似论文） [Note] 笔记总结 [Map] 导航
	"type"  TEXT NOT NULL DEFAULT '',

	"keys" TEXT NOT NULL DEFAULT '', -- "关键词1|...|关键词N"

	-- "entry1 entry2:2* | entry3~" 预备知识的词条标签（空格允许多个， | 两边的空格允许没有）
	-- 在每个 entry 后面用 ":数字" 表示只需要哪个子节点（从 1 开始）， 没有数字默认最后一个节点（即整篇）
	-- 然后用 * 表示发现循环时优先被程序忽略， 再用 ~ 表示是否被知识树忽略， 用 "|" 分隔多个 pentry 列表
	-- 每个 pentry 列表默认引用前一个 pentry 代表的节点（不能明确列出来）
	"pentry"	TEXT NOT NULL DEFAULT '',

	"draft"	INTEGER NOT NULL DEFAULT 2, -- [0|1|2] 是否草稿（词条是否标记 \issueDraft， 2 代表未知）
	"issues"	TEXT NOT NULL DEFAULT '', -- "XXX XXX XXX" 其中 XXX 是 \issueXXX 中的， 不包括 \issueDraft
	"issueOther"	TEXT NOT NULL DEFAULT '', -- \issueOther{} 中的文字

	"deleted"	INTEGER NOT NULL DEFAULT 0, -- [0|1] 是否已删除
	"occupied"	INTEGER NOT NULL DEFAULT -1, -- [-1|authors.id] 是否正在被占用（审核发布后解除）
	"last_pub"	TEXT NOT NULL DEFAULT '', -- 最后发布时间 YYYYMMDDHHMM

	"figures"	TEXT NOT NULL DEFAULT '', -- "figId1 figId2" 定义的 figures
	"labels"	TEXT NOT NULL DEFAULT '', -- "label1 label2" 定义的 labels （除图片代码）

	"refs"	TEXT NOT NULL DEFAULT '', -- "label1 label2" 用 \autoref 引用的 labels
	"bibs"	TEXT NOT NULL DEFAULT '', -- "bib1 bib2" 用 \cite 引用的文献
	"files"	TEXT NOT NULL DEFAULT '', -- "id1 id2" 引用的附件
	"uprefs"	TEXT NOT NULL DEFAULT '', -- "entry1 entry2" 引用的其他词条（所有的 upref）
	
	"ref_by"	TEXT NOT NULL DEFAULT '', -- 【生成】"entry1 entry2" 被哪些词条列为 "uprefs"， 包括 pentry 中的（为空才能删除本词条）

	PRIMARY KEY("id"),
	FOREIGN KEY("part") REFERENCES "parts"("id"),
	FOREIGN KEY("chapter") REFERENCES "chapters"("id")
);

-- 防止 FOREIGN KEY 报错
INSERT INTO "entries" ("id", "caption", "deleted") VALUES ('', '无', 1);

-- 部分
-- 目录中 \label{prt_xxx} 中 xxx 为 "id"
CREATE TABLE "parts" (
	"id"	TEXT UNIQUE NOT NULL, -- 命名规则和词条一样
	"order"	INTEGER UNIQUE NOT NULL, -- 目录中出现的顺序，从 1 开始（0 代表不在目录中）
	"caption"	TEXT NOT NULL UNIQUE, -- 标题
	"chap_first"	TEXT NOT NULL UNIQUE, -- 第一章
	"chap_last"	INTEGER NOT NULL UNIQUE, -- 最后一章
	"subject"	TEXT NOT NULL DEFAULT '', -- [phys|math|cs] 学科
	PRIMARY KEY("id"),
	FOREIGN KEY("chap_first") REFERENCES "chapters"("id"),
	FOREIGN KEY("chap_last") REFERENCES "chapters"("id")
);

-- 防止 FOREIGN KEY 报错
INSERT INTO "parts" VALUES('', 0, '无', '', '', '');

-- 章
-- 目录中 \label{cpt_xxx} 中 xxx 为 "id"
CREATE TABLE "chapters" (
	"id"	TEXT UNIQUE NOT NULL, -- 命名规则和词条一样
	"order"	INTEGER UNIQUE NOT NULL, -- 目录中出现的顺序，从 1 开始（0 代表不在目录中）
	"caption"	TEXT NOT NULL, -- 标题
	"part"	TEXT NOT NULL, -- 所在部分（不能为 0）
	"entry_first"	TEXT NOT NULL, -- 第一个词条
	"entry_last"	TEXT NOT NULL, -- 最后一个词条
	PRIMARY KEY("id"),
	FOREIGN KEY("part") REFERENCES "parts"("id"),
	FOREIGN KEY("entry_first") REFERENCES "entries"("id"),
	FOREIGN KEY("entry_last") REFERENCES "entries"("id")
);

-- 防止 FOREIGN KEY 报错
INSERT INTO "chapters" VALUES('', 0, '无', '', '', ''); -- 不在目录中

-- 图片环境（所有图片环境必须带标签）
-- \label{fig_xxx} 中 xxx 为 "id"
CREATE TABLE "figures" (
	"id"	TEXT UNIQUE NOT NULL,
	"caption"	TEXT NOT NULL DEFAULT '', -- 标题 \caption{xxx}
	"authors"	TEXT NOT NULL DEFAULT '', -- 作者，格式和 entries.authors 相同
	"entry"	TEXT NOT NULL DEFAULT '', -- 【生成】所在词条（以 entries.figures 为准， '' 代表未被使用）
	"chapter" TEXT NOT NULL DEFAULT '', -- 所属章（即使 entry 为空也需要把图片归类， 否则很难找到）

	"order"	INTEGER NOT NULL DEFAULT 0, -- 显示编号（从 1 开始， 0 代表未知）
	"image"	TEXT NOT NULL DEFAULT '', -- [hash.png|hash.pdf] 文件 SHA1 的前 16 位 + 拓展名（当前 pdf 文件暂时使用 svg 文件的 hash）
	"image_alt"	TEXT NOT NULL DEFAULT '', -- "hash1.svg hash2.gif ..." 其他的文件格式（pdf 必须有对应的 svg）
	"image_old"	TEXT NOT NULL DEFAULT '', -- "hash1.svg hash2.gif ..." 图片历史版本（对应 images 表）
	"files"	TEXT NOT NULL DEFAULT '', -- "id1 id2" 附件（创作该图片的项目文件、源码等）（对应 files 表， 其中有历史版本信息）
	"source"	TEXT NOT NULL DEFAULT '', -- 来源（如果非原创）
	"ref_by"	TEXT NOT NULL DEFAULT '', -- 【生成】"entry1 entry2" 引用的词条（以 entries.refs 为准）
	"aka"	TEXT NOT NULL DEFAULT '', -- "figures.id" 以下信息为空且由另一条记录管理： "image_alt", "image_old", "files", "source"
	PRIMARY KEY("id"),
	FOREIGN KEY("entry") REFERENCES "entries"("id"),
	FOREIGN KEY("image") REFERENCES "images"("hash"),
	FOREIGN KEY("aka") REFERENCES "figures"("id"),
	UNIQUE("entry", "order")
);

-- 防止 FOREIGN KEY 报错
INSERT INTO "figures" ("id", "caption") VALUES ('', 0, '无', '', '', '');

-- 图片文件（包括历史版本)
CREATE TABLE "images" (
	"hash"	TEXT UNIQUE NOT NULL, -- 文件 SHA1 的前 16 位（如果 svg 需要先把 CRLF 变为 LF）
	"ext"	TEXT NOT NULL, -- [pdf|svg|png|jpg|gif] 拓展名
	"figures"	TEXT NOT NULL DEFAULT '', -- "id1 id2" 被哪些图片环境使用（包括 image, image_alt）
	"figures_old"	TEXT NOT NULL DEFAULT '', -- "id1 id2" 被哪些图片环境作为 image_old
	"author"	INTEGER NOT NULL DEFAULT '', -- 当前版本修改者
	"license"	TEXT NOT NULL DEFAULT '', -- 当前版本协议， 格式和 entries.license 相同
	"time"	TEXT NOT NULL DEFAULT '', -- 上传时间
	PRIMARY KEY("hash"),
	FOREIGN KEY("author") REFERENCES "authors"("id")
);

-- 文件（百科附件）
CREATE TABLE "files" (
	"id"	TEXT UNIQUE NOT NULL, -- 命名规则和词条一样
	"name"	TEXT UNIQUE NOT NULL, -- 文件名（含拓展名）
	"description"	TEXT UNIQUE NOT NULL, -- 描述
	"hash"	TEXT UNIQUE NOT NULL, -- 当前版本文件的 MD5
	"hash_old"	TEXT NOT NULL DEFAULT '', -- 历史版本
	"ref_by"	TEXT NOT NULL DEFAULT '', -- "entry1 entry2" 引用的词条
	PRIMARY KEY("id"),
	FOREIGN KEY("hash") REFERENCES "file_lib"("hash")
);

-- 文件（被 "files" 和 "figures.files" 使用）
CREATE TABLE "file_lib" (
	"hash"	TEXT UNIQUE NOT NULL, -- MD5
	"ref_by"	TEXT NOT NULL DEFAULT '', -- "entry1 entry2" 引用的词条
	"used_by_figures"	TEXT NOT NULL DEFAULT '', -- 被哪些图片环境使用
	"author"	INTEGER NOT NULL, -- 当前版本修改者
	"license"	TEXT NOT NULL, -- [Xiao|CC|ask] 当前版本协议
	"time"	TEXT UNIQUE NOT NULL, -- 上传时间
	PRIMARY KEY("hash"),
	FOREIGN KEY("author") REFERENCES "authors"("id")
);

-- 带标签的代码环境
-- \label{code_xxx} 中 xxx 为 "id"
CREATE TABLE "code" (
	"id"	TEXT UNIQUE NOT NULL,
    "entry" TEXT UNIQUE NOT NULL, -- 所在词条
	"caption"	TEXT UNIQUE NOT NULL, -- 文件名
	"language"	TEXT NOT NULL, -- [none|matlab|...] 高亮语言
	"order"	INTEGER NOT NULL, -- 显示编号
	"license"	TEXT NOT NULL, -- [orig|CC|ask] 版权
	"source"	TEXT NOT NULL, -- 来源（如果非原创）
	"ref_by"	TEXT NOT NULL, -- "entry1 entry2" 引用的词条
	PRIMARY KEY("id"),
	FOREIGN KEY("entry") REFERENCES "entries"("id")
);

-- 词条中的其他标签（除公式图片代码）
-- \label{yyy_xxxx} 中 yyy_xxxx 为 id, yyy 为 type
CREATE TABLE "labels" (
	"id"	TEXT UNIQUE NOT NULL,
	"type"	TEXT NOT NULL, -- [sub|tab|def|lem|the|cor|ex|exe] 标签类型
	"entry"	TEXT NOT NULL, -- 【生成】所在词条（以 entries.labels 为准）
	"order"	INTEGER NOT NULL, -- 显示编号
	"ref_by"	TEXT NOT NULL DEFAULT '', -- 【生成】"entry1 entry2" 被哪些词条引用（以 entries.refs 为准）
	PRIMARY KEY("id"),
	FOREIGN KEY("entry") REFERENCES "entries"("id"),
	UNIQUE("type", "entry", "order")
);

-- 参考文献
CREATE TABLE "bibliography" (
	"id"	TEXT UNIQUE NOT NULL, -- \cite{xxx} 中的 xxx
	"order"	INTEGER UNIQUE NOT NULL, -- 显示编号
	"details"	TEXT NOT NULL, -- 详细信息（TODO: 待拆分）
	"ref_by"	TEXT NOT NULL DEFAULT '', -- 【生成】"entry1 entry2" 被哪些词条引用（以 entries.cite 为准）
	PRIMARY KEY("id")
);

-- 防止 FOREIGN KEY 报错
INSERT INTO "bibliography" ("id", "order", "details") VALUES ('', 0, '无');

-- 编辑历史（一个记录 5 分钟）
CREATE TABLE "history" (
	"hash"	TEXT UNIQUE NOT NULL, -- SHA1 的前 16 位
	"time"	TEXT NOT NULL, -- 备份时间， 格式 YYYYMMDDHHMM（下同）
	"author"	INTEGER NOT NULL, -- 作者
	"entry"	TEXT NOT NULL, -- 词条
	"add"	INTEGER NOT NULL DEFAULT -1, -- 新增字符数（-1: 未知）
	"del"	INTEGER NOT NULL DEFAULT -1, -- 减少字符数（-1: 未知）
	PRIMARY KEY("hash"),
	FOREIGN KEY("author") REFERENCES "authors"("id"),
	FOREIGN KEY("entry") REFERENCES "entries"("id")
);

-- 审稿记录
CREATE TABLE "review" (
	"time"	TEXT NOT NULL, -- 审稿提交时间 YYYYMMDDHHMM
	"refID"	INTEGER NOT NULL, -- 审稿人 ID
	"entry"	TEXT NOT NULL, -- 词条
	"hash" TEXT NOT NULL, -- history.hash
	"author"	INTEGER NOT NULL, -- 作者
	"action"	TEXT NOT NULL DEFAULT '', -- [Pub] 发布 [Udo] 撤回 [Fix] 继续完善
	"comment"	TEXT NOT NULL DEFAULT '', -- 意见（也可以直接修改正文或在正文中评论）
	FOREIGN KEY("hash") REFERENCES "history"("hash"),
	FOREIGN KEY("refID") REFERENCES "authors"("id"),
	FOREIGN KEY("entry") REFERENCES "entries"("id"),
	FOREIGN KEY("author") REFERENCES "authors"("id")
);

-- 贡献调整（history 记录之外的贡献，例如转载、画图、代码等）
CREATE TABLE "contribution" (
	"id"	INTEGER UNIQUE NOT NULL, -- 编号
	"time"	TEXT NOT NULL DEFAULT '', -- 何时做出的贡献
	"time_adj"	TEXT NOT NULL DEFAULT '', -- 何时做出的调整
	"entry"	TEXT NOT NULL, -- 词条
	"author"	INTEGER NOT NULL, -- 作者
	"reason"	TEXT NOT NULL DEFAULT '', -- [fig|code|thnk] 原因
	"work"	INTEGER NOT NULL, -- 工作量（分钟，可以为负）
	"approved"	INTEGER NOT NULL, -- [0|1] 是否生效（需要审核）
	PRIMARY KEY("id" AUTOINCREMENT),
	FOREIGN KEY("entry") REFERENCES "entries"("id"),
	FOREIGN KEY("author") REFERENCES "authors"("id")
);

-- 所有作者
CREATE TABLE "authors" (
	"id"	INTEGER UNIQUE NOT NULL, -- ID
	"name"	TEXT NOT NULL, -- 昵称
	"applied"	INTEGER NOT NULL DEFAULT 0, -- [0|1] 已申请
	"salary"	INTEGER NOT NULL DEFAULT 0, -- 时薪
	"banned"	INTEGER NOT NULL DEFAULT 0, -- [0|1] 禁用编辑器
	"hide"	INTEGER NOT NULL DEFAULT 0, -- [0|1] 不出现在文章作者列表
	"aka"	INTEGER NOT NULL DEFAULT -1, -- 是否是其他 id 的小号（所有贡献和记录都算入大号）
	"contrib"	INTEGER NOT NULL DEFAULT 0, -- 贡献的分钟数（折算）
	"recent_entries"	TEXT NOT NULL DEFAULT '', -- "entry1 entry2" 最近 10 个编辑的词条（按时间从新到老）
	"referee"	TEXT NOT NULL DEFAULT '', -- "part:part1 sub:math sub:phys" 哪些部分/学科的审稿人
	PRIMARY KEY("id" AUTOINCREMENT),
	FOREIGN KEY("aka") REFERENCES "authors"("id")
);

-- 防止 FOREIGN KEY 报错
INSERT INTO "authors" ("id", "name") VALUES (-1, '');

-- 工资历史
CREATE TABLE "salary" (
	"author"	INTEGER NOT NULL, -- 作者
	"begin"	TEXT NOT NULL, -- 生效时间
	"salary"	INTEGER NOT NULL DEFAULT 0, -- 时薪（元）
	FOREIGN KEY("author") REFERENCES "authors"("id")
);

-- 时薪折扣（例如用于小众词条）
CREATE TABLE "discount" (
	"entry"	TEXT NOT NULL, -- 词条
	"begin"	TEXT NOT NULL, -- 生效时间
	"percent"	INTEGER NOT NULL, -- 时薪折扣 %
	FOREIGN KEY("entry") REFERENCES "entries"("id")
);
