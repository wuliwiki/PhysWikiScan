#pragma once

// check if each line size is less than limit
// return the line number if limit exceeded
// return -1 if ok
inline Long line_size_lim(Str_I str, Long_I lim)
{
    Long ind0 = 0, line = 0, ind_old = 0;
    Str32 str32 = u32(str);
    while (true) {
        ind0 = str32.find(U"\n", ind0);
        ++line;
        if (ind0 < 0) {
            if (size(str32) - ind_old > lim)
                return line;
            else
                return -1;
        }
        if (ind0 - ind_old > lim)
            return line;
        ++ind0;
        ind_old = ind0;
    }
}

// process all lstlisting environments
// return the number of \Command{} processed
inline Long lstlisting(Str_IO str, vecStr_I str_verb)
{
    Long ind0 = 0, ind1 = 0;
    Intvs intvIn, intvOut;
    Str code, ind_str;
    find_env(intvIn, str, u8"lstlisting", 'i');
    Long N = find_env(intvOut, str, u8"lstlisting", 'o');
    Str lang, caption, capption_str; // language, caption
    Str code_tab_str;
    Long Ncaption = 0;
    // get number of captions
    for (Long i = 0; i < N; ++i) {
        ind0 = expect(str, u8"[", intvIn.L(i));
        if (ind0 > 0) {
            ind1 = pair_brace(str, ind0 - 1);
            ind0 = str.find(u8"caption", ind0);
            if (ind0 > 0 && ind0 < ind1)
                ++Ncaption;
        }
    }
    // main loop
    for (Long i = N-1; i >= 0; --i) {
        // get language
        ind0 = expect(str, u8"[", intvIn.L(i));
        ind1 = -1;
        if (ind0 > 0) {
            ind1 = pair_brace(str, ind0-1);
            ind0 = str.find(u8"language", ind0);
            if (ind0 > 0 && ind0 < ind1) {
                ind0 = expect(str, u8"=", ind0 + 8);
                if (ind0 < 0)
                    throw scan_err(u8"lstlisting 方括号中指定语言格式错误（[language=xxx]）");
                Long ind2 = str.find(',', ind0);
                if (ind2 >= 0 && ind2 <= ind1)
                    lang = str.substr(ind0, ind2 - ind0);
                else
                    lang = str.substr(ind0, ind1 - ind0);
                trim(lang);
            }
        }
        else {
            throw scan_err(u8"lstlisting 需要在方括号中指定语言，格式：[language=xxx]。 建议使用菜单中的按钮插入该环境。");
        }

        // get caption
        caption.clear();
        ind0 = expect(str, u8"[", intvIn.L(i));
        capption_str.clear();
        if (ind0 > 0) {
            ind1 = pair_brace(str, ind0 - 1);
            ind0 = str.find(u8"caption", ind0);
            if (ind0 > 0 && ind0 < ind1) {
                ind0 = expect(str, u8"=", ind0 + 7);
                if (ind0 < 0)
                    throw scan_err(u8"lstlisting 方括号中标题格式错误（[caption=xxx]）");
                Long ind2 = str.find(',', ind0);
                if (ind2 >= 0 && ind2 <= ind1)
                    caption = str.substr(ind0, ind2 - ind0);
                else
                    caption = str.substr(ind0, ind1 - ind0);
                Long ind3 = 0;
                trim(caption);
                if (caption.empty())
                    throw scan_err(u8"lstlisting 方括号中标题不能为空（[caption=xxx]）");
                while (1) {
                    ind3 = caption.find('_', ind3);
                    if (ind3 < 0) break;
                    if (ind3 > 0 && caption[ind3-1] != '\\')
                        throw scan_err(u8"lstlisting 标题中下划线前面要加 \\");
                    ++ind3;
                }
                replace(caption, u8"\\_", u8"_");
                capption_str = u8"<div align = \"center\">代码 " + num2str(Ncaption) + u8"：" + caption + u8"</div>\n";
                --Ncaption;
            }
        }
        
        if (ind1 > 0)
            ind0 = ind1 + 1;
        else
            ind0 = intvIn.L(i);

        // TODO: get [label=xxx]
        // SLS_WARN("lstlisting 的 label 暂时没有处理！");

        // recover code from verbatim index
        ind_str = str.substr(ind0, intvIn.R(i) + 1 - ind0);
        trim(ind_str, u8"\n ");
        code = str_verb[str2int(ind_str)];
        if (line_size_lim(code, 78) >= 0)
            throw scan_err(u8"单行代码过长（防止 pdf 中代码超出长度）");
        // TODO: 即使是 pdf 的 lstlisting 应该也可以自动换行吧！
        
        // highlight
        Str prism_lang, prism_line_num;
        if (lang == u8"matlabC") {
            prism_lang = u8" class=\"language-matlab\"";
        }
        else if (lang == u8"matlab" || lang == u8"cpp" || lang == u8"python" || lang == u8"bash" || lang == u8"makefile" || lang == u8"julia" || lang == u8"latex") {
            prism_lang = u8" class=\"language-" + lang + u8"\"";
            prism_line_num = u8"class=\"line-numbers\"";
        }
        else {
            prism_line_num = u8"class=\"line-numbers\"";
            if (!lang.empty()) {
                prism_lang = u8" class=\"language-" + lang + u8"\"";
                SLS_WARN(u8"lstlisting 环境不支持 " + lang + u8" 语言， 可能未添加高亮！");
            }
            else
                prism_lang = u8" class=\"language-plain\"";
        }
        if (lang == u8"matlab" && gv::is_wiki) {
            if (!caption.empty() && caption.back() == 'm') {
                Str fname = gv::path_out + u8"code/" + lang + "/" + caption;
                if (gv::is_entire && file_exist(fname))
                    throw scan_err("代码文件名重复： " + fname);
                if (code.back() != '\n')
                    write(code+'\n', fname);
                else
                    write(code, fname);
            }
        }
        replace(code, u8"<", u8"&lt;"); replace(code, u8">", u8"&gt;");
        str.replace(intvOut.L(i), intvOut.R(i) - intvOut.L(i) + 1, capption_str +
            u8"<pre " + prism_line_num + u8"><code" + prism_lang + u8">" + code + u8"\n</code></pre>\n");
    }
    return N;
}
