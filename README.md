## 使用说明
* 把 PhysWiki 项目的所有 .tex, .m, .svg, .png 文件放在 PhysWikiScanTest 项目的 "PhysWikiOnline/" 目录下, 其中 PhysWiki.tex 文件用于生成目录。
* 把所有新增 Matlab 代码用 highlight 文件夹中的 Matlab 程序转成 html 放在 PhysWikiScanTest 项目的 "PhysWikiOnline/codes" 目录下。
* 如果不想调试代码， Windows 下直接运行 PhysWikiScan.exe 即可（无需安装 Visual Studio）， 注意 PhysWikiScan 文件夹必须与 PhysWikiScanTest 文件夹在同一目录。 运行完后， 会在 "PhysWikiScanTest/PhysWikiOnline/" 目录下生成所有 html 文件。
* 注意 PhysWiki.tex 中不存在的词条也会被转成 html, 但不会在 online 目录中出现。

## 开发笔记
* 如果想用 Visual Studio 调试代码， 打开 PhysWikiScan.sln， 按 F5 即可编译并运行。
* 如果要 debug 某一个文件的编译， 在 "PhysWikiScan.h" 中搜索 "one file debug"， 输入文件名设置 break point 即可。
* 每次 commit 以前编译一个 release 版本， 将 x64/Release/PhysWikiScan.exe 拷贝到 PhysWikiScan 目录下， 测试并写上日期。

## TODO
* 考虑自己写一个 Matlab 转网页的函数（应该不难）
