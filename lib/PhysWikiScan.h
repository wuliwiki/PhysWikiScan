#pragma once
#include "../SLISC/file/sqlite_ext.h"
#include <SQLiteCpp/Statement.h>
#include <SQLiteCpp/Database.h>
#include "../SLISC/str/unicode.h"
#include "../SLISC/algo/graph.h"
#include "../highlight/matlab2html.h"
// #include "../highlight/cpp2html.h"
#include "../SLISC/util/sha1sum.h"
#include "../SLISC/str/str.h"
#include "../SLISC/algo/sort.h"
#include "../SLISC/str/disp.h"

using namespace slisc;

// global variables, must be set only once
namespace gv {
    Str32 path_in; // e.g. ../PhysWiki/
    Str32 path_out; // e.g. ../littleshi.cn/online/
    Str32 path_data; // e.g. ../littleshi.cn/data/
    Str32 url; // e.g. https://wuli.wiki/online/
    Bool is_wiki; // editing wiki or note
    Bool is_eng = false; // use english for auto-generated text (Eq. Fig. etc.)
    Bool is_entire = false; // running one tex or the entire wiki
}

#include "../TeX/tex2html.h"

// get the title (defined in the first comment, can have space after %)
// limited to 20 characters
inline void get_title(Str32_O title, Str32_I str)
{
    if (size(str) == 0 || str.at(0) != U'%')
        throw Str32(U"请在第一行注释标题！");
    Long ind1 = str.find(U'\n');
    if (ind1 < 0)
        ind1 = size(str) - 1;
    title = str.substr(1, ind1 - 1);
    trim(title);
    if (title.empty())
        throw Str32(U"请在第一行注释标题（不能为空）！"); 
    Long ind0 = title.find(U"\\");
    if (ind0 >= 0)
        throw Str32(U"第一行注释的标题不能含有 “\\” ");
}

// get label type (eq, fig, ...)
inline Str32 label_type(Str32_I label)
{
    assert(label[0] != U' '); assert(label.back() != U' ');
    Long ind1 = label.find(U'_');
    return label.substr(0, ind1);
}

inline Str label_type(Str_I label)
{
    assert(label[0] != ' '); assert(label.back() != ' ');
    Long ind1 = label.find('_');
    return label.substr(0, ind1);
}

// get label id
inline Str32 label_id(Str32_I label)
{
    assert(label[0] != U' '); assert(label.back() != U' ');
    Long ind1 = label.find(U'_');
    return label.substr(ind1+1);
}

inline Str label_id(Str_I label)
{
    assert(label[0] != ' '); assert(label.back() != ' ');
    Long ind1 = label.find('_');
    return label.substr(ind1+1);
}

// get entry from label (old format)
inline Str32 label_entry_old(Str32_I label)
{
    Str32 id = label_id(label);
    Long ind = id.rfind(U'_');
    return id.substr(0, ind);
}

inline Str label_entry_old(Str_I label)
{
    Str id = label_id(label);
    Long ind = id.rfind('_');
    return id.substr(0, ind);
}

// check if an entry is labeled "\issueDraft"
inline Bool is_draft(Str32_I str)
{
    Intvs intv;
    find_env(intv, str, U"issues");
    if (intv.size() > 1)
        throw Str32(U"每个词条最多支持一个 issues 环境!");
    else if (intv.empty())
        return false;
    Long ind = str.find(U"\\issueDraft", intv.L(0));
    if (ind < 0)
        return false;
    if (ind < intv.R(0))
        return true;
    else
        throw Str32(U"\\issueDraft 命令不在 issues 环境中!");
}

// trim "\n" and " " on both sides
// remove unnecessary "\n"
// replace “\n\n" with "\n</p>\n<p>　　\n"
inline Long paragraph_tag1(Str32_IO str)
{
    Long ind0 = 0, N = 0;
    trim(str, U" \n");
    // delete extra '\n' (more than two continuous)
    while (1) {
        ind0 = str.find(U"\n\n\n", ind0);
        if (ind0 < 0)
            break;
        eatR(str, ind0 + 2, U"\n");
    }

    // replace "\n\n" with "\n</p>\n<p>　　\n"
    N = replace(str, U"\n\n", U"\n</p>\n<p>　　\n");
    return N;
}

std::unordered_set<Char32> illegal_chars;

// globally forbidden characters
// assuming comments are deleted and verbose envs are not hidden
inline void global_forbid_char(Str32_I str)
{
    // some of these may be fine in wuli.wiki/editor, but will not show when compiling pdf with XeLatex
    Str32 forbidden = U"αΑ∵⊥βΒ⋂◯⋃•∩∪⋯∘χΧΔδ⋄ϵ∃Ε≡⊓⊔⊏⊐□⋆ηΗ∀Γγ⩾≥≫⋙∠≈ℏ∈∫∬∭∞ιΙΚκΛλ⩽⟵⟶"
        U"⟷⟸⟹⟺≤⇐←⇇↔≪⋘↦∡∓μΜ≠∋∉⊈νΝ⊙∮ωΩ⊕⊗∥∂⟂∝ΦϕπΠ±ΨψρΡ⇉⇒σΣ∼≃⊂⊆"
        U"⊃⊇∑Ττ∴θ×Θ→⊤◁▷↕⇕⇈⇑Υ↑≐↓⇊†‡⋱⇓υε∅ϰφςϖϱϑ∨∧ΞξΖζ▽Οο⊖"
        U"⓪①②③④⑤⑥⑦⑧⑨⑩⑪⑫⑬⑭⑮⑯⑰⑱⑲⑳";

    // check repetition
    static bool checked = false;
    if (!checked) {
        checked = true;
        Long ind = 0;
        while (ind < size(forbidden)) {
            ind = find_repeat(forbidden, ind);
            if (ind < 0) break;
            cout << "ind = " << ind << ", char = " << forbidden[ind] << endl;
            throw Str32(U"found repeated char in `forbidden`");
            ++ind;
        };
    }

    Long ind = str.find_first_of(forbidden);
    if (ind >= 0) {
        Long beg = max(Long(0),ind-30);
        SLS_WARN(U"latex 代码中出现非法字符，建议使用公式环境和命令表示： " + str.substr(beg, ind-beg) + U"？？？");
        // TODO: scan 应该搞一个批量替换功能
        illegal_chars.insert(str[ind]);
    }

    Long N = str.size();
    for (Long i = 0; i < N; ++i) {
        // overleaf only suppors Basic Multilingual Plane (BMP) characters
        // we should do that too (except for comments)
        if (Long(str[i]) > 65536) {
            Long beg = max(Long(0),i-30);
            SLS_WARN(U"latex 代码中出现非法字符，建议使用公式环境和命令表示： " + str.substr(beg, i-beg) + U"？？？");
            illegal_chars.insert(str[ind]);
        }
    }
}

inline Long paragraph_tag(Str32_IO str)
{
    Long N = 0, ind0 = 0, left = 0, length = -500, ikey = -500;
    Str32 temp, env, end, begin = U"<p>　　\n";

    // "begin", and commands that cannot be in a paragraph
    vecStr32 commands = {U"begin", U"subsection", U"subsubsection", U"pentry"};

    // environments that must be in a paragraph (use "<p>" instead of "<p>　　" when at the start of the paragraph)
    vecStr32 envs_eq = {U"equation", U"align", U"gather", U"lstlisting"};

    // environments that needs paragraph tags inside
    vecStr32 envs_p = { U"example", U"exercise", U"definition", U"theorem", U"lemma", U"corollary"};

    // 'n' (for normal); 'e' (for env_eq); 'p' (for env_p); 'f' (end of file)
    char next, last = 'n';

    Intvs intv, intvInline, intv_tmp;

    // handle normal text intervals separated by
    // commands in "commands" and environments
    while (1) {
        // decide mode
        if (ind0 == size(str)) {
            next = 'f';
        }
        else {
            ind0 = find_command(ikey, str, commands, ind0);
            find_double_dollar_eq(intv, str, 'o');
            find_sqr_bracket_eq(intv_tmp, str, 'o');
            combine(intv, intv_tmp);
            Long itmp = is_in_no(ind0, intv);
            if (itmp >= 0) {
                ind0 = intv.R(itmp) + 1; continue;
            }
            if (ind0 < 0)
                next = 'f';
            else {
                next = 'n';
                if (ikey == 0) { // found environment
                    find_inline_eq(intvInline, str);
                    if (is_in(ind0, intvInline)) {
                        ++ind0; continue; // ignore in inline equation
                    }
                    command_arg(env, str, ind0);
                    if (search(env, envs_eq) >= 0) {
                        next = 'e';
                    }
                    else if (search(env, envs_p) >= 0) {
                        next = 'p';
                    }
                }
            }
        }

        // decide ending tag
        if (next == 'n' || next == 'p') {
            end = U"\n</p>\n";
        }
        else if (next == 'e') {
            // equations can be inside paragraph
            if (ExpectKeyReverse(str, U"\n\n", ind0 - 1) >= 0) {
                end = U"\n</p>\n<p>\n";
            }
            else
                end = U"\n";
        }
        else if (next == 'f') {
            end = U"\n</p>\n";
        }

        // add tags
        length = ind0 - left;
        temp = str.substr(left, length);
        N += paragraph_tag1(temp);
        if (temp.empty()) {
            if (next == 'e' && last != 'e') {
                temp = U"\n<p>\n";
            }
            else if (last == 'e' && next != 'e') {
                temp = U"\n</p>\n";
            }
            else {
                temp = U"\n";
            }
        }
        else {
            temp = begin + temp + end;
        }
        str.replace(left, length, temp);
        find_inline_eq(intvInline, str);
        if (next == 'f')
            return N;
        ind0 += temp.size() - length;
        
        // skip
        if (ikey == 0) {
            if (next == 'p') {
                Long ind1 = skip_env(str, ind0);
                Long left, right;
                left = inside_env(right, str, ind0, 2);
                length = right - left;
                temp = str.substr(left, length);
                paragraph_tag(temp);
                str.replace(left, length, temp);
                find_inline_eq(intvInline, str);
                ind0 = ind1 + temp.size() - length;
            }
            else
                ind0 = skip_env(str, ind0);
        }
        else {
            ind0 = skip_command(str, ind0, 1);
            if (ind0 < size(str)) {
                Long ind1 = expect(str, U"\\label", ind0);
                if (ind1 > 0)
                    ind0 = skip_command(str, ind1 - 6, 1);
            }
        }

        left = ind0;
        last = next;
        if (ind0 == size(str)) {
            continue;
        }
        
        // beginning tag for next interval
        
        if (last == 'n' || last == 'p')
            begin = U"\n<p>　　\n";
        else if (last == 'e') {
            if (expect(str, U"\n\n", ind0) >= 0) {
                begin = U"\n</p>\n<p>　　\n";
            }
            else
                begin = U"\n";
        }    
    }
}

inline void limit_env_cmd(Str32_I str)
{
    // commands not supported
    if (find_command(str, U"documentclass") >= 0)
        throw Str32(U"\"documentclass\" 命令已经在 main.tex 中，每个词条文件是一个 section，请先阅读说明");
    if (find_command(str, U"newcommand") >= 0)
        throw Str32(U"不支持用户 \"newcommand\" 命令， 可以尝试在设置面板中定义自动补全规则， 或者向管理员申请 newcommand");
    if (find_command(str, U"renewcommand") >= 0)
        throw Str32(U"不支持用户 \"renewcommand\" 命令， 可以尝试在设置面板中定义自动补全规则");
    if (find_command(str, U"usepackage") >= 0)
        throw Str32(U"不支持 \"usepackage\" 命令， 每个词条文件是一个 section，请先阅读说明");
    if (find_command(str, U"newpage") >= 0)
        throw Str32(U"暂不支持 \"newpage\" 命令");
    if (find_command(str, U"title") >= 0)
        throw Str32(U"\"title\" 命令已经在 main.tex 中，每个词条文件是一个 section，请先阅读说明");
    if (find_command(str, U"author") >= 0)
        throw Str32(U"不支持 \"author\" 命令， 每个词条文件是一个 section，请先阅读说明");
    if (find_command(str, U"maketitle") >= 0)
        throw Str32(U"不支持 \"maketitle\" 命令， 每个词条文件是一个 section，请先阅读说明");

    // allowed environments
    // there are bugs when auto inserting label into align or gather, e.g. when there are "\\" within matrices
    vecStr32 envs_allow = { U"equation", U"gather", U"gather*", U"gathered", U"align", U"align*",
        /*U"alignat", U"alignat*", U"alignedat",*/ U"aligned", U"split", U"figure", U"itemize",
        U"enumerate", U"lstlisting", U"example", U"exercise", U"lemma", U"theorem", U"definition",
        U"corollary", U"matrix", U"pmatrix", U"vmatrix", U"table", U"tabular", U"cases", U"array",
        U"case", U"Bmatrix", U"bmatrix", /*U"eqnarray", U"eqnarray*", U"multline", U"multline*",
        U"smallmatrix",*/ U"subarray", U"Vmatrix", U"issues" };

    Str32 env;
    Long ind0 = -1;
    while (1) {
        ind0 = find_command(str, U"begin", ind0+1);
        if (ind0 < 0)
            break;
        command_arg(env, str, ind0);
        if (search(env, envs_allow) < 0) {
            if (env == U"document")
                throw Str32(U"document 环境已经在 main.tex 中，每个词条文件是一个 section，请先阅读说明。");
            else
                throw Str32(U"暂不支持 " + env + U" 环境！ 如果你认为 MathJax 支持该环境， 请联系管理员。");
        }
    }
}

// add html id tag before environment if it contains a label, right before "\begin{}" and delete that label
// output html id and corresponding label for future reference
// `gather` and `align` environments has id starting wth `ga` and `ali`
// `idNum` is in the idNum-th environment of the same name (not necessarily equal to displayed number)
// no comment allowed
// return number of labels processed, or -1 if failed
inline Long EnvLabel(vecStr32_O labels, vecLong_O label_orders, Str32_I entry, Str32_IO str)
{
    Long ind0{}, ind2{}, ind3{}, ind4{}, ind5{}, N{},
        Ngather{}, Nalign{}, i{}, j{};
    Str32 type; // "eq", "fig", "ex", "sub"...
    Str32 envName; // "equation" or "figure" or "example"...
    Long idN{}; // convert to idNum
    Str32 label, id;
    labels.clear(); label_orders.clear();
    while (1) {
        ind5 = find_command(str, U"label", ind0);
        if (ind5 < 0) return N;
        // detect environment kind
        ind2 = str.rfind(U"\\end", ind5);
        ind4 = str.rfind(U"\\begin", ind5);
        if (ind4 < 0 || (ind4 >= 0 && ind2 > ind4)) {
            // label not in environment, must be a subsection label
            type = U"sub"; envName = U"subsection";
            // TODO: make sure the label follows a \subsection{} command
        }
        else {
            Long ind1 = expect(str, U"{", ind4 + 6);
            if (ind1 < 0)
                throw(Str32(U"\\begin 后面没有 {"));
            if (expect(str, U"equation", ind1) > 0) {
                type = U"eq"; envName = U"equation";
            }
            else if (expect(str, U"figure", ind1) > 0) {
                type = U"fig"; envName = U"figure";
            }
            else if (expect(str, U"definition", ind1) > 0) {
                type = U"def"; envName = U"definition";
            }
            else if (expect(str, U"lemma", ind1) > 0) {
                type = U"lem"; envName = U"lemma";
            }
            else if (expect(str, U"theorem", ind1) > 0) {
                type = U"the"; envName = U"theorem";
            }
            else if (expect(str, U"corollary", ind1) > 0) {
                type = U"cor"; envName = U"corollary";
            }
            else if (expect(str, U"example", ind1) > 0) {
                type = U"ex"; envName = U"example";
            }
            else if (expect(str, U"exercise", ind1) > 0) {
                type = U"exe"; envName = U"exercise";
            }
            else if (expect(str, U"table", ind1) > 0) {
                type = U"tab"; envName = U"table";
            }
            else if (expect(str, U"gather", ind1) > 0) {
                type = U"eq"; envName = U"gather";
            }
            else if (expect(str, U"align", ind1) > 0) {
                type = U"eq"; envName = U"align";
            }
            else {
                throw Str32(U"该环境不支持 label");
            }
        }
        
        // check label format and save label
        ind0 = expect(str, U"{", ind5 + 6);
        ind3 = expect(str, type + U'_' + entry, ind0);
        if (ind3 < 0) {
            throw Str32(U"label " + str.substr(ind0, 20) + U"... 格式错误， 是否为 \"" + type + U'_' + entry + U"\"？");
        }
        ind3 = str.find(U"}", ind3);
        
        label = str.substr(ind0, ind3 - ind0);
        trim(label);
        Long ind = search(label, labels);
        if (ind < 0)
            labels.push_back(label);
        else
            throw Str32(U"标签多次定义： " + labels[ind]);
        
        // count idNum, insert html id tag, delete label
        Intvs intvEnv;
        if (type == U"eq") { // count equations
            idN = find_env(intvEnv, str.substr(0,ind4), U"equation");
            Ngather = find_env(intvEnv, str.substr(0,ind4), U"gather");
            if (Ngather > 0) {
                for (i = 0; i < Ngather; ++i) {
                    for (j = intvEnv.L(i); j < intvEnv.R(i); ++j) {
                        if (str.at(j) == '\\' && str.at(j+1) == '\\')
                            ++idN;
                    }
                    ++idN;
                }
            }
            Nalign = find_env(intvEnv, str.substr(0,ind4), U"align");
            if (Nalign > 0) {
                for (i = 0; i < Nalign; ++i) {
                    for (j = intvEnv.L(i); j < intvEnv.R(i); ++j) {
                        if (str.at(j) == '\\' && str.at(j + 1) == '\\')
                            ++idN;
                    }
                    ++idN;
                }
            }
            if (envName == U"gather" || envName == U"align") {
                for (i = ind4; i < ind5; ++i) {
                    if (str.at(i) == U'\\' && str.at(i + 1) == U'\\')
                        ++idN;
                }
            }
            ++idN;
        }
        else if (type == U"sub") { // count \subsection number
            Long ind = -1; idN = 0; ind4 = -1;
            while (1) {
                ind = find_command(str, U"subsection", ind + 1);
                if (ind > ind5 || ind < 0)
                    break;
                ind4 = ind; ++idN;
            }
        }
        else {
            idN = find_env(intvEnv, str.substr(0, ind4), envName) + 1;
        }
        label_orders.push_back(idN);
        str.erase(ind5, ind3 - ind5 + 1);
        str.insert(ind4, U"<span id = \"" + label + U"\"></span>");
        ind0 = ind4 + 6;
    }
}

// replace figure environment with html image
// convert vector graph to SVG, font must set to "convert to outline"
// if svg image doesn't exist, use png
// path must end with '\\'
// `imgs` is the list of image names, `mark[i]` will be set to 1 when `imgs[i]` is used
// if `imgs` is empty, `imgs` and `mark` will be ignored
inline Long FigureEnvironment(vecStr32_O img_ids, vecLong_O img_orders, vecStr32_O img_hashes,
        Str32_IO str, Str32_I entry)
{
    img_ids.clear(); img_orders.clear(); img_hashes.clear();
    Long N = 0;
    Intvs intvFig;
    Str32 figName, fname_in, fname_out, href, format, caption, widthPt, figNo, version;
    Long Nfig = find_env(intvFig, str, U"figure", 'o');
    for (Long i = Nfig - 1; i >= 0; --i) {
        num2str(figNo, i + 1);
        img_orders.push_back(i + 1);
        // get label and id
        Str32 tmp1 = U"id = \"fig_";
        Long ind_label = str.rfind(tmp1, intvFig.L(i));
        if (ind_label < 0 || (i-1 >= 0 && ind_label < intvFig.R(i-1)))
            throw Str32(U"图片必须有标签， 请使用上传图片按钮。");
        ind_label += tmp1.size();
        Long ind_label_end = str.find(U'\"', ind_label);
        SLS_ASSERT(ind_label_end > 0);
        Str32 id = str.substr(ind_label, ind_label_end - ind_label);
        img_ids.push_back(id);
        // get width of figure
        Long ind0 = str.find(U"width", intvFig.L(i)) + 5;
        ind0 = expect(str, U"=", ind0);
        ind0 = find_num(str, ind0);
        Doub width; // figure width in cm
        str2double(width, str, ind0);
        if (width > 14.25)
            throw Str32(U"图" + figNo + U"尺寸不能超过 14.25cm");

        // get file name of figure
        Long indName1 = str.find(U"figures/", ind0) + 8;
        Long indName2 = str.find(U"}", ind0) - 1;
        if (indName1 < 0 || indName2 < 0) {
            throw Str32(U"读取图片名错误");
        }
        figName = str.substr(indName1, indName2 - indName1 + 1);
        trim(figName);

        // get format (png or svg)
        Long Nname = figName.size();
        if (figName.substr(Nname - 4, 4) == U".png") {
            format = U"png";
            figName = figName.substr(0, Nname - 4);
        }
        else if (figName.substr(Nname - 4, 4) == U".pdf") {
            format = U"svg";
            figName = figName.substr(0, Nname - 4);
        }
        else
            throw Str32(U"图片格式不支持");

        fname_in = gv::path_in + U"figures/" + figName + U"." + format;

        if (!file_exist(fname_in))
            throw Str32(U"图片 \"" + fname_in + U"\" 未找到");

        version.clear();
        // last_modified(version, fname_in);
        Str tmp; read(tmp, u8(fname_in)); CRLF_to_LF(tmp);
        Str32 img_hash = u32(sha1sum(tmp).substr(0, 16));
        img_hashes.push_back(img_hash);

        // ===== rename figure files with hash ========
        Str32 fname_in2 = gv::path_in + U"figures/" + img_hash + U"." + format;
        if (file_exist(fname_in) && fname_in != fname_in2)
            file_move(u8(fname_in2), u8(fname_in), true);
        if (format == U"svg") {
            Str32 fname_in_svg = gv::path_in + U"figures/" + figName + U".pdf";
            Str32 fname_in_svg2 = gv::path_in + U"figures/" + img_hash + U".pdf";
            if (file_exist(fname_in_svg) && fname_in_svg2 != fname_in_svg)
                file_move(u8(fname_in_svg2), u8(fname_in_svg), true);
        }

        // ===== modify original .tex file =======
        Str32 str_mod, tex_fname = gv::path_in + "contents/" + entry + U".tex";
        read(str_mod, tex_fname);
        if (format == U"png")
            replace(str_mod, figName + U".png", img_hash + U".png");
        else {
            replace(str_mod, figName + U".pdf", img_hash + U".pdf");
            replace(str_mod, figName + U".svg", img_hash + U".svg");
        }
        write(str_mod, tex_fname);
        // ===========================================

        fname_out = gv::path_out + img_hash + U"." + format;
        file_copy(fname_out, fname_in2, true);

        // get caption of figure
        ind0 = find_command(str, U"caption", ind0);
        if (ind0 < 0) {
            throw Str32(U"图片标题未找到");
        }
        command_arg(caption, str, ind0);
        if ((Long)caption.find(U"\\footnote") >= 0)
            throw Str32(U"图片标题中不能添加 \\footnote");
        // insert html code
        num2str(widthPt, Long(33 / 14.25 * width * 100)/100.0);
        href = gv::url + img_hashes.back() + U"." + format;
        str.replace(intvFig.L(i), intvFig.R(i) - intvFig.L(i) + 1,
            U"<div class = \"w3-content\" style = \"max-width:" + widthPt + U"em;\">\n"
            + U"<a href=\"" + href + U"\" target = \"_blank\"><img src = \"" + href
            + U"\" alt = \"" + (gv::is_eng?U"Fig":U"图") + "\" style = \"width:100%;\"></a>\n</div>\n<div align = \"center\"> "
            + (gv::is_eng ? U"Fig. " : U"图 ") + figNo
            + U"：" + caption + U"</div>");
        ++N;
    }
    return N;
}

// get dependent entries (id) from \pentry{}
inline Long depend_entry(vecStr32_O pentries, Str32_I str, SQLite::Database &db)
{
    Long ind0 = 0, N = 0;
    Str32 temp, depEntry;
    pentries.clear();
    while (1) {
        ind0 = find_command(str, U"pentry", ind0);
        if (ind0 < 0)
            return N;
        command_arg(temp, str, ind0, 0, 't');
        Long ind1 = 0;
        while (1) {
            ind1 = find_command(temp, U"upref", ind1);
            if (ind1 < 0)
                return N;
            command_arg(depEntry, temp, ind1, 0, 't');
            if (!exist(db.getHandle(), "entries", "id", u8(depEntry)))
                throw Str32(U"\\pentry{} 中 \\upref 引用的词条未找到: " + depEntry + ".tex");
            if (search(depEntry, pentries) >= 0)
                throw Str32(U"\\pentry{} 中预备知识重复： " + depEntry + ".tex");
            pentries.push_back(depEntry);
            ++N; ++ind1;
        }
    }
    return N;
}

// replace \pentry comman with html round panel
inline Long pentry(Str32_IO str)
{
    if (!gv::is_eng)
        return Command2Tag(U"pentry", U"<div class = \"w3-panel w3-round-large w3-light-blue\"><b>预备知识</b>　", U"</div>", str);
    return Command2Tag(U"pentry", U"<div class = \"w3-panel w3-round-large w3-light-blue\"><b>Prerequisite</b>　", U"</div>", str);
}

// issue environment
inline Long issuesEnv(Str32_IO str)
{
    Long Nenv = Env2Tag(U"issues", U"<div class = \"w3-panel w3-round-large w3-sand\"><ul>", U"</ul></div>", str);
    if (Nenv > 1)
        throw Str32(U"不支持多个 issues 环境， issues 必须放到最开始");
    else if (Nenv == 0)
        return 0;

    Long ind0 = -1, ind1 = -1, N = 0;
    Str32 arg;
    // issueDraft
    ind0 = find_command(str, U"issueDraft");
    if (ind0 > 0) {
        ind1 = skip_command(str, ind0);
        str.replace(ind0, ind1 - ind0, U"<li>本词条处于草稿阶段。</li>");
        ++N;
    }
    // issueTODO
    ind0 = find_command(str, U"issueTODO");
    if (ind0 > 0) {
        ind1 = skip_command(str, ind0);
        str.replace(ind0, ind1 - ind0, U"<li>本词条存在未完成的内容。</li>");
        ++N;
    }
    // issueMissDepend
    ind0 = find_command(str, U"issueMissDepend");
    if (ind0 > 0) {
        ind1 = skip_command(str, ind0);
        str.replace(ind0, ind1 - ind0, U"<li>本词条缺少预备知识， 初学者可能会遇到困难。</li>");
        ++N;
    }
    // issueAbstract
    ind0 = find_command(str, U"issueAbstract");
    if (ind0 > 0) {
        ind1 = skip_command(str, ind0);
        str.replace(ind0, ind1 - ind0, U"<li>本词条需要更多讲解， 便于帮助理解。</li>");
        ++N;
    }
    // issueNeedCite
    ind0 = find_command(str, U"issueNeedCite");
    if (ind0 > 0) {
        ind1 = skip_command(str, ind0);
        str.replace(ind0, ind1 - ind0, U"<li>本词条需要更多参考文献。</li>");
        ++N;
    }
    // issueOther
    ind0 = 0;
    N += Command2Tag(U"issueOther", U"<li>", U"</li>", str);
    return N;
}

// mark incomplete
inline Long addTODO(Str32_IO str)
{
    return Command2Tag(U"addTODO", U"<div class = \"w3-panel w3-round-large w3-sand\">未完成：", U"</div>", str);
}

// remove special .tex files from a list of name
// return number of names removed
// names has ".tex" extension
inline Long ignore_entries(vecStr32_IO names)
{
    Long i{}, j{}, N{}, Nnames{}, Nnames0;
    vecStr32 names0; // names to remove
    names0.push_back(U"FrontMatters");
    names0.push_back(U"FrontMattersSample");
    names0.push_back(U"bibliography");
    // add other names here
    Nnames = names.size();
    Nnames0 = names0.size();
    for (i = 0; i < Nnames; ++i) {
        for (j = 0; j < Nnames0; ++j) {
            if (names[i] == names0[j]) {
                names.erase(names.begin() + i);
                ++N; --Nnames; --i; break;
            }
        }
    }
    return N;
}

// these environments are already not in a paragraph
// return number of environments processed
inline Long theorem_like_env(Str32_IO str)
{
    Long N, N_tot = 0, ind0;
    Intvs intvIn, intvOut;
    Str32 env_title, env_num;
    vecStr32 envNames = {U"definition", U"lemma", U"theorem",
        U"corollary", U"example", U"exercise"};
    vecStr32 envCnNames;
    if (!gv::is_eng)
        envCnNames = {U"定义", U"引理", U"定理",
            U"推论", U"例", U"习题"};
    else
        envCnNames = { U"Definition ", U"Lemma ", U"Theorem ",
            U"Corollary ", U"Example ", U"Exercise " };
    vecStr32 envBorderColors = { U"w3-border-red", U"w3-border-red", U"w3-border-red",
        U"w3-border-red", U"w3-border-yellow", U"w3-border-green" };
    for (Long i = 0; i < size(envNames); ++i) {
        ind0 = 0; N = 0;
        while (1) {
            ind0 = find_command_spec(str, U"begin", envNames[i], ind0);
            if (ind0 < 0)
                break;
            ++N; num2str(env_num, N);
            ++N_tot;
            command_arg(env_title, str, ind0, 1);

            // positions of the environment
            Long ind1 = skip_command(str, ind0, 2);
            Long ind2 = find_command_spec(str, U"end", envNames[i], ind0);
            Long ind3 = skip_command(str, ind2, 1);

            // replace
            str.replace(ind2, ind3 - ind2, U"</div>\n");
            // str.replace(ind1, ind2 - ind1, temp);
            str.replace(ind0, ind1 - ind0, U"<div class = \"w3-panel "
                + envBorderColors[i]
                + U" w3-leftbar\">\n <h3 style=\"margin-top: 0; padding-top: 0;\"><b>"
                + envCnNames[i] + U" "
                + env_num + U"</b>　" + env_title + U"</h3>\n");
        }
    }
    return N_tot;
}

// warn if no space after \autoref{}
inline Long autoref_space(Str32_I str, Bool_I error)
{
    Long ind0 = 0, N = 0;
    Str32 follow = U" ，、．。）~”\n";
    vecStr32 follow2 = { U"\\begin" };
    Str32 msg;
    while (1) {
        ind0 = find_command(str, U"autoref", ind0);
        Long start = ind0;
        if (ind0 < 0)
            return N;
        try {ind0 = skip_command(str, ind0, 1);}
        catch (...) {
            throw Str32(U"\\autoref 后面没有大括号: " + str.substr(ind0, 20));
        }
        if (ind0 >= size(str))
            return N;
        Bool continue2 = false;
        for (Long i = 0; i < size(follow2); ++i) {
            if (expect(str, follow2[i], ind0) >= 0) {
                continue2 = true; break;
            }
        }
        if (continue2)
            continue;
        if (Llong(follow.find(str[ind0])) >= 0)
            continue;
        msg = U"\\autoref{} 后面需要空格: “" + str.substr(start, ind0 + 20 - start) + U"”";
        if (error)
            throw Str32(msg);
        else
            SLS_WARN(u8(msg));
        ++N;
    }
}

// 1. make sure there is a ~ between autoref and upref
// 2. make sure autoref and upref is for the same entry
// 3. make sure label in autoref has underscore
// 4. make sure autoref for other entry always has upref before or after
inline Long autoref_tilde_upref(Str32_IO str, Str32_I entry)
{
    Long ind0 = 0, N = 0;
    Str32 label, entry1, entry2;
    while (1) {
        ind0 = find_command(str, U"autoref", ind0);
        if (ind0 < 0)
            return N;
        command_arg(label, str, ind0);
        Long ind5 = ind0;
        ind0 = skip_command(str, ind0, 1);
        if (ind0 == size(str))
            return N;
        Long ind1 = expect(str, U"~", ind0);
        if (ind1 < 0) {
            if (expect(str, U"\\upref", ind0) > 0)
                throw Str32(U"\\autoref{} 和 \\upref{} 中间应该有 ~");
            Long ind2 = label.find(U'_');
            if (ind2 < 0)
                throw Str32(U"\\autoref{" + label + U"} 中必须有下划线， 请使用“内部引用”或“外部引用”按钮");
            ind5 = str.rfind(U"\\upref", ind5-1); entry2.clear();
            if (ind5 > 0 && ExpectKeyReverse(str, U"~", ind5 - 1) < 0)
                command_arg(entry2, str, ind5);
            Str32 tmp = label_entry_old(label);
            if (tmp != entry && tmp != entry2)
                throw Str32(U"\\autoref{" + label + U"} 引用其他词条时， 后面必须有 ~\\upref{}， 建议使用“外部引用”按钮； 也可以把 \\upref{} 放到前面");
            continue;
        }
        Long ind2 = expect(str, U"\\upref", ind1);
        if (ind2 < 0)
            throw Str32(U"\\autoref{} 后面不应该有单独的 ~");
        command_arg(entry1, str, ind2 - 6);
        if (label_entry_old(label) != entry1)
            throw Str32(U"\\autoref{" + label + U"}~\\upref{" + entry1 + U"} 不一致， 请使用“外部引用”按钮");
        str.erase(ind1-1, 1); // delete ~
        ind0 = ind2;
    }
}

// replace autoref with html link
// no comment allowed
// does not add link for \autoref inside eq environment (equation, align, gather)
// return number of autoref replaced, or -1 if failed
inline Long autoref(Str32_IO str, Str32_I entry, SQLite::Statement &stmt_select, SQLite::Statement &stmt_update_ref_by,
                    SQLite::Statement &stmt_select_fig, SQLite::Statement &stmt_update_ref_by_fig)
{
    Long ind0{}, ind1{}, ind2{}, ind3{}, N{}, ienv{};
    Bool inEq;
    Str32 entry1, label0, type, kind, newtab, file;
    vecStr32 envNames{U"equation", U"align", U"gather"};
    Str32 db_ref_by_str;
    Str ref_by_str;
    set<Str> ref_by;
    while (1) {
        newtab.clear(); file.clear();
        ind0 = find_command(str, U"autoref", ind0);
        if (is_in_tag(str, U"code", ind0)) {
            ++ind0; continue;
        }
        if (ind0 < 0)
            return N;
        inEq = index_in_env(ienv, ind0, envNames, str);
        ind1 = expect(str, U"{", ind0 + 8);
        if (ind1 < 0)
            throw Str32(U"\\autoref 变量不能为空");
        ind1 = NextNoSpace(entry1, str, ind1);
        ind2 = str.find('_', ind1);
        if (ind2 < 0)
            throw Str32(U"\\autoref 格式错误");
        ind3 = find_num(str, ind2);
        if (ind3 < 0)
            throw Str32(U"autoref 格式错误");
        Long ind30 = str.find('_', ind2 + 1);
        entry1 = str.substr(ind2 + 1, ind30 - ind2 - 1);
        type = str.substr(ind1, ind2 - ind1);
        if (!gv::is_eng) {
            if (type == U"eq") kind = U"式";
            else if (type == U"fig") kind = U"图";
            else if (type == U"def") kind = U"定义";
            else if (type == U"lem") kind = U"引理";
            else if (type == U"the") kind = U"定理";
            else if (type == U"cor") kind = U"推论";
            else if (type == U"ex") kind = U"例";
            else if (type == U"exe") kind = U"习题";
            else if (type == U"tab") kind = U"表";
            else if (type == U"sub") kind = U"子节";
            else if (type == U"lst") {
                kind = U"代码";
                SLS_WARN(u8(U"autoref lstlisting 功能未完成！"));
                ++ind0; continue;
            }
            else {
                throw Str32(U"\\label 类型错误， 必须为 eq/fig/def/lem/the/cor/ex/exe/tab/sub/lst 之一");
            }
        }
        else {
            if      (type == U"eq")  kind = U"eq. ";
            else if (type == U"fig") kind = U"fig. ";
            else if (type == U"def") kind = U"def. ";
            else if (type == U"lem") kind = U"lem. ";
            else if (type == U"the") kind = U"thm. ";
            else if (type == U"cor") kind = U"cor. ";
            else if (type == U"ex")  kind = U"ex. ";
            else if (type == U"exe") kind = U"exer. ";
            else if (type == U"tab") kind = U"tab. ";
            else if (type == U"sub") kind = U"sub. ";
            else if (type == U"lst") {
                kind = U"code. ";
                SLS_WARN(u8(U"autoref lstlisting 功能未完成！"));
                ++ind0; continue;
            }
            else
                throw Str32(U"\\label 类型错误， 必须为 eq/fig/def/lem/the/cor/ex/exe/tab/sub/lst 之一");
        }
        ind3 = str.find('}', ind3);
        label0 = str.substr(ind1, ind3 - ind1); trim(label0);
        SQLite::Statement *stmt;
        if (type == U"fig") {
            stmt_select_fig.bind(1, u8(label_id(label0)));
            stmt = &stmt_select_fig;
        } else {
            stmt_select.bind(1, u8(label0));
            stmt = &stmt_select;
        }

        if (!stmt->executeStep())
            throw Str32(U"\\autoref{} 中标签未找到： " + label0);
        Long db_label_order = int(stmt->getColumn(0));
        parse(ref_by, stmt->getColumn(1));
        stmt->reset();

        if (ref_by.insert(u8(entry)).second) {
            // inserted, update db
            join(ref_by_str, ref_by);
            stmt_update_ref_by.bind(1, ref_by_str);
            stmt_update_ref_by.bind(2,u8(label0));
            stmt_update_ref_by.exec(); stmt_update_ref_by.reset();
        }

        file = gv::url + entry1 + U".html";
        if (entry1 != entry) // reference another entry1
            newtab = U"target = \"_blank\"";
        if (!inEq)
            str.insert(ind3 + 1, U" </a>");
        str.insert(ind3 + 1, kind + U' ' + num2str32(db_label_order));
        if (!inEq)
            str.insert(ind3 + 1, U"<a href = \"" + file + U"#" + label0 + U"\" " + newtab + U">");
        str.erase(ind0, ind3 - ind0 + 1);
        ++N;
    }
}

// get a new label name for an environment for an entry
void new_label_name(Str32_O label, Str32_I envName, Str32_I entry, Str32_I str)
{
    Str32 label0;
    for (Long num = 1; ; ++num) {
        Long ind0 = 0;
        while (1) {
            label = envName + "_" + entry + "_" + num2str(num);
            ind0 = find_command(str, U"label", ind0);
            if (ind0 < 0)
                return; // label is unique
            command_arg(label0, str, ind0, 0);
            if (label == label0)
                break;
            ++ind0;
        }
    }
}

// check if a label exist, if not, add it
// ind is not the label number, but the displayed number
// if exist, return 1, output label
// if doesn't exist, return 0
// dry_run : don't actually modify tex file
Long check_add_label(Str32_O label, Str32_I entry, Str32_I type, Long ind, Bool_I dry_run = false)
{
    Long ind0 = 0;
    Str32 label0, newtab;

    SQLite::Database db(u8(gv::path_data + "scan.db"), SQLite::OPEN_READWRITE);

    while (1) {
        SQLite::Statement stmt_select(db,
            R"(SELECT "id" FROM "labels" WHERE "type"=? AND "entry"=? AND "order"=?;)");
        stmt_select.bind(1, u8(type));
        stmt_select.bind(2, u8(entry));
        stmt_select.bind(3, (int)ind);
        if (!stmt_select.executeStep())
            break;
        label = u32(stmt_select.getColumn(0));
        return 1;
    }

    // label does not exist
    if (type == U"fig")
        throw Str32(U"每个图片上传后都会自动创建 label， 如果没有请手动在 \\caption{} 后面添加。");

    // insert \label{} command
    Str32 full_name = gv::path_in + "/contents/" + entry + ".tex";
    if (!file_exist(full_name))
        throw Str32(U"文件不存在： " + entry + ".tex");
    Str32 str;
    read(str, full_name);

    // find comments
    Intvs intvComm;
    find_comments(intvComm, str, U"%");

    vecStr32 types = { U"eq", U"fig", U"def", U"lem",
        U"the", U"cor", U"ex", U"exe", U"tab", U"sub" };
    vecStr32 envNames = { U"equation", U"figure", U"definition", U"lemma",
        U"theorem", U"corollary", U"example", U"exercise", U"table"};

    Long idNum = search(type, types);
    if (idNum < 0)
        throw Str32(U"\\label 类型错误， 必须为 eq/fig/def/lem/the/cor/ex/exe/tab/sub 之一");
    
    // count environment display number starting at ind4
    Intvs intvEnv;
    if (type == U"eq") { // add equation labels
        // count equations
        ind0 = 0;
        Long idN = 0;
        vecStr32 eq_envs = { U"equation", U"gather", U"align" };
        Str32 env0;
        while (1) {
            ind0 = find_command(str, U"begin", ind0);
            if (ind0 < 0) {
                throw Str32(U"被引用公式不存在");
            }
            if (is_in(ind0, intvComm)) {
                ++ind0; continue;
            }
            command_arg(env0, str, ind0);
            Long ienv = search(env0, eq_envs);
            if (ienv < 0) {
                ++ind0; continue;
            }
            // found one of eq_envs
            ++idN;
            if (idN == ind) {
                new_label_name(label, type, entry, str);
                ind0 = skip_command(str, ind0, 1);
                if (!dry_run) {
                    str.insert(ind0, U"\\label{" + label + "}");
                    write(str, full_name);
                }
                break;
            }
            if (ienv > 0) {// found gather or align
                throw Str32(U"暂不支持引用含有 align 和 gather 环境的文件，请手动插入 label 并引用。");
                Long ind1 = skip_env(str, ind0);
                for (Long i = ind0; i < ind1; ++i) {
                    if (str.substr(i, 2) == U"\\\\" && current_env(i, str) == eq_envs[ienv]) {
                        ++idN;
                        if (idN == ind) {
                            new_label_name(label, type, entry, str);
                            ind0 = skip_command(str, ind0, 1);
                            if (!dry_run) {
                                str.insert(i + 2, U"\n\\label{" + label + "}");
                                write(str, full_name);
                            }
                            goto break2;
                        }
                    }        
                }
            }
            ++ind0;
        }
        break2: ;
    }
    else if (type == U"sub") { // add subsection labels
        Long ind0 = -1;
        for (Long i = 0; i < ind; ++i) {
            ind0 = find_command(str, U"subsection", ind0+1);
            if (ind0 < 0)
                throw Str32(U"被引用对象不存在");
        }
        ind0 = skip_command(str, ind0, 1);
        new_label_name(label, type, entry, str);
        if (!dry_run) {
            str.insert(ind0, U"\\label{" + label + "}");
            write(str, full_name);
        }
    }
    else { // add environment labels
        Long Nid = find_env(intvEnv, str, envNames[idNum], 'i');
        if (ind > Nid)
            throw Str32(U"被引用对象不存在");

        new_label_name(label, type, entry, str);
        // this doesn't work for figure invironment, since there is an [ht] option
        if (!dry_run) {
            str.insert(intvEnv.L(ind - 1), U"\\label{" + label + "}");
            write(str, full_name);
        }
    }
    SQLite::Statement stmt_insert(db,
        R"(INSERT INTO "labels" ("id", "type", "entry", "order") VALUES (?, ?, ?, ?);)");
    stmt_insert.bind(1, u8(label));
    stmt_insert.bind(2, u8(type));
    stmt_insert.bind(3, u8(entry));
    stmt_insert.bind(4, int(ind));
    stmt_insert.exec();
    return 0;
}

// calculate author list of an entry, based on "history" table counts in db
inline Str32 author_list(Str32_I entry, SQLite::Database &db)
{
    SQLite::Statement stmt_select(db, R"(SELECT "authors" FROM "entries" WHERE "id"=?;)");
    stmt_select.bind(1, u8(entry));
    if (!stmt_select.executeStep())
        throw Str32(U"内部错误： author_list(): 数据库中不存在词条： " + entry);

    vecLong author_ids;
    Str32 str = u32(stmt_select.getColumn(0));
    stmt_select.reset();
    if (str.empty())
        return U"待更新";
    parse(author_ids, str);

    vecStr32 authors;
    SQLite::Statement stmt_select2(db, R"(SELECT "name" FROM "authors" WHERE "id"=?;)");
    for (int id : author_ids) {
        stmt_select2.bind(1, id);
        if (!stmt_select2.executeStep())
            throw Str32("词条： " + entry + " 作者 id 不存在： " + num2str(id));
        authors.push_back(u32(stmt_select2.getColumn(0)));
        stmt_select2.reset();
    }
    join(str, authors, U"; ");
    return str;
}

// replace "\href{http://example.com}{name}"
// with <a href="http://example.com">name</a>
inline Long href(Str32_IO str)
{
    Long ind0 = 0, N = 0, tmp;
    Str32 name, url;
    while (1) {
        ind0 = find_command(str, U"href", ind0);
        if (ind0 < 0)
            return N;
        if (index_in_env(tmp, ind0, { U"equation", U"align", U"gather" }, str)) {
            ++ind0; continue;
        }
        command_arg(url, str, ind0, 0);
        command_arg(name, str, ind0, 1);
        if (url.substr(0, 7) != U"http://" &&
            url.substr(0, 8) != U"https://") {
            throw Str32(U"链接格式错误: " + url);
        }

        Long ind1 = skip_command(str, ind0, 2);
        str.replace(ind0, ind1 - ind0,
            "<a href=\"" + url + "\" target = \"_blank\">" + name + "</a>");
        ++N; ++ind0;
    }
}

// process upref
// path must end with '\\'
inline Long upref(Str32_IO str, Str32_I entry)
{
    Long ind0 = 0, right, N = 0;
    Str32 entry1;
    while (1) {
        ind0 = find_command(str, U"upref", ind0);
        if (ind0 < 0)
            return N;
        command_arg(entry1, str, ind0);
        if (entry1 == entry)
            throw Str32(U"不允许 \\upref{" + entry1 + U"} 本词条");
        trim(entry1);
        if (!file_exist(gv::path_in + U"contents/" + entry1 + U".tex")) {
            throw Str32(U"\\upref 引用的文件未找到： " + entry1 + U".tex");
        }
        right = skip_command(str, ind0, 1);
        str.replace(ind0, right - ind0,
                    U"<span class = \"icon\"><a href = \""
                    + gv::url + entry1 +
                    U".html\" target = \"_blank\"><i class = \"fa fa-external-link\"></i></a></span>");
        ++N;
    }
    return N;
}

// replace "\cite{}" with `[?]` cytation linke
inline Long cite(Str32_IO str, SQLite::Database &db)
{
    SQLite::Statement stmt_select(db,
        R"(SELECT "order", "details" from "bibliography" WHERE "id"=?;)");
    Long ind0 = 0, N = 0;
    Str32 bib_label;
    while (1) {
        ind0 = find_command(str, U"cite", ind0);
        if (ind0 < 0)
            return N;
        ++N;
        command_arg(bib_label, str, ind0);
        stmt_select.bind(1, u8(bib_label));
        if (!stmt_select.executeStep())
            throw Str32(U"文献 label 未找到（请检查并编译 bibliography.tex）：" + bib_label);
        Long ibib = (int)stmt_select.getColumn(0);
        // bib_detail = u32(stmt_select.getColumn(1));
        stmt_select.reset();
        Long ind1 = skip_command(str, ind0, 1);
        str.replace(ind0, ind1 - ind0, U" <a href=\"" + gv::url + "bibliography.html#"
            + num2str(ibib+1) + "\">[" + num2str(ibib+1) + "]</a> ");
    }
}

// get `entries` from "contents/*.tex" (ignore some)
// get `titles` from main.tex
// warn repeated `titles` or `entries` in main.tex (case insensitive)
// warn `entries` not in main.tex (including comments)
inline void entries_titles(vecStr32_O entries, vecStr32_O titles)
{
    // 文件夹中的词条文件（忽略不用编译的）
    entries.clear(); titles.clear();
    file_list_ext(entries, gv::path_in + "contents/", Str32(U"tex"), false);
    ignore_entries(entries);
    if (entries.size() == 0)
        throw Str32(U"未找到任何词条");
    sort_case_insens(entries);

    { // 检查同名词条文件（不区分大小写）
        vecStr32 entries_low;
        to_lower(entries_low, entries);
        Long ind = find_repeat(entries_low);
        if (ind >= 0)
            throw Str32(U"发现同名词条文件（不区分大小写）：" + entries[ind]);
    }

    Long ind0 = 0;
    titles.resize(entries.size());
    vecBool in_toc(entries.size(), false); // in main.tex
    vecBool in_toc_comment(entries.size(), false); // in main.tex but commented

    Str32 title, entry, str;
    read(str, gv::path_in + "main.tex");
    CRLF_to_LF(str);
    Str32 str0 = str;
    rm_comments(str); // remove comments
    if (str.empty()) str = U" ";
    if (str0.empty()) str = U" ";

    // go through uncommented entries in main.tex
    while (1) {
        ind0 = str.find(U"\\entry", ind0);
        if (ind0 < 0)
            break;

        // get chinese title and entry label
        command_arg(title, str, ind0);
        replace(title, U"\\ ", U" ");
        if (title.empty())
            throw Str32(U"main.tex 中词条中文名不能为空");
        command_arg(entry, str, ind0, 1);

        Long ind = search(entry, entries);
        if (ind < 0)
            throw Str32(U"main.tex 中词条文件 " + entry + U" 未找到");
        if (in_toc[ind])
            throw Str32(U"目录中出现重复词条：" + entry);
        in_toc[ind] = true;
        titles[ind] = title;
        ++ind0;
    }
    
    // check repeated titles
    for (Long i = 0; i < size(titles); ++i) {
        if (titles[i].empty())
            continue;
        for (Long j = i + 1; j < size(titles); ++j) {
            if (titles[i] == titles[j])
                throw Str32(U"目录中标题重复：" + titles[i]);
        }
    }

    // go through commented entries in main.tex
    ind0 = -1;
    while (1) {
        ind0 = str0.find(U"\\entry", ++ind0);
        if (ind0 < 0)
            break;
        // get entry label
        if (command_arg(entry, str0, ind0, 1) == -1) continue;
        trim(entry);
        if (entry.empty()) continue;
        Long ind = search(entry, entries);
        if (ind < 0 || in_toc[ind])
            continue;
        // found commented \entry{}{}
        in_toc_comment[ind] = true;
    }

    Bool warned = false;
    for (Long i = 0; i < size(entries); ++i) {
        if (!in_toc[i] && !in_toc_comment[i]) {
            if (!warned) {
                cout << u8"\n\n警告: 以下词条没有被 main.tex 提及（注释中也没有），但仍会被编译" << endl;
                warned = true;
            }
            read(str, gv::path_in + U"contents/" + entries[i] + ".tex");
            CRLF_to_LF(str);
            get_title(title, str);
            titles[i] = title;
            cout << std::setw(10) << std::left << entries[i]
                << std::setw(20) << std::left << title << endl;
        }
    }

    warned = false;
    for (Long i = 0; i < size(entries); ++i) {
        if (in_toc_comment[i]) {
            if (!warned) {
                cout << u8"\n\n警告: 以下词条在 main.tex 中被注释，但仍会被编译" << endl;
                warned = true;
            }
            read(str, gv::path_in + U"contents/" + entries[i] + ".tex");
            CRLF_to_LF(str);
            get_title(title, str);
            titles[i] = title;
            cout << std::setw(10) << std::left << entries[i]
                << std::setw(20) << std::left << title << endl;
        }
    }
    cout << endl;
}

// delete spaces around chinese punctuations
inline Long rm_punc_space(Str32_IO str)
{
    vecStr32 keys = { U"，", U"、", U"．", U"。", U"？", U"（", U"）", U"：", U"；", U"【", U"】", U"…"};
    Long ind0 = 0, N = 0, ikey;
    while (1) {
        ind0 = find(ikey, str, keys, ind0);
        if (ind0 < 0)
            return N;
        if (ind0 + 1 < size(str) && str[ind0 + 1] == U' ')
            str.erase(ind0 + 1, 1);
        if (ind0 > 0 && str[ind0 - 1] == U' ') {
            str.erase(ind0 - 1, 1); --ind0;
        }
        ++N; ++ind0;
    }
}

// [recycle] this problem is already fixed
// prevent line wraping before specific chars
// by using <span style="white-space:nowrap">...</space>
// consecutive chars can be grouped together
inline void puc_no_wrap(Str32_IO str)
{
    Str32 keys = U"，、．。！？）：”】", tmp;
    Long ind0 = 0, ind1;
    while (1) {
        ind0 = str.find_first_of(keys, ind0);
        if (ind0 < 0)
            break;
        if (ind0 == 0 || str[ind0 - 1] == U'\n') {
            ++ind0; continue;
        }
        ind1 = ind0 + 1; --ind0;
        while (search(str[ind1], keys) >= 0 && ind1 < size(str))
            ++ind1;
        tmp = U"<span style=\"white-space:nowrap\">" + str.substr(ind0, ind1 - ind0) + U"</span>";
        str.replace(ind0, ind1 - ind0, tmp);
        ind0 += tmp.size();
    }
}

// warn english punctuations , . " ? ( ) in normal text
// when replace is false, set error = true to throw error instead of console warning
// return the number replaced
inline Long check_normal_text_punc(Str32_IO str, Bool_I error, Bool_I replace = false)
{
    vecStr32 keys = {U",", U".", U"?", U"(", U":"};
    Str32 keys_rep = U"，。？（：";
    Intvs intvNorm;
    FindNormalText(intvNorm, str);
    Long ind0 = -1, ikey = -100, N = 0;
    while (1) {
        ind0 = find(ikey, str, keys, ind0 + 1);
        if (ind0 < 0)
            break;

        if (is_in(ind0, intvNorm)) {
            Long ind1 = str.find_last_not_of(U" ", ind0-1);
            if (ind1 >= 0 && is_chinese(str[ind1])) {
                str.find_first_not_of(U" ", ind0+1);
                // --- exceptions ---
                if (ikey == 1 && is_alphanum(str[ind0 + 1]))
                    continue; // e.g. "文件名.jpg"
                else if (ikey == 3) {
                    if (is_alphanum(str[ind0+1])) {
                        if (is_alphanum(str[ind0+2])) {
                            if (str[ind0+3] == U')')
                                continue; // e.g. "(a)", "(1)"
                        }
                        else if (str[ind0+2] == U')')
                            continue; // e.g. "(a1)", "(24)"
                    }
                }
                // --- end exceptions ---
                if (replace) {
                    str[ind0] = keys_rep[ikey]; ++N;
                    if (keys[ikey] == U"(") {
                        // replace possible matching ')' as well
                        Long ind1 = str.find_first_of(U"()", ind0);
                        if (ind1 > 0 && str[ind1] == U')' && is_in(ind1, intvNorm)) {
                            str[ind1] = U'）'; ++N;
                        }
                    }
                }
                else {
                    if (error)
                        throw Str32(U"正文中使用英文标点：“" + str.substr(ind0, 40) + U"”");
                    else
                        SLS_WARN(u8(U"正文中使用英文标点：“" + str.substr(ind0, 40) + U"”\n"));
                }
            }
        }
    }
    return N;
}

// check escape characters in normal text i.e. `& # _ ^`
inline Long check_normal_text_escape(Str32_IO str)
{
//    vecStr32 keys = {U"&", U"#", U"_", U"^"};
//    Intvs intvNorm;
//    FindNormalText(intvNorm, str);
//    Long ind0 = -1, ikey, N = 0;
//    while (1) {
//        ind0 = find(ikey, str, keys, ind0 + 1);
//        if (ind0 < 0)
//            break;
//        if (is_in(ind0, intvNorm))
//            if (ind0 > 0 && str[ind0-1] != '\\')
//                throw Str32(U"正文中出现非法字符： " + str.substr(ind0, 20));
//    }
//    return N;
    return 0;
}

// create table of content from main.tex
// path must end with '\\'
// return the number of entries
// names is a list of filenames
// titles[i] is the chinese title of entries[i]
// `entry_part[i]` is the part number of `entries[i]`, 0: no info, 1: the first part, etc.
// `entry_chap[i]` is similar to `entry_part[i]`, for chapters
// if entry_part.size() == 0, `chap_names, entry_chap, entry_part, part_names` will be ignored
inline void table_of_contents(
        vecStr32_O part_ids, vecStr32_O part_names, vecStr32_O chap_first, vecStr32_O chap_last,
        vecStr32_O chap_ids, vecStr32_O chap_names, vecLong_O chap_part, vecStr32_O chap_entry_first, vecStr32_O chap_entry_last,
        vecStr32_O entries, vecStr32_O titles, vecBool_O is_draft, vecLong_O entry_part, vecLong_O entry_chap, SQLite::Database &db)
{
    SQLite::Statement stmt_select(
            db,
            R"(SELECT "caption", "draft" FROM "entries" WHERE "id"=?;)"
            );

    Long ind0 = 0, ind1 = 0, ikey = -500, chapNo = -1, chapNo_tot = 0, partNo = 0;
    vecStr32 keys{ U"\\part", U"\\chapter", U"\\entry", U"\\bibli"};
    //keys.push_back(U"\\entry"); keys.push_back(U"\\chapter"); keys.push_back(U"\\part");
    
    Str32 title; // chinese entry name, chapter name, or part name
    Str32 entry; // entry label
    Str32 str, str2, toc, class_draft;

    part_ids.clear(); part_names.clear(); chap_first.clear(); chap_last.clear();
    chap_ids.clear(); chap_names.clear(); chap_part.clear();
    chap_entry_first.clear(); chap_entry_last.clear(); entry_chap.clear();

    part_ids.push_back(U""); part_names.push_back(U"无"); chap_first.push_back(U""); chap_last.push_back(U"");
    chap_ids.push_back(U""); chap_names.push_back(U"无");  chap_part.push_back(0);
    chap_entry_first.push_back(U""); chap_entry_last.push_back(U"");

    read(str, gv::path_in + "main.tex");
    CRLF_to_LF(str);
    read(toc, gv::path_out + "templates/index_template.html"); // read html template
    CRLF_to_LF(toc);
    ind0 = toc.find(U"PhysWikiHTMLbody", ind0);
    if (ind0 < 0)
        throw Str32(U"内部错误： index_template.html 中没有找到 PhysWikiHTMLbody");
    toc.erase(ind0, 16);

    rm_comments(str); // remove comments
    if (str.empty()) str = U" ";

    Char last_command = 'n'; // 'p': \part, 'c': \chapter, 'e': \entry

    while (1) {
        ind1 = find(ikey, str, keys, ind1);
        if (ind1 < 0)
            break;
        if (ikey == 2) {
            // ============ found "\entry" ==============
            if (last_command != 'e' && last_command != 'c')
                throw Str32(U"main.tex 中 \\entry{}{} 必须在 \\chapter{} 或者其他 \\entry{}{} 之后： " + title);
            if (last_command == 'c')
                chap_entry_last.push_back(entry);

            // parse \entry{title}{entry}
            command_arg(title, str, ind1);
            command_arg(entry, str, ind1, 1);
            replace(title, U"\\ ", U" ");
            if (title.empty())
                throw Str32(U"main.tex 中词条中文名不能为空");
            if (search(entry, entries) >= 0)
                throw Str32(U"main.tex 中词条重复： " + entry);
            entries.push_back(entry);
            if (last_command == 'c')
                chap_entry_first.push_back(entry);

            // get db info
            stmt_select.bind(1, u8(entry));
            if (!stmt_select.executeStep())
                throw Str32(U"数据库中找不到 main.tex 中词条： " + entry);
            titles.push_back(u32(stmt_select.getColumn(0)));
            is_draft.push_back((int)stmt_select.getColumn(1));
            stmt_select.reset();

            if (title != titles.back())
                throw Str32(U"目录标题 “" + title + U"” 与数据库中词条标题 “" + titles.back() + U"” 不符！");

            // insert entry into html table of contents
            class_draft = (is_draft.back()) ? U"class=\"draft\" " : U"";
            ind0 = insert(toc, U"<a " + class_draft + U"href = \"" + gv::url + entry + ".html" + "\" target = \"_blank\">"
                               + title + U"</a>　\n", ind0);

            entry_part.push_back(partNo);
            entry_chap.push_back(chapNo_tot);
            ++ind1; last_command = 'e';
        }
        else if (ikey == 1) {
            // ========== found "\chapter" =============
            // get chinese chapter name
            command_arg(title, str, ind1);
            replace(title, U"\\ ", U" ");
            if (last_command != 'p' && last_command != 'e') 
                throw Str32(U"main.tex 中 \\chapter{} 必须在 \\entry{}{} 或者 \\part{} 之后， 不允许空的 \\chapter{}： " + title);
            if (last_command == 'p' && chap_ids.size() > 1)
                chap_last.push_back(chap_ids.back());
            // get chapter id from label cpt_xxx, where xxx is id
            Long ind_label = find_command(str, U"label", ind1);
            Long ind_LF = str.find(U'\n', ind1);
            if (ind_label < 0 || ind_label > ind_LF)
                throw Str32(U"每一个 \\chapter{} 后面（同一行）必须要有 \\label{cpt_XXX}");
            Str32 label; command_arg(label, str, ind_label);
            chap_ids.push_back(label_id(label));
            if (last_command == 'p')
                chap_first.push_back(chap_ids.back());
            // insert chapter into html table of contents
            ++chapNo; ++chapNo_tot;
            if (chapNo > 0)
                ind0 = insert(toc, U"</p>", ind0);
            chap_names.push_back(title);
            chap_part.push_back(partNo);
            ind0 = insert(toc, U"\n\n<h3><b>第" + num2chinese(chapNo+1) + U"章 " + title
                + U"</b></h5>\n<div class = \"tochr\"></div><hr><div class = \"tochr\"></div>\n<p class=\"toc\">\n", ind0);
            ++ind1; last_command = 'c';
        }
        else if (ikey == 0) {
            // =========== found "\part" ===============
            // get chinese part name
            chapNo = -1;
            command_arg(title, str, ind1);
            replace(title, U"\\ ", U" ");
            if (last_command != 'e' && last_command != 'n')
                throw Str32(U"main.tex 中 \\part{} 必须在 \\entry{} 之后或者目录开始， 不允许空的 \\chapter{} 或 \\part{}： " + title);
            // get part id from label prt_xxx, where xxx is id
            Long ind_label = find_command(str, U"label", ind1);
            Long ind_LF = str.find(U'\n', ind1);
            if (ind_label < 0 || ind_label > ind_LF)
                throw Str32(U"每一个 \\part{} 后面（同一行）必须要有 \\label{prt_XXX}");
            Str32 label; command_arg(label, str, ind_label);
            part_ids.push_back(label_id(label));
            // insert part into html table of contents
            ++partNo;
            part_names.push_back(title);
            
            ind0 = insert(toc,
                U"</p></div>\n\n<div class = \"w3-container w3-center w3-teal w3-text-white\">\n"
                U"<h2 align = \"center\" style=\"padding-top: 0px;\" id = \"part"
                    + num2str32(partNo) + U"\">第" + num2chinese(partNo) + U"部分 " + title + U"</h3>\n"
                U"</div>\n\n<div class = \"w3-container\">\n"
                , ind0);
            ++ind1; last_command = 'p';
        }
        else if (ikey == 3) {
            // =========  found "\bibli" ==========
            title = U"参考文献";
            ind0 = insert(toc, U"<a href = \"" + gv::url + U"bibliography.html\" target = \"_blank\">"
                + title + U"</a>　\n", ind0);
            ++ind1;
        }
    }
    toc.insert(ind0, U"</p>\n</div>");

    chap_last.push_back(chap_ids.back());
    chap_entry_last.push_back(entry);

    // list parts
    ind0 = toc.find(U"PhysWikiPartList", 0);
    if (ind0 < 0)
        throw Str32(U"内部错误： PhysWikiPartList not found!");
    toc.erase(ind0, 16);
    ind0 = insert(toc, U"|", ind0);
    for (Long i = 1; i < size(part_names); ++i) {
        ind0 = insert(toc, U"   <a href = \"#part" + num2str32(i) + U"\">"
                           + part_names[i] + U"</a> |\n", ind0);
    }

    // write to index.html
    write(toc, gv::path_out + "index.html");
    Long kk = find_repeat(chap_ids);
    if (kk >= 0)
        throw Str32(U"\\chapter{} label 重复定义： " + chap_ids[kk]);
    kk = find_repeat(part_ids);
    if (kk >= 0)
        throw Str32(U"\\part{} label 重复定义： " + part_ids[kk]);
}

// add line numbers to the code (using table)
inline void code_table(Str32_O table_str, Str32_I code)
{
    Str32 line_nums;
    Long ind1 = 0, N = 0;
    while (1) {
        ++N;
        line_nums += num2str(N) + U"<br>";
        ind1 = code.find(U'\n', ind1);
        if (ind1 < 0)
            break;
        ++ind1;
    }
    line_nums.erase(line_nums.size() - 4, 4);
    table_str =
        U"<table class=\"code\"><tr class=\"code\"><td class=\"linenum\">\n"
        + line_nums +
        U"\n</td><td class=\"code\">\n"
        U"<div class = \"w3-code notranslate w3-pale-yellow\">\n"
        U"<div class = \"nospace\"><pre class = \"mcode\">\n"
        + code +
        U"\n</pre></div></div>\n"
        U"</td></tr></table>";
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

// get keywords from the comment in the second line
// return numbers of keywords found
// e.g. "关键词1|关键词2|关键词3"
inline Long get_keywords(vecStr32_O keywords, Str32_I str)
{
    keywords.clear();
    Str32 word;
    Long ind0 = str.find(U"\n", 0);
    if (ind0 < 0 || ind0 == size(str)-1)
        return 0;
    ind0 = expect(str, U"%", ind0+1);
    if (ind0 < 0) {
        // SLS_WARN(u8"请在第二行注释关键词： 例如 \"% 关键词1|关键词2|关键词3\"！");
        return 0;
    }
    Str32 line; get_line(line, str, ind0);
    Long tmp = line.find(U"|", 0);
    if (tmp < 0) {
        // SLS_WARN(u8"请在第二行注释关键词： 例如 \"% 关键词1|关键词2|关键词3\"！");
        return 0;
    }

    ind0 = 0;
    while (1) {
        Long ind1 = line.find(U"|", ind0);
        if (ind1 < 0)
            break;
        word = line.substr(ind0, ind1 - ind0);
        trim(word);
        if (word.empty())
            throw Str32(U"第二行关键词格式错误！ 正确格式为 “% 关键词1|关键词2|关键词3”");
        keywords.push_back(word);
        ind0 = ind1 + 1;
    }
    word = line.substr(ind0);
    trim(word);
    if (word.empty())
        throw Str32(U"第二行关键词格式错误！ 正确格式为 “% 关键词1|关键词2|关键词3”");
    keywords.push_back(word);
    return keywords.size();
}

// insert space between chinese and alpha-numeric characters
inline Long chinese_alpha_num_space(Str32_IO str)
{
    Long N = 0;
    for (Long i = size(str) - 1; i >= 1; --i) {
        if (is_chinese(str[i - 1]) && is_alphanum(str[i])) {
            str.insert(str.begin() + i, U' ');
            ++N;
        }
        if (is_alphanum(str[i - 1]) && is_chinese(str[i])) {
            str.insert(str.begin() + i, U' ');
            ++N;
        }
    }
    return N;
}

// ensure spaces outside of chinese double quotes, if it's next to a chinese character
// return the number of spaces added
inline Long chinese_double_quote_space(Str32_IO str)
{
    Str32 quotes = { Char32(8220) , Char32(8221) }; // chinese left and right quotes
    Intvs intNorm;
    FindNormalText(intNorm, str);
    vecLong inds; // locations to insert space
    Long ind = -1;
    while (1) {
        ind = str.find_first_of(quotes, ind + 1);
        if (ind < 0)
            break;
        if (is_in(ind, intNorm)) {
            if (str[ind] == quotes[0]) { // left quote
                if (ind > 0) {
                    Char32 c = str[ind - 1];
                    /*if (search(c, U" \n，。．！？…：()（）：【】") >= 0)
                        continue;*/
                    if (is_chinese(c) || is_alphanum(c))
                        inds.push_back(ind);
                }
            }
            else { // right quote
                if (ind < size(str) - 1) {
                    Char32 c = str[ind + 1];
                    /*if (search(c, U" \n，。．！？…：()（）：【】") >= 0)
                        continue;*/
                    if (is_chinese(c) || is_alphanum(c))
                        inds.push_back(ind + 1);
                }
                    
            }
        }
    }
    for (Long i = inds.size() - 1; i >= 0; --i) 
        str.insert(str.begin() + inds[i], U' ');
    return inds.size();
}

inline Long inline_eq_space(Str32_IO str)
{
    Long N = 0;
    Intvs intv;
    find_inline_eq(intv, str, 'o');
    for (Long i = intv.size() - 1; i >= 0; --i) {
        Long ind0 = intv.R(i) + 1;
        if (is_chinese(str[ind0])) {
            str.insert(str.begin() + ind0, U' ');
            ++N;
        }
        ind0 = intv.L(i) - 1;
        if (is_chinese(str[ind0])) {
            str.insert(str.begin() + ind0 + 1, U' ');
        }
    }
    return N;
}

// ensure space around a char
inline Long ensure_space_around(Str32_IO str, Char32_I c)
{
    Long N = 0;
    for (Long i = size(str) - 1; i >= 0; --i) {
        if (str[i] == c) {
            // check right
            if (i == size(str) - 1) {
                str += U" "; ++N;
            }
            else if (str[i + 1] != U' ') {
                str.insert(i + 1, U" "); ++N;
            }
            // check left
            if (i == 0 || str[i - 1] != U' ') {
                str.insert(i, U" "); ++N;
            }
        }
    }
    return N;
}

// replace "<" and ">" in equations
inline Long rep_eq_lt_gt(Str32_IO str)
{
    Long N = 0;
    Intvs intv, intv1;
    Str32 tmp;
    find_inline_eq(intv, str);
    find_display_eq(intv1, str); combine(intv, intv1);
    for (Long i = intv.size() - 1; i >= 0; --i) {
        Long ind0 = intv.L(i), Nstr = intv.R(i) - intv.L(i) + 1;
        tmp = str.substr(ind0, Nstr);
        N += ensure_space_around(tmp, U'<') + ensure_space_around(tmp, U'>');
        str.replace(ind0, Nstr, tmp);
    }
    return N;
}

// add equation tags
inline Long equation_tag(Str32_IO str, Str32_I nameEnv)
{
    Long page_width = 900;
    Long i{}, N{}, Nenv;
    Intvs intvEnvOut, intvEnvIn;
    Nenv = find_env(intvEnvIn, str, nameEnv, 'i');
    find_env(intvEnvOut, str, nameEnv, 'o');

    for (i = Nenv - 1; i >= 0; --i) {
        Long iname, width = page_width - 35;
        if (index_in_env(iname, intvEnvOut.L(i), { U"example", U"exercise", U"definition", U"theorem", U"lemma", U"corollary"}, str))
            width -= 40;
        if (index_in_env(iname, intvEnvOut.L(i), { U"itemize", U"enumerate" }, str))
            width -= 40;
        Str32 strLeft = U"<div class=\"eq\"><div class = \"w3-cell\" style = \"width:" + num2str32(width) + U"px\">\n\\begin{" + nameEnv + U"}";
        Str32 strRight = U"\\end{" + nameEnv + U"}\n</div></div>";
        str.replace(intvEnvIn.R(i) + 1, intvEnvOut.R(i) - intvEnvIn.R(i), strRight);
        str.replace(intvEnvOut.L(i), intvEnvIn.L(i) - intvEnvOut.L(i), strLeft);
    }
    return N;
}

// replace all wikipedia domain to another mirror domain
// return the number replaced
inline Long wikipedia_link(Str32_IO str)
{
    Str32 alter_domain = U"jinzhao.wiki";
    Long ind0 = 0, N = 0;
    Str32 link;
    while (1) {
        ind0 = find_command(str, U"href", ind0);
        if (ind0 < 0)
            return N;
        command_arg(link, str, ind0);
        ind0 = skip_command(str, ind0);
        ind0 = expect(str, U"{", ind0);
        if (ind0 < 0) throw Str32(U"\\href{网址}{文字} 命令格式错误！");
        Long ind1 = pair_brace(str, ind0 - 1);
        if (Long(link.find(U"wikipedia.org")) > 0) {
            replace(link, U"wikipedia.org", alter_domain);
            str.replace(ind0, ind1 - ind0, link);
        }
        ++N;
    }
}

inline Long subsections(Str32_IO str)
{
    Long ind0 = 0, N = 0;
    Str32 subtitle;
    while (1) {
        ind0 = find_command(str, U"subsection", ind0);
        if (ind0 < 0)
            return N;
        ++N;
        command_arg(subtitle, str, ind0);
        Long ind1 = skip_command(str, ind0, 1);
        str.replace(ind0, ind1 - ind0, U"<h2 class = \"w3-text-indigo\"><b>" + num2str32(N) + U". " + subtitle + U"</b></h2>");
    }
}

// if a position is in \pay...\paid
inline Bool ind_in_pay(Str32_I str, Long_I ind)
{
    Long ikey;
    Long ind0 = rfind(ikey, str, { U"\\pay", U"\\paid" }, ind);
    if (ind0 < 0)
        return false;
    else if (ikey == 1)
        return false;
    else if (ikey == 0)
        return true;
    else
        throw Str32(U"ind_in_pay(): unknown!");
}

// deal with "\pay"..."\paid"
inline Long pay2div(Str32_IO str)
{
    Long ind0 = 0, N = 0;
    while (1) {
        ind0 = find_command(str, U"pay", ind0);
        if (ind0 < 0)
            return N;
        ++N;
        str.replace(ind0, 4, U"<div id=\"pay\" style=\"display:inline\">");
        ind0 = find_command(str, U"paid", ind0);
        if (ind0 < 0)
            throw Str32(U"\\pay 命令没有匹配的 \\paid 命令");
        str.replace(ind0, 5, U"</div>");
    }
}

inline void last_next_buttons(Str32_IO html, SQLite::Database &db, Long_I order, Str32_I entry, Str32_I title)
{
    Str32 last_entry, last_title, last_url, next_entry, next_title, next_url;
    SQLite::Statement stmt_select(db,
        R"(SELECT "id", "caption" FROM "entries" WHERE "order"=?;)");

    if (order < 0)
        throw Str32(U"内部错误： last_next_buttons()");

    // find last
    if (order == 0) {
        last_entry = entry;
        last_title = U"没有上一篇了哟~（本文不在目录中）";
    }
    else if (order == 1) {
        last_entry = entry;
        last_title = U"没有上一篇了哟~";
    }
    else {
        stmt_select.bind(1, (int)order-1);
        if (!stmt_select.executeStep())
            throw Str32(U"内部错误： 找不到具有以下 order 的词条： " + num2str(order-1));
        else {
            last_entry = u32(stmt_select.getColumn(0));
            last_title = u32(stmt_select.getColumn(1));
        }
        stmt_select.reset();
    }

    // find next
    if (order == 0) {
        next_entry = entry;
        next_title = U"没有下一篇了哟~（本文不在目录中）";
    }
    else {
        stmt_select.bind(1, (int) order + 1);
        if (!stmt_select.executeStep()) {
            next_entry = entry;
            next_title = U"没有下一篇了哟~";
        } else {
            next_entry = u32(stmt_select.getColumn(0));
            next_title = u32(stmt_select.getColumn(1));
        }
        stmt_select.reset();
    }

    // insert into html
    if (replace(html, U"PhysWikiLastEntryURL", gv::url+last_entry+U".html") != 2)
        throw Str32(U"内部错误： \"PhysWikiLastEntry\" 在 entry_template.html 中数量不对");
    if (replace(html, U"PhysWikiNextEntryURL", gv::url+next_entry+U".html") != 2)
        throw Str32(U"内部错误： \"PhysWikiNextEntry\" 在 entry_template.html 中数量不对");
    if (replace(html, U"PhysWikiLastTitle", last_title) != 2)
        throw Str32(U"内部错误： \"PhysWikiLastTitle\" 在 entry_template.html 中数量不对");
    if (replace(html, U"PhysWikiNextTitle", next_title) != 2)
        throw Str32(U"内部错误： \"PhysWikiNextTitle\" 在 entry_template.html 中数量不对");
}

// generate html from a single tex
inline void PhysWikiOnline1(Str32_O title, vecStr32_O img_ids, vecLong_O img_orders, vecStr32_O img_hashes,
    Bool_O draft, vecStr32_O keywords, vecStr32_O labels, vecLong_O label_orders,
    vecStr32_O pentries, Str32_I entry, vecStr32_I rules, SQLite::Database &db)
{
    // insert a new entry (simulate what editor does)
    SQLite::Statement stmt_insert(db,
        R"(INSERT INTO "entries" ("id", "caption", "draft") VALUES (?, ?, 1);)");

    Str32 str;
    read(str, gv::path_in + "contents/" + entry + ".tex"); // read tex file
    CRLF_to_LF(str);

    // read title from first comment
    get_title(title, str);
    draft = is_draft(str);
    Str32 db_title, authors, db_keys_str, db_pentry_str;
    Long order, db_draft;
    SQLite::Statement stmt_select
            (db,
             R"(SELECT "caption", "authors", "order", "keys", "pentry", "draft" FROM "entries" WHERE "id"=?;)");
    stmt_select.bind(1, u8(entry));
    if (!stmt_select.executeStep()) {
        SLS_WARN(U"内部错误： 词条不存在数据库中， 将添加： " + entry);
        stmt_insert.bind(1, u8(entry));
        stmt_insert.bind(2, u8(title));
        stmt_insert.exec(); stmt_insert.reset();
        db_title = title;
        authors = U"待更新";
        order = 0; // 0 代表不在目录中
        db_draft = 2; // 2 代表未知
    } else {
        db_title = u32(stmt_select.getColumn(0));
        authors = u32(stmt_select.getColumn(1));
        order = (int)stmt_select.getColumn(2);
        db_keys_str = u32(stmt_select.getColumn(3));
        db_pentry_str = u32(stmt_select.getColumn(4));
        db_draft = (int)stmt_select.getColumn(5);
    }

    // read html template and \newcommand{}
    Str32 html;
    read(html, gv::path_out + "templates/entry_template.html");
    CRLF_to_LF(html);


    // check language: U"\n%%eng\n" at the end of file means english, otherwise chinese
    if ((size(str) > 7 && str.substr(size(str) - 7) == U"\n%%eng\n") ||
            gv::path_in.substr(gv::path_in.size()-4) == U"/en/")
        gv::is_eng = true;
    else
        gv::is_eng = false;

    // add keyword meta to html
    if (get_keywords(keywords, str) > 0) {
        Str32 keywords_str = keywords[0];
        for (Long i = 1; i < size(keywords); ++i) {
            keywords_str += U"," + keywords[i];
        }
        if (replace(html, U"PhysWikiHTMLKeywords", keywords_str) != 1)
            throw Str32(U"内部错误： \"PhysWikiHTMLKeywords\" 在 entry_template.html 中数量不对");
    }
    else {
        if (replace(html, U"PhysWikiHTMLKeywords", U"高中物理, 物理竞赛, 大学物理, 高等数学") != 1)
            throw Str32(U"内部错误： \"PhysWikiHTMLKeywords\" 在 entry_template.html 中数量不对");
    }

    // TODO: enable this when editor can do the rename in db
//    if (!db_title.empty() && title != db_title)
//        throw Str32(U"检测到标题改变（" + db_title + U" ⇒ " + title + U"）， 请使用重命名按钮修改标题");

    // insert HTML title
    if (replace(html, U"PhysWikiHTMLtitle", title) != 1)
        throw Str32(U"内部错误： \"PhysWikiHTMLtitle\" 在 entry_template.html 中数量不对");

    // check globally forbidden char
    global_forbid_char(str);
    // save and replace verbatim code with an index
    vecStr32 str_verb;
    verbatim(str_verb, str);
    // wikipedia_link(str);
    rm_comments(str); // remove comments
    limit_env_cmd(str);
    if (!gv::is_eng)
        autoref_space(str, true); // set true to error instead of warning
    autoref_tilde_upref(str, entry);
    if (str.empty()) str = U" ";
    // ensure spaces between chinese char and alphanumeric char
    chinese_alpha_num_space(str);
    // ensure spaces outside of chinese double quotes
    chinese_double_quote_space(str);
    // check escape characters in normal text i.e. `& # _ ^`
    check_normal_text_escape(str);
    // check non ascii char in equations (except in \text)
    check_eq_ascii(str);
    // forbid empty lines in equations
    check_eq_empty_line(str);
    // add spaces around inline equation
    inline_eq_space(str);
    // replace "<" and ">" in equations
    rep_eq_lt_gt(str);
    // escape characters
    NormalTextEscape(str);
    // add paragraph tags
    paragraph_tag(str);
    // add html id for links
    EnvLabel(labels, label_orders, entry, str);
    // replace environments with html tags
    equation_tag(str, U"equation"); equation_tag(str, U"align"); equation_tag(str, U"gather");
    // itemize and enumerate
    Itemize(str); Enumerate(str);
    // process table environments
    Table(str);
    // ensure '<' and '>' has spaces around them
    EqOmitTag(str);
    // process example and exercise environments
    theorem_like_env(str);
    // process figure environments
    FigureEnvironment(img_ids, img_orders, img_hashes, str, entry);
    // get dependent entries from \pentry{}
    depend_entry(pentries, str, db);
    // issues environment
    issuesEnv(str);
    addTODO(str);
    // process \pentry{}
    pentry(str);
    // replace user defined commands
    newcommand(str, rules);
    subsections(str);
    // replace \name{} with html tags
    Command2Tag(U"subsubsection", U"<h3><b>", U"</b></h3>", str);
    Command2Tag(U"bb", U"<b>", U"</b>", str); Command2Tag(U"textbf", U"<b>", U"</b>", str);
    Command2Tag(U"textsl", U"<i>", U"</i>", str);
    pay2div(str); // deal with "\pay" "\paid" pseudo command
    // replace \upref{} with link icon
    upref(str, entry);
    href(str); // hyperlinks
    cite(str, db); // citation
    
    // footnote
    footnote(str, entry, gv::url);
    // delete redundent commands
    replace(str, U"\\dfracH", U"");
    // remove spaces around chinese punctuations
    rm_punc_space(str);
    // verbatim recover (in inverse order of `verbatim()`)
    lstlisting(str, str_verb);
    lstinline(str, str_verb);
    verb(str, str_verb);

    Command2Tag(U"x", U"<code>", U"</code>", str);
    // insert body Title
    if (replace(html, U"PhysWikiTitle", title) != 1)
        throw Str32(U"内部错误： \"PhysWikiTitle\" 在 entry_template.html 中数量不对");
    // insert HTML body
    if (replace(html, U"PhysWikiHTMLbody", str) != 1)
        throw Str32(U"\"PhysWikiHTMLbody\" 在 entry_template.html 中数量不对");
    if (replace(html, U"PhysWikiEntry", entry) != (gv::is_wiki? 8:2))
        throw Str32(U"内部错误： \"PhysWikiEntry\" 在 entry_template.html 中数量不对");

    last_next_buttons(html, db, order, entry, title);

    if (replace(html, U"PhysWikiAuthorList", author_list(entry, db)) != 1)
        throw Str32(U"内部错误： \"PhysWikiAuthorList\" 在 entry_template.html 中数量不对");;

    // save html file
    write(html, gv::path_out + entry + ".html.tmp");

    // update db
    Str32 key_str; join(key_str, keywords, U"|");
    Str32 pentry_str; join(pentry_str, pentries);
    if (title != db_title || key_str != db_keys_str
        || draft != db_draft || pentry_str != db_pentry_str) {
        SQLite::Statement stmt_update
                (db,
                 R"(UPDATE "entries" SET "caption"=?, "keys"=?, "draft"=?, "pentry"=? )"
                 R"(WHERE "id"=?;)");
        stmt_update.bind(1, u8(title));
        stmt_update.bind(2, u8(key_str));
        stmt_update.bind(3, (int) draft);
        stmt_update.bind(4, u8(pentry_str));
        stmt_update.bind(5, u8(entry));
        stmt_update.exec(); stmt_update.reset();
    }
}

// get table of content info from db "chapters", in ascending "order"
inline void db_get_chapters(vecStr32_O ids, vecStr32_O captions, vecStr32_O parts,
                            SQLite::Database &db)
{
    SQLite::Statement stmt(db,
                           R"(SELECT "id", "order", "caption", "part" FROM "chapters" ORDER BY "order" ASC)");
    vecLong orders;
    while (stmt.executeStep()) {
        ids.push_back(u32(stmt.getColumn(0)));
        orders.push_back((int)stmt.getColumn(1));
        captions.push_back(u32(stmt.getColumn(2)));
        parts.push_back(u32(stmt.getColumn(3)));
    }
    for (Long i = 0; i < size(orders); ++i)
        if (orders[i] != i)
            SLS_ERR("something wrong!");
}

// get table of content info from db "parts", in ascending "order"
inline void db_get_parts(vecStr32_O ids, vecStr32_O captions, SQLite::Database &db)
{
    SQLite::Statement stmt(db,
                           R"(SELECT "id", "order", "caption" FROM "parts" ORDER BY "order" ASC)");
    vecLong orders;
    while (stmt.executeStep()) {
        ids.push_back(u32(stmt.getColumn(0)));
        orders.push_back((int)stmt.getColumn(1));
        captions.push_back(u32(stmt.getColumn(2)));
    }
    for (Long i = 0; i < size(orders); ++i)
        if (orders[i] != i)
            SLS_ERR("something wrong!");
}

// get dependency tree from database
// edge direction is the learning direction
inline void db_get_tree(vector<DGnode> &tree, vecStr32_O entries, vecStr32_O titles,
                        vecStr32_O parts, vecStr32_O chapters, SQLite::Database &db)
{
    tree.clear(); entries.clear(); titles.clear(); parts.clear(); chapters.clear();
    SQLite::Statement stmt_select(
            db,
            R"(SELECT "id", "caption", "part", "chapter", "pentry" FROM "entries" WHERE "deleted" = 0 ORDER BY "id" COLLATE NOCASE ASC;)");
    Str32 pentry_str;
    vector<vecStr32> pentries;

    while (stmt_select.executeStep()) {
        entries.push_back(u32(stmt_select.getColumn(0)));
        titles.push_back(u32(stmt_select.getColumn(1)));
        parts.push_back(u32(stmt_select.getColumn(2)));
        chapters.push_back(u32(stmt_select.getColumn(3)));
        pentry_str = u32(stmt_select.getColumn(4));
        pentries.emplace_back();
        parse(pentries.back(), pentry_str);
    }
    tree.resize(entries.size());

    // construct tree
    for (Long i = 0; i < size(entries); ++i) {
        for (auto &pentry : pentries[i]) {
            Long from = search(pentry, entries);
            if (from < 0) {
                throw Str32(U"内部错误： 预备知识未找到（应该已经在 PhysWikiOnline1() 中检查了不会发生才对： " + pentry + U" -> " + entries[i]);
            }
            tree[from].push_back(i);
        }
    }

    // check cyclic
    vecLong cycle;
    if (!dag_check(cycle, tree)) {
        Str32 msg = U"存在循环预备知识: ";
        for (auto ind : cycle)
            msg += to_string(ind) + "." + titles[ind] + " (" + entries[ind] + ") -> ";
        msg += titles[cycle[0]] + " (" + entries[cycle[0]] + ")";
        throw msg;
    }

    // check redundency
    vector<pair<Long,Long>> edges;
    dag_reduce(edges, tree);
    std::stringstream ss;
    if (size(edges)) {
        ss << u8"\n" << endl;
        ss << u8"==============  多余的预备知识  ==============" << endl;
        for (auto &edge : edges) {
            ss << titles[edge.first] << " (" << entries[edge.first] << ") -> "
                 << titles[edge.second] << " (" << entries[edge.second] << ")" << endl;
        }
        ss << u8"=============================================\n" << endl;
        throw ss.str();
    }
}

// get dependency tree from database, for 1 entry
// also check for cycle, and check for any of it's pentries are redundant
// entries[0] (tree[0]) will be `entry`
inline void db_get_tree1(vector<DGnode> &tree, vecStr32_O entries, vecStr32_O titles,
                        vecStr32_O parts, vecStr32_O chapters, Str32_I entry, SQLite::Database &db)
{
    tree.clear(); entries.clear(); titles.clear(); parts.clear(); chapters.clear();

    SQLite::Statement stmt_select(
            db,
            R"(SELECT "caption", "part", "chapter", "pentry" FROM "entries" WHERE "id" = ?;)");

    Str pentry_str, e;
    vector<vecStr> pentries;
    deque<Str> q; q.push_back(u8(entry));

    // broad first search (BFS)
    while (!q.empty()) {
        e = q.front(); q.pop_front();
        stmt_select.bind(1, e);
        SLS_ASSERT(stmt_select.executeStep());
        entries.push_back(u32(e));
        titles.push_back(u32(stmt_select.getColumn(0)));
        parts.push_back(u32(stmt_select.getColumn(1)));
        chapters.push_back(u32(stmt_select.getColumn(2)));
        pentry_str = (const char*)stmt_select.getColumn(3);
        stmt_select.reset();
        pentries.emplace_back();
        parse(pentries.back(), pentry_str);
        for (auto &pentry : pentries.back()) {
            if (search(u32(pentry), entries) < 0
                && find(q.begin(), q.end(), pentry) == q.end())
                q.push_back(pentry);
        }
    }
    tree.resize(entries.size());

    // construct tree
    for (Long i = 0; i < size(entries); ++i) {
        for (auto &pentry : pentries[i]) {
            Long from = search(u32(pentry), entries);
            if (from < 0)
                throw Str32(U"内部错误： 预备知识未找到（应该已经在 PhysWikiOnline1() 中检查了不会发生才对： "
                    + pentry + U" -> " + entries[i]);
            tree[from].push_back(i);
        }
    }

    //    // debug: print edges
    //    for (Long i = 0; i < size(tree); ++i)
    //        for (auto &j : tree[i])
    //            cout << i << "." << titles[i] << "(" << entries[i] << ") => "
    //                 << j << "." << titles[j] << "(" << entries[j] << ")" << endl;

    // check cyclic
    vecLong cycle;
    if (!dag_check(cycle, tree)) {
        Str32 msg = U"存在循环预备知识: ";
        for (auto ind : cycle)
            msg += to_string(ind) + "." + titles[ind] + " (" + entries[ind] + ") -> ";
        msg += titles[cycle[0]] + " (" + entries[cycle[0]] + ")";
        throw msg;
    }

    // check redundancy
    dag_inv(tree);
    vector<vecLong> alt_paths;
    dag_reduce(alt_paths, tree, 0);
    std::stringstream ss;
    if (size(alt_paths)) {
        for (Long j = 0; j < size(alt_paths); ++j) {
            auto &alt_path = alt_paths[j];
            Long i_beg = alt_path[0], i_bak = alt_path.back();
            ss << u8"\n\n存在多余的预备知识： " << titles[i_bak] << "(" << entries[i_bak] << ")" << endl;
            ss << "   已存在路径： " << endl;
            ss << "   " << titles[i_beg] << "(" << entries[i_beg] << ")" << endl;
            for (Long i = 1; i < size(alt_path); ++i)
                ss << "<- " << titles[alt_path[i]] << "(" << entries[alt_path[i]] << ")" << endl;
        }
        throw ss.str();
    }
    dag_inv(tree);
}

// generate json file containing dependency tree
// empty elements of 'titles' will be ignored
inline Long dep_json(SQLite::Database &db)
{
    vecStr32 entries, titles, entry_chap, entry_part, chap_ids, chap_names, chap_parts, part_ids, part_names;
    vector<DGnode> tree;
    db_get_parts(part_ids, part_names, db);
    db_get_chapters(chap_ids, chap_names, chap_parts, db);
    db_get_tree(tree, entries, titles,entry_part, entry_chap, db);

    Str32 str;
    // write part names
    str += U"{\n  \"parts\": [\n";
    for (Long i = 0; i < size(part_names); ++i)
        str += U"    {\"name\": \"" + part_names[i] + "\"},\n";
    str.pop_back(); str.pop_back();
    str += U"\n  ],\n";
    // write chapter names
    str += U"  \"chapters\": [\n";
    for (Long i = 0; i < size(chap_names); ++i)
        str += U"    {\"name\": \"" + chap_names[i] + "\"},\n";
    str.pop_back(); str.pop_back();
    str += U"\n  ],\n";
    // write entries
    str += U"  \"nodes\": [\n";
    for (Long i = 0; i < size(titles); ++i) {
        if (titles[i].empty())
            continue;
        str += U"    {\"id\": \"" + entries[i] + U"\"" +
               ", \"part\": " + num2str32(search(entry_part[i], part_ids)) +
               ", \"chap\": " + num2str32(search(entry_chap[i], chap_ids)) +
               ", \"title\": \"" + titles[i] + U"\""
            ", \"url\": \"../online/" +
            entries[i] + ".html\"},\n";
    }
    str.pop_back(); str.pop_back();
    str += U"\n  ],\n";

    // write links
    str += U"  \"links\": [\n";
    Long Nedge = 0;
    for (Long i = 0; i < size(tree); ++i) {
        for (auto &j : tree[i]) {
            str += U"    {\"source\": \"" + entries[i] + "\", ";
            str += U"\"target\": \"" + entries[j] + U"\", ";
            str += U"\"value\": 1},\n";
            ++Nedge;
        }
    }
    if (Nedge > 0) {
        str.pop_back(); str.pop_back();
    }
    str += U"\n  ]\n}\n";
    write(str, gv::path_out + U"../tree/data/dep.json");
    return 0;
}

inline Long bibliography(vecStr32_O bib_labels, vecStr32_O bib_details)
{
    Long N = 0;
    Str32 str, bib_list, bib_label;
    bib_labels.clear(); bib_details.clear();
    read(str, gv::path_in + U"bibliography.tex");
    CRLF_to_LF(str);
    Long ind0 = 0;
    while (1) {
        ind0 = find_command(str, U"bibitem", ind0);
        if (ind0 < 0) break;
        command_arg(bib_label, str, ind0);
        bib_labels.push_back(bib_label);
        ind0 = skip_command(str, ind0, 1);
        ind0 = expect(str, U"\n", ind0);
        bib_details.push_back(getline(str, ind0));
        href(bib_details.back()); Command2Tag(U"textsl", U"<i>", U"</i>", bib_details.back());
        ++N;
        bib_list += U"<span id = \"" + num2str32(N) + "\"></span>[" + num2str32(N) + U"] " + bib_details.back() + U"<br>\n";
    }
    Long ret = find_repeat(bib_labels);
    if (ret > 0)
        throw Str32(U"文献 label 重复：" + bib_labels[ret]);
    Str32 html;
    read(html, gv::path_out + U"templates/bib_template.html");
    replace(html, U"PhysWikiBibList", bib_list);
    write(html, gv::path_out + U"bibliography.html");
    return N;
}

// update db entries.authors, based on backup count in "history"
// TODO: use more advanced algorithm, counting other factors
inline void db_update_authors1(vecLong &author_ids, vecLong &minutes, Str32_I entry, SQLite::Database &db)
{
    author_ids.clear(); minutes.clear();
    SQLite::Statement stmt_count(db,
        R"(SELECT "author", COUNT(*) as record_count FROM "history" WHERE "entry"=? GROUP BY "author" ORDER BY record_count DESC;)");
    SQLite::Statement stmt_select(db,
        R"(SELECT "hide" FROM "authors" WHERE "id"=? AND "hide"=1;)");

    try {
        stmt_count.bind(1, u8(entry));
    } catch (std::exception &e) {
        cout << "SQLiteCpp: bind: " << e.what() << endl;
        throw e;
    }
    while (stmt_count.executeStep()) {
        Long id = (int)stmt_count.getColumn(0);
        Long time = 5*(int)stmt_count.getColumn(1);
        // 十分钟以上才算作者
        if (time <= 10) continue;
        // 检查是否隐藏作者
        stmt_select.bind(1, int(id));
        bool hidden = stmt_select.executeStep();
        stmt_select.reset();
        if (hidden) continue;
        author_ids.push_back(id);
        minutes.push_back(time);
    }
    stmt_count.reset();
    Str32 str;
    join(str, author_ids);
    SQLite::Statement stmt_update(db,
        R"(UPDATE "entries" SET "authors"=? WHERE "id"=?;)");
    stmt_update.bind(1, u8(str));
    stmt_update.bind(2, u8(entry));
    stmt_update.exec();
    SLS_ASSERT(stmt_update.getChanges() > 0);
    stmt_update.reset();
}

// update all authors
inline void db_update_authors(SQLite::Database &db)
{
    cout << "updating database for author lists...." << endl;
    SQLite::Statement stmt_select( db,
        R"(SELECT "id" FROM "entries";)");
    Str32 entry; vecLong author_ids, counts;
    while (stmt_select.executeStep()) {
        entry = u32(stmt_select.getColumn(0));
        db_update_authors1(author_ids, counts, entry, db);
    }
    stmt_select.reset();
    cout << "done!" << endl;
}

// update "bibliography" table of sqlite db
inline void db_update_bib(vecStr32_I bib_labels, vecStr32_I bib_details) {
    SQLite::Database db(u8(gv::path_data + "scan.db"), SQLite::OPEN_READWRITE);
    vector<vecStr> db_data0;
    check_foreign_key(db.getHandle());
    
    get_matrix(db_data0, db.getHandle(), "bibliography", {"id", "details"});
    unordered_map<Str, Str> db_data;
    for (auto &row : db_data0)
        db_data[row[0]] = row[1];

    SQLite::Statement stmt_insert_bib(db,
        R"(INSERT INTO bibliography ("id", "order", "details") VALUES (?, ?, ?);)");

    for (Long i = 0; i < size(bib_labels); i++) {
        Str bib_label = u8(bib_labels[i]), bib_detail = u8(bib_details[i]);
        if (db_data.count(bib_label)) {
            if (db_data[bib_label] != bib_detail) {
                SLS_WARN("数据库文献详情 bib_detail 发生变化需要更新！ 该功能未实现，将不更新： " + bib_label);
                cout << "数据库的 bib_detail： " << db_data[bib_label] << endl;
                cout << "当前的   bib_detail： " << bib_detail << endl;
            }
            continue;
        }
        SLS_WARN("数据库中不存在文献（将添加）： " + num2str(i) + ". " + bib_label);
        stmt_insert_bib.bind(1, bib_label);
        stmt_insert_bib.bind(2, int(i));
        stmt_insert_bib.bind(3, bib_detail);
        stmt_insert_bib.exec(); stmt_insert_bib.reset();
    }
}

// delete and rewrite "chapters" and "parts" table of sqlite db
inline void db_update_parts_chapters(
        vecStr32_I part_ids, vecStr32_I part_name, vecStr32_I chap_first, vecStr32_I chap_last,
        vecStr32_I chap_ids, vecStr32_I chap_name, vecLong_I chap_part,
        vecStr32_I entry_first, vecStr32_I entry_last)
{
    cout << "updating sqlite database (" << part_name.size() << " parts, "
         << chap_name.size() << " chapters) ..." << endl;
    cout.flush();
    SQLite::Database db(u8(gv::path_data + "scan.db"), SQLite::OPEN_READWRITE);
    cout << "clear parts and chatpers tables" << endl;
    table_clear(db.getHandle(), "parts");
    table_clear(db.getHandle(), "chapters");

    // insert parts
    SQLite::Statement stmt_insert_part(db,
        R"(INSERT INTO "parts" ("id", "order", "caption", "chap_first", "chap_last") VALUES (?, ?, ?, ?, ?);)");
    for (Long i = 0; i < size(part_name); ++i) {
        // cout << "part " << i << ". " << part_ids[i] << ": " << part_name[i] << " chapters: " << chap_first[i] << " -> " << chap_last[i] << endl;
        stmt_insert_part.bind(1, u8(part_ids[i]));
        stmt_insert_part.bind(2, int(i));
        stmt_insert_part.bind(3, u8(part_name[i]));
        stmt_insert_part.bind(4, u8(chap_first[i]));
        stmt_insert_part.bind(5, u8(chap_last[i]));
        stmt_insert_part.exec(); stmt_insert_part.reset();
    }
    cout << "\n\n\n" << endl;

    // insert chapters
    SQLite::Statement stmt_insert_chap(db,
        R"(INSERT INTO "chapters" ("id", "order", "caption", "part", "entry_first", "entry_last") VALUES (?, ?, ?, ?, ?, ?);)");

    for (Long i = 0; i < size(chap_name); ++i) {
        // cout << "chap " << i << ". " << chap_ids[i] << ": " << chap_name[i] << " chapters: " << entry_first[i] << " -> " << entry_last[i] << endl;
        stmt_insert_chap.bind(1, u8(chap_ids[i]));
        stmt_insert_chap.bind(2, int(i));
        stmt_insert_chap.bind(3, u8(chap_name[i]));
        stmt_insert_chap.bind(4, u8(part_ids[chap_part[i]]));
        stmt_insert_chap.bind(5, u8(entry_first[i]));
        stmt_insert_chap.bind(6, u8(entry_last[i]));
        stmt_insert_chap.exec(); stmt_insert_chap.reset();
    }
    cout << "done." << endl;
}

// update "entries" table of sqlite db, based on the info from main.tex
// `entries` are a list of entreis from main.tex
inline void db_update_entries_from_toc(
        vecStr32_I entries, vecLong_I entry_part, vecStr32_I part_ids,
        vecLong_I entry_chap, vecStr32_I chap_ids)
{
    cout << "updating sqlite database (" <<
        entries.size() << " entries) with info from main.tex..." << endl; cout.flush();
    SQLite::Database db(u8(gv::path_data + "scan.db"), SQLite::OPEN_READWRITE);

    SQLite::Statement stmt_update(db,
        R"(UPDATE "entries" SET "part"=?, "chapter"=?, "order"=? WHERE "id"=?;)");
    
    for (Long i = 0; i < size(entries); i++) {
        Long entry_order = i + 1;
        Str entry = u8(entries[i]);

        // does entry already exist (expected)?
        Bool entry_exist;
        entry_exist = exist(db.getHandle(), "entries", "id", entry);
        if (entry_exist) {
            // check if there is any change (unexpected)
            vecStr row_str; vecLong row_int;
            get_row(row_str, db.getHandle(), "entries", "id", entry, {"caption", "keys", "pentry", "part", "chapter"});
            get_row(row_int, db.getHandle(), "entries", "id", entry, {"order", "draft", "deleted"});
            bool changed = false;
            if (part_ids[entry_part[i]] != u32(row_str[3])) {
                SLS_WARN(entry + ": part has changed from " + row_str[3]+ " to " + part_ids[entry_part[i]]);
                changed = true;
            }
            if (chap_ids[entry_chap[i]] != u32(row_str[4])) {
                SLS_WARN(entry + ": chapter has changed from " + row_str[4] + " to " + chap_ids[entry_chap[i]]);
                changed = true;
            }
            if (entry_order != row_int[0]) {
                SLS_WARN(entry + ": order has changed from " + to_string(row_int[0]) + " to " + to_string(entry_order));
                changed = true;
            }
            if (changed) {
                stmt_update.bind(1, u8(part_ids[entry_part[i]]));
                stmt_update.bind(2, u8(chap_ids[entry_chap[i]]));
                stmt_update.bind(3, (int) entry_order);
                stmt_update.bind(4, entry);
                stmt_update.exec(); stmt_update.reset();
            }
        }
        else // entry_exist == false
            throw Str32(U"main.tex 中的词条在数据库中未找到： " + entry);
    }
    cout << "done." << endl;
}

// updating sqlite database "authors" and "history" table from backup files
inline void db_update_author_history(Str32_I path, SQLite::Database &db)
{
    vecStr32 fnames;
    unordered_map<Str, Long> new_authors;
    Str author;
    Str sha1, time, entry;
    file_list_ext(fnames, path, U"tex", false);
    cout << "updating sqlite database \"history\" table (" << fnames.size()
        << " backup) ..." << endl; cout.flush();

    // update "history" table
    check_foreign_key(db.getHandle());

    vector<vecStr> db_data10;
    
    vecLong db_data20;
    get_matrix(db_data10, db.getHandle(), "history", {"hash","time","entry"});
    get_column(db_data20, db.getHandle(), "history", "author");
    Long id_max = db_data20.empty() ? -1 : max(db_data20);

    cout << "there are already " << db_data10.size() << " backup (history) records in database." << endl;

    //            hash       time author entry
    unordered_map<Str, tuple<Str, Long,    Str>> db_data;
    for (Long i = 0; i < size(db_data10); ++i)
        db_data[db_data10[i][0]] = make_tuple(db_data10[i][1], db_data20[i], db_data10[i][2]);
    db_data10.clear(); db_data20.clear();
    db_data10.shrink_to_fit(); db_data20.shrink_to_fit();

    vecStr db_authors0; vecLong db_author_ids0;
    get_column(db_author_ids0, db.getHandle(), "authors", "id");
    get_column(db_authors0, db.getHandle(), "authors", "name");
    cout << "there are already " << db_author_ids0.size() << " author records in database." << endl;
    unordered_map<Long, Str> db_id_to_author;
    unordered_map<Str, Long> db_author_to_id;
    for (Long i = 0; i < size(db_author_ids0); ++i) {
        db_id_to_author[db_author_ids0[i]] = db_authors0[i];
        db_author_to_id[db_authors0[i]] = db_author_ids0[i];
    }

    db_author_ids0.clear(); db_authors0.clear();
    db_author_ids0.shrink_to_fit(); db_authors0.shrink_to_fit();

    SQLite::Statement stmt_insert(db,
        R"(INSERT INTO history ("hash", "time", "author", "entry") VALUES (?, ?, ?, ?);)");

    vecStr entries0;
    get_column(entries0, db.getHandle(), "entries", "id");
    unordered_set<Str> entries(entries0.begin(), entries0.end()), entries_deleted_inserted;
    entries0.clear(); entries0.shrink_to_fit();

    // insert a deleted entry (to ensure FOREIGN KEY exist)
    SQLite::Statement stmt_insert_entry(db,
        R"(INSERT INTO entries ("id", "deleted") VALUES (?, 1);)");

    // insert new_authors to "authors" table
    SQLite::Statement stmt_insert_auth(db,
        R"(INSERT INTO authors ("id", "name") VALUES (?, ?);)");

    for (Long i = 0; i < size(fnames); ++i) {
        auto &fname = fnames[i];
        sha1 = sha1sum_f(u8(path + fname) + ".tex").substr(0, 16);
        bool sha1_exist = db_data.count(sha1);

        time = u8(fname.substr(0, 12));
        Long ind = fname.rfind('_');
        author = u8(fname.substr(13, ind-13));
        Long authorID;
        if (db_author_to_id.count(author))
            authorID = db_author_to_id[author];
        else if (new_authors.count(author))
            authorID = new_authors[author];
        else {
            authorID = ++id_max;
            SLS_WARN("备份文件中的作者不在数据库中（将添加）： " + author + " ID: " + to_string(id_max));
            new_authors[author] = id_max;
            stmt_insert_auth.bind(1, int(id_max));
            stmt_insert_auth.bind(2, author);
            stmt_insert_auth.exec(); stmt_insert_auth.reset();
        }
        entry = u8(fname.substr(ind+1));
        if (entries.count(entry) == 0 &&
                            entries_deleted_inserted.count(entry) == 0) {
            SLS_WARN("备份文件中的词条不在数据库中（将添加）： " + entry);
            stmt_insert_entry.bind(1, entry);
            stmt_insert_entry.exec(); stmt_insert_entry.reset();

            entries_deleted_inserted.insert(entry);
        }

        if (sha1_exist) {
            auto &time_author_entry = db_data[sha1];
            if (get<0>(time_author_entry) != time)
                SLS_WARN("备份 " + fname + " 信息与数据库中的时间不同， 数据库中为（将不更新）： " + get<0>(time_author_entry));
            if (db_id_to_author[get<1>(time_author_entry)] != author)
                SLS_WARN("备份 " + fname + " 信息与数据库中的作者不同， 数据库中为（将不更新）： " +
                         to_string(get<1>(time_author_entry)) + "." + db_id_to_author[get<1>(time_author_entry)]);
            if (get<2>(time_author_entry) != entry)
                SLS_WARN("备份 " + fname + " 信息与数据库中的文件名不同， 数据库中为（将不更新）： " + get<2>(time_author_entry));
            db_data.erase(sha1);
        }
        else {
            SLS_WARN("数据库的 history 表格中不存在备份文件（将添加）：" + sha1 + " " + fname);
            stmt_insert.bind(1, sha1);
            stmt_insert.bind(2, time);
            stmt_insert.bind(3, int(authorID));
            stmt_insert.bind(4, entry);
            stmt_insert.exec(); stmt_insert.reset();
        }
    }

    if (!db_data.empty()) {
        SLS_WARN("以下数据库中记录的备份无法找到对应文件： ");
        for (auto &row : db_data) {
            cout << row.first << ", " << get<0>(row.second) << ", " <<
                get<1>(row.second) << ", " << get<2>(row.second) << endl;
        }
    }
    cout << "\ndone." << endl;

    for (auto &author : new_authors)
        cout << "新作者： " << author.second << ". " << author.first << endl;

    cout << "done." << endl;
}

// db table "figures" and "figure_store"
inline void db_update_figures(const vecStr32_I entries, const vector<vecStr32> &img_ids,
    const vector<vecLong> &img_orders, const vector<vecStr32> &img_hashes,
    SQLite::Statement &stmt_select, SQLite::Statement &stmt_insert, SQLite::Statement &stmt_update)
{
    // cout << "updating db for figures environments..." << endl;
    Str32 db_id, db_entry, db_hash;
    // vecStr32 new_ids, new_entries, new_hash;
    Long db_order;

    Str32 entry, id, hash, ext;
    Long order;
    for (Long i = 0; i < size(entries); ++i) {
        entry = entries[i];

        for (Long j = 0; j < size(img_ids[i]); ++j) {
            id = img_ids[i][j]; order = img_orders[i][j]; hash = img_hashes[i][j];
            stmt_select.bind(1, u8(id));
            if (!stmt_select.executeStep()) {
                stmt_select.reset();
                SLS_WARN("发现数据库中没有的图片环境（将添加）：" + id);
                stmt_insert.bind(1, u8(id));
                stmt_insert.bind(2, u8(entry));
                stmt_insert.bind(3, int(order));
                stmt_insert.bind(4, u8(hash));
                stmt_insert.exec(); stmt_insert.reset();
            } else { // 数据库中有 id, 检查其他信息是否改变
                db_entry = u32(stmt_select.getColumn(0));
                db_order = (int)stmt_select.getColumn(1);
                db_hash = u32(stmt_select.getColumn(2));
                stmt_select.reset();

                bool update = false;
                if (entry != db_entry) {
                    SLS_WARN("发现数据库中图片 entry 改变（将更新）：" + id + ": "
                             + db_entry + " -> " + entry);
                    update = true;
                }
                if (order != db_order) {
                    SLS_WARN("发现数据库中图片 order 改变（将更新）：" + id + ": "
                             + to_string(db_order) + " -> " + to_string(order));
                    update = true;
                }
                if (hash != db_hash) {
                    SLS_WARN("发现数据库中图片 hash 改变（将更新）：" + id + ": "
                             + db_hash + " -> " + hash);
                    update = true;
                }
                if (update) {
                    stmt_update.bind(1, u8(entry));
                    stmt_update.bind(2, int(order));
                    stmt_update.bind(3, u8(hash));
                    stmt_update.bind(4, u8(id));
                    stmt_update.exec(); stmt_update.reset();
                }
            }
        }
    }
    // cout << "done!" << endl;
}

// update labels table of database
// `ids` are xxx### where xxx is "type", and ### is "order"
inline void db_update_labels(vecStr32_I entries, const vector<vecStr32> &v_labels, const vector<vecLong> v_label_orders,
    SQLite::Statement &stmt_select, SQLite::Statement &stmt_insert, SQLite::Statement &stmt_update)
{
    // cout << "updating db for labels..." << endl;
    Str32 label, type, entry; Long order;

    // db_data[id] = {type, entry, order}
    std::unordered_map<Str32, tuple<Str32, Str32, Long>> db_data;
    while (stmt_select.executeStep()) {
        label = u32(stmt_select.getColumn(0));
        type = u32(stmt_select.getColumn(1));
        entry = u32(stmt_select.getColumn(2));
        order = (int)stmt_select.getColumn(3);
        db_data[label] = std::make_tuple(type, entry, order);
    }
    stmt_select.reset();

    for (Long j = 0; j < size(entries); ++j) {
        entry = entries[j];
        for (Long i = 0; i < size(v_labels[j]); ++i) {
            label = v_labels[j][i];
            order = v_label_orders[j][i];
            type = label_type(label);
            if (type == U"fig" || type == U"code")
                continue;
            if (type != U"eq" && type != U"sub" && type != U"tab" && type != U"def" && type != U"lem" &&
                type != U"the" && type != U"cor" && type != U"ex" && type != U"exe")
                throw Str32(U"未知标签类型： " + type);

            if (db_data.count(label)) { // label exist in db
                auto &db_row = db_data[label];
                bool changed = false;
                if (type != get<0>(db_row)) {
                    SLS_WARN(U"type 发生改变（将更新）：" + get<0>(db_row) + " -> " + type);
                    changed = true;
                }
                if (entry != get<1>(db_row)) {
                    SLS_WARN(U"entry 发生改变（将更新）：" + get<1>(db_row) + " -> " + entry);
                    changed = true;
                }
                if (order != get<2>(db_row)) {
                    SLS_WARN(U"order 发生改变（将更新）：" + to_string(get<2>(db_row)) + " -> " + to_string(order));
                    changed = true;
                }
                if (changed) {
                    stmt_update.bind(1, u8(type));
                    stmt_update.bind(2, u8(entry));
                    stmt_update.bind(3, int(order));
                    stmt_update.bind(4, u8(label));
                    stmt_update.exec(); stmt_update.reset();
                }
            } else { // label not in db
                SLS_WARN(U"数据库中不存在 label（将插入）：" + label + ", " + type + ", " + entry + ", " +
                         to_string(order));
                stmt_insert.bind(1, u8(label));
                stmt_insert.bind(2, u8(type));
                stmt_insert.bind(3, u8(entry));
                stmt_insert.bind(4, int(order));
                stmt_insert.exec(); stmt_insert.reset();
            }
        }
    }
    // cout << "done!" << endl;
}

// convert PhysWiki/ folder to wuli.wiki/online folder
inline void PhysWikiOnline()
{
    SQLite::Database db(u8(gv::path_data + "scan.db"), SQLite::OPEN_READWRITE);

    // get entries from folder, titles from main.tex
    vecStr32 entries;

    {
        vecStr32 titles;
        entries_titles(entries, titles);
        write_vec_str(titles, gv::path_data + U"titles.txt");
        write_vec_str(entries, gv::path_data + U"entries.txt");

        SQLite::Statement stmt_insert(db,
            R"(INSERT OR IGNORE INTO "entries" ("id") VALUES (?);)");
        for (Long i = 0; i < size(entries); ++i) {
            stmt_insert.bind(1, u8(entries[i]));
            stmt_insert.exec();
            if (stmt_insert.getChanges() > 0)
                SLS_WARN(U"已插入数据库中不存在的词条（仅 id）： " + entries[i]);
            stmt_insert.reset();
        }
    }

    // for newcommand()
    vecStr32 rules;
    define_newcommands(rules);

    // process bibliography
    {
        vecStr32 bib_labels, bib_details;
        bibliography(bib_labels, bib_details);
        db_update_bib(bib_labels, bib_details);
    }

    // html tag id and corresponding latex label (e.g. Idlist[i]: "eq5", "fig3")
    // the number in id is the n-th occurrence of the same type of environment

    vecStr32 titles; // titles from files directly
    titles.resize(entries.size());

    {
        Bool draft;
        vecLong img_orders, label_orders;
        vecStr32 keywords, labels, img_ids, img_hashes, pentries;

        SQLite::Statement stmt_select(db,
                                      R"(SELECT "id", "type", "entry", "order" FROM "labels";)");
        SQLite::Statement stmt_insert(db,
                                      R"(INSERT INTO "labels" ("id", "type", "entry", "order") VALUES (?,?,?,?);)");
        SQLite::Statement stmt_update(db,
                                      R"(UPDATE "labels" SET "type"=?, "entry"=?, "order"=? WHERE "id"=?;)");
        SQLite::Statement stmt_select_fig(db,
                                              R"(SELECT "entry", "order", "hash" FROM "figures" WHERE "id"=?;)");

        SQLite::Statement stmt_insert_fig(db,
                                              R"(INSERT INTO "figures" ("id", "entry", "order", "hash") VALUES (?, ?, ?, ?);)");

        SQLite::Statement stmt_update_fig(db,
                                              R"(UPDATE "figures" SET "entry"=?, "order"=?, "hash"=?; WHERE "id"=?;)");

        // 1st loop through tex files
        cout << u8"\n\n\n\n======  第 1 轮转换 ======\n" << endl;
        for (Long i = 0; i < size(entries); ++i) {
            cout << std::setw(5) << std::left << i
                 << std::setw(10) << std::left << entries[i]; cout.flush();

            PhysWikiOnline1(titles[i], img_ids, img_orders, img_hashes, draft,
                            keywords, labels, label_orders, pentries, entries[i],
                            rules, db);

            cout << std::setw(20) << std::left << titles[i] << endl; cout.flush();

            db_update_labels({entries[i]}, {labels}, {label_orders}, stmt_select, stmt_insert, stmt_update);
            db_update_figures({entries[i]}, {img_ids}, {img_orders}, {img_hashes}, stmt_select_fig, stmt_insert_fig, stmt_update_fig);
        }
    }

    cout << u8"\n\n\n\n\n正在从 main.tex 生成目录 index.html ...\n" << endl;
    {
        vecBool is_drafts;
        vecLong entry_part, entry_chap, chap_part;
        vecStr32 entries_toc, titles_toc;
        vecStr32 part_ids, part_name, chap_first, chap_last; // \part{}
        vecStr32 chap_ids, chap_name, entry_first, entry_last; // \chapter{}

        table_of_contents(part_ids, part_name, chap_first, chap_last,
                          chap_ids, chap_name, chap_part, entry_first, entry_last,
                          entries_toc, titles_toc, is_drafts, entry_part, entry_chap, db);

        db_update_entries_from_toc(entries_toc, entry_part, part_ids, entry_chap, chap_ids);
        db_update_parts_chapters(part_ids, part_name, chap_first, chap_last, chap_ids, chap_name, chap_part,
                                 entry_first, entry_last);
    }

    // generate dep.json
    if (file_exist(gv::path_out + U"../tree/data/dep.json"))
        dep_json(db);

    // deal with autoref()
    {
        cout << "\n\n\n\n" << u8"====== 第 2 轮转换 ======\n" << endl;
        Str32 html, fname;
        SQLite::Statement stmt_select(db,
            R"(SELECT "order", "ref_by" FROM "labels" WHERE "id"=?;)");
        SQLite::Statement stmt_update_ref_by(db,
            R"(UPDATE "labels" SET "ref_by"=? WHERE "id"=?;)");
        SQLite::Statement stmt_select_fig(db,
            R"(SELECT "order", "ref_by" FROM "figures" WHERE "id"=?;)");
        SQLite::Statement stmt_update_ref_by_fig(db,
            R"(UPDATE "figures" SET "ref_by"=? WHERE "id"=?;)");

        for (Long i = 0; i < size(entries); ++i) {
            cout << std::setw(5) << std::left << i
                 << std::setw(10) << std::left << entries[i]
                 << std::setw(20) << std::left << titles[i] << endl; cout.flush();
            fname = gv::path_out + entries[i] + ".html";
            read(html, fname + ".tmp"); // read html file
            // process \autoref and \upref
            autoref(html, entries[i], stmt_select, stmt_update_ref_by, stmt_select_fig, stmt_update_ref_by);
            write(html, fname); // save html file
            file_remove(u8(fname) + ".tmp");
        }
        cout << endl;
    }

    // TODO: warn unused figures, based on "ref_by"
}

// like PhysWikiOnline, but convert only specified files
// requires ids.txt and labels.txt output from `PhysWikiOnline()`
inline void PhysWikiOnlineN(vecStr32_I entries)
{
    SQLite::Database db(u8(gv::path_data + "scan.db"), SQLite::OPEN_READWRITE);
    SQLite::Statement stmt_select(
            db,
            R"(SELECT "caption", "keys", "pentry", "draft", "issues", "issueOther" from "entries" WHERE "id"=?;)");

    vecStr32 rules;  // for newcommand()
    define_newcommands(rules);

    vecStr32 titles(entries.size());

    {
        cout << u8"\n\n======  第 1 轮转换 ======\n" << endl;
        Bool isdraft;
        vecStr32 keywords;
        vecLong img_orders, label_orders;
        vecStr32 img_ids, img_hashes, labels, pentries;

        SQLite::Statement stmt_select(db,
                                      R"(SELECT "id", "type", "entry", "order" FROM "labels";)");
        SQLite::Statement stmt_insert(db,
                                      R"(INSERT INTO "labels" ("id", "type", "entry", "order") VALUES (?,?,?,?);)");
        SQLite::Statement stmt_update(db,
                                      R"(UPDATE "labels" SET "type"=?, "entry"=?, "order"=? WHERE "id"=?;)");
        SQLite::Statement stmt_select_fig(db,
                                          R"(SELECT "entry", "order", "hash" FROM "figures" WHERE "id"=?;)");
        SQLite::Statement stmt_insert_fig(db,
                                          R"(INSERT INTO "figures" ("id", "entry", "order", "hash") VALUES (?, ?, ?, ?);)");
        SQLite::Statement stmt_update_fig(db,
                                          R"(UPDATE "figures" SET "entry"=?, "order"=?, "hash"=?; WHERE "id"=?;)");

        for (Long i = 0; i < size(entries); ++i) {
            cout << std::left << entries[i]; cout.flush();

            PhysWikiOnline1(titles[i], img_ids, img_orders, img_hashes, isdraft,
                            keywords, labels, label_orders, pentries, entries[i],
                            rules, db);
            
            cout << std::setw(20) << std::left << titles[i] << endl; cout.flush();

            db_update_labels({entries[i]}, {labels}, {label_orders}, stmt_select, stmt_insert, stmt_update);
            db_update_figures({entries[i]}, {img_ids}, {img_orders}, {img_hashes}, stmt_select_fig, stmt_insert_fig, stmt_update_fig);

            // check dependency tree
            {
                vector<DGnode> tree;
                vecStr32 _entries, _titles, parts, chapters;
                db_get_tree1(tree, _entries, _titles, parts, chapters, entries[i], db);
            }
        }
    }

    {
        cout << "\n\n\n\n" << u8"====== 第 2 轮转换 ======\n" << endl;
        Str32 html, fname;
        SQLite::Statement stmt_select2(db,
                                       R"(SELECT "order", "ref_by" FROM "labels" WHERE "id"=?;)");
        SQLite::Statement stmt_update_ref_by(db,
                                             R"(UPDATE "labels" SET "ref_by"=? WHERE "id"=?;)");
        SQLite::Statement stmt_select_fig(db,
                                          R"(SELECT "order", "ref_by" FROM "figures" WHERE "id"=?;)");
        SQLite::Statement stmt_update_ref_by_fig(db,
                                                 R"(UPDATE "figures" SET "ref_by"=? WHERE "id"=?;)");
        for (Long i = 0; i < size(entries); ++i) {
            cout << std::setw(5) << std::left << i
                 << std::setw(10) << std::left << entries[i]
                 << std::setw(20) << std::left << titles[i] << endl; cout.flush();
            fname = gv::path_out + entries[i] + ".html";
            read(html, fname + ".tmp"); // read html file
            // process \autoref and \upref
            autoref(html, entries[i], stmt_select2, stmt_update_ref_by, stmt_select_fig, stmt_update_ref_by);
            write(html, fname); // save html file
            file_remove(u8(fname) + ".tmp");
        }
        cout << endl; cout.flush();
    }
}

// search all commands
inline void all_commands(vecStr32_O commands, Str32_I in_path)
{
    vecStr32 fnames;
    Str32 str, name;
    file_list_ext(fnames, in_path, U"tex");
    for (Long i = 0; i < size(fnames); ++i) {
        read(str, in_path + fnames[i]);
        Long ind0 = 0;
        while (1) {
            ind0 = str.find(U"\\", ind0);
            if (ind0 < 0)
                break;
            command_name(name, str, ind0);
            if (!search(name, commands))
                commands.push_back(name);
            ++ind0;
        }
    }
}

// for google translation: hide code to protect them from translation
// replace with `\verb|编号|`
inline Long hide_verbatim(vecStr32_O str_verb, Str32_IO str)
{
    Long ind0 = 0, ind1, ind2;
    Char32 dlm;
    Str32 tmp;

    // verb
    while (1) {
        ind0 = find_command(str, U"verb", ind0);
        if (ind0 < 0)
            break;
        ind1 = str.find_first_not_of(U' ', ind0 + 5);
        if (ind1 < 0)
            throw Str32(U"\\verb 没有开始");
        dlm = str[ind1];
        if (dlm == U'{')
            throw Str32(U"\\verb 不支持 {...}， 请使用任何其他符号如 \\verb|...|， \\verb@...@");
        ind2 = str.find(dlm, ind1 + 1);
        if (ind2 < 0)
            throw Str32(U"\\verb 没有闭合");
        if (ind2 - ind1 == 1)
            throw Str32(U"\\verb 不能为空");
        tmp = str.substr(ind0, ind2 + 1 - ind0);
        replace(tmp, U"\n", U"PhysWikiScanLF");
        str_verb.push_back(tmp);
        str.replace(ind0, ind2 + 1 - ind0, U"\\verb|" + num2str32(size(str_verb)-1, 4) + U"|");
        ind0 += 3;
    }

    // lstinline
    ind0 = 0;
    while (1) {
        ind0 = find_command(str, U"lstinline", ind0);
        if (ind0 < 0)
            break;
        ind1 = str.find_first_not_of(U' ', ind0 + 10);
        if (ind1 < 0)
            throw Str32(U"\\lstinline 没有开始");
        dlm = str[ind1];
        if (dlm == U'{')
            throw Str32(U"lstinline 不支持 {...}， 请使用任何其他符号如 \\lstinline|...|， \\lstinline@...@");
        ind2 = str.find(dlm, ind1 + 1);
        if (ind2 < 0)
            throw Str32(U"\\lstinline 没有闭合");
        if (ind2 - ind1 == 1)
            throw Str32(U"\\lstinline 不能为空");
        tmp = str.substr(ind0, ind2 + 1 - ind0);
        replace(tmp, U"\n", U"PhysWikiScanLF");
        str_verb.push_back(tmp);

        str.replace(ind0, ind2 + 1 - ind0, U"\\verb|" + num2str32(size(str_verb)-1, 4) + U"|");
        ind0 += 3;
    }

    // lstlisting
    vecStr32 str_verb1;
    ind0 = 0;
    Intvs intv;
    Str32 code;
    Long N = find_env(intv, str, U"lstlisting", 'o');
    Str32 lang = U""; // language
    for (Long i = N - 1; i >= 0; --i) {
        tmp = str.substr(intv.L(i), intv.R(i) + 1 - intv.L(i));
        replace(tmp, U"\n", U"PhysWikiScanLF");
        str_verb1.push_back(tmp);
        str.replace(intv.L(i), intv.R(i) + 1 - intv.L(i), U"\\verb|" + num2str32(size(str_verb)+i, 4) + U"|");
    }
    for (Long i = 0; i < N; ++i) {
        str_verb.push_back(str_verb1.back());
        str_verb1.pop_back();
    }
    return str_verb.size();
}

// for google translation: hide equations and verbatims
inline void hide_eq_verb(Str32_IO str)
{
    Str32 tmp;
    vecStr32 eq_list, verb_list;
    Intvs intv, intv1;
    find_inline_eq(intv, str, 'o');
    find_display_eq(intv1, str, 'o');
    combine(intv, intv1);

    for (Long i = intv.size() - 1; i >= 0; --i) {
        tmp = str.substr(intv.L(i), intv.R(i) - intv.L(i) + 1);
        replace(tmp, U"\n", U"PhysWikiScanLF");
        eq_list.push_back(tmp);
        str.replace(intv.L(i), intv.R(i) - intv.L(i) + 1, U"$" + num2str32(size(eq_list)-1, 4) + U"$");
    }
    hide_verbatim(verb_list, str);
    // save to files
    write_vec_str(eq_list, gv::path_data + U"eq_list.txt");
    write_vec_str(verb_list, gv::path_data + U"verb_list.txt");
}

inline void unhide_eq_verb(Str32_IO str)
{
    Str32 tmp, label;
    vecStr32 eq_list, verb_list;
    read_vec_str(eq_list, gv::path_data + U"eq_list.txt");
    read_vec_str(verb_list, gv::path_data + U"verb_list.txt");
    for (Long i = 0; i < size(eq_list); ++i) {
        label = U"$" + num2str32(i, 4) + U"$";
        Long ind = str.find(label);
        tmp = eq_list[i];
        replace(tmp, U"PhysWikiScanLF", U"\n");
        if (ind < 0)
            SLS_WARN(label + U" 没有找到，替换： \n" + tmp + U"\n");
        else
            str.replace(ind, label.size(), tmp);
    }

    for (Long i = 0; i < size(verb_list); ++i) {
        label = U"\\verb|" + num2str32(i, 4) + U"|";
        Long ind = str.find(label);
        tmp = verb_list[i];
        replace(tmp, U"PhysWikiScanLF", U"\n");
        if (ind < 0)
            SLS_WARN(label + U" 没有找到，替换： \n" + tmp + U"\n");
        else
            str.replace(ind, label.size(), tmp);
    }
}

// check format and auto correct .tex files in path0
inline void add_space_around_inline_eq(Str32_I path_in)
{
    vecStr32 names, str_verb;
    Str32 fname, str;
    Intvs intv;
    file_list_ext(names, path_in + "contents/", U"tex", false);
    
    //RemoveNoEntry(names);
    if (names.size() <= 0) return;
    //names.resize(0); names.push_back(U"Sample"));
    
    for (unsigned i{}; i < names.size(); ++i) {
        cout << i << " ";
        cout << names[i] << "...";
        if (names[i] == U"Sample" || names[i] == U"edTODO")
            continue;
        // main process
        fname = path_in + "contents/" + names[i] + ".tex";
        read(str, fname);
        CRLF_to_LF(str);
        // verbatim(str_verb, str);
        // TODO: hide comments then recover at the end
        // find_comments(intv, str, "%");
        Long N;
        try{ N = inline_eq_space(str); }
        catch (...) {
            continue;
        }

        // verb_recover(str, str_verb);
        if (N > 0)
            write(str, fname);
        cout << endl;
    }
}
