#pragma once

// check if each line size is less than limit
// return the line number if limit exceeded
// return -1 if ok
inline Long line_size_lim(Str32_I str, Long_I lim)
{
    Long ind0 = 0, line = 0, ind_old = 0;
    while (true) {
        ind0 = str.find(U"\n", ind0);
        ++line;
        if (ind0 < 0) {
            if (size(str) - ind_old > lim)
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
inline Long lstlisting(Str32_IO str, vecStr32_I str_verb)
{
    Long ind0 = 0, ind1 = 0;
    Intvs intvIn, intvOut;
    Str32 code, ind_str;
    find_env(intvIn, str, U"lstlisting", 'i');
    Long N = find_env(intvOut, str, U"lstlisting", 'o');
    Str32 lang, caption, capption_str; // language, caption
    Str32 code_tab_str;
    Long Ncaption = 0;
    // get number of captions
    for (Long i = 0; i < N; ++i) {
        ind0 = expect(str, U"[", intvIn.L(i));
        if (ind0 > 0) {
            ind1 = pair_brace(str, ind0 - 1);
            ind0 = str.find(U"caption", ind0);
            if (ind0 > 0 && ind0 < ind1)
                ++Ncaption;
        }
    }
    // main loop
    for (Long i = N-1; i >= 0; --i) {
        // get language
        ind0 = expect(str, U"[", intvIn.L(i));
        ind1 = -1;
        if (ind0 > 0) {
            ind1 = pair_brace(str, ind0-1);
            ind0 = str.find(U"language", ind0);
            if (ind0 > 0 && ind0 < ind1) {
                ind0 = expect(str, U"=", ind0 + 8);
                if (ind0 < 0)
                    throw Str32(U"lstlisting 方括号中指定语言格式错误（[language=xxx]）");
                Long ind2 = str.find(U',', ind0);
                if (ind2 >= 0 && ind2 <= ind1)
                    lang = str.substr(ind0, ind2 - ind0);
                else
                    lang = str.substr(ind0, ind1 - ind0);
                trim(lang);
            }
        }
        else {
            throw Str32(U"lstlisting 需要在方括号中指定语言，格式：[language=xxx]。 建议使用菜单中的按钮插入该环境。");
        }

        // get caption
        caption.clear();
        ind0 = expect(str, U"[", intvIn.L(i));
        capption_str.clear();
        if (ind0 > 0) {
            ind1 = pair_brace(str, ind0 - 1);
            ind0 = str.find(U"caption", ind0);
            if (ind0 > 0 && ind0 < ind1) {
                ind0 = expect(str, U"=", ind0 + 7);
                if (ind0 < 0)
                    throw Str32(U"lstlisting 方括号中标题格式错误（[caption=xxx]）");
                Long ind2 = str.find(U',', ind0);
                if (ind2 >= 0 && ind2 <= ind1)
                    caption = str.substr(ind0, ind2 - ind0);
                else
                    caption = str.substr(ind0, ind1 - ind0);
                Long ind3 = 0;
                trim(caption);
                if (caption.empty())
                    throw Str32(U"lstlisting 方括号中标题不能为空（[caption=xxx]）");
                while (1) {
                    ind3 = caption.find(U'_', ind3);
                    if (ind3 < 0) break;
                    if (ind3 > 0 && caption[ind3-1] != U'\\')
                        throw Str32(U"lstlisting 标题中下划线前面要加 \\");
                    ++ind3;
                }
                replace(caption, U"\\_", U"_");
                capption_str = U"<div align = \"center\">代码 " + num2str32(Ncaption) + U"：" + caption + U"</div>\n";
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
        trim(ind_str, U"\n ");
        code = str_verb[str2int(ind_str)];
        if (line_size_lim(code, 78) >= 0)
            throw Str32(U"单行代码过长");
        
        // highlight
        Str32 prism_lang, prism_line_num;
        if (lang == U"matlabC") {
            prism_lang = U" class=\"language-matlab\"";
        }
        else if (lang == U"matlab" || lang == U"cpp" || lang == U"python" || lang == U"bash" || lang == U"makefile" || lang == U"julia" || lang == U"latex") {
            prism_lang = U" class=\"language-" + lang + U"\"";
            prism_line_num = U"class=\"line-numbers\"";
        }
        else {
            prism_line_num = U"class=\"line-numbers\"";
            if (!lang.empty()) {
                prism_lang = U" class=\"language-" + lang + U"\"";
                SLS_WARN(u8"lstlisting 环境不支持 " + u8(lang) + u8" 语言， 可能未添加高亮！");
            }
            else
                prism_lang = U" class=\"language-plain\"";
        }
        if (lang == U"matlab" && gv::is_wiki) {
            if (!caption.empty() && caption.back() == U'm') {
                Str32 fname = gv::path_out + U"code/" + lang + "/" + caption;
                if (gv::is_entire && file_exist(fname))
                    throw Str32(U"代码文件名重复： " + fname);
                if (code.back() != U'\n')
                    write(code+U'\n', fname);
                else
                    write(code, fname);
            }
        }
        replace(code, U"<", U"&lt;"); replace(code, U">", U"&gt;");
        str.replace(intvOut.L(i), intvOut.R(i) - intvOut.L(i) + 1, capption_str +
            U"<pre " + prism_line_num + U"><code" + prism_lang + U">" + code + U"\n</code></pre>\n");
    }
    return N;
}
