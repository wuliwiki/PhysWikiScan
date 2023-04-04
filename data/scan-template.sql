-- 小时百科在线编辑器数据库
-- 为方便编程， 所有数据都必须 NOT NULL

-- 所有词条
-- 目录中的 \entry{标题}{xxx} 中 xxx 为 "id"
CREATE TABLE "entries" (
	"id"	TEXT UNIQUE NOT NULL,
	"caption"	TEXT NOT NULL DEFAULT '', -- 标题
	"authors"	TEXT NOT NULL DEFAULT '待更新', -- "id1 id2 id3" 作者 ID
	
	"part"	TEXT NOT NULL DEFAULT '', -- 部分
	"chapter"	TEXT NOT NULL DEFAULT '', -- 章
	"order"	INTEGER NOT NULL DEFAULT 0, -- 目录中出现的顺序（从 1 开始，全局编号，0 代表不在目录中）

	-- [CC] CC BY-SA 3.0 [Xiao] 小时科技版权 [Use] 作者版权和署名权，百科永久使用和修改权
	-- [Copy] 经作者同意转载（不可编辑） [Deri] 经作者同意转载（可编辑）
	"license"	TEXT NOT NULL DEFAULT '',

	-- [Wiki] 百科（综述） [Map] 导航 [Tutor] 教程（类似教材） [Art] 文章（类似论文） [Note] 笔记总结
	"type"  TEXT NOT NULL DEFAULT '',

	"keys" TEXT NOT NULL DEFAULT '', -- "关键词1|...|关键词N"
	"pentry"	TEXT NOT NULL DEFAULT '', -- "entry1 entry2 entry3" 预备知识的词条标签

	"draft"	INTEGER NOT NULL DEFAULT 2, -- [0|1|2] 是否草稿（词条是否标记 \issueDraft， 2 代表未知）
	"issues"	TEXT NOT NULL DEFAULT '', -- "XXX XXX XXX" 其中 XXX 是 \issueXXX 中的， 不包括 \issueDraft
	"issueOther"	TEXT NOT NULL DEFAULT '', -- \issueOther{} 中的文字

	"deleted"	INTEGER NOT NULL DEFAULT 0, -- [0|1] 是否已删除
	"occupied"	INTEGER NOT NULL DEFAULT -1, -- [-1|authorID] 是否正在被占用（审核发布后解除）

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
	"chap_first"	TEXT NOT NULL, -- 第一章
	"chap_last"	INTEGER NOT NULL, -- 最后一章
	PRIMARY KEY("id"),
	FOREIGN KEY("chap_first") REFERENCES "chapters"("id"),
	FOREIGN KEY("chap_last") REFERENCES "chapters"("id")
);

-- 防止 FOREIGN KEY 报错
INSERT INTO "parts" VALUES('', 0, '无', '', '');

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

-- 带标签的公式
-- \label{eq_xxx} 中 xxx 为 "id"
-- 老格式为 \label{xxx_eq#}， 转换时 xxx# 为 "id"（在 html 仍然需要添加老 <id> 保持兼容性），下同
CREATE TABLE "equations" (
	"id"	TEXT UNIQUE NOT NULL, -- 重要公式手动起名规则和 entry 一样， 其他的自动编号
	"entry"	TEXT NOT NULL, -- 所在词条
	"order"	INTEGER NOT NULL, -- 显示编号
	"ref_by"	TEXT NOT NULL, -- "entry1 entry2" 引用的词条（包括本词条）， 只有为空才能删除该标签， 下同
	PRIMARY KEY("id"),
	FOREIGN KEY("entry") REFERENCES "entries"("id")
);

-- 图片环境（必须带标签）
-- \label{fig_xxx} 中 xxx 为 "id"
CREATE TABLE "figures" (
	"id"	TEXT UNIQUE NOT NULL,
	"caption"	TEXT NOT NULL DEFAULT '', -- 标题 \caption{xxx}
	"entry"	TEXT NOT NULL, -- 所在词条
	"order"	INTEGER NOT NULL, -- 显示编号
	"hash"	TEXT NOT NULL, -- 文件 SHA1 前 16 位， 多个 id 可以使用同一个 hash 共用一个文件（svg 和 pdf 同名的，使用后者）
	"source"	TEXT NOT NULL DEFAULT '', -- 来源（如果非原创）
	"ref_by"	TEXT NOT NULL DEFAULT '', -- "entry1 entry2" 引用的词条
	"deleted"	INTEGER NOT NULL DEFAULT 0, -- [0|1] 该环境是否已删除
	PRIMARY KEY("id"),
	FOREIGN KEY("entry") REFERENCES "entries"("id"),
	FOREIGN KEY("hash") REFERENCES "figure_files"("hash")
);

-- 图片文件
-- 图片文件名为 "hash"."ext"
CREATE TABLE "figure_store" (
	"hash"	TEXT UNIQUE NOT NULL, -- SHA1 的前 16 位
	"ext"	TEXT UNIQUE NOT NULL, -- [png|svg] 文件类型（svg 需要有同名 pdf）， 文件使用 id
	"use_count"	INTEGER NOT NULL, -- 被多少个 "figures" 使用， 只有为 0 时可以被删除
	"author"	INTEGER NOT NULL, -- 当前版本修改者
	"right"	TEXT NOT NULL, -- [Xiao|CC|ask] 当前版本协议
	"time"	TEXT NOT NULL, -- 上传时间
	"files"	TEXT NOT NULL, -- "hash1 hash2"， 附件（创作该图片的项目文件、源码等）
	PRIMARY KEY("hash"),
	FOREIGN KEY("author") REFERENCES "authors"("id")
);

-- 文件（词条附件）
CREATE TABLE "files" (
	"id"	TEXT UNIQUE NOT NULL,
	"hash"	TEXT UNIQUE NOT NULL,
	"ref_by"	TEXT NOT NULL, -- "entry1 entry2" 引用的词条
	"description"	TEXT UNIQUE NOT NULL, -- 描述
	PRIMARY KEY("id"),
	FOREIGN KEY("hash") REFERENCES "files"("id")
);

-- 文件（被 "files" 和 "figure_store" 使用的）
CREATE TABLE "file_store" (
	"hash"	TEXT UNIQUE NOT NULL, -- MD5
	"name"	TEXT UNIQUE NOT NULL, -- 文件名（含拓展名）
	"use_count"	INTEGER NOT NULL, -- 被使用多少次
	"used"	INTEGER NOT NULL, -- [0|1] 是否被使用
	"author"	INTEGER NOT NULL, -- 当前版本修改者
	"right"	TEXT NOT NULL, -- [Xiao|CC|ask] 当前版本协议
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
	"right"	TEXT NOT NULL, -- [orig|CC|ask] 版权
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
	"entry"	TEXT NOT NULL, -- 所在词条
	"order"	INTEGER NOT NULL, -- 显示编号
	"ref_by"	TEXT NOT NULL DEFAULT '', -- "entry1 entry2" 被哪些词条引用
	PRIMARY KEY("id"),
	FOREIGN KEY("entry") REFERENCES "entries"("id"),
	UNIQUE("type", "entry", "order")
);

-- 参考文献
CREATE TABLE "bibliography" (
	"bib"	TEXT UNIQUE NOT NULL, -- \cite{} 中的标签
	"details"	TEXT NOT NULL, -- 详细信息（待拆分成若干field）
	PRIMARY KEY("bib")
);

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
	"time"	TEXT NOT NULL, -- 审稿提交时间
	"refID"	INTEGER NOT NULL, -- 审稿人 ID
	"entry"	TEXT NOT NULL, -- 词条
	"author"	INTEGER NOT NULL, -- 作者
	"action"	TEXT NOT NULL DEFAULT '', -- [Pub] 发布 [Udo] 撤回 [Fix] 继续完善
	"comment"	TEXT NOT NULL DEFAULT '', -- 意见（也可以直接修改正文或在正文中评论）
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
	PRIMARY KEY("id" AUTOINCREMENT)
);

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
