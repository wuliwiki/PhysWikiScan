# PhysWikiScan
PhysWikiScan 是小时百科 wuli.wiki 文章编辑器后台负责把 latex 转换为 html 的程序。 一般需要另外一个仓库 `PhysWiki`， 一般和 `littleshi.cn` 和 `PhysWiki` （非开源）仓库放在同一个目录。

## 使用说明

**设置路径**
* 在 `set_path.txt` 中设置输入路径和输出路径（一般不用设置）。
* 输入路径可以是 `PhysWiki` 文件夹， 只会读取所有 `.tex` 文件。 `main.tex` 文件用于生成目录。
* 在任何命令最后放 `--path 数字` 可以指定 `set_path.txt` 中的某套路径和 url 设置。
* 若不指定 `--path` 则默认使用 `--path 0`。
* 在命令最后放 `--path "用户名"` 或者 `用户名/changed` 可以指定某个用户笔记文件夹（`../user-notes/用户名/changed`）， 使用 `用户名/online` 则可以把子目录 `/changed` 改为 `/online`（发布）。 最后可以加一个 `/` 也可以不加。
* 也可以通过 `--path-in-out-data-url 输入路径 输出路径 数据文件路径 url` 来指定输入，输出路径，和数据文件路径，网页所在 url 路径（如 `http://wuli.wiki/online/`， 也可以为空， 用于文章，公式，图片，脚注等链接）。 该选项必须出现在命令最后， 不能和 `--path` 混用。
* 路径可以是绝对路径或相对路径（相对于当前路径）

**日常命令**
* 如果不输入任何参数， 程序运行后会提示从命令行输入参数。
* `PhysWikiScan --version` 显示版本号
* `PhysWikiScan .` 把全部 `contents/*.tex` 文件转换为 `online/*.html`， 并使用 `main.tex` 生成完整目录 `index.html`。 `main.tex` 中不存在的文章也会被转换（将警告）。
* `PhysWikiScan --titles`: 扫描所有 tex 文件， 扫描 `main.tex`， 更新数据库。
* `PhysWikiScan --toc`: 生成完整目录 `index.html`， 更新数据库。
* `PhysWikiScan --entry 文章1 文章2 ...`：转换 `contents` 中指定的文章 `文章1.tex 文章2.tex ...`。
* `PhysWikiScan --autoref 文章 eq 8` 查找 `contents/文章.tex` 文章的网页公式序号 `8` 是否定义了 `\label{xxx}`。 如果 label 不存在， 就试图对被引用的公式插入唯一的 `\label{eq_文章_*}`， 把 label `xxx` 输出到命令行再另起一行输出 `added`， 更新数据库。 如果 label 已经存在， 就直接把 label 输出到命令行且输出 `exist`。 该功能一般被编辑器的 “引用” 按钮调用。
* `PhysWikiScan --autoref-dry 文章 eq 8` 和上一条一模一样， 除了不会真的给文章文件添加 label。
* `PhysWikiScan --backup 文章 作者id` 把 `contents/文章.tex` 备份到 `../PhysWiki-backup/YYYYMMDDHHMM_作者id_文章.tex`， 若 hash 已经在数据库中则不备份并输出 `exist 已存在的文件名 history.id`。 同一文章同一作者，若距离上次备份超过 30 分钟，则使用当前时间， 否则增加到上次备份时间加五分钟的整数倍，若已经存在，则覆盖。 如果新增了文件和记录，就会在 stdout 输出 `added 文件名 history.id`； 若替换了文件和记录，就会输出 `replaced 文件名 history.id`。
* `PhysWikiScan --bib` 通过 `bibliography.tex` 生成文献列表 `bibliography.html`， 更新数据库。
* `PhysWikiScan --delete 文章1 文章2 ...` 相当于先把文章除了前两行的注释外的内容都清空，编译一次（如果其定义的标签等被引用，就会报错）。 然后检查是否文章本身被别处 `\upref`， 如果有就报错。 确保和最后一次备份的 hash 相同， 否则就增加一个备份。 最后更新 `entries.deleted`， 删除文章文件。
* `PhysWikiScan --delete-hard 文章1 文章2 ...` 如果文章没有标记删除就先使用 `--delete 文章`， 然后删除指定文章所有数据和相关文件和备份（在多个文章之间共享的除外）。
* `PhysWikiScan --delete-figure 图片1 图片2 ...` 删除数据库中已经被标记 `figures.deleted` 的图片以及所有相关 `images` 和图片文件。

**批量修改文章文件**
* `PhysWikiScan --inline-eq-space` 批量把 tex 文件中的行内公式两边添加空格（如果是中文）（有少量 bug，碰到会自动跳过该文章）
* `PhysWikiScan ,` 批量把正文中汉字两边的英文标点变为中文（有少量 bug，碰到会跳过该文章）
* `PhysWikiScan --check-url 文章1 文章2 ...` 会转换输出路径中 `文章1.html 文章2.html` 的所有 `http` 开头的链接是否可以正常访问。 含 `wikipedia.org` 的除外。 如果不输入 `文章1 文章2 ...` 则检查输出目录中的所有 `html` 文件。
* `PhysWikiScan --check-url-from 文章1` 检查输出目录的所有 `html` 文件， 但从 `文章1` 开始。
* 【暂不使用】`PhysWikiScan --hide 文章` 把文章中的公式和代码全部替换成 `$四位编号$` 以及 `\verb|四位编号|`, 防止 google 翻译改写。 数据写入 `eq_list.txt` 和 `verb_list.txt`， 编号对应 txt 中的行号（从 0 开始）
* 【暂不使用】`PhysWikiScan --unhide 文章` 把文章中的公式和代码恢复。

***统计与数据库更新***
* `PhysWikiScan --wc` 统计中文字符数（含标点）
* `PhysWikiScan --history-all` 把 `../PhysWiki-backup/*.tex` 备份文件信息更新到数据库。 更新 `entries.authors`， `history.last`， 未计算的 `history.add/del`。 若在 `--history-all` 后面加一个任意参数， 则重新计算所有 `history.add/del`。
* `PhysWikiScan --stat yyyymmddhhmm yyyymmddhhmm` 统计一段时间内所有作者的贡献详情
* `PhysWikiScan --author-char-stat yyyymmddhhmm yyyymmddhhmm 用户名` 统计某个作者在某段时间内（包括）的字符数增减（数据库 history.add/del）
* `PhysWikiScan --history-normalize` 有时候编辑器不到 5 分钟就会产生备份（并非 bug）， 该功能把备份删除或重命名以模拟 5 分钟备份， 半小时不编辑重新开始计时。
* `PhysWikiScan --fix-db` 重新生成数据库中标记为【生成】的数据（用于 debug）。
* `PhysWikiScan --migrate-db /path/data1.db /path/data2.db` 用于把某个旧的数据库迁移到新格式的数据库中， `data2.db` 将被覆盖。
* `PhysWikiScan --migrate-user-db` 用于把 `../user-notes/用户名/cmd_data/scan.db` 转换为 `../user-notes/note-template/cmd_data/scan.db` 的格式。
* `PhysWikiScan --all-users` 编译所有用户的笔记， 输出到用户的 `changed` 文件夹。 用 `--all-users-online` 输出到 `online` 文件夹。
* 脚本 `db_dump.sh` 用于备份数据库， `db_restore.sh` 用于恢复（需要安装 `sqlite3`）。
* 程序只会输出 `html` 和图片文件到输出路径（覆盖同名文件）， 不会改变其他文件。
* 程序默认输出到 `stdout`， 如果有错误， 会输出到 `stderr`。

## 编译
* 支持 g++8.3 及以上版本编译器
* 在 linux 系统中（g++ 编译器）， 用 `make` 编译， `make clean` 清空编译产生的文件。
* 也可以直接用 `cmake`， 默认选项即可。
* 可以用 CLion 导入 CMakeLists.txt 编译调试。
* `SLISC/` 中的代码来自 [SLISC](https://github.com/MacroUniverse/SLISC) 仓库。

## 已知 BUG
* `\begin{example}{}` 或类似环境下面如果多空一行， html 也会空间过大
* `findNormalText()` 函数不应该包含 subtitle，subsubtitle 等
* `\eentry` 和 `\rentry` 没有处理
* 正文中禁止 \\ 换行，以及其他禁止的格式如 `\noindent`， 用户笔记不禁止
* `\textbackslash` 后面需要删除一个空格（如果有）
* 例题和习题中的标题仍然是 `<h5>`

## 待办
* 主要目标是最终完全不需要使用 `./PhysWikiScan .` 命令， 每次用 `--entry` 都更新数据库中所有必要的数据。
* 不要试图追踪错误的行号！太难了，不值得。 用内容定位足够了。（浪费了一天）
* 原来的 `PhysWikiCheck()` 去哪了？
* 代码块应该也像公式一样可以拖动， 而且需要可以上下拖动。 每行长度不应该有限制（研究一下 pdf 里面的代码块能否自动换行）。
* 检查文章名， `\subsection{}` 和 `\subsubsection{}` 中是否有空格， 提示应该替换成 `\ `
* 想想怎样编译历史版本或已删除文章。
