-- 小时百科在线编辑器数据库
-- 为方便编程， 所有数据都必须 NOT NULL

-- 所有词条
-- 目录中的 \entry{标题}{xxx} 中 xxx 为 "id"
CREATE TABLE "entries" (
	"id"	TEXT UNIQUE NOT NULL,
	"caption"	TEXT NOT NULL DEFAULT '', -- 标题（以 main.tex 中为准， 若不在目录中则以首行注释为准）
	"authors"	TEXT NOT NULL DEFAULT '', -- 【生成】"id1 id2 id3" 作者 ID
	
	"part"	TEXT NOT NULL DEFAULT '', -- 部分， 空代表不在目录中
	"chapter"	TEXT NOT NULL DEFAULT '', -- 章， 空代表不在目录中
	"last"	TEXT NOT NULL DEFAULT '', -- 目录中的上一个词条， 空代表这是第一个或不在目录中
	"next"	TEXT NOT NULL DEFAULT '', -- 目录中的下一个词条， 空代表这是最后一个或不在目录中

	"license"	TEXT NOT NULL DEFAULT 'Usr', -- 协议
	"type"  TEXT NOT NULL DEFAULT '',
	"keys" TEXT NOT NULL DEFAULT '', -- "关键词1|...|关键词N"

	-- "entry1 entry2:2* | entry3~" 预备知识列表， 列出每个 \pentry 的词条id， 用 "|" 隔开多个 \pentry（空格允许多个， "|" 两边的空格允许没有）
	-- 每个词条若有 n 个 \pentry， 则在树状图中表示为 n 个节点（编号从 1 开始），每个节点的内容是对应的 \pentry 到下一个 \pentry 之间的内容
	-- 每个节点默认依赖前一个节点（不需要在 \pentry 中列出来也不需要写进数据库）
	-- 在每个 entry 后面用 ":编号" 表示只需要哪个子节点（\upref[编号]{}）， 不指定编号则默认最后一个节点（即整篇）
	-- 然后用 * 标记发现循环时优先被程序忽略的节点（\upreff 命令）， 再用 ~ 标记是否被知识树忽略（多余或循环的预备知识）
	"pentry"	TEXT NOT NULL DEFAULT '',

	"draft"	INTEGER NOT NULL DEFAULT 2, -- [0|1|2] 是否草稿（词条是否标记 \issueDraft， 2 代表未知）
	"issues"	TEXT NOT NULL DEFAULT '', -- "XXX XXX XXX" 其中 XXX 是 \issueXXX 中的， 不包括 \issueDraft
	"issueOther"	TEXT NOT NULL DEFAULT '', -- \issueOther{} 中的文字

	"deleted"	INTEGER NOT NULL DEFAULT 0, -- [0|1] 是否已删除
	"last_pub"	TEXT NOT NULL DEFAULT '', -- 最后发布，空代表没有 (review.hash)
	"last_backup"	TEXT NOT NULL DEFAULT '', -- 最后备份，空代表没有 (history.hash)

	"figures"	TEXT NOT NULL DEFAULT '', -- 【生成】"figId1 figId2" 图片环境（包括删除的），以 figures.entry 为准
	"labels"	TEXT NOT NULL DEFAULT '', -- "label1 label2" 定义的 labels （除图片和代码）

	"refs"	TEXT NOT NULL DEFAULT '', -- "label1 label2" 用 \autoref 引用的 labels
	"bibs"	TEXT NOT NULL DEFAULT '', -- "bib1 bib2" 用 \cite 引用的文献
	"files"	TEXT NOT NULL DEFAULT '', -- "id1 id2" 引用的附件
	"uprefs"	TEXT NOT NULL DEFAULT '', -- "entry1 entry2" 引用的其他词条（所有的 upref）
	
	"ref_by"	TEXT NOT NULL DEFAULT '', -- 【生成】"entry1 entry2" 被哪些词条列为 "uprefs"， 包括 pentry 中的（为空才能删除本词条）

	PRIMARY KEY("id"),
	FOREIGN KEY("last") REFERENCES "entries"("id"),
	FOREIGN KEY("next") REFERENCES "entries"("id"),
	FOREIGN KEY("last_pub") REFERENCES "review"("hash"),
	FOREIGN KEY("last_backup") REFERENCES "history"("hash"),
	FOREIGN KEY("part") REFERENCES "parts"("id"),
	FOREIGN KEY("chapter") REFERENCES "chapters"("id"),
	FOREIGN KEY("license") REFERENCES "licenses"("id"),
	FOREIGN KEY("type") REFERENCES "types"("id")
);

INSERT INTO "entries" ("id", "caption", "deleted") VALUES ('', '无', 1); -- 防止 FOREIGN KEY 报错

-- 创作协议
CREATE TABLE "licenses" (
	"id"	TEXT UNIQUE NOT NULL, -- 协议 id，只允许字母和数字，字母开头，空代表未知
	"caption"	TEXT UNIQUE NOT NULL, -- 协议官方名称
	"url"	TEXT NOT NULL DEFAULT '', -- 协议官方 url
	"intro"	TEXT NOT NULL DEFAULT '', -- 协议简介和说明
	"text"	TEXT NOT NULL DEFAULT '', -- 协议全文
	PRIMARY KEY("id")
);

INSERT INTO "licenses" ("id", "caption") VALUES ('', '未知'); -- 防止 FOREIGN KEY 报错

-- 文章类型
CREATE TABLE "types" (
	"id"	TEXT UNIQUE NOT NULL,
	"caption"	TEXT UNIQUE NOT NULL, -- 中文名
	"intro"	TEXT NOT NULL DEFAULT '', -- 协议简介和说明
	PRIMARY KEY("id")
);

INSERT INTO "types" ("id", "caption", "intro") VALUES ('', '未知', ''); -- 防止 FOREIGN KEY 报错

-- 词条占用列表
CREATE TABLE "occupied" (
	"entry"	TEXT UNIQUE NOT NULL, -- entries.id
	"author"	INTEGER NOT NULL, -- authors.id
	"time" TEXT NOT NULL, -- 开始占用的时间
	PRIMARY KEY("entry"),
	FOREIGN KEY("entry") REFERENCES "entries"("id"),
	FOREIGN KEY("author") REFERENCES "authors"("id")
);

-- 部分
-- 目录中 \label{prt_xxx} 中 xxx 为 "id"
CREATE TABLE "parts" (
	"id"	TEXT UNIQUE NOT NULL, -- 命名规则和词条一样
	"order"	INTEGER UNIQUE NOT NULL, -- 目录中出现的顺序，从 1 开始（0 代表不在目录中）
	"caption"	TEXT UNIQUE NOT NULL, -- 标题
	"chap_first"	TEXT UNIQUE NOT NULL, -- 第一章
	"chap_last"	INTEGER UNIQUE NOT NULL, -- 最后一章
	"subject"	TEXT NOT NULL DEFAULT '', -- [phys|math|cs] 学科
	PRIMARY KEY("id"),
	FOREIGN KEY("chap_first") REFERENCES "chapters"("id"),
	FOREIGN KEY("chap_last") REFERENCES "chapters"("id")
);

INSERT INTO "parts" VALUES('', 0, '无', '', '', ''); -- 防止 FOREIGN KEY 报错

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

INSERT INTO "chapters" VALUES('', 0, '无', '', '', ''); -- 防止 FOREIGN KEY 报错

-- 图片环境（所有图片环境必须带标签）
-- \label{fig_xxx} 中 xxx 为 "id"
CREATE TABLE "figures" (
	"id"	TEXT UNIQUE NOT NULL,
	"caption"	TEXT NOT NULL DEFAULT '', -- 标题 \caption{xxx}
	"width"	TEXT NOT NULL DEFAULT '6', -- 图片环境宽度（单位 cm）
	"authors"	TEXT NOT NULL DEFAULT '', -- 【生成】作者，格式和 entries.authors 相同（以 images.author 为准）
	"entry"	TEXT NOT NULL DEFAULT '', -- 所在词条，若环境被删除就显示最后所在的词条，'' 代表从未被使用
	"chapter" TEXT NOT NULL DEFAULT '', -- 所属章（即使 entry 为空也需要把图片归类， 否则很难找到）
	"order"	INTEGER NOT NULL DEFAULT 0, -- 显示编号（从 1 开始， 0 代表未知）
	"image"	TEXT NOT NULL DEFAULT '', -- latex 代码中图片文件 SHA1 的前 16 位（文本图片如 svg 都先转换为 LF）
	"image_alt"	TEXT NOT NULL DEFAULT '', -- "hash1 hash2 ..." 其他格式的图片的 SHA1 前 16 位（pdf 必须有对应的 svg）
	"image_old"	TEXT NOT NULL DEFAULT '', -- "hash1 hash2 ..." 图片历史版本的 SHA1 前 16 位（所有格式）
	"source"	TEXT NOT NULL DEFAULT '', -- 来源（如果非原创）
	"ref_by"	TEXT NOT NULL DEFAULT '', -- 【生成】"entry1 entry2" 引用的词条（以 entries.refs 为准）
	"aka"	TEXT NOT NULL DEFAULT '', -- "figures.id" 若不为空，由另一条记录（被标记 deleted 也没关系）管理： "authors", "image_alt", "image_old", "files", "source"（本记录这些列为空）。 本记录 "image" 必须在 "image/image_alt/image_old" 之一中。
	"deleted"	INTEGER NOT NULL DEFAULT 0, -- [0] entry 源码中定义了该环境 [1] 定义后被删除
	PRIMARY KEY("id"),
	FOREIGN KEY("entry") REFERENCES "entries"("id"),
	FOREIGN KEY("image") REFERENCES "images"("hash"),
	FOREIGN KEY("aka") REFERENCES "figures"("id")
);

INSERT INTO "figures" ("id", "caption") VALUES ('', '无'); -- 防止 FOREIGN KEY 报错

-- 图片文件（包括历史版本)
CREATE TABLE "images" (
	"hash"	TEXT UNIQUE NOT NULL, -- 文件 SHA1 的前 16 位（如果 svg 需要先把 CRLF 变为 LF）
	"ext"	TEXT NOT NULL, -- [pdf|svg|png|jpg|gif] 拓展名
	"figure"	TEXT NOT NULL DEFAULT '', -- 【生成】本图片文件归哪个图片环境管理，该环境的 figures.aka 为空。 本图的 hash 可能出现在该环境的 figures.image/image_alt/image_old 中的一个。
	"figures_aka"	TEXT NOT NULL DEFAULT '', -- 【生成】"id1 id2" 被 figures 中哪些环境作为 image 或 image_alt， 且它们的 figures.aka 都是本图的 "figure"
	"author"	INTEGER NOT NULL DEFAULT '', -- 当前版本修改者
	"license"	TEXT NOT NULL DEFAULT '', -- 当前版本协议
	"time"	TEXT NOT NULL DEFAULT '', -- 上传时间
	"files"	TEXT NOT NULL DEFAULT '', -- "hash1 hash2" 附件（创作该图片的项目文件、源码等）（对应 file_lib 表）
	PRIMARY KEY("hash"),
	FOREIGN KEY("figure") REFERENCES "figures"("id"),
	FOREIGN KEY("author") REFERENCES "authors"("id"),
	FOREIGN KEY("license") REFERENCES "licenses"("id")
);

-- 文件（百科附件）
CREATE TABLE "files" (
	"id"	TEXT UNIQUE NOT NULL, -- 命名规则和词条一样
	"description"	TEXT UNIQUE NOT NULL, -- 描述
	"hash"	TEXT UNIQUE NOT NULL, -- 当前版本文件的 file_lib.hash
	"hash_old"	TEXT NOT NULL DEFAULT '', -- 历史版本（从新到老排序）
	"ref_by"	TEXT NOT NULL DEFAULT '', -- "entry1 entry2" 引用的词条
	PRIMARY KEY("id"),
	FOREIGN KEY("hash") REFERENCES "file_lib"("hash")
);

-- 文件（被 "files" 和 "figures.files" 使用）
CREATE TABLE "file_lib" (
	"hash"	TEXT UNIQUE NOT NULL, -- 文件 SHA1 的前 16 位
	"name"	TEXT UNIQUE NOT NULL, -- 文件名（含拓展名）
	"description"	TEXT UNIQUE NOT NULL, -- 备注（类似 commit 信息）
	"ref_by"	TEXT NOT NULL DEFAULT '', -- "entry1 entry2" 引用的词条
	"used_by_figures"	TEXT NOT NULL DEFAULT '', -- 被哪些图片环境使用
	"author"	INTEGER NOT NULL, -- 当前版本修改者
	"license"	TEXT NOT NULL, -- 当前版本协议
	"time"	TEXT UNIQUE NOT NULL, -- 上传时间
	PRIMARY KEY("hash"),
	FOREIGN KEY("author") REFERENCES "authors"("id"),
	FOREIGN KEY("license") REFERENCES "licenses"("id")
);

-- 带标签的代码环境
-- \label{code_xxx} 中 xxx 为 "id"
CREATE TABLE "code" (
	"id"	TEXT UNIQUE NOT NULL,
    "entry" TEXT UNIQUE NOT NULL, -- 所在词条
	"caption"	TEXT UNIQUE NOT NULL, -- 文件名
	"language"	TEXT NOT NULL, -- [none|matlab|...] 高亮语言
	"order"	INTEGER NOT NULL, -- 显示编号
	"license"	TEXT NOT NULL, -- 协议
	"source"	TEXT NOT NULL, -- 来源（如果非原创）
	"ref_by"	TEXT NOT NULL, -- "entry1 entry2" 引用的词条
	PRIMARY KEY("id"),
	FOREIGN KEY("entry") REFERENCES "entries"("id"),
	FOREIGN KEY("license") REFERENCES "licenses"("id")
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

INSERT INTO "bibliography" ("id", "order", "details") VALUES ('', 0, '无'); -- 防止 FOREIGN KEY 报错

-- 编辑历史（一个记录 5 分钟）
CREATE TABLE "history" (
	"hash"	TEXT UNIQUE NOT NULL, -- SHA1 的前 16 位
	"time"	TEXT NOT NULL, -- 备份时间， 格式 YYYYMMDDHHMM（下同）
	"author"	INTEGER NOT NULL, -- 作者
	"entry"	TEXT NOT NULL, -- 词条
	"add"	INTEGER NOT NULL DEFAULT -1, -- 新增字符数（-1: 未知）
	"del"	INTEGER NOT NULL DEFAULT -1, -- 减少字符数（-1: 未知）
	"last"	TEXT NOT NULL DEFAULT '', -- 本词条上次备份的 hash， '' 代表首个
	PRIMARY KEY("hash"),
	FOREIGN KEY("last") REFERENCES "history"("hash"),
	FOREIGN KEY("author") REFERENCES "authors"("id"),
	FOREIGN KEY("entry") REFERENCES "entries"("id"),
	UNIQUE("time", "author", "entry")
);

INSERT INTO "history" ("hash", "time", "author", "entry") VALUES ('', '000000000000', 0, ''); -- 防止 FOREIGN KEY 报错

-- 审稿记录
CREATE TABLE "review" (
	"hash" TEXT UNIQUE NOT NULL, -- history.hash
	"time"	TEXT NOT NULL, -- 审稿提交时间 YYYYMMDDHHMM
	"refID"	INTEGER NOT NULL, -- 审稿人 ID
	"entry"	TEXT NOT NULL, -- 词条
	"author"	INTEGER NOT NULL, -- 作者
	"action"	TEXT NOT NULL DEFAULT '', -- [Pub] 发布 [Udo] 撤回 [Fix] 继续完善
	"comment"	TEXT NOT NULL DEFAULT '', -- 意见（也可以直接修改正文或在正文中评论）
	"last" TEXT UNIQUE NOT NULL DEFAULT '', -- 该词条上次审稿的 hash， '' 代表首个
	PRIMARY KEY("hash"),
	FOREIGN KEY("hash") REFERENCES "history"("hash"),
	FOREIGN KEY("last") REFERENCES "review"("hash"),
	FOREIGN KEY("refID") REFERENCES "authors"("id"),
	FOREIGN KEY("entry") REFERENCES "entries"("id"),
	FOREIGN KEY("author") REFERENCES "authors"("id")
);

INSERT INTO "review" ("hash", "time", "refID", "entry", "author") VALUES ('', '000000000000', 0, '', 0); -- 防止 FOREIGN KEY 报错

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
	"recent_entries"	TEXT NOT NULL DEFAULT '', -- "entry1 entry2" 所有编辑的词条（按时间从新到老排序，不能重复）
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
