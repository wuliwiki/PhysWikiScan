## 使用说明
* 在 `set_path.txt` 中设置输入路径和输出路径。
* 输入路径可以是 `PhysWiki` 文件夹， 只会读取所有 `tex` 文件和 `m` 文件。 `PhysWiki.tex` 文件用于生成目录。
* 程序只会输出 `html` 文件到输出路径， 不会改变其他任何文件。
* 输出路径中需要有所有 `svg` 或 `png` 图片。
* 如果不想调试代码， Windows 下直接运行 PhysWikiScan.exe 即可（无需安装 Visual Studio）, 但是无法设置路径。
* 注意 PhysWiki.tex 中不存在的词条也会被转成 html, 但不会在 `index.html` 中的目录中出现。 这些词条在运行的时候会提示 warning。

## PhysWikiScan 所有控制行命令
* `PhysWikiScan .` 全部 tex 转换为 html， 并生成完整目录 `index.html`， 生成 `entries.txt`, `titles.txt`， `ids.txt`， `labels.txt`
* `PhysWikiScan titles`: 只更新 `entries.txt` 和 `titles.txt`
* `PhysWikiScan toc`: 生成完整目录 `index.html`
* `PhysWikiScan toc-changed`：生成目录 `changed.html`， 只含有 `changed.txt` 中列出的词条
* `PhysWikiScan entry fname`：单个词条转换， 不更新目录, 更新 `ids.txt` 和 `labels.txt` （必须已经存在）
* `PhysWikiScan autoref fname eq 8` 查找 `fname.tex` 词条的网页公式序号 (8) 是否存在 label。 如果 label 不存在， 就试图对被引用的公式插入唯一的 `\label{fname_eq#}`， 把插入的 label 保存到 `autoref.txt` 第 1 行， 第 2 行为 `added`， 更新 `ids.tex` 和 `labels.tex` （必须已经存在）。 如果 label 已经存在， 就直接把 label 保存到 autoref.txt 第 1 行， 第 2 行为 `exist`。 

## 开发笔记
* 如果想用 Visual Studio 调试代码， 打开 PhysWikiScan.sln， 按 F5 即可编译并运行。
* 如果要 debug 某一个文件的编译， 在 "PhysWikiScan.h" 中搜索 "one file debug"， 输入文件名设置 break point 即可。
* 每次 commit 以前编译一个 release 版本， 将 x64/Release/PhysWikiScan.exe 拷贝到 PhysWikiScan 目录下， 测试并写上日期。

## BUG
* table 没有标题
* 文献引用还没有处理
* 处理 bug 的截图
* Matlab 代码块不会自动换行， 因为是 verbatim
* `end` keyword 用于 slicing 的时候不能高亮， html 和 lstlisting 都有这个问题
* 处理 bibliographies
* `\eentry` 和 `\rentry` 没有处理
* 看看能不能根据预备知识给每个词条生成一个学习路线图（线性的或者树状的）
* 正文中禁止 \\ 换行，以及其他禁止的格式如 `noindent`

## New Features
* 表格像公式一样可以拖动，以便在移动设备上看
* 为了在搜索引擎中更方便搜到， 在每个词条的 html 中 `<meta name="keywords" content="xxxx"/>` 详见 `littleshi.cn/index.html`
