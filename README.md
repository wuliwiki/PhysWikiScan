## 使用说明
* 目前用 g++8.3 编译（g++7 在 -O3 优化下有遇到 bug）
* 在 `set_path.txt` 中设置输入路径和输出路径。
* 输入路径可以是 `PhysWiki` 文件夹， 只会读取所有 `tex` 文件和 `m` 文件。 `PhysWiki.tex` 文件用于生成目录。
* 程序默认使用 `set_path.txt` 的路径 0 （即 `--path 0`）， 在线编辑器需要使用路径 1 （`--path 1`）， 这个选项必须在最后
* 也可以通过 `--path-in-out-data-url 输入路径 输出路径 数据文件路径 url` 来指定输入，输出路径，和数据文件路径， 网页所在 url（如 `http://wuli.wiki/online/`， 也可以为空， 用于词条，公式，图片，脚注等链接）。 该选项必须出现在命令最后
* 使用 `--clean` 清空 `changed.txt` 和 `authors.txt`
* 路径可以是绝对路径或相对路径（相对于当前路径）
* 程序只会输出 `html` 文件到输出路径（覆盖同名文件）， 不会改变其他文件。
* 输出路径中需要有所有 `svg` 或 `png` 图片。
* `PhysWiki.tex` 中不存在的词条也会被 `PhysWikiScan .` 命令转换。 这些词条在运行的时候会提示 warning。
* 在 linux 系统中（g++ 编译器）， 用 `make` 编译， `make clean` 清空编译产生的文件。
* 程序默认输出到 `stdout`， 如果有错误， 会输出到 `stderr`。

## Visual Studio 编译
* 目前除 Matlab 代码的高亮需要调用 linux 命令行的 gnu source-highlight 项目， 所以在 Windows 下这些代码不会被高亮。
* 在任意版本 Visual Studio 中（需要支持 c++17）， 新建 empty project， 设置 c++17 标准（因为要使用 filesystem 头文件）， 设置 64 位编译即可。 目前使用 Visual Studio 2019 测试成功。

## PhysWikiScan 所有控制行命令
* 如果不输入任何 argument， 程序运行后会提示从 stdin 输入 arguments
* `PhysWikiScan .` 全部 tex 转换为 html， 并生成完整目录 `index.html`， 生成 `entries.txt`, `titles.txt`， `ids.txt`， `labels.txt`
* `PhysWikiScan --titles`: 只更新 `entries.txt` 和 `titles.txt`
* `PhysWikiScan --toc`: 生成完整目录 `index.html`
* `PhysWikiScan --entry fname1 fname2 ...`：指定要转换的词条， 不更新目录, 更新 `ids.txt` 和 `labels.txt` （必须已经存在）
* `PhysWikiScan --autoref fname eq 8` 查找 `fname.tex` 词条的网页公式序号 (8) 是否存在 label。 如果 label 不存在， 就试图对被引用的公式插入唯一的 `\label{fname_eq#}`， 把插入的 label 保存到 `autoref.txt` 第 1 行， 第 2 行为 `added`， 更新 `ids.tex` 和 `labels.tex` （必须已经存在）。 如果 label 已经存在， 就直接把 label 保存到 `autoref.txt` 第 1 行， 第 2 行为 `exist`。 
* `PhysWikiScan --bib` 生成文献列表

## 开发笔记
* 如果要 debug 某一个文件的编译， 在 "PhysWikiScan.h" 中搜索 "one file debug"， 输入文件名设置 break point 即可。
* 每次 commit 以前编译一个 release 版本， 将 x64/Release/PhysWikiScan.exe 拷贝到 PhysWikiScan 目录下， 测试并写上日期。

## BUG
* \begin{example}{} 或类似环境下面如果多空一行， html 也会空间过大
* findNormalText 函数不应该包含subtitle，subsubtitle 等
* 处理 bug 的截图
* Matlab 小括号里面的 end 不高亮
* `\eentry` 和 `\rentry` 没有处理
* 正文中禁止 \\ 换行，以及其他禁止的格式如 `\noindent`
* `\textbackslash` 后面需要删除一个空格（如果有）
* 例题和习题中的标题仍然是 `<h5>`
* 编译 `\lstinline|\begin|` 会出错， 最好的解决办法应该是先把 `\lstinline||` 里面的内容储存起来， 最后再粘贴回去
* 中文字符间的英文两边有无空格都不应该影响排版
* 代码中的 `<` 和 `>` 不会被替换为 `&lt` 和 `&gt`
* 蓝色小标题和黑色小标题中不能用 `\ ` 作为空格。

## New Features
* 代码高亮考虑使用 `PrismJS` 高亮代码， 支持许多其他语言
* 自动通过 `lstlisting` 环境生成代码文件（只生成有文件名的代码）
* 保证 aligned 环境里面不能有空行（equation 环境也不行吗？）
* 注意微软雅黑是收费的！ 用免费字体（方正的？）
* 每个页面下面的 “编辑词条” 链接直接打开该词条， 如 `wuli.wiki/editor/?entry=AMAdd`
* 在 `dep.json` 文件中添加章节信息， 给每个部分显示为不同颜色
* `\autoref{}` 外部引用时程序要检查 `\upref{}` 是否存在， 否则报错。
* 正文中不需要有空格（尤其是文字和符号之间）， 除了 `\autoref{}` 前面不加空格后面要。
* PhysWikiCheck 检查 latex 代码中 `\autoref{}` 前面是否没有空格而后面有
* 正文中的英文逗号句号啥的在 PhysWikiCheck 中检查（是不是已经有这个功能了）
* PhysWikiCheck 使用命令行选项调用
* 代码块应该也像公式一样可以拖动， 而且需要可以上下拖动。 每行长度不应该有限制。
* 检查词条名， `\subsection{}` 和 `\subsubsection{}` 中是否有空格， 提示应该替换成 `\ `
