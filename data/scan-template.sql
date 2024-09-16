-- 小时百科在线编辑器数据库
-- 为方便编程， 所有字段都必须 NOT NULL，用 0,01,'' 代替
-- 为方便 sqldiff 对比版本，所有表必须有 PRIMARY KEY

-- 所有表定义的版本
CREATE TABLE "table_version" ( -- 20240403
	"table"      TEXT    NOT NULL UNIQUE, -- 当前数据库中的所有表
	"version"    TEXT    NOT NULL,        -- 每个表的当前版本（版本号是更新时间）
	"importance" INTEGER NOT NULL,        -- [0] 可生成 [1] 重要数据 [2] 临时数据（无需备份）
	PRIMARY KEY("table")
);

INSERT INTO "table_version" VALUES ('bibliography',      '20240617', 0);
INSERT INTO "table_version" VALUES ('chapters',          '20240403', 0);
INSERT INTO "table_version" VALUES ('entries',           '20240403', 0);
INSERT INTO "table_version" VALUES ('entry_uprefs',      '20240403', 0);
INSERT INTO "table_version" VALUES ('entry_authors',     '20240403', 0);
INSERT INTO "table_version" VALUES ('entry_bibs',        '20240405', 0);
INSERT INTO "table_version" VALUES ('entry_refs',        '20240403', 0);
INSERT INTO "table_version" VALUES ('entry_tags',        '20240403', 0);
INSERT INTO "table_version" VALUES ('edges',             '20240403', 0);
INSERT INTO "table_version" VALUES ('history',           '20240403', 0);
INSERT INTO "table_version" VALUES ('labels',            '20240403', 0);
INSERT INTO "table_version" VALUES ('nodes',             '20240403', 0);
INSERT INTO "table_version" VALUES ('parts',             '20240403', 0);
INSERT INTO "table_version" VALUES ('seo_keys',          '20240403', 0);

INSERT INTO "table_version" VALUES ('authors',           '20240414', 1);
INSERT INTO "table_version" VALUES ('author_rights',     '20240403', 1);
INSERT INTO "table_version" VALUES ('bib_type',          '20240403', 1);
INSERT INTO "table_version" VALUES ('bib_doi',           '20240403', 1);
INSERT INTO "table_version" VALUES ('bib_url',           '20240403', 1);
INSERT INTO "table_version" VALUES ('bib_journal',       '20240403', 1);
INSERT INTO "table_version" VALUES ('bib_all_tags',      '20240403', 1);
INSERT INTO "table_version" VALUES ('bib_tags',          '20240403', 1);
INSERT INTO "table_version" VALUES ('bib_cite',          '20240403', 1);
INSERT INTO "table_version" VALUES ('bib_all_authors',   '20240403', 1);
INSERT INTO "table_version" VALUES ('bib_authors',       '20240403', 1);
INSERT INTO "table_version" VALUES ('contrib_adjust',    '20240414', 1);
INSERT INTO "table_version" VALUES ('code',              '20240403', 1);
INSERT INTO "table_version" VALUES ('code_lang',         '20240617', 1);
INSERT INTO "table_version" VALUES ('entry_score',       '20240403', 1);
INSERT INTO "table_version" VALUES ('entry_files',       '20240403', 1);
INSERT INTO "table_version" VALUES ('figures',           '20240403', 1);
INSERT INTO "table_version" VALUES ('files',             '20240403', 1);
INSERT INTO "table_version" VALUES ('figure_files',      '20240403', 1);
INSERT INTO "table_version" VALUES ('images',            '20240403', 1);
INSERT INTO "table_version" VALUES ('journals',          '20240403', 1);
INSERT INTO "table_version" VALUES ('licenses',          '20240403', 1);
INSERT INTO "table_version" VALUES ('license_apply',     '20240403', 1);
INSERT INTO "table_version" VALUES ('locked',            '20240403', 1);
INSERT INTO "table_version" VALUES ('repost',            '20240403', 1);
INSERT INTO "table_version" VALUES ('referee',           '20240414', 1);
INSERT INTO "table_version" VALUES ('review',            '20240403', 1);
INSERT INTO "table_version" VALUES ('rights',            '20240403', 1);
INSERT INTO "table_version" VALUES ('right_set',         '20240404', 1);
INSERT INTO "table_version" VALUES ('salary',            '20240403', 1);
INSERT INTO "table_version" VALUES ('types',             '20240403', 1);
INSERT INTO "table_version" VALUES ('tags',              '20240403', 1);
INSERT INTO "table_version" VALUES ('table_version',     '20240407', 1);

INSERT INTO "table_version" VALUES ('entries_to_update', '20240420', 2);
INSERT INTO "table_version" VALUES ('occupied',          '20240403', 2);
INSERT INTO "table_version" VALUES ('opened',            '20240403', 2);

----------------------------------------------------------------------------------------------------------
-- 百科文章
----------------------------------------------------------------------------------------------------------

-- 所有文章
-- 目录中的 \entry{标题}{xxx} 中 xxx 为 "id"
CREATE TABLE "entries" ( -- 20240403
	"id"           TEXT    NOT NULL UNIQUE,
	"caption"      TEXT    NOT NULL DEFAULT '',      -- 标题（以 main.tex 中为准， 若不在目录中则以首行注释为准）
	"part"         TEXT    NOT NULL DEFAULT '',      -- 部分， 空代表不在目录中
	"chapter"      TEXT    NOT NULL DEFAULT '',      -- 章， 空代表不在目录中
	"last"         TEXT    NOT NULL DEFAULT '',      -- 目录中的上一篇文章， 空代表这是第一个或不在目录中
	"next"         TEXT    NOT NULL DEFAULT '',      -- 目录中的下一篇文章， 空代表这是最后一个或不在目录中
	"license"      TEXT    NOT NULL DEFAULT 'Usr',   -- 协议
	"type"         TEXT    NOT NULL DEFAULT '',      -- 类型
	"keys"         TEXT    NOT NULL DEFAULT '',      -- 【待迁移到 seo_keys 表】"关键词1|...|关键词N"
	"draft"        INTEGER NOT NULL DEFAULT 2,       -- 【待迁移到 entry_tags 表】[0|1|2] 是否草稿（文章是否标记 \issueDraft， 2 代表未知）
	"deleted"      INTEGER NOT NULL DEFAULT 0,       -- 【待迁移到 entry_tags 表】[0|1] 是否已删除（tex 文件删除，只留备份文件）
	"last_pub"     TEXT    NOT NULL DEFAULT '',      -- 最后过审，空代表没有 (review.hash)
	"last_backup"  TEXT    NOT NULL DEFAULT '',      -- 最后备份，空代表没有 (history.hash)
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
CREATE TABLE "entry_uprefs" ( -- 20240403
	"entry"      TEXT NOT NULL,     -- entries.id
	"upref"      TEXT NOT NULL,     -- entries.id
	PRIMARY KEY("entry", "upref"),
	FOREIGN KEY("entry")  REFERENCES "entries"("id"),
	FOREIGN KEY("upref")  REFERENCES "entries"("id")
);

CREATE INDEX idx_entry_uprefs_entry ON "entry_uprefs"("entry");
CREATE INDEX idx_entry_uprefs_upref ON "entry_uprefs"("upref");

-- 文章作者列表
-- 有备份的都要记录，显示时再根据 contrib, authors.hide 隐藏
CREATE TABLE "entry_authors" ( -- 20240403
	"entry"        TEXT    NOT NULL,
	"author"       INTEGER NOT NULL,
	"contrib"      INTEGER NOT NULL,              -- 贡献时长（分钟）根据 "history" 和 "contrib_adjust" 生成
	"last_backup"  TEXT    NOT NULL DEFAULT '',   -- 最后备份，例如用于查看最后编辑时间
	PRIMARY KEY("entry", "author"),
	FOREIGN KEY("entry")       REFERENCES "entries"("id"),
	FOREIGN KEY("author")      REFERENCES "authors"("id"),
	FOREIGN KEY("last_backup") REFERENCES "history"("hash")
);

CREATE INDEX idx_entry_authors_entry ON "entry_authors"("entry");
CREATE INDEX idx_entry_authors_author ON "entry_authors"("author");

-- 文章中的 \cite{}
CREATE TABLE "entry_bibs" ( -- 20240405
	"entry"    TEXT NOT NULL,                -- entries.id
	"bib"      TEXT NOT NULL,                -- bibliography.id
	"order"    INTEGER NOT NULL DEFAULT 0,   -- \cite{} 在文章中出现的顺序，从 1 开始
	PRIMARY KEY("entry", "bib"),
	FOREIGN KEY("entry") REFERENCES "entries"("id"),
	FOREIGN KEY("bib") REFERENCES "bibliography"("id")
);

CREATE INDEX idx_entry_bibs_entry ON "entry_bibs"("entry");
CREATE INDEX idx_entry_bibs_bib  ON "entry_bibs"("bib");

-- 文章中的所有 \autoref{} （不仅仅是 labels 表中的）
CREATE TABLE "entry_refs" ( -- 20240403
	"entry"    TEXT NOT NULL,     -- entries.id
	"label"    TEXT NOT NULL,     -- labels.id
	PRIMARY KEY("entry", "label"),
	FOREIGN KEY("entry")  REFERENCES "entries"("id")
);

CREATE INDEX idx_entry_refs_entry ON "entry_refs"("entry");
CREATE INDEX idx_entry_refs_label ON "entry_refs"("label");

-- 创作协议
CREATE TABLE "licenses" ( -- 20240403
	"id"        TEXT NOT NULL UNIQUE,      -- 协议 id，只允许字母和数字，字母开头，空代表未知
	"caption"   TEXT NOT NULL UNIQUE ,     -- 协议官方名称
	"url"       TEXT NOT NULL DEFAULT '',  -- 协议官方 url
	"intro"     TEXT NOT NULL DEFAULT '',  -- 协议简介和说明
	"text"      TEXT NOT NULL DEFAULT '',  -- 协议全文
	PRIMARY KEY("id")
);

INSERT INTO "licenses" ("id", "caption") VALUES ('', '未知'); -- 防止 FOREIGN KEY 报错
INSERT INTO licenses VALUES('CCBYSA3','CC BY-SA 3.0','https://creativecommons.org/licenses/by-sa/3.0/deed.zh-Hans','常用于开源作品，如维基百科。运行转载、修改，但需要注明出处，且使用一样或兼容的协议发布。','');

-- 创作协议适用范围
CREATE TABLE "license_apply" ( -- 20240403
	"license"   TEXT    NOT NULL,              -- licenses.id
	"apply"     TEXT    NOT NULL,              -- 适用于：[e] 文章， [i] 图片 [c] 代码 [f] 文件 [a] 全部
	"order"     INTEGER NOT NULL,              -- 显示优先级（UI 中从小到大排列）
	"wiki_note" TEXT    NOT NULL DEFAULT 'a',  -- 适用于：[w] 百科 [n] 笔记 [a] 百科和笔记
	PRIMARY KEY("license", "apply"),
	FOREIGN KEY("license")  REFERENCES "licenses"("id")
);

CREATE INDEX idx_license_apply_apply ON "license_apply"("apply");
CREATE INDEX idx_license_apply_wiki_note ON "license_apply"("wiki_note");

-- 文章类型
CREATE TABLE "types" ( -- 20240403
	"id"        TEXT NOT NULL UNIQUE,
	"caption"   TEXT NOT NULL UNIQUE,      -- 中文名
	"intro"     TEXT NOT NULL DEFAULT '',  -- 说明
	PRIMARY KEY("id")
);

INSERT INTO "types" VALUES('', '未知', ''); -- 防止 FOREIGN KEY 报错
INSERT INTO "types" VALUES('Art','文章','类似于学术论文');
INSERT INTO "types" VALUES('Map','导航','介绍百科中的一章、一部分等，具有大量链接到其中的词条');
INSERT INTO "types" VALUES('Note','内部笔记','一些可能用于百科创作的笔记或草稿，不公开');
INSERT INTO "types" VALUES('Sum','总结','类似于讲义或复习资料，把知识要点列出');
INSERT INTO "types" VALUES('Test','测试','临时测试，将定期删除');
INSERT INTO "types" VALUES('Toc','目录','百科页面的目录');
INSERT INTO "types" VALUES('Tutor','教程','类似于教材的一节');
INSERT INTO "types" VALUES('Wiki','综述','类似于维基百科的条目，中立、全面、一般性的介绍');

-- SEO 关键词（用于 html header 中的搜索引擎优化）
-- TODO: 用于代替 entries.keys
CREATE TABLE "seo_keys" ( -- 20240403
	"entry"  TEXT NOT NULL,
	"key"    TEXT NOT NULL,
	"order"  INTEGER NOT NULL,
	PRIMARY KEY("entry", "key"),
	FOREIGN KEY("entry")  REFERENCES "entries"("id")
);

CREATE INDEX idx_seo_keys_key ON "seo_keys"("key");

-- 知识树节点 \pentry{...\req{}，...\reqq{}}{id}
-- 每篇文章自动添加一个文章节点（nodes.id = nodes.entry，order 最大），用于把整篇文章（即最后一个节点）作为预备知识
CREATE TABLE "nodes" ( -- 20240403
	"id"        TEXT    NOT NULL UNIQUE,    -- \pentry{}{id}
	"entry"     TEXT    NOT NULL,           -- entries.id
	"order"     INTEGER NOT NULL,           -- 在文章中出现的顺序（从 1 开始）
	PRIMARY KEY("id"),
	FOREIGN KEY("entry")  REFERENCES "entries"("id")
);

CREATE INDEX idx_nodes_id ON "nodes"("id");
CREATE INDEX idx_nodes_entry ON "nodes"("entry");

-- 知识树的边（\pentry{} 中的 \req{}）
-- 每个节点对本文上一个节点的默认连接不需要记录
-- 不允许 \req{} 引用同一文章的其他节点
CREATE TABLE "edges" ( -- 20240403
	"from"     TEXT    NOT NULL,             -- nodes.id （若等于 entries.id 则表示依赖整篇文章，即最后一个节点）
	"to"       TEXT    NOT NULL,             -- nodes.id
	"weak"     INTEGER NOT NULL,             -- [0|1] 循环依赖时优先隐藏（原 * 标记）（\wnref{}）
	"hide"     INTEGER NOT NULL DEFAULT -1,  -- [0|1|-1] 多余的预备知识（原 ~ 标记）， 不在知识树中显示， -1 代表未知
	PRIMARY KEY("from", "to"),
	FOREIGN KEY("to")  REFERENCES "nodes"("id"),
	FOREIGN KEY("from")  REFERENCES "nodes"("id")
);

CREATE INDEX idx_edges_from ON "edges"("from");
CREATE INDEX idx_edges_to ON "edges"("to");

-- 所有文章标签（包括 issues 环境中的）
CREATE TABLE "tags" ( -- 20240403
	"id"       INTEGER NOT NULL UNIQUE,   -- id
	"name"     TEXT NOT NULL UNIQUE,      -- 全名
	"order"    INTEGER NOT NULL,          -- 菜单中的排列顺序
	"comment"  TEXT NOT NULL DEFAULT '',  -- 说明
	PRIMARY KEY("id" AUTOINCREMENT)
);

CREATE INDEX idx_tags_name ON "tags"("name");

-- 文章标签
-- （issues 环境属于该表）
-- keys 表是传统的关键词，多是术语。 tags 可以是各种方面的分类
CREATE TABLE "entry_tags" ( -- 20240403
	"entry"    TEXT NOT NULL,
	"tag"      INTEGER NOT NULL,
	"comment"  TEXT  NOT NULL,     -- issueOthers{} 或其他支持评论的 issue 类型
	PRIMARY KEY("entry", "tag"),
	FOREIGN KEY("entry")  REFERENCES "entries"("id"),
	FOREIGN KEY("tag")  REFERENCES "tags"("id")
);

CREATE INDEX idx_entry_tags_tag ON "entry_tags"("tag");

-- 文章转载（到其他平台）
CREATE TABLE "repost" ( -- 20240403
	"entry"    TEXT    NOT NULL,   -- entries.id
	"url"      TEXT    NOT NULL,   -- 网址
	"updated"  TEXT    NOT NULL,   -- 最后更新时间
	PRIMARY KEY("entry", "url"),
	FOREIGN KEY("entry") REFERENCES "entries"("id")
);

CREATE INDEX idx_repost_entry ON "repost"("entry");
CREATE INDEX idx_repost_updated ON "repost"("updated");

-- 文章评分
CREATE TABLE "entry_score" ( -- 20240403
	"entry"   TEXT     NOT NULL,   -- entries.id
	"author"  INTEGER  NOT NULL,   -- 评分者
	"score"   REAL     NOT NULL,   -- 评分（0-10)
	"version" TEXT     NOT NULL,   -- 文章版本
	"time"    TEXT     NOT NULL,   -- 评分时间
	"comment" TEXT     NOT NULL,   -- 评分理由等
	PRIMARY KEY("entry", "author"),
	FOREIGN KEY("entry") REFERENCES "entries"("id"),
	FOREIGN KEY("author") REFERENCES "authors"("id"),
	FOREIGN KEY("version") REFERENCES "history"("id")
);

CREATE INDEX idx_entry_score_entry ON "entry_score"("entry");
CREATE INDEX idx_entry_score_version ON "entry_score"("version");
CREATE INDEX idx_entry_score_time ON "entry_score"("time");

-- 编译文章（包括 main.tex）到 online 时，也需要更新的其他文章（例如 \auroef{} 编号改变等）
-- 编译文章到 change 时会根据数据库改变更新该表
CREATE TABLE "entries_to_update" ( -- 20240420
	"entry"    TEXT    NOT NULL,     -- entries.id
	"update"   TEXT    NOT NULL,     -- entries.id
	PRIMARY KEY("entry", "update"),
	FOREIGN KEY("entry")  REFERENCES "entries"("id"),
	FOREIGN KEY("update")  REFERENCES "entries"("id")
);

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
CREATE TABLE "locked" ( -- 20240403
	"entry"    TEXT    NOT NULL UNIQUE, -- entries.id
	"author"   INTEGER NOT NULL,        -- authors.id
	"time"     TEXT    NOT NULL,        -- 开始锁定时间
	PRIMARY KEY("entry"),
	FOREIGN KEY("entry")  REFERENCES "entries"("id"),
	FOREIGN KEY("author") REFERENCES "authors"("id")
);

-- 文章打开列表
-- 用于恢复用户之前打开的文章
CREATE TABLE "opened" ( -- 20240403
	"author"   INTEGER NOT NULL UNIQUE, -- authors.id
	"entries"  TEXT    NOT NULL,        -- "entry1 entry2"
	"time"     TEXT    NOT NULL,        -- 打开时间
	PRIMARY KEY("author"),
	FOREIGN KEY("author")  REFERENCES "authors"("id")
);

-- 部分
-- 目录中 \label{prt_xxx} 中 xxx 为 "id"
CREATE TABLE "parts" ( -- 20240403
	"id"          TEXT    NOT NULL UNIQUE,     -- 命名规则和文章一样
	"order"       INTEGER NOT NULL,            -- 目录中出现的顺序，从 1 开始（0 代表不在目录中）
	"caption"     TEXT    NOT NULL,            -- 标题
	"chap_first"  TEXT    NOT NULL,            -- 第一章
	"chap_last"   INTEGER NOT NULL,            -- 最后一章
	"subject"     TEXT    NOT NULL DEFAULT '', -- [phys|math|cs] 学科
	PRIMARY KEY("id"),
	FOREIGN KEY("chap_first") REFERENCES "chapters"("id"),
	FOREIGN KEY("chap_last")  REFERENCES "chapters"("id")
);

INSERT INTO "parts" VALUES('', 0, '无', '', '', ''); -- 防止 FOREIGN KEY 报错

-- 章
-- 目录中 \label{cpt_xxx} 中 xxx 为 "id"
CREATE TABLE "chapters" ( -- 20240403
	"id"            TEXT    NOT NULL UNIQUE, -- 命名规则和文章一样
	"order"         INTEGER NOT NULL,        -- 目录中出现的顺序，从 1 开始（0 代表不在目录中）
	"caption"       TEXT    NOT NULL,        -- 标题
	"part"          TEXT    NOT NULL,        -- 所在部分（不能为 ''）
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
CREATE TABLE "figures" ( -- 20240403
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
	"aka"         TEXT    NOT NULL DEFAULT '',  -- "figures.id" 若不为空，由另一条记录（aka 必须为空，允许被标记 deleted）管理： 所有图片文件（"images.figure"）, "authors", "last", "files", "source"（本记录这些列为空）。 本记录 "image" 必须在另一条记录的图片文件中。
	"deleted"     INTEGER NOT NULL DEFAULT 0,   -- [0] entry 源码中定义了该环境 [1] 定义后被删除
	"remark"     TEXT    NOT NULL DEFAULT '',   -- 备注信息
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
CREATE TABLE "images" ( -- 20240403
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
CREATE TABLE "files" ( -- 20240403
	"hash"          TEXT    NOT NULL UNIQUE,      -- 文件 SHA1 的前 16 位
	"name"          TEXT    NOT NULL,             -- 文件名（含拓展名）
	"description"   TEXT    NOT NULL DEFAULT '',  -- 备注（类似 commit 信息）
	"last"          TEXT    NOT NULL DEFAULT '',  -- 上一个版本
	"author"        INTEGER NOT NULL DEFAULT -1,  -- 当前版本修改者
	"license"       TEXT    NOT NULL DEFAULT '',  -- 当前版本协议
	"time"          TEXT    NOT NULL DEFAULT '',  -- 上传时间
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
CREATE TABLE "figure_files" ( -- 20240403
	"figure"    TEXT    NOT NULL,     -- figures.id
	"file"      TEXT    NOT NULL,     -- files.hash
	PRIMARY KEY("figure", "file"),
	FOREIGN KEY("figure")  REFERENCES "figures"("id"),
	FOREIGN KEY("file") REFERENCES "files"("hash")
);

CREATE INDEX idx_figure_files_figure ON "figure_files"("figure");
CREATE INDEX idx_figure_files_file ON "figure_files"("file");

-- 文章附件
CREATE TABLE "entry_files" ( -- 20240403
	"entry"            TEXT    NOT NULL,     -- entries.id
	"file"             TEXT    NOT NULL,     -- files.hash
	PRIMARY KEY("entry", "file"),
	FOREIGN KEY("entry")  REFERENCES "entries"("id"),
	FOREIGN KEY("file") REFERENCES "files"("hash")
);

CREATE INDEX idx_entry_files_figure ON "entry_files"("entry");
CREATE INDEX idx_entry_files_file ON "entry_files"("file");

-- 带标签的代码环境
CREATE TABLE "code" ( -- 20240403
	"id"          TEXT     NOT NULL UNIQUE,      -- \label{code_xxx} 中 xxx
	"entry"       TEXT     NOT NULL,             -- 所在文章
	"caption"     TEXT     NOT NULL DEFAULT '',  -- 文件名（含拓展名）
	"language"    TEXT     NOT NULL DEFAULT '',  -- [matlab|...] 高亮语言，空代表 none
	"order"       INTEGER  NOT NULL,             -- 显示编号
	"license"     TEXT     NOT NULL,             -- 协议
	"source"      TEXT     NOT NULL,             -- 来源（如果非原创）
	PRIMARY KEY("id"),
	FOREIGN KEY("entry")    REFERENCES "entries"("id"),
	FOREIGN KEY("language") REFERENCES "code_langs"("id"),
	FOREIGN KEY("license")  REFERENCES "licenses"("id")
);

CREATE INDEX idx_code_entry ON "entry_files"("entry");
CREATE INDEX idx_code_caption ON "entry_files"("entry");
CREATE INDEX idx_code_lang ON "entry_files"("entry");

-- 编程语言
CREATE TABLE "code_langs" ( -- 20240403
	"id"    TEXT NOT NULL UNIQUE,  -- code.language
	"name"  TEXT NOT NULL UNIQUE,  -- 名字
	PRIMARY KEY("id")
);

INSERT INTO "code_langs" ("id", "name") VALUES ('', '无'); -- 防止 FOREIGN KEY 报错

-- 文章中的其他标签（除图片、代码）
CREATE TABLE "labels" ( -- 20240403
	"id"       TEXT    NOT NULL UNIQUE,     -- \label{yyy_xxxx} 中 yyy_xxxx 是 id， yyy 是 "type"
	"type"     TEXT    NOT NULL,            -- [eq|sub|tab|def|lem|the|cor|ex|exe] 标签类型
	"entry"    TEXT    NOT NULL,            -- 所在文章（以 entries.labels 为准）
	"order"    INTEGER NOT NULL,            -- 显示编号
	PRIMARY KEY("id"),
	FOREIGN KEY("entry") REFERENCES "entries"("id")
);

CREATE INDEX idx_labels_type ON "labels"("type");
CREATE INDEX idx_labels_entry ON "labels"("entry");

-- 编辑历史（一个记录 5 分钟）
CREATE TABLE "history" ( -- 20240403
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

-- 审稿人（可以给任意文章审稿）
CREATE TABLE "referee" ( -- 20240414
	"author"    INTEGER NOT NULL UNIQUE,      -- author.id
	"subjects"  TEXT    NOT NULL DEFAULT '',  -- 审稿范围（学科、方向等，仅供人阅读）
	PRIMARY KEY("author"),
	FOREIGN KEY("author") REFERENCES "authors"("id")
);

-- 审稿记录
CREATE TABLE "review" ( -- 20240403
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

----------------------------------------------------------------------------------------------------------
-- 百科作者
----------------------------------------------------------------------------------------------------------

-- 所有作者
CREATE TABLE "authors" ( -- 20240414
	"id"         INTEGER NOT NULL UNIQUE,     -- ID
	"uuid"       TEXT    NOT NULL DEFAULT '', -- "8-4-4-4-12" 网站用户系统的 ID
	"name"       TEXT    NOT NULL,            -- 昵称
	"aka"        INTEGER NOT NULL DEFAULT -1, -- 百科编辑器将该作者视为其他 id 登录，笔记编辑器忽略（scan 应该忽略该字段）
	"contrib"    INTEGER NOT NULL DEFAULT 0,  -- 总贡献（分钟）根据 "history" 和 "contrib_adjust" 生成
	PRIMARY KEY("id" AUTOINCREMENT),
	FOREIGN KEY("aka") REFERENCES "authors"("id")
);

INSERT INTO "authors" ("id", "name") VALUES (-1, ''); -- 防止 FOREIGN KEY 报错
INSERT INTO "authors" VALUES(900,'','服务器后台',-1,0);

CREATE INDEX idx_authors_uuid ON "authors"("uuid");
CREATE INDEX idx_authors_name ON "authors"("name");
CREATE INDEX idx_authors_applied ON "authors"("applied");
CREATE INDEX idx_authors_aka ON "authors"("aka");

-- 权限或限制种类
CREATE TABLE "rights" ( -- 20240403
	"id"       TEXT    NOT NULL UNIQUE,
	"name"     TEXT    NOT NULL UNIQUE,     -- 中文名
	"comment"  TEXT    NOT NULL DEFAULT '', -- 具体说明（可选）
	PRIMARY KEY("id")
);

INSERT INTO rights VALUES('admin',    '超级管理员', '修改任意用户权限');
INSERT INTO rights VALUES('export',   '导出 md',   '用于转载到知乎等');
INSERT INTO rights VALUES('license',  '付费协议',   '更改任意文章、代码、图片等协议到付费协议或移除付费协议');
INSERT INTO rights VALUES('note',     '笔记调试',   '以任意用户的身份登录笔记');
INSERT INTO rights VALUES('occupy',   '占用管理',   '');
INSERT INTO rights VALUES('pub',      '发布文章',   '');
INSERT INTO rights VALUES('salary',   '修改工资',   '');
INSERT INTO rights VALUES('toc',      '编辑目录',   '');

INSERT INTO rights VALUES('hide',     '匿名',      '不出现在文章作者列表中');
INSERT INTO rights VALUES('nonote',   '禁用笔记',   '');
INSERT INTO rights VALUES('nowiki',   '禁编百科',   '禁止使用百科编辑器');

-- 权限或限制的集合
CREATE TABLE "right_set" ( -- 20240404
	"id"      TEXT     NOT NULL UNIQUE,
	"name"    TEXT     NOT NULL UNIQUE,      -- 中文名
	"rights"  TEXT     NOT NULL,             -- [id1 id2...] rights.id
	"comment" TEXT     NOT NULL DEFAULT '',  -- 说明
	PRIMARY KEY("id")
);

INSERT INTO right_set VALUES('all',     '超级管理员', 'admin salary note occupy license pub export',  '一切权限（限制除外）');
INSERT INTO right_set VALUES('editor',  '编辑',      'occupy license pub export',                    '审稿和协调创作');
INSERT INTO right_set VALUES('author',  '作者',      'toc',                                          '通过申请后的普通作者');
INSERT INTO right_set VALUES('guest',   '游客',      '',                                             '游客（authors.applied=0）登录编辑器后的默认权限');

-- 作者权限或限制
CREATE TABLE "author_rights" ( -- 20240403
	"author"   INTEGER NOT NULL,    -- authors.id
	"right"    TEXT    NOT NULL,    -- rights.id
	PRIMARY KEY("author", "right"),
	FOREIGN KEY("author") REFERENCES "authors"("id"),
	FOREIGN KEY("right") REFERENCES "rights"("id")
);

CREATE INDEX idx_author_rights_author ON "author_rights"("author");
CREATE INDEX idx_author_rights_right ON "author_rights"("right");

-- 工资规则
-- rule: 作者 -> 时薪
-- rule: 作者 + 协议 -> 时薪
-- rule: 作者 + 文章 -> 缩放
-- rule: 文章 -> 缩放
-- 工资 = 时长 * 时薪 * 缩放； 如果有多条适用的时薪规则，就取高的。缩放同理。
CREATE TABLE "salary" ( -- 20240403
	"id"            INTEGER NOT NULL UNIQUE,           -- 编号
	"author"        INTEGER NOT NULL DEFAULT -1,       -- 作者（-1 代表所有）
	"entry"         TEXT    NOT NULL DEFAULT '',       -- 文章（空代表所有）
	"license"       TEXT    NOT NULL DEFAULT '',       -- 协议（空代表所有）
	"begin"         TEXT    NOT NULL DEFAULT '',       -- 生效时间（空代表所有）
	"end"           TEXT    NOT NULL DEFAULT '',       -- 截止时间（空代表所有）
	"value"         REAL    NOT NULL DEFAULT -1,       -- 时薪（元）（-1 代表 NULL）
	"scale"         REAL    NOT NULL DEFAULT  1,       -- 缩放
	"comment"       TEXT    NOT NULL DEFAULT '',       -- 备注
	"creator"       INTEGER NOT NULL,                  -- 制定者
	"comment_admin" TEXT    NOT NULL DEFAULT '',       -- 备注（仅管理员可见）
	PRIMARY KEY("id" AUTOINCREMENT),
	FOREIGN KEY("author") REFERENCES "authors"("id"),
	FOREIGN KEY("entry") REFERENCES "entries"("id"),
	FOREIGN KEY("license") REFERENCES "licenses"("id"),
	FOREIGN KEY("creator") REFERENCES "authors"("id")
);

CREATE INDEX idx_salary_author ON "salary"("author");
CREATE INDEX idx_salary_entry ON "salary"("entry");
CREATE INDEX idx_salary_license ON "salary"("license");
CREATE INDEX idx_salary_begin ON "salary"("begin");
CREATE INDEX idx_salary_end ON "salary"("end");

-- 贡献调整
-- 用于作者排名或者补贴的一次性修改
CREATE TABLE "contrib_adjust" ( -- 20240407
	"id"                  INTEGER NOT NULL UNIQUE,
	"entry"               TEXT    NOT NULL,            -- 文章
	"author"              INTEGER NOT NULL,            -- 如有 authors.aka 必须使用
	"minutes"             INTEGER NOT NULL,            -- 增减的分钟数
	"adjust_salary"       INTEGER NOT NULL DEFAULT 0,  -- [0|1] 是否计入补贴
	"adjust_author_list"  INTEGER NOT NULL DEFAULT 0,  -- [0|1] 是否影响作者列表排名
	"time"                TEXT    NOT NULL,            -- 何时做出贡献
	"reason"              TEXT    NOT NULL DEFAULT '', -- 贡献内容（可以由申请者填写）
	"approved"            INTEGER NOT NULL DEFAULT -1, -- 批准者id，-1 表示未批准
	"comment2"            TEXT    NOT NULL DEFAULT '', -- 备注（仅管理员可见）
	PRIMARY KEY("id" AUTOINCREMENT),
	FOREIGN KEY("entry")  REFERENCES "entries"("id"),
	FOREIGN KEY("author")  REFERENCES "authors"("id")
);

CREATE INDEX idx_contrib_adjust_entry ON "contrib_adjust"("entry");
CREATE INDEX idx_contrib_adjust_author ON "contrib_adjust"("author");
CREATE INDEX idx_contrib_adjust_time ON "contrib_adjust"("time");


----------------------------------------------------------------------------------------------------------
-- 参考文献
----------------------------------------------------------------------------------------------------------

-- 文献
CREATE TABLE "bibliography" ( -- 20240617
	"id"        TEXT NOT NULL UNIQUE,         -- \cite{xxx} 中的 xxx
	"order"     INTEGER NOT NULL,             -- 编号（bibliography.tex 中的顺序，从 1 开始）
	"title"     TEXT NOT NULL DEFAULT '',     -- 标题
	"type"      TEXT NOT NULL DEFAULT '',     -- 类型
	"date"      TEXT NOT NULL DEFAULT '',     -- 发表/出版日期
	"details"	TEXT NOT NULL,                -- 显示文字
	PRIMARY KEY("id"),
	FOREIGN KEY("type") REFERENCES "bib_type"("id")
);

INSERT INTO "bibliography" VALUES ('', 0, '无', '', '', ''); -- 防止 FOREIGN KEY 报错

-- 文献类型
CREATE TABLE "bib_type" ( -- 20240403
	"id"       TEXT NOT NULL UNIQUE,
	PRIMARY KEY("id")
);

INSERT INTO "bib_type" VALUES (''); -- 防止 FOREIGN KEY 报错

-- 文献 DOI
CREATE TABLE "bib_doi" ( -- 20240403
	"bib" TEXT NOT NULL UNIQUE,
	"doi" TEXT NOT NULL UNIQUE,
	PRIMARY KEY("bib"),
	FOREIGN KEY("bib") REFERENCES "bibliography"("id")
);

CREATE INDEX idx_bib_doi_doi ON "bib_doi"("doi");

-- 文献 url （如果有 url 是 doi 的，就不需要）
CREATE TABLE "bib_url" ( -- 20240403
	"bib" TEXT NOT NULL UNIQUE,
	"url" TEXT NOT NULL UNIQUE,
	PRIMARY KEY("bib"),
	FOREIGN KEY("bib") REFERENCES "bibliography"("id")
);

CREATE INDEX idx_bib_url_url ON "bib_url"("url");

-- 文献所有杂志
CREATE TABLE "journals" ( -- 20240403
	"id"      TEXT NOT NULL UNIQUE,
	"title"   TEXT NOT NULL,
	PRIMARY KEY("id")
);

CREATE INDEX idx_journals_title ON "journals"("title");

-- 文献的杂志
CREATE TABLE "bib_journal" ( -- 20240403
	"bib"     TEXT    NOT NULL UNIQUE,
	"journal" TEXT    NOT NULL,
	"volume"  INTEGER NOT NULL DEFAULT -1,
	"issue"   INTEGER NOT NULL DEFAULT -1,
	PRIMARY KEY("bib"),
	FOREIGN KEY("bib") REFERENCES "bibliography"("id"),
	FOREIGN KEY("journal") REFERENCES "journals"("id")
);

CREATE INDEX idx_bib_journal_journal ON "bib_journal"("journal");

-- 文献所有标签
CREATE TABLE "bib_all_tags" ( -- 20240403
	"id"      TEXT NOT NULL UNIQUE,
	"name"    TEXT NOT NULL DEFAULT '',
	"comment" TEXT NOT NULL DEFAULT '',
	PRIMARY KEY("id")
);

-- 文献的标签
CREATE TABLE "bib_tags" ( -- 20240403
	"bib"     TEXT NOT NULL UNIQUE,
	"tag"     TEXT NOT NULL,
	FOREIGN KEY("bib") REFERENCES "bibliography"("id"),
	FOREIGN KEY("tag") REFERENCES "bib_all_tags"("id")
);

CREATE INDEX idx_bib_tags_tag ON "bib_tags"("tag");

-- 文献引用关系
CREATE TABLE "bib_cite" ( -- 20240403
	"bib"  TEXT NOT NULL,
	"cite" TEXT NOT NULL,
	PRIMARY KEY("bib", "cite"),
	FOREIGN KEY("bib") REFERENCES "bibliography"("id"),
	FOREIGN KEY("cite") REFERENCES "bibliography"("id")
);

CREATE INDEX idx_bib_cite_cite ON "bib_cite"("cite");

-- 所有文献作者
CREATE TABLE "bib_all_authors" ( -- 20240403
	"id"         INTEGER NOT NULL UNIQUE,
	"first_name" TEXT NOT NULL DEFAULT '',  -- 名
	"last_name"  TEXT NOT NULL DEFAULT '',  -- 姓
	PRIMARY KEY("id" AUTOINCREMENT)
);

CREATE INDEX idx_bib_all_authors_first_name ON "bib_all_authors"("first_name");
CREATE INDEX idx_bib_all_authors_last_name ON "bib_all_authors"("last_name");

-- 文献作者
CREATE TABLE "bib_authors" ( -- 20240403
	"bib"     TEXT     NOT NULL,
	"author"  INTEGER  NOT NULL,
	"order"   INTEGER  NOT NULL,   -- 第几作者
	PRIMARY KEY("bib", "author"),
	FOREIGN KEY("bib") REFERENCES "bibliography"("id"),
	FOREIGN KEY("author") REFERENCES "bib_all_authors"("id")
);

CREATE INDEX idx_bib_authors_author ON "bib_authors"("author");
