## 使用说明
* 在 `set_path.txt` 中设置输入路径和输出路径。
* 输入路径可以是 `PhysWiki` 文件夹， 只会读取所有 `tex` 文件和 `m` 文件。 `PhysWiki.tex` 文件用于生成目录。
* 程序默认使用 `set_path.txt` 的路径 0 （即 `--path 0`）， 网页编辑器需要使用路径 1 （`--path 1`）， 这个选项必须在最后
* 程序只会输出 `html` 文件到输出路径（覆盖同名文件）， 不会改变其他文件。
* 输出路径中需要有所有 `svg` 或 `png` 图片。
* 如果不想调试代码， Windows 下直接运行 `PhysWikiScan.exe` 即可（无需安装 Visual Studio）, 但是无法设置路径。
* `PhysWiki.tex` 中不存在的词条也会被 `PhysWikiScan .` 命令转换。 这些词条在运行的时候会提示 warning。
* 在 linux 系统中（g++ 编译器）， 用 `make` 编译， `make clean` 清空编译产生的文件。
* 程序默认输出到 `stdout`， 如果有错误， 会输出到 `stderr`。

## PhysWikiScan 所有控制行命令
* 如果不输入任何 argument， 程序运行后会提示从 stdin 输入 arguments
* `PhysWikiScan .` 全部 tex 转换为 html， 并生成完整目录 `index.html`， 生成 `entries.txt`, `titles.txt`， `ids.txt`， `labels.txt`
* `PhysWikiScan --titles`: 只更新 `entries.txt` 和 `titles.txt`
* `PhysWikiScan --toc`: 生成完整目录 `index.html`
* `PhysWikiScan --toc-changed`：生成目录 `changed.html`， 只含有 `changed.txt` 中列出的词条
* `PhysWikiScan --entry fname1 fname2 ...`：指定要转换的词条， 不更新目录, 更新 `ids.txt` 和 `labels.txt` （必须已经存在）
* `PhysWikiScan --autoref fname eq 8` 查找 `fname.tex` 词条的网页公式序号 (8) 是否存在 label。 如果 label 不存在， 就试图对被引用的公式插入唯一的 `\label{fname_eq#}`， 把插入的 label 保存到 `autoref.txt` 第 1 行， 第 2 行为 `added`， 更新 `ids.tex` 和 `labels.tex` （必须已经存在）。 如果 label 已经存在， 就直接把 label 保存到 `autoref.txt` 第 1 行， 第 2 行为 `exist`。 

## 开发笔记
* 如果想用 Visual Studio 2017 调试代码， 打开 PhysWikiScan.sln， 按 F5 即可编译并运行。
* 如果要 debug 某一个文件的编译， 在 "PhysWikiScan.h" 中搜索 "one file debug"， 输入文件名设置 break point 即可。
* 每次 commit 以前编译一个 release 版本， 将 x64/Release/PhysWikiScan.exe 拷贝到 PhysWikiScan 目录下， 测试并写上日期。

## BUG
* `.` 模式下运行， 如果重试， `dep.json` 就会生成重复的 link。 解决方法： 不要在 `PhysWikiOnline1()` 里面读取 `pentry()`， 而是专门搞一个 `loop` 来专门读取每个文件中的 `pentry()`。 这样又可以加一个单独的选项 `PhysWiki --tree`。 `PhysWiki .` 需要包含 `PhysWiki --tree` 功能
* table 没有标题
* 文献引用还没有处理
* 处理 bug 的截图
* Matlab 代码块不会自动换行， 因为是 verbatim
* `end` keyword 用于 slicing 的时候不能高亮， html 和 lstlisting 都有这个问题
* 处理 bibliographies
* `\eentry` 和 `\rentry` 没有处理
* 看看能不能根据预备知识给每个词条生成一个学习路线图（线性的或者树状的）
* 正文中禁止 \\ 换行，以及其他禁止的格式如 `noindent`
* 段末注释会导致不能分段（必须要空两行才行）
* `\textbackslash` 后面需要删除一个空格（如果有）
* Matlab 小括号里面的 end 不高亮
* 例题和习题中的标题仍然是 `<h5>`
* 编译 `\lstinline|\begin|` 会出错
* 美元符号插入的行内公式两边有无空格都不应该影响排版
* 中文字符间的英文两边有无空格都不应该影响排版

## New Features
* 保证 aligned 环境里面不能有空行（equation 环境也不行吗？）
* 注意微软雅黑是收费的！ 用免费字体（方正的？）
* 每个页面下面的 “编辑词条” 链接直接打开该词条， 如 `wuli.wiki/editor/?entry=AMAdd`
* 命令行加一个搜索并替换空心句号的选项。 FileSeek 还是太慢。
* 表格像公式一样可以拖动，以便在移动设备上看
* 为了在搜索引擎中更方便搜到， 在每个词条的 html 中添加 `<meta name="keywords" content="xxxx"/>` 详见 `littleshi.cn/index.html`。 keyword 已经在 tex 文档开始注释
* 添加 C++ 高亮功能
* 实现 `lstinline| |`
* 在 `dep.json` 文件中添加章节信息， 给每个部分显示为不同颜色
* `\autoref{}` 外部引用时程序要检查 `\upref{}` 是否存在， 否则报错。
* 没有被用到的文件/图片全部都要警告
* 正文中不应该有空格（尤其是文字和符号之间）， 除了 `\autoref{}` 前面不加空格后面要。
* PhysWikiCheck 检查 latex 代码中 `\autoref{}` 前面是否没有空格而后面有
* 正文中的英文逗号句号啥的在 PhysWikiCheck 中检查（是不是已经有这个功能了）
* PhysWikiCheck 使用命令行选项调用
* link 前面也加一个图标， 像超理一样
* 代码块应该也像表格和公式一样可以拖动。
