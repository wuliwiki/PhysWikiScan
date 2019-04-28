## 使用说明
* 把 PhysWiki 项目的所有 .tex, .m, .svg, .png 文件放在 PhysWikiScanTest 项目的 "PhysWikiOnline/" 目录下, 其中 PhysWiki.tex 文件用于生成目录。
* 把所有新增 Matlab 代码用 highlight 文件夹中的 Matlab 程序转成 html 放在 PhysWikiScanTest 项目的 "PhysWikiOnline/codes" 目录下。
* 如果不想调试代码， Windows 下直接运行 PhysWikiScan.exe 即可（无需安装 Visual Studio）， 注意 PhysWikiScan 文件夹必须与 PhysWikiScanTest 文件夹在同一目录。 运行完后， 会在 "PhysWikiScanTest/PhysWikiOnline/" 目录下生成所有 html 文件。
* 注意 PhysWiki.tex 中不存在的词条也会被转成 html, 但不会在 online 目录中出现。

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
* 考虑自己写一个 Matlab 转网页的函数（应该不难）
* 还有好多 bug 的截图需要处理的。。。
* 直接在程序中 highlight Matlab， 先把以前的 Matlab 代码处理程序删掉。 代码以 code/ 中的 m 文件中的为准， 编码用 utf8， 取消下载按钮， 统一通过压缩包下载， 压缩包内是 GB2312 编码的 m 文件（通过 VScode 手动转换）。
* Matlab 代码块不会自动换行啊， 因为是 verbatim
* `end` keyword 用于 slicing 的时候不能高亮， html 和 lstlisting 都有这个问题
* 行号是必须的
* fix the compiler warnings
