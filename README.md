## 使用说明
* 在 `set_path.txt` 中设置输入路径和输出路径。
* 输入路径可以是 `PhysWiki` 文件夹， 只会读取所有 `tex` 文件和 `m` 文件。 `PhysWiki.tex` 文件用于生成目录。
* 程序只会输出 `html` 文件到输出路径， 不会改变其他任何文件。
* 输出路径中需要有所有 `svg` 或 `png` 图片。
* 如果不想调试代码， Windows 下直接运行 PhysWikiScan.exe 即可（无需安装 Visual Studio）, 但是无法设置路径。
* 注意 PhysWiki.tex 中不存在的词条也会被转成 html, 但不会在 `index.html` 中的目录中出现。 这些词条在运行的时候会提示 warning。

## 错误提示
* `figure xxx not found`: PhysWiki 中每一张 pdf 图片都必须装换为 svg 图片放在 PhysWikiOnline/ 下， 每一张 png 图片直接复制即可。 如果控制行提示 ， 就说明找不到 svg 或者 png 图片。
* `Chinese title inconsistent!`: 说明当前词条在 `PhysWiki.tex` 中的标题和词条文件首行注释的标题不一致。
* `Need a title!`: 当前词条首行没有注释标题
* `Body is empty!`: 当前词条不能为空
* `label not in an environment!` 当前词条正文中出现了 `\label{}` 命令
* `environment not supported for label!` 当前环境不支持 `\label{}` 命令
* `label format error! expecting xxx` label 格式错误
* `error when reading figure name!`
* `fig caption not found!` 找不到 figure 标题
* `autoref format error!` autoref 命令格式错误
* `unknown id name!` label 类型不支持
* `label xxx not found!` 找不到 autoref 的 label
* `upref file not found!` 找不到 upref 命令引用的文件
* `Chinese title is empty in PhysWiki.tex!`
* `File not found for an entry in PhysWiki.tex!`
* `code file "xxx.html" not found!` 代码文件需要转成 html 放在 PhysWikiScanTest/PhysWikiOnline/code 文件夹
* `wrong format in xxx.html` 代码 html 文件格式错误
* `code file "xxx.m" not found!` 代码文件需要放在 PhysWikiScanTest/PhysWikiOnline/ 文件夹
* `"PhysWikiTitle" not found in entry_template.html!` 词条网页模板格式不对
* `"PhysWikiHTMLbody" not found in entry_template.html!` 词条网页模板格式不对

## 开发笔记
* 如果想用 Visual Studio 调试代码， 打开 PhysWikiScan.sln， 按 F5 即可编译并运行。
* 如果要 debug 某一个文件的编译， 在 "PhysWikiScan.h" 中搜索 "one file debug"， 输入文件名设置 break point 即可。
* 每次 commit 以前编译一个 release 版本， 将 x64/Release/PhysWikiScan.exe 拷贝到 PhysWikiScan 目录下， 测试并写上日期。

## TODO
* 还有好多 bug 的截图需要处理的。。。
* Matlab 代码块不会自动换行啊， 因为是 verbatim
* `end` keyword 用于 slicing 的时候不能高亮， html 和 lstlisting 都有这个问题
* 检查所有文件的大小写是否正确（包括 tex 文件，以 PhysWiki.tex 中的为准）， windows 是不区分大小写的。
* 处理 bibliographies
* `\eentry` 和 `\rentry` 没有处理
* 看看能不能根据预备知识给每个词条生成一个学习路线图（线性的或者树状的）
