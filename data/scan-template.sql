-- 小时百科在线编辑器数据库
-- 为方便编程， 所有字段都必须 NOT NULL

-- 所有文章
-- 目录中的 \entry{标题}{xxx} 中 xxx 为 "id"
CREATE TABLE "entries" (
	"id"        TEXT NOT NULL UNIQUE,
	"caption"   TEXT NOT NULL DEFAULT '',       -- 标题（以 main.tex 中为准， 若不在目录中则以首行注释为准）
	"authors"   TEXT NOT NULL DEFAULT '',       -- 【生成】"id1 id2 id3" 作者 ID（根据 history 和某种算法生成）
	"part"      TEXT NOT NULL DEFAULT '',       -- 部分， 空代表不在目录中
	"chapter"   TEXT NOT NULL DEFAULT '',       -- 章， 空代表不在目录中
	"last"      TEXT NOT NULL DEFAULT '',       -- 目录中的上一篇文章， 空代表这是第一个或不在目录中
	"next"      TEXT NOT NULL DEFAULT '',       -- 目录中的下一篇文章， 空代表这是最后一个或不在目录中
	"license"   TEXT NOT NULL DEFAULT 'Usr',    -- 协议
	"type"      TEXT NOT NULL DEFAULT '',       -- 类型
	"keys"      TEXT NOT NULL DEFAULT '',       -- "关键词1|...|关键词N"
	-- 【待迁移到 nodes/edges 表】"entry1 entry2:2* | entry3~" 预备知识列表， 列出每个 \pentry 的文章id， 用 "|" 隔开多个 \pentry（空格允许多个， "|" 两边的空格允许没有）
	-- 每篇文章若有 n 个 \pentry， 则在树状图中表示为 n 个节点（编号从 1 开始），每个节点的内容是对应的 \pentry 到下一个 \pentry 之间的内容
	-- 每个节点默认依赖前一个节点（不需要在 \pentry 中列出来也不需要写进数据库）
	-- 在每个 entry 后面用 ":编号" 表示只需要哪个子节点（\upref[编号]{}）， 不指定编号则默认最后一个节点（即整篇）
	-- 然后用 * 标记发现循环时优先被程序忽略的节点（\upreff 命令）， 再用 ~ 标记是否被知识树忽略（多余或循环的预备知识）
	"pentry"       TEXT    NOT NULL DEFAULT '',
	"draft"        INTEGER NOT NULL DEFAULT 2,  -- [0|1|2] 是否草稿（文章是否标记 \issueDraft， 2 代表未知）
	"deleted"      INTEGER NOT NULL DEFAULT 0,  -- [0|1] 是否已删除
	"last_pub"     TEXT    NOT NULL DEFAULT '', -- 最后发布，空代表没有 (review.hash)
	"last_backup"  TEXT    NOT NULL DEFAULT '', -- 最后备份，空代表没有 (history.hash)
	"refs"         TEXT    NOT NULL DEFAULT '', -- 【待迁移到 entry_refs 表】"label1 label2" 用 \autoref 引用的 labels， 不仅仅是 labels 表中的
	"bibs"         TEXT    NOT NULL DEFAULT '', -- 【待迁移到 entry_bibs 表】"bib1 bib2" 用 \cite 引用的文献
	PRIMARY KEY("id"),
	FOREIGN KEY("last")        REFERENCES "entries"("id"),
	FOREIGN KEY("next")        REFERENCES "entries"("id"),
	FOREIGN KEY("last_pub")    REFERENCES "review"("hash"),
	FOREIGN KEY("last_backup") REFERENCES "history"("hash"),
	FOREIGN KEY("part")        REFERENCES "parts"("id"),
	FOREIGN KEY("chapter")     REFERENCES "chapters"("id"),
	FOREIGN KEY("license")     REFERENCES "licenses"("id"),
	FOREIGN KEY("type")        REFERENCES "types"("id")
);

INSERT INTO "entries" ("id", "caption", "deleted") VALUES ('', '无', 1); -- 防止 FOREIGN KEY 报错

-- 文章中的 \upref{}
-- 包括 \pentry{} 中的
CREATE TABLE "entry_uprefs" (
	"entry"      TEXT NOT NULL,     -- entries.id
	"upref"      TEXT NOT NULL,     -- entries.id
	UNIQUE("entry", "upref"),
	FOREIGN KEY("entry")  REFERENCES "entries"("id"),
	FOREIGN KEY("upref")  REFERENCES "entries"("id")
);

CREATE INDEX idx_entry_uprefs_entry ON "entry_uprefs"("entry");
CREATE INDEX idx_entry_uprefs_upref ON "entry_uprefs"("upref");

-- 文章中的 \cite{}
-- TODO: 用于替代 entries.bibs 和 bibliography.ref_by
CREATE TABLE "entry_bibs" (
	"entry"    TEXT NOT NULL,     -- entries.id
	"bib"      TEXT NOT NULL,     -- bibliography.id
	UNIQUE("entry", "bib"),
	FOREIGN KEY("entry") REFERENCES "entries"("id"),
	FOREIGN KEY("bib") REFERENCES "bibliography"("id")
);

CREATE INDEX idx_entry_bibs_entry ON "entry_uprefs"("entry");
CREATE INDEX idx_entry_bibs_bib  ON "entry_bibs"("bib");

-- 文章中的所有 \autoref{} （不仅仅是 labels 表中的）
-- TODO: 用于替代 entries.refs 和 labels.ref_by 和 figures.ref_by
CREATE TABLE "entry_refs" (
	"entry"    TEXT NOT NULL,     -- entries.id
	"label"    TEXT NOT NULL,     -- labels.id
	UNIQUE("entry", "label"),
	FOREIGN KEY("entry")  REFERENCES "entries"("id")
);

CREATE INDEX idx_entry_refs_entry ON "entry_uprefs"("entry");
CREATE INDEX idx_entry_refs_label ON "entry_uprefs"("label");

-- 创作协议
CREATE TABLE "licenses" (
	"id"        TEXT NOT NULL UNIQUE,      -- 协议 id，只允许字母和数字，字母开头，空代表未知
	"caption"   TEXT NOT NULL UNIQUE ,     -- 协议官方名称
	"url"       TEXT NOT NULL DEFAULT '',  -- 协议官方 url
	"intro"     TEXT NOT NULL DEFAULT '',  -- 协议简介和说明
	"text"      TEXT NOT NULL DEFAULT '',  -- 协议全文
	PRIMARY KEY("id")
);

INSERT INTO "licenses" ("id", "caption") VALUES ('', '未知'); -- 防止 FOREIGN KEY 报错

-- 创作协议适用范围
CREATE TABLE "license_apply" (
	"license"  TEXT NOT NULL,      -- licenses.id
	"apply"    INTEGER NOT NULL,   -- 适用于：[e] 文章 [i] 图片 [c] 代码 [f] 文件
	"order"    INTEGER NOT NULL    -- 显示优先级（UI 中从小到大排列）
);

-- 文章类型
CREATE TABLE "types" (
	"id"        TEXT NOT NULL UNIQUE,
	"caption"   TEXT NOT NULL UNIQUE,      -- 中文名
	"intro"     TEXT NOT NULL DEFAULT '',  -- 协议简介和说明
	PRIMARY KEY("id")
);

INSERT INTO "types" ("id", "caption", "intro") VALUES ('', '未知', ''); -- 防止 FOREIGN KEY 报错

-- 知识树节点（\pentry{}）
-- 每篇文章自动添加一个文章节点， 使 nodes.id 和 entries.id 相同，表示最后一个节点（即整篇文章）
CREATE TABLE "nodes" (
	"id"        TEXT    NOT NULL UNIQUE,    -- \pentry{}{id}
	"entry"     TEXT    NOT NULL,           -- entries.id
	"order"     INTEGER NOT NULL,           -- 在文章中出现的顺序（从 1 开始）
	PRIMARY KEY("id"),
	FOREIGN KEY("entry")  REFERENCES "entries"("id")
);

CREATE INDEX idx_nodes_label ON "nodes"("label");
CREATE INDEX idx_nodes_entry ON "nodes"("entry");

-- 知识树的边（\pentry{} 中的 \upref{}）
CREATE TABLE "edges" (
	"to"       INTEGER NOT NULL,      -- nodes.id
	"from"     INTEGER NOT NULL,      -- nodes.id （若与 entries.id 相同则表示最后一个节点，即依赖整篇文章）
	"weak"     INTEGER NOT NULL,      -- [0|1] 循环依赖时优先被 hide（\upreff{}）（原 * 标记）
	"hide"     INTEGER NOT NULL,      -- 多余的预备知识， 不在知识树中显示（原 ~ 标记）
	PRIMARY KEY("to", "from"),
	FOREIGN KEY("to")  REFERENCES "nodes"("id"),
	FOREIGN KEY("from")  REFERENCES "nodes"("id")
);

CREATE INDEX idx_edges_to ON "edges"("to");
CREATE INDEX idx_edges_from ON "edges"("from");

-- 文章标记
-- （issues 环境属于该表）
CREATE TABLE "tags" (
	"entry"      TEXT    NOT NULL,     -- entries.id
	"type"       TEXT    NOT NULL,     -- 暂时不包括 \issueDraft
	"comment"    TEXT    NOT NULL,     -- issueOthers{} 或其他支持评论的 issue 类型
	PRIMARY KEY("entry"),
	FOREIGN KEY("entry")  REFERENCES "entries"("id"),
	FOREIGN KEY("type")   REFERENCES "tag_types"("id")
);

CREATE INDEX idx_tags_type ON "tags"("type");

CREATE TABLE "tag_types" (
	"id"      TEXT    NOT NULL UNIQUE, -- entries.id （暂时不包括 \issueDraft）
	"name"    TEXT    NOT NULL,        -- 中文名
	PRIMARY KEY("id")
);

-- 文章转载（到其他平台）
CREATE TABLE "repost" (
	"entry"    TEXT    NOT NULL,   -- entries.id
	"url"      TEXT    NOT NULL,   -- 网址
	"updated"  TEXT    NOT NULL,   -- 最后更新时间
	FOREIGN KEY("entry") REFERENCES "entries"("id")
);

CREATE INDEX idx_repost_entry ON "repost"("entry");
CREATE INDEX idx_repost_updated ON "repost"("updated");

-- 评分
CREATE TABLE "score" (
	"entry"   TEXT     NOT NULL,   -- entries.id
	"score"   REAL     NOT NULL,   -- 评分（0-10)
	"author"  INTEGER  NOT NULL,   -- 评分者
	"version" TEXT     NOT NULL,   -- 文章版本
	"time"    TEXT     NOT NULL,   -- 评分时间
	"comment" TEXT     NOT NULL,   -- 评分理由等
	FOREIGN KEY("entry") REFERENCES "entries"("id"),
	FOREIGN KEY("author") REFERENCES "authors"("id"),
	FOREIGN KEY("version") REFERENCES "history"("id")
);

CREATE INDEX idx_score_entry ON "score"("entry");
CREATE INDEX idx_score_version ON "score"("version");
CREATE INDEX idx_score_time ON "score"("time");

-- 文章占用列表
CREATE TABLE "occupied" (
	"entry"    TEXT    NOT NULL UNIQUE, -- entries.id
	"author"   INTEGER NOT NULL,        -- authors.id
	"time"     TEXT    NOT NULL,        -- 开始占用的时间
	PRIMARY KEY("entry"),
	FOREIGN KEY("entry")  REFERENCES "entries"("id"),
	FOREIGN KEY("author") REFERENCES "authors"("id")
);

-- 文章锁定列表 (任何人无法编辑文章内容和信息，除非先解锁)
CREATE TABLE "locked" (
	"entry"    TEXT    NOT NULL UNIQUE, -- entries.id
	"author"   INTEGER NOT NULL,        -- authors.id
	"time"     TEXT    NOT NULL,        -- 开始锁定时间
	PRIMARY KEY("entry"),
	FOREIGN KEY("entry")  REFERENCES "entries"("id"),
	FOREIGN KEY("author") REFERENCES "authors"("id")
);

-- 文章打开列表
-- 用于恢复用户之前打开的文章
CREATE TABLE "opened" (
	"author"   INTEGER NOT NULL UNIQUE, -- authors.id
	"entries"  TEXT    NOT NULL,        -- "entry1 entry2"
	"time"     TEXT    NOT NULL,        -- 打开时间
	PRIMARY KEY("author"),
	FOREIGN KEY("author")  REFERENCES "authors"("id")
);

-- 部分
-- 目录中 \label{prt_xxx} 中 xxx 为 "id"
CREATE TABLE "parts" (
	"id"          TEXT    NOT NULL UNIQUE,     -- 命名规则和文章一样
	"order"       INTEGER NOT NULL UNIQUE,     -- 目录中出现的顺序，从 1 开始（0 代表不在目录中）
	"caption"     TEXT    NOT NULL UNIQUE,     -- 标题
	"chap_first"  TEXT    NOT NULL UNIQUE,     -- 第一章
	"chap_last"   INTEGER NOT NULL UNIQUE,     -- 最后一章
	"subject"     TEXT    NOT NULL DEFAULT '', -- [phys|math|cs] 学科
	PRIMARY KEY("id"),
	FOREIGN KEY("chap_first") REFERENCES "chapters"("id"),
	FOREIGN KEY("chap_last")  REFERENCES "chapters"("id")
);

INSERT INTO "parts" VALUES('', 0, '无', '', '', ''); -- 防止 FOREIGN KEY 报错

-- 章
-- 目录中 \label{cpt_xxx} 中 xxx 为 "id"
CREATE TABLE "chapters" (
	"id"            TEXT    NOT NULL UNIQUE, -- 命名规则和文章一样
	"order"         INTEGER NOT NULL UNIQUE, -- 目录中出现的顺序，从 1 开始（0 代表不在目录中）
	"caption"       TEXT    NOT NULL,        -- 标题
	"part"          TEXT    NOT NULL,        -- 所在部分（不能为 0）
	"entry_first"   TEXT    NOT NULL,        -- 第一篇文章
	"entry_last"    TEXT    NOT NULL,        -- 最后一篇文章
	PRIMARY KEY("id"),
	FOREIGN KEY("part")        REFERENCES "parts"("id"),
	FOREIGN KEY("entry_first") REFERENCES "entries"("id"),
	FOREIGN KEY("entry_last")  REFERENCES "entries"("id")
);

INSERT INTO "chapters" VALUES('', 0, '无', '', '', ''); -- 防止 FOREIGN KEY 报错

-- 图片环境
-- 一个记录管理同一版本不同格式的图片文件
-- \label{fig_xxx} 中 xxx 为 "id"（百科所有图片环境必须带标签）
CREATE TABLE "figures" (
	"id"          TEXT    NOT NULL UNIQUE,
	"caption"     TEXT    NOT NULL DEFAULT '',  -- 标题 \caption{xxx}
	"width"       TEXT    NOT NULL DEFAULT '6', -- 图片环境宽度（单位 cm）
	"authors"     TEXT    NOT NULL DEFAULT '',  -- 【生成】"作者id1 作者id2" 相同（由所有历史版本的 images.author 根据某种算法生成）
	"entry"       TEXT    NOT NULL DEFAULT '',  -- 所在文章，若环境被删除就显示最后所在的文章，'' 代表从未被使用
	"chapter"     TEXT    NOT NULL DEFAULT '',  -- 所属章（即使 entry 为空也需要把图片归类， 否则很难找到）
	"order"       INTEGER NOT NULL DEFAULT 0,   -- 显示编号（从 1 开始， 0 代表未知）
	"image"       TEXT    NOT NULL DEFAULT '',  -- latex 图片环境的文件 SHA1 的前 16 位（文本图片如 svg 都先转换为 LF），可能是多个 images.figure=id 中的一个
	"last"        TEXT    NOT NULL DEFAULT '',  -- "figures.id" 上一个版本（若从百科其他图修改而来）。 可以生成一个版本树。
	"source"      TEXT    NOT NULL DEFAULT '',  -- 外部来源（如果非原创）
	"ref_by"      TEXT    NOT NULL DEFAULT '',  -- 【待迁移到 entry_refs 表】【生成】"entry1 entry2" 引用本图的文章（以 entries.refs 为准）
	"aka"         TEXT    NOT NULL DEFAULT '',  -- "figures.id" 若不为空，由另一条记录（aka 必须为空，允许被标记 deleted）管理： 所有图片文件（"images.figure"）, "authors", "last", "files", "source"（本记录这些列为空）。 本记录 "image" 必须在另一条记录的图片文件中。
	"deleted"     INTEGER NOT NULL DEFAULT 0,   -- [0] entry 源码中定义了该环境 [1] 定义后被删除
	"comment"     TEXT    NOT NULL DEFAULT '',  -- 备注信息
	PRIMARY KEY("id"),
	FOREIGN KEY("entry") REFERENCES "entries"("id"),
	FOREIGN KEY("aka")   REFERENCES "figures"("id"),
	FOREIGN KEY("last")  REFERENCES "figures"("id")
);

INSERT INTO "figures" ("id", "caption") VALUES ('', '无'); -- 防止 FOREIGN KEY 报错
CREATE INDEX idx_figures_entry ON "figures"("entry");
CREATE INDEX idx_figures_last ON "figures"("last");
CREATE INDEX idx_figures_image ON "figures"("image");
CREATE INDEX idx_figures_aka ON "figures"("aka");

-- 图片文件
-- 一个记录是一个图片文件
-- pdf 和 svg 必须一一对应且 images.figure 相同
CREATE TABLE "images" (
	"hash"         TEXT    NOT NULL UNIQUE,     -- 文件 SHA1 的前 16 位（如果 svg 需要先把 CRLF 变为 LF）
	"ext"          TEXT    NOT NULL,            -- [pdf|svg|png|jpg|gif|...] 拓展名
	"figure"       TEXT    NOT NULL DEFAULT '', -- 本图片文件归哪个图片环境管理（figures.aka 必须为空）。 可以多条记录（具有不同的 ext）对应一个 figures 记录， 但有且只有一个是 figures.image
	"author"       INTEGER NOT NULL DEFAULT -1, -- 当前版本作者/修改者
	"license"      TEXT    NOT NULL DEFAULT '', -- 当前版本协议
	"time"         TEXT    NOT NULL DEFAULT '', -- 上传时间
	PRIMARY KEY("hash"),
	UNIQUE ("figure", "ext"),
	FOREIGN KEY("figure")  REFERENCES "figures"("id"),
	FOREIGN KEY("author")  REFERENCES "authors"("id"),
	FOREIGN KEY("license") REFERENCES "licenses"("id")
);

INSERT INTO "images" ("hash", "ext") VALUES ('', ''); -- 防止 FOREIGN KEY 报错
CREATE INDEX idx_images_figure ON "images"("figure");

-- 文件
CREATE TABLE "files" (
	"hash"             TEXT    NOT NULL UNIQUE,      -- 文件 SHA1 的前 16 位
	"name"             TEXT    NOT NULL,             -- 文件名（含拓展名）
	"description"      TEXT    NOT NULL DEFAULT '',  -- 备注（类似 commit 信息）
	"last"             TEXT    NOT NULL DEFAULT '',  -- 上一个版本
	"author"           INTEGER NOT NULL DEFAULT -1,  -- 当前版本修改者
	"license"          TEXT    NOT NULL DEFAULT '',  -- 当前版本协议
	"time"             TEXT    NOT NULL DEFAULT '',  -- 上传时间
	PRIMARY KEY("hash"),
	FOREIGN KEY("author")  REFERENCES "authors"("id"),
	FOREIGN KEY("license") REFERENCES "licenses"("id"),
	FOREIGN KEY("last") REFERENCES "files"("hash")
);

INSERT INTO "files" ("hash", "name") VALUES ('', '无'); -- 防止 FOREIGN KEY 报错
CREATE INDEX idx_files_name ON "files"("name");
CREATE INDEX idx_files_last ON "files"("last");
CREATE INDEX idx_files_time ON "files"("time");

-- 图片附件
CREATE TABLE "figure_files" (
	"figure"           TEXT    NOT NULL,     -- figures.id
	"file"             TEXT    NOT NULL,     -- files.hash
	UNIQUE("figure", "file"),
	FOREIGN KEY("figure")  REFERENCES "figures"("id"),
	FOREIGN KEY("file") REFERENCES "files"("hash")
);

CREATE INDEX idx_figure_files_figure ON "figure_files"("figure");
CREATE INDEX idx_figure_files_file ON "figure_files"("file");

-- 文章附件
CREATE TABLE "entry_files" (
	"entry"            TEXT    NOT NULL,     -- entries.id
	"file"             TEXT    NOT NULL,     -- files.hash
	UNIQUE("entry", "file"),
	FOREIGN KEY("entry")  REFERENCES "entries"("id"),
	FOREIGN KEY("file") REFERENCES "files"("hash")
);

CREATE INDEX idx_entry_files_figure ON "entry_files"("entry");
CREATE INDEX idx_entry_files_file ON "entry_files"("file");

-- 带标签的代码环境
CREATE TABLE "code" (
	"id"          TEXT     NOT NULL UNIQUE,      -- \label{code_xxx} 中 xxx
	"entry"       TEXT     NOT NULL,             -- 所在文章
	"caption"     TEXT     NOT NULL DEFAULT '',  -- 文件名（含拓展名）
	"language"    TEXT     NOT NULL DEFAULT '',  -- [matlab|...] 高亮语言，空代表 none
	"order"       INTEGER  NOT NULL,             -- 显示编号
	"license"     TEXT     NOT NULL,             -- 协议
	"source"      TEXT     NOT NULL,             -- 来源（如果非原创）
	PRIMARY KEY("id"),
	FOREIGN KEY("entry")    REFERENCES "entries"("id"),
	FOREIGN KEY("language") REFERENCES "languages"("id"),
	FOREIGN KEY("license")  REFERENCES "licenses"("id")
);

CREATE INDEX idx_code_entry ON "entry_files"("entry");
CREATE INDEX idx_code_caption ON "entry_files"("entry");
CREATE INDEX idx_code_lang ON "entry_files"("entry");

-- 编程语言
CREATE TABLE "languages" (
	"id"    TEXT NOT NULL UNIQUE,  -- code.language
	"name"  TEXT NOT NULL UNIQUE,  -- 名字
	PRIMARY KEY("id")
);

INSERT INTO "languages" ("id", "name") VALUES ('', '无'); -- 防止 FOREIGN KEY 报错

-- 文章中的其他标签（除图片、代码）
CREATE TABLE "labels" (
	"id"       TEXT    NOT NULL UNIQUE,     -- \label{yyy_xxxx} 中 yyy_xxxx 是 id， yyy 是 "type"
	"type"     TEXT    NOT NULL,            -- [eq|sub|tab|def|lem|the|cor|ex|exe] 标签类型
	"entry"    TEXT    NOT NULL,            -- 所在文章（以 entries.labels 为准）
	"order"    INTEGER NOT NULL,            -- 显示编号
	"ref_by"   TEXT    NOT NULL DEFAULT '', -- 【待迁移到 entry_refs 表】【生成】"entry1 entry2" 被哪些文章引用（以 entries.refs 为准）
	PRIMARY KEY("id"),
	FOREIGN KEY("entry") REFERENCES "entries"("id"),
	UNIQUE("type", "entry", "order")
);

CREATE INDEX idx_labels_type ON "labels"("type");
CREATE INDEX idx_labels_entry ON "labels"("entry");

-- 参考文献
CREATE TABLE "bibliography" (
	"id"        TEXT    NOT NULL UNIQUE,     -- \cite{xxx} 中的 xxx
	"order"     INTEGER NOT NULL UNIQUE,     -- 显示编号
	"details"   TEXT    NOT NULL,            -- 详细信息（TODO: 待拆分）
	"ref_by"    TEXT    NOT NULL DEFAULT '', -- 【待迁移到 entry_bibs 表】【生成】"entry1 entry2" 被哪些文章引用（以 entries.bibs 为准）
	PRIMARY KEY("id")
);

INSERT INTO "bibliography" ("id", "order", "details") VALUES ('', 0, '无'); -- 防止 FOREIGN KEY 报错

-- 编辑历史（一个记录 5 分钟）
CREATE TABLE "history" (
	"hash"     TEXT    NOT NULL UNIQUE,      -- SHA1 的前 16 位
	"time"     TEXT    NOT NULL,             -- 备份时间， 格式 YYYYMMDDHHMM（下同）
	"author"   INTEGER NOT NULL,             -- 作者
	"entry"    TEXT    NOT NULL,             -- 文章
	"license"  TEXT    NOT NULL DEFAULT '',  -- entries.license
	"add"      INTEGER NOT NULL DEFAULT -1,  -- 新增字符数（-1: 未知）
	"del"      INTEGER NOT NULL DEFAULT -1,  -- 减少字符数（-1: 未知）
	"last"     TEXT    NOT NULL DEFAULT '',  -- 本文上次备份的 hash， '' 代表首个
	PRIMARY KEY("hash"),
	FOREIGN KEY("last")   REFERENCES "history"("hash"),
	FOREIGN KEY("author") REFERENCES "authors"("id"),
	FOREIGN KEY("entry")  REFERENCES "entries"("id"),
	UNIQUE("time", "author", "entry")
);

INSERT INTO "history" ("hash", "time", "author", "entry") VALUES ('', '', 0, ''); -- 防止 FOREIGN KEY 报错
CREATE INDEX idx_history_time ON "history"("time");
CREATE INDEX idx_history_author ON "history"("author");
CREATE INDEX idx_history_last ON "history"("last");

-- 审稿记录
CREATE TABLE "review" (
	"hash"     TEXT    NOT NULL UNIQUE,            -- history.hash
	"time"     TEXT    NOT NULL,                   -- 审稿提交时间 YYYYMMDDHHMM
	"refID"    INTEGER NOT NULL,                   -- 审稿人 ID
	"entry"    TEXT    NOT NULL,                   -- 文章
	"author"   INTEGER NOT NULL,                   -- 作者
	"action"   TEXT    NOT NULL DEFAULT '',        -- [Pub] 发布 [Udo] 撤回 [Fix] 继续完善
	"comment"  TEXT    NOT NULL DEFAULT '',        -- 意见（也可以直接修改正文或在正文中评论）
	PRIMARY KEY("hash"),
	FOREIGN KEY("hash")   REFERENCES "history"("hash"),
	FOREIGN KEY("refID")  REFERENCES "authors"("id"),
	FOREIGN KEY("entry")  REFERENCES "entries"("id"),
	FOREIGN KEY("author") REFERENCES "authors"("id")
);

INSERT INTO "review" ("hash", "time", "refID", "entry", "author") VALUES ('', '', 0, '', 0); -- 防止 FOREIGN KEY 报错
CREATE INDEX idx_review_time ON "review"("time");
CREATE INDEX idx_review_refID ON "review"("refID");
CREATE INDEX idx_review_entry ON "review"("entry");

-- 所有作者
CREATE TABLE "authors" (
	"id"         INTEGER NOT NULL UNIQUE,     -- ID
	"uuid"       TEXT    NOT NULL DEFAULT '', -- "8-4-4-4-12" 网站用户系统的 ID
	"name"       TEXT    NOT NULL,            -- 昵称
	"applied"    INTEGER NOT NULL DEFAULT 0,  -- [0|1] 已申请
	"hide"       INTEGER NOT NULL DEFAULT 0,  -- 【待迁移到 authors_rights 表】[0|1] 不出现在文章作者列表
	"aka"        INTEGER NOT NULL DEFAULT -1, -- 是否是其他 id 的小号（所有贡献和记录都算入大号）
	"contrib"    INTEGER NOT NULL DEFAULT 0,  -- 贡献的分钟数（折算）
	"referee"    TEXT    NOT NULL DEFAULT '', -- "part:parts.id chap:chapters.id sub:phys" 哪些部分/学科的审稿人
	PRIMARY KEY("id" AUTOINCREMENT),
	FOREIGN KEY("aka") REFERENCES "authors"("id")
);

INSERT INTO "authors" ("id", "name") VALUES (-1, ''); -- 防止 FOREIGN KEY 报错

-- 权限或限制种类
CREATE TABLE "rights" (
	"id"       TEXT    NOT NULL UNIQUE,
	"name"     TEXT    NOT NULL UNIQUE,     -- 中文名
	"comment"  TEXT    NOT NULL DEFAULT '', -- 具体说明（可选）
	PRIMARY KEY("id")
);

CREATE INDEX idx_authors_uuid ON "authors"("uuid");
CREATE INDEX idx_authors_name ON "authors"("name");
CREATE INDEX idx_authors_applied ON "authors"("applied");
CREATE INDEX idx_authors_aka ON "authors"("aka");

-- 作者权限或限制
CREATE TABLE "author_rights" (
	"author"   INTEGER NOT NULL UNIQUE,  -- authors.id
	"right"    TEXT    NOT NULL UNIQUE,  --rights.id
	PRIMARY KEY("author", "right"),
	FOREIGN KEY("author") REFERENCES "authors"("id"),
	FOREIGN KEY("right") REFERENCES "rights"("id")
);

CREATE INDEX idx_author_rights_author ON "author_rights"("author");
CREATE INDEX idx_author_rights_right ON "author_rights"("right");

-- 工资规则
-- rule: 作者 + 协议 -> 时薪
-- rule: 作者 + 文章 -> 缩放
-- rule: 文章 -> 缩放
CREATE TABLE "salary" (
	"id"      INTEGER NOT NULL UNIQUE,           -- 编号
	"author"  INTEGER NOT NULL DEFAULT -1,       -- 作者（-1 代表所有）
	"entry"   TEXT    NOT NULL DEFAULT '',       -- 文章（空代表所有）
	"license" TEXT    NOT NULL DEFAULT '',       -- 协议（空代表所有）
	"begin"   TEXT    NOT NULL DEFAULT '',       -- 生效时间（空代表所有）
	"end"     TEXT    NOT NULL DEFAULT '',       -- 截止时间（空代表所有）
	"value"   REAL    NOT NULL DEFAULT -1,       -- 时薪（元）（-1 代表 NULL）
	"scale"   REAL    NOT NULL DEFAULT  1,       -- 缩放
	"comment" TEXT    NOT NULL DEFAULT '',       -- 备注
	"comment_admin" TEXT    NOT NULL DEFAULT '', -- 备注（仅管理员可见）
	PRIMARY KEY("id" AUTOINCREMENT),
	FOREIGN KEY("author") REFERENCES "authors"("id"),
	FOREIGN KEY("entry") REFERENCES "entries"("id"),
	FOREIGN KEY("license") REFERENCES "licenses"("id")
);

CREATE INDEX idx_salary_author ON "salary"("author");
CREATE INDEX idx_salary_entry ON "salary"("entry");
CREATE INDEX idx_salary_license ON "salary"("license");
CREATE INDEX idx_salary_begin ON "salary"("begin");
CREATE INDEX idx_salary_end ON "salary"("end");

-- 补贴手动调整
-- 贡献调整（history 记录之外的贡献，例如转载、画图、代码等）
CREATE TABLE "salary_change" (
	"id"        INTEGER NOT NULL UNIQUE,           -- 编号
	"author"    INTEGER NOT NULL,                  -- 作者（空代表所有）
	"entry"     TEXT    NOT NULL DEFAULT '',       -- 文章
	"time"      TEXT    NOT NULL DEFAULT '',       -- 何时做出的调整
	"value"     REAL    NOT NULL,                  -- 金额（非零实数）
	"comment"   TEXT    NOT NULL DEFAULT '',       -- 备注
	"approved"  INTEGER NOT NULL,                  -- [0|1] 是否生效（需要审核）
	"comment2"  TEXT    NOT NULL DEFAULT '',       -- 备注（仅管理员可见）
	PRIMARY KEY("id" AUTOINCREMENT),
	FOREIGN KEY("entry")  REFERENCES "entries"("id"),
	FOREIGN KEY("author") REFERENCES "authors"("id")
);

CREATE INDEX idx_salary_change_author ON "salary_change"("author");
CREATE INDEX idx_salary_change_entry ON "salary_change"("entry");
CREATE INDEX idx_salary_change_time ON "salary_change"("time");
