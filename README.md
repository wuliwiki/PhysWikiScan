## 使用说明
* 把 PhysWiki 项目的所有 .tex, .m, .svg, .png 文件放在 littleshi.cn 项目的 "PhysWikiOnline/" 目录下, 其中 PhysWiki.tex 文件用于生成目录。
* 把所有新增 Matlab 代码用 highlight 文件夹中的 Matlab 程序转成 html 放在 littleshi.cn 项目的 "PhysWikiOnline/codes" 目录下。
* 搜索 "break point here", 然后在这些行设置 break point. 然后用 debug 模式运行（F5）。
* 在 PhysWiki.tex 中不存在的词条也会被转成 html, 但不会在 online 目录中出现。

## 开发笔记
* 如果要 debug 某一个文件的编译， 搜索 "one file debug"， 输入文件名设置 break point 即可。

## TODO
* 把错误提示改得更具体
* 出错时给一个重试的机会，而不是直接退出程序
* 考虑自己写一个 Matlab 转网页的函数（应该不难）
