# PhysWikiScan
PhysWikiScan 是小时百科 wuli.wiki 词条编辑器后台负责把 latex 转换为 html 的程序。 一般需要另外一个仓库 `PhysWiki`， 一般和 `littleshi.cn` 和 `PhysWiki` （非开源）仓库放在同一个目录。

## 使用说明
* 如果不输入任何参数， 程序运行后会提示从命令行输入参数。
* `PhysWikiScan .` 把全部 `contents/*.tex` 文件转换为 `online/*.html`， 并使用 `main.tex` 生成完整目录 `index.html`。 `main.tex` 中不存在的词条也会被转换（将警告）。
* `PhysWikiScan --titles`: 扫描所有 tex 文件， 扫描 `main.tex`， 更新数据库。
* `PhysWikiScan --toc`: 生成完整目录 `index.html`， 更新数据库。
* `PhysWikiScan --entry 词条1 词条2 ...`：转换 `contents` 中指定的词条 `词条1.tex 词条2.tex ...`。
* `PhysWikiScan --autoref 词条 eq 8` 查找 `contents/词条.tex` 词条的网页公式序号 `8` 是否定义了 `\label{xxx}`。 如果 label 不存在， 就试图对被引用的公式插入唯一的 `\label{eq_词条_*}`， 把 label `xxx` 输出到命令行再另起一行输出 `added`， 更新数据库。 如果 label 已经存在， 就直接把 label 输出到命令行且输出 `exist`。 该功能一般被编辑器的 “引用” 按钮调用。
* `PhysWikiScan --autoref-dry fname eq 8` 和上一条一模一样， 除了不会真的给词条文件添加 label。
* `PhysWikiScan --bib` 通过 `bibliography.tex` 生成文献列表 `bibliography.html`， 更新数据库。
* `PhysWikiScan --hide fname` 把词条中的公式和代码全部替换成 `$四位编号$` 以及 `\verb|四位编号|`, 防止 google 翻译改写。 数据写入 `eq_list.txt` 和 `verb_list.txt`， 编号对应 txt 中的行号（从 0 开始）
* `PhysWikiScan --unhide fname` 把词条中的公式和代码恢复。
* `PhysWikiScan --inline-eq-space` 批量把 tex 文件中的行内公式两边添加空格（如果是中文）（有少量 bug，碰到会自动跳过该词条）
* `PhysWikiScan ,` 批量把正文中汉字两边的英文标点变为中文（有少量 bug，碰到会跳过该词条）
* `PhysWikiScan --wc` 统计中文字符数（含标点）
* `PhysWikiScan --history-all` 把 `../PhysWiki-backup/*.tex` 备份文件信息更新到数据库。 更新 `entries.authors`， `history.last`， 未计算的 `history.add/del`。 若在 `--history-all` 后面加一个任意参数， 则重新计算所有 `history.add/del`。
* `PhysWikiScan --author-char-stat yyyymmddhhmm yyyymmddhhmm 用户名` 统计某个作者在某段时间内（包括）的字符数增减（数据库 history.add/del）
* `PhysWikiScan --fix-db` 重新生成数据库中标记为【生成】的数据（用于 debug）。
* `PhysWikiScan --migrate-db /path/data1.db /path/data2.db` 用于把某个旧的数据库迁移到新格式的数据库中， `data2.db` 将被覆盖。
* `PhysWikiScan --migrate-user-db` 用于把 `../user-notes/用户名/cmd_data/scan.db` 转换为 `../user-notes/note-template/cmd_data/scan.db` 的格式。
* `PhysWikiScan --all-users` 编译所有用户的笔记， 输出到 `changed` 文件夹。 用 `--all-users-online` 输出到 `online` 文件夹。
* `PhysWikiScan --history-normalize` 有时候编辑器不到 5 分钟就会产生备份（并非 bug）， 该功能把备份删除或重命名以模拟 5 分钟备份， 半小时不编辑重新开始计时。
* 脚本 `db_dump.sh` 用于备份数据库， `db_restore.sh` 用于恢复（需要安装 `sqlite3`）。
* 在 `set_path.txt` 中设置输入路径和输出路径。
* 输入路径可以是 `PhysWiki` 文件夹， 只会读取所有 `tex` 文件和 `m` 文件。 `main.tex` 文件用于生成目录。
* 在命令最后放 `--path 数字` 可以指定 `set_path.txt` 中的某套路径和 url 设置。
* 在命令最后放 `--path "用户名"` 或者 `用户名/changed` 可以指定某个用户笔记文件夹（`../user-notes/用户名/changed`）， 使用 `用户名/online` 则可以把子目录 `/changed` 改为 `/online`（发布）。 最后可以加一个 `/` 也可以不加。
* 若不指定 `--path` 则默认使用 `--path 0`。
* 也可以通过 `--path-in-out-data-url 输入路径 输出路径 数据文件路径 url` 来指定输入，输出路径，和数据文件路径，网页所在 url 路径（如 `http://wuli.wiki/online/`， 也可以为空， 用于词条，公式，图片，脚注等链接）。 该选项必须出现在命令最后， 不能和 `--path` 混用。
* 路径可以是绝对路径或相对路径（相对于当前路径）
* 程序只会输出 `html` 和图片文件到输出路径（覆盖同名文件）， 不会改变其他文件。
* 程序默认输出到 `stdout`， 如果有错误， 会输出到 `stderr`。

## 编译
* 支持 g++8.3 及以上版本编译器
* 在 linux 系统中（g++ 编译器）， 用 `make` 编译， `make clean` 清空编译产生的文件。
* 可以用 CLion 导入 CMakeLists.txt 编译调试。
* `SLISC/` 中的代码来自 [SLISC](https://github.com/MacroUniverse/SLISC) 仓库。

## BUG
* `\begin{example}{}` 或类似环境下面如果多空一行， html 也会空间过大
* `findNormalText()` 函数不应该包含 subtitle，subsubtitle 等
* `\eentry` 和 `\rentry` 没有处理
* 正文中禁止 \\ 换行，以及其他禁止的格式如 `\noindent`， 用户笔记不禁止
* `\textbackslash` 后面需要删除一个空格（如果有）
* 例题和习题中的标题仍然是 `<h5>`

## New Features
* 原来的 `PhysWikiCheck()` 去哪了？
* 代码块应该也像公式一样可以拖动， 而且需要可以上下拖动。 每行长度不应该有限制。
* 检查词条名， `\subsection{}` 和 `\subsubsection{}` 中是否有空格， 提示应该替换成 `\ `
* 不要试图追踪错误的行号！太难了，不值得。 用内容定位足够了。（浪费了一天）
* 目标是最终完全不需要使用 `./PhysWikiScan .` 命令， 每次用 `--entry` 都更新数据库中所有必要的数据。
* 添加命令 `--delete 词条id` 只有满足以下条件的词条可以删除（否则提示为什么不能删除）： `entries.ref_by` 为空， `figures` 对应的 `figures.ref_by` 为空， `labels` 对应的 `labels.ref_by` 为空。 删除后更新 `entries.deleted`。
* 统计字数增减时， 应该一个英文单词或者一个 latex 命令或者一个汉字或标点作为一个增减。
