#pragma once
#include "../SLISC/file.h"
#include "../SLISC/unicode.h"
#include "../SLISC/tree.h"
#include "../TeX/tex2html.h"
#include "../highlight/matlab2html.h"
#include "../highlight/cpp2html.h"

using namespace slisc;

// get the title (defined in the first comment, can have space after %)
// limited to 20 characters
inline void get_title(Str32_O title, Str32_I str)
{
    if (str.at(0) != U'%') {
        throw Str32(U"请在第一行注释标题!"); // break point here
    }
    Str32 c;
    Long ind0 = NextNoSpace(c, str, 1);
    if (ind0 < 0) {
        throw Str32(U"标题不能为空!"); // break point here
    }
    Long ind1 = str.find(U'\n', ind0);
    if (ind1 < 0) {
        throw Str32(U"正文不能为空!"); // break point here
    }
    title = str.substr(ind0, ind1 - ind0);
    ind0 = title.find(U"\\");
    if (ind0 >= 0) {
        throw Str32(U"注释的标题不能含有 “\\” !"); // break point here
    }
}

// trim "\n" and " " on both sides
// remove unnecessary "\n"
// replace “\n\n" with "\n</p>\n<p>　　\n"
inline Long paragraph_tag1(Str32_IO str)
{
    Long ind0 = 0, N = 0;
    trim(str, U" \n");
    // delete extra '\n' (more than two continuous)
    while (true) {
        ind0 = str.find(U"\n\n\n", ind0);
        if (ind0 < 0)
            break;
        eatR(str, ind0 + 2, U"\n");
    }

    // replace "\n\n" with "\n</p>\n<p>　　\n"
    N = replace(str, U"\n\n", U"\n</p>\n<p>　　\n");
    return N;
}

inline Long paragraph_tag(Str32_IO str)
{
    Long N = 0, ind0 = 0, left = 0, length, ikey;
    Intvs intv;
    Str32 temp, env, end, begin = U"<p>　　\n";

    // "begin", and commands that cannot be in a paragraph
    vecStr32 commands = {U"begin",
        U"subsection", U"subsubsection", U"pentry", U"code", U"Code"};

    // environments that must be in a paragraph (use "<p>" instead of "<p>　　" when at the start of the paragraph)
    vecStr32 envs_eq = {U"equation", U"align", U"gather", U"lstlisting"};

    // environments that needs paragraph tags inside
    vecStr32 envs_p = { U"example", U"exercise"};

    // 'n' (for normal); 'e' (for env_eq); 'p' (for env_p); 'f' (end of file)
    char next, last = 'n';

    Intvs intvInline;
    find_inline_eq(intvInline, str);

    // handle normal text intervals separated by
    // commands in "commands" and environments
    while (true) {
        // decide mode
        if (ind0 == str.size()) {
            next = 'f';
        }
        else {
            ind0 = find_command(ikey, str, commands, ind0);
            if (ind0 < 0)
                next = 'f';
            else {
                next = 'n';
                if (ikey == 0) { // found environment
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
        else
            ind0 = skip_command(str, ind0, 1);

        left = ind0;
        last = next;
        if (ind0 == str.size()) {
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

inline void limit_env(Str32_I str)
{
    Intvs intv; Long ind0 = 0;
    
    find_env(intv, str, U"document");
    if (intv.size() > 0)
        throw Str32(U"document 环境已经在 main.tex 中， 每个词条文件是一个 section， 请先阅读编辑器说明");

    // supported environments
    vecStr32 envs = { U"equation", U"gather", U"gather*", U"gathered", U"align", U"align*",
        U"alignat", U"alignat*", U"alignedat", U"aligned", U"split", U"figure", U"itemize",
        U"enumerate", U"lstlisting", U"example", U"exercise", U"lemma", U"theorem", U"definition",
        U"corollary", U"matrix", U"pmatrix", U"vmatrix", U"table", U"tabular", U"cases", U"array",
        U"case", U"Bmatrix", U"bmatrix", U"eqnarray", U"eqnarray*", U"multline", U"multline*",
        U"smallmatrix", U"subarray", U"Vmatrix"};
    Str32 env;
    while (true) {
        ind0 = find_command(str, U"begin", ind0);
        if (ind0 < 0)
            break;
        command_arg(env, str, ind0);
        if (search(env, envs) < 0)
            throw Str32(U"暂不支持 " + env + " 环境！ 如果你认为 MathJax 支持该环境， 请联系管理员。");
        ++ind0;
    }
}

// add html id tag before environment if it contains a label, right before "\begin{}" and delete that label
// output html id and corresponding label for future reference
// `gather` and `align` environments has id starting wth `ga` and `ali`
// `idNum` is in the idNum-th environment of the same name (not necessarily equal to displayed number)
// no comment allowed
// return number of labels processed, or -1 if failed
inline Long EnvLabel(vecStr32_IO ids, vecStr32_IO labels,
    Str32_I entryName, Str32_IO str)
{
    Long ind0{}, ind1{}, ind2{}, ind3{}, ind4{}, ind5{}, N{}, temp{},
        Ngather{}, Nalign{}, i{}, j{};
    Str32 idName; // "eq" or "fig" or "ex"...
    Str32 envName; // "equation" or "figure" or "example"...
    Str32 idNum{}; // id = idName + idNum
    Long idN{}; // convert to idNum
    Str32 label, id;
    // clean existing labels of this entry
    for (Long i = labels.size() - 1; i >= 0; --i) {
        if (labels[i].substr(0, entryName.size() + 1) == entryName + U'_') {
            labels.erase(labels.begin()+i);
            ids.erase(ids.begin() + i);
        }
    }
    while (true) {
        ind5 = find_command(str, U"label", ind0);
        if (ind5 < 0) return N;
        // make sure label is inside an environment
        ind2 = str.rfind(U"\\end", ind5);
        ind4 = str.rfind(U"\\begin", ind5);
        if (ind2 >= ind4) {
            throw Str32(U"label 不在环境内!"); // break point here
        }
        // detect environment kind
        ind1 = expect(str, U"{", ind4 + 6);
        if (expect(str, U"equation", ind1) > 0) {
            idName = U"eq"; envName = U"equation";
        }
        else if (expect(str, U"figure", ind1) > 0) {
            idName = U"fig"; envName = U"figure";
        }
        else if (expect(str, U"definition", ind1) > 0) {
            idName = U"def"; envName = U"definition";
        }
        else if (expect(str, U"lemma", ind1) > 0) {
            idName = U"lem"; envName = U"lemma";
        }
        else if (expect(str, U"theorem", ind1) > 0) {
            idName = U"the"; envName = U"theorem";
        }
        else if (expect(str, U"corollary", ind1) > 0) {
            idName = U"cor"; envName = U"corollary";
        }
        else if (expect(str, U"example", ind1) > 0) {
            idName = U"ex"; envName = U"example";
        }
        else if (expect(str, U"exercise", ind1) > 0) {
            idName = U"exe"; envName = U"exercise";
        }
        else if (expect(str, U"table", ind1) > 0) {
            idName = U"tab"; envName = U"table";
        }
        else if (expect(str, U"gather", ind1) > 0) {
            idName = U"eq"; envName = U"gather";
        }
        else if (expect(str, U"align", ind1) > 0) {
            idName = U"eq"; envName = U"align";
        }
        else {
            throw Str32(U"该环境不支持 label!");
        }
        // check label format and save label
        ind0 = expect(str, U"{", ind5 + 6);
        ind3 = expect(str, entryName + U'_' + idName, ind0);
        if (ind3 < 0) {
            throw Str32(U"label 格式错误， 是否为 \"" + entryName + U'_' + idName + U"\"？");
        }
        ind3 = str.find(U"}", ind3);
        
        label = str.substr(ind0, ind3 - ind0);
        trim(label);
        Long ind = search(label, labels);
        if (ind < 0)
            labels.push_back(label);
        else {
            throw Str32(U"标签多次定义： " + labels[ind]);
        }
        
        // count idNum, insert html id tag, delete label
        Intvs intvEnv;
        if (idName != U"eq") {
            idN = find_env(intvEnv, str.substr(0,ind4), envName) + 1;
        }
        else { // count equations
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
        num2str(idNum, idN);
        id = idName + idNum;
        if (ind < 0)
            ids.push_back(id);
        else
            ids[ind] = id;
        str.erase(ind5, ind3 - ind5 + 1);
        str.insert(ind4, U"<span id = \"" + id + U"\"></span>");
        ind0 = ind4 + 6;
    }
}

// replace figure environment with html image
// convert vector graph to SVG, font must set to "convert to outline"
// if svg image doesn't exist, use png
// path must end with '\\'
inline Long FigureEnvironment(Str32_IO str, Str32_I path_out, Str32_I path_in, Str32_I url)
{
    Long i{}, N{}, Nfig{}, ind0{}, ind1{}, indName1{}, indName2{};
    double width{}; // figure width in cm
    Intvs intvFig;
    Str32 figName, fname_in, fname_out;
    Str32 format, caption, widthPt, figNo;
    Nfig = find_env(intvFig, str, U"figure", 'o');
    for (i = Nfig - 1; i >= 0; --i) {
        // get width of figure
        ind0 = str.find(U"width", intvFig.L(i)) + 5;
        ind0 = expect(str, U"=", ind0);
        ind0 = find_num(str, ind0);
        str2double(width, str, ind0);

        // get file name of figure
        indName1 = str.find(U"figures/", ind0) + 8;
        indName2 = str.find(U"}", ind0) - 1;
        if (indName1 < 0 || indName2 < 0) {
            throw Str32(U"读取图片名错误!");
        }
        figName = str.substr(indName1, indName2 - indName1 + 1);
        trim(figName);

        // get format
        Long Nname = figName.size();
        if (figName.substr(Nname - 4, 4) == U".png") {
            format = U"png";
            figName = figName.substr(0, Nname - 4);
        }
        else if (figName.substr(Nname - 4, 4) == U".pdf") {
            format = U"svg";
            figName = figName.substr(0, Nname - 4);
        }
        else {
            throw Str32(U"图片格式不支持!");
        }

        fname_in = path_in + U"figures/" + figName + U"." + format;
        fname_out = path_out + figName + U"." + format;
        if (!file_exist(fname_in)) {
            throw Str32(U"图片 \"" + fname_in + U"\" 未找到!");
        }

        file_copy(fname_out, fname_in, true);

        // get caption of figure
        ind0 = find_command(str, U"caption", ind0);
        if (ind0 < 0) {
            throw Str32(U"图片标题未找到!");
        }
        command_arg(caption, str, ind0);

        // insert html code
        num2str(widthPt, (Long)(33.4 * width));
        num2str(figNo, i + 1);
        str.replace(intvFig.L(i), intvFig.R(i) - intvFig.L(i) + 1,
            U"<div class = \"w3-content\" style = \"max-width:" + widthPt
            + U"pt;\">\n" + U"<img src = \"" + url + figName + U"." + format
            + U"\" alt = \"图\" style = \"width:100%;\">\n</div>\n<div align = \"center\"> 图" + figNo
            + U"：" + caption + U"</div>");
        ++N;
    }
    return N;
}

// get dependent entries from \pentry{}
// links are file names, not chinese titles
// links[i][0] --> links[i][1]
inline Long depend_entry(vecLong_IO links, Str32_I str, vecStr32_I entryNames, Long_I ind)
{
    Long ind0 = 0, N = 0;
    Str32 temp;
    Str32 depEntry;
    Str32 link[2];
    while (true) {
        ind0 = find_command(str, U"pentry", ind0);
        if (ind0 < 0)
            return N;
        command_arg(temp, str, ind0, 0, 't');
        Long ind1 = 0;
        while (true) {
            ind1 = find_command(temp, U"upref", ind1);
            if (ind1 < 0)
                return N;
            command_arg(depEntry, temp, ind1, 0, 't');
            Long i; Bool flag = false;
            for (i = 0; i < size(entryNames); ++i) {
                if (depEntry == entryNames[i]) {
                    flag = true; break;
                }
            }
            if (!flag) {
                throw Str32(U"\\upref 引用的文件未找到: " + depEntry + ".tex");
            }
            links.push_back(i);
            links.push_back(ind);
            ++N; ++ind1;
        }
    }
}

// replace \pentry comman with html round panel
inline Long pentry(Str32_IO str)
{
    return Command2Tag(U"pentry", U"<div class = \"w3-panel w3-round-large w3-light-blue\"><b>预备知识</b>　", U"</div>", str);
}

// remove special .tex files from a list of name
// return number of names removed
// names has ".tex" extension
inline Long RemoveNoEntry(vecStr32_IO names)
{
    Long i{}, j{}, N{}, Nnames{}, Nnames0;
    vecStr32 names0; // names to remove
    names0.push_back(U"FrontMatters");
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
    Long N, N_tot = 0, ind0, ind1{};
    Intvs intvIn, intvOut;
    Str32 env_title, env_num;
    vecStr32 envNames = {U"definition", U"lemma", U"theorem",
        U"corollary", U"example", U"exercise"};
    vecStr32 envCnNames = {U"定义", U"引理", U"定理",
        U"推论", U"例", U"习题"};
    vecStr32 envBorderColors = { U"w3-border-red", U"w3-border-red", U"w3-border-red",
        U"w3-border-red", U"w3-border-yellow", U"w3-border-green" };
    for (Long i = 0; i < size(envNames); ++i) {
        ind0 = 0; N = 0;
        while (true) {
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
                + U" w3-leftbar\">\n <h5><b>"
                + envCnNames[i]
                + env_num + U"</b>　" + env_title + U"</h5>\n");
        }
    }
    return N_tot;
}

// replace autoref with html link
// no comment allowed
// does not add link for \autoref inside eq environment (equation, align, gather)
// return number of autoref replaced, or -1 if failed
inline Long autoref(vecStr32_I ids, vecStr32_I labels, Str32_I entryName, Str32_IO str, Str32_I url)
{
    unsigned i{};
    Long ind0{}, ind1{}, ind2{}, ind3{}, ind4{}, ind5{}, N{}, Neq{}, ienv{};
    Bool inEq;
    Str32 entry, label0, idName, idNum, kind, newtab, file;
    vecStr32 envNames{U"equation", U"align", U"gather"};
    while (true) {
        newtab.clear(); file.clear();
        ind0 = find_command(str, U"autoref", ind0);
        if (is_in_tag(str, U"code", ind0)) {
            ++ind0; continue;
        }
        if (ind0 < 0)
            return N;
        inEq = index_in_env(ienv, ind0, envNames, str);
        ind1 = expect(str, U"{", ind0 + 8);
        if (ind1 < 0) {
            throw Str32(U"\\autoref 变量不能为空!");
        }
        ind1 = NextNoSpace(entry, str, ind1);
        ind2 = str.find('_', ind1);
        if (ind2 < 0) {
            throw Str32(U"\\autoref 格式错误!");
        }
        ind3 = find_num(str, ind2);
        if (ind3 < 0) {
            throw Str32(U"autoref 格式错误!");
        }
        idName = str.substr(ind2 + 1, ind3 - ind2 - 1);
        if (idName == U"eq") kind = U"式";
        else if (idName == U"fig") kind = U"图";
        else if (idName == U"def") kind = U"定义";
        else if (idName == U"lem") kind = U"引理";
        else if (idName == U"the") kind = U"定理";
        else if (idName == U"cor") kind = U"推论";
        else if (idName == U"ex") kind = U"例";
        else if (idName == U"exe") kind = U"练习";
        else if (idName == U"tab") kind = U"表";
        else {
            throw Str32(U"\\label 类型错误， 必须为 eq/fig/def/lem/the/cor/ex/exe/tab 之一!");
        }
        ind3 = str.find('}', ind3);
        // find id of the label
        label0 = str.substr(ind1, ind3 - ind1); trim(label0);
        for (i = 0; i < labels.size(); ++i) {
            if (label0 == labels[i]) break;
        }
        if (i == labels.size()) {
            throw Str32(U"label \"" + label0 + U"\" 未找到!");
        }
        ind4 = find_num(ids[i], 0);
        idNum = ids[i].substr(ind4);     
        entry = str.substr(ind1, ind2 - ind1);
        file = url + entry + U".html";
        if (entry != entryName) // reference another entry
            newtab = U"target = \"_blank\"";
        if (!inEq)
            str.insert(ind3 + 1, U" </a>");
        str.insert(ind3 + 1, kind + U' ' + idNum);
        if (!inEq)
            str.insert(ind3 + 1, U"<a href = \"" + file + U"#" + ids[i] + U"\" " + newtab + U">");
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
        while (true) {
            label = entry + "_" + envName + num2str(num);
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
Long check_add_label(Str32_O label, Str32_I entry, Str32_I idName, Long ind,
    vecStr32_I labels, vecStr32_I ids, Str32_I path_in)
{
    Long ind0 = 0;
    Str32 label0, newtab;

    while (true) {
        ind0 = search(idName + num2str(ind), ids, ind0);
        if (ind0 < 0)
            break;
        Long len = labels[ind0].rfind(U'_');
        if (labels[ind0].substr(0, len) != entry) {
            ++ind0; continue;
        }
        label = labels[ind0];
        return 1;
    }

    // label does not exist
    Str32 full_name = path_in + "/contents/" + entry + ".tex";
    if (!file_exist(full_name)) {
        throw Str32(U"文件不存在： " + entry + ".tex");
    }
    Str32 str;
    read(str, full_name);

    // find comments
    Intvs intvComm;
    find_comments(intvComm, str, U"%");

    vecStr32 idNames = { U"eq", U"fig", U"def", U"lem",
        U"the", U"cor", U"ex", U"exe", U"tab" };
    vecStr32 envNames = { U"equation", U"figure", U"definition", U"lemma",
        U"theorem", U"corollary", U"example", U"exercise", U"table"};

    Long idNum = search(idName, idNames);
    if (idNum < 0) {
        throw Str32(U"\\label 类型错误， 必须为 eq/fig/def/lem/the/cor/ex/exe/tab 之一!");
    }
    
    // count environment display number starting at ind4
    Intvs intvEnv;
    if (idName != U"eq") {
        Long Nid = find_env(intvEnv, str, envNames[idNum], 'i');
        if (ind > Nid) {
            throw Str32(U"被引用对象不存在!");
        }

        new_label_name(label, idName, entry, str);
        str.insert(intvEnv.L(ind - 1), U"\\label{" + label + "}");
        write(str, full_name);
        return 0;
    }
    else { // count equations
        ind0 = 0;
        Long idN = 0;
        vecStr32 eq_envs = { U"equation", U"gather", U"align" };
        Str32 env0;
        while (true) {
            ind0 = find_command(str, U"begin", ind0);
            if (ind0 < 0) {
                throw Str32(U"被引用公式不存在!");
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
                new_label_name(label, idName, entry, str);
                ind0 = skip_command(str, ind0, 1);
                str.insert(ind0, U"\\label{" + label + "}");
                write(str, full_name);
                return 0;
            }
            if (ienv > 0) {// found gather or align
                Long ind1 = skip_env(str, ind0);
                for (Long i = ind0; i < ind1; ++i) {
                    if (str.substr(i, 2) == U"\\\\") {
                        ++idN;
                        if (idN == ind) {
                            new_label_name(label, idName, entry, str);
                            ind0 = skip_command(str, ind0, 1);
                            str.insert(i + 2, U"\n\\label{" + label + "}");
                            write(str, full_name);
                            return 0;
                        }
                    }        
                }
            }
            ++ind0;
        }
    }
}

// replace "\href{http://example.com}{name}"
// with <a href="http://example.com">name</a>
inline Long href(Str32_IO str)
{
    Long ind0 = 0, N = 0;
    Str32 name, url;
    while (true) {
        ind0 = find_command(str, U"href", ind0);
        if (ind0 < 0)
            return N;
        command_arg(url, str, ind0, 0);
        command_arg(name, str, ind0, 1);
        if (url.substr(0, 7) != U"http://" &&
            url.substr(0, 8) != U"https://") {
            throw Str32(U"链接格式错误: " + url);
        }

        Long ind1 = skip_command(str, ind0, 2);
        str.replace(ind0, ind1 - ind0,
            "<a href=\"" + url + "\">" + name + "</a>");
        ++N; ++ind0;
    }
}

// process upref
// path must end with '\\'
inline Long upref(Str32_IO str, Str32_I path_in, Str32_I url)
{
    Long ind0 = 0, right, N = 0;
    Str32 entryName;
    while (true) {
        ind0 = find_command(str, U"upref", ind0);
        if (ind0 < 0)
            return N;
        command_arg(entryName, str, ind0);
        trim(entryName);
        if (!file_exist(path_in + U"contents/" + entryName + U".tex")) {
            throw Str32(U"\\upref 引用的文件未找到： " + entryName + U".tex");
        }
        right = skip_command(str, ind0, 1);
        str.replace(ind0, right - ind0,
            U"<span class = \"icon\"><a href = \""
            + url + entryName +
            U".html\" target = \"_blank\"><i class = \"fa fa-external-link\"></i></a></span>");
        ++N;
    }
    return N;
}

// update entries.txt and titles.txt
// return the number of entries in main.tex
inline void entries_titles(vecStr32_O titles, vecStr32_O entries, Str32_I path_in)
{
    entries.clear(); titles.clear();
    file_list_ext(entries, path_in + "contents/", Str32(U"tex"), false);
    RemoveNoEntry(entries);
    if (entries.size() == 0) {
        throw Str32(U"未找到任何词条");
    }

    Long ind0 = 0;

    Str32 title;
    Str32 entryName; // entry label
    Str32 str; read(str, path_in + "main.tex");
    vecStr32 entryNames;
    CRLF_to_LF(str);
    titles.resize(entries.size());

    // remove comments
    Intvs intvComm;
    find_comments(intvComm, str, U"%");
    for (Long i = intvComm.size() - 1; i >= 0; --i)
        str.erase(intvComm.L(i), intvComm.R(i) - intvComm.L(i) + 1);

    while (true) {
        ind0 = str.find(U"\\entry", ind0);
        if (ind0 < 0)
            break;
        // get chinese title and entry label
        command_arg(title, str, ind0);
        replace(title, U"\\ ", U" ");
        if (title.empty()) {
            throw Str32(U"main.tex 中词条中文名不能为空!");
        }
        command_arg(entryName, str, ind0, 1);

        // record Chinese title
        Long ind = search(entryName, entries);
        if (ind < 0) {
            throw Str32(U"main.tex 中词条文件 " + entryName + U" 未找到!");
        }
        if (search(entryName, entryNames) < 0)
            entryNames.push_back(entryName);
        else
            throw Str32(U"目录中出现重复词条：" + entryName);
        titles[ind] = title;
        ++ind0;
    }

    cout << u8"\n\n警告: 以下词条没有被 main.tex 收录，但仍会被编译" << endl;
    for (Long i = 0; i < size(titles); ++i) {
        if (titles[i].empty()) {
            read(str, path_in + U"contents/" + entries[i] + ".tex");
            CRLF_to_LF(str);
            get_title(title, str);
            titles[i] = title;
            cout << std::setw(10) << std::left << entries[i]
                << std::setw(20) << std::left << title << endl;
        }
    }
    cout << endl;
}

// create table of content from main.tex
// path must end with '\\'
// return the number of entries
// names is a list of filenames
// titles[i] is the chinese title of entries[i]
inline Long table_of_contents(vecStr32_I entries, Str32_I path_in, Str32_I path_out)
{
    Long i{}, N{}, ind0{}, ind1{}, ind2{}, ikey{}, chapNo{ -1 }, partNo{ -1 };
    vecStr32 keys{ U"\\part", U"\\chapter", U"\\entry", U"\\Entry", U"\\laserdog"};
    vecStr32 chineseNo{U"一", U"二", U"三", U"四", U"五", U"六", U"七", U"八", U"九",
                            U"十", U"十一", U"十二", U"十三", U"十四", U"十五"};
    //keys.push_back(U"\\entry"); keys.push_back(U"\\chapter"); keys.push_back(U"\\part");
    
    Str32 title; // chinese entry name, chapter name, or part name
    Str32 entryName; // entry label
    Str32 str, toc;

    read(str, path_in + "main.tex");
    CRLF_to_LF(str);
    read(toc, path_out + "templates/index_template.html"); // read html template
    CRLF_to_LF(toc);

    ind0 = toc.find(U"PhysWikiHTMLtitle");
    toc.replace(ind0, 17, U"小时物理");

    ind0 = toc.find(U"PhysWikiHTMLbody", ind0);
    toc.erase(ind0, 16);

    // remove comments
    Intvs intvComm;
    find_comments(intvComm, str, U"%");
    for (i = intvComm.size() - 1; i >= 0; --i)
        str.erase(intvComm.L(i), intvComm.R(i) - intvComm.L(i) + 1);

    while (true) {
        ind1 = find(ikey, str, keys, ind1);
        if (ind1 < 0)
            break;
        if (ikey >= 2) { // found "\entry"
            // get chinese title and entry label
            ++N;
            command_arg(title, str, ind1);
            replace(title, U"\\ ", U" ");
            if (title.empty()) {
                throw Str32(U"main.tex 中词条中文名不能为空!");
            }
            command_arg(entryName, str, ind1, 1);
            // insert entry into html table of contents
            ind0 = insert(toc, U"<a href = \"" + entryName + ".html" + "\" target = \"_blank\">"
                + title + U"</a>　\n", ind0);
            // record Chinese title
            if (search(entryName, entries) < 0) {
                throw Str32(U"main.tex 中词条文件 " + entryName + " 未找到!");
            }
            ++ind1;
        }
        else if (ikey == 1) { // found "\chapter"
            // get chinese chapter name
            command_arg(title, str, ind1);
             replace(title, U"\\ ", U" ");
             // insert chapter into html table of contents
             ++chapNo;
             if (chapNo > 0)
                 ind0 = insert(toc, U"</p>", ind0);
             ind0 = insert(toc, U"\n\n<h3><b>第" + chineseNo[chapNo] + U"章 " + title
                 + U"</b></h5>\n<div class = \"tochr\"></div><hr><div class = \"tochr\"></div>\n<p class=\"toc\">\n", ind0);
            ++ind1;
         }
         else if (ikey == 0){ // found "\part"
             // get chinese part name
             chapNo = -1;
            command_arg(title, str, ind1);
             replace(title, U"\\ ", U" ");
             // insert part into html table of contents
             ++partNo;
            
             ind0 = insert(toc,
                U"</p></div>\n\n<div class = \"w3-container w3-center w3-teal w3-text-white\">\n"
                U"<h2 align = \"center\">第" + chineseNo[partNo] + U"部分 " + title + U"</h3>\n"
                U"</div>\n\n<div class = \"w3-container\">\n"
                , ind0);
            ++ind1;
         }
    }
    toc.insert(ind0, U"</p>\n</div>");
    write(toc, path_out + "index.html");
    return N;
}

// create table of content from main.tex
// path must end with '\\'
// return the number of entries
// names is a list of filenames
// titles[i] is the chinese title of entries[i]
inline Long table_of_changed(vecStr32_I titles, vecStr32_I entries, Str32_I path_in, Str32_I path_out, Str32_I path_data)
{
    Long i{}, N{}, ind0{};
    //keys.push_back(U"\\entry"); keys.push_back(U"\\chapter"); keys.push_back(U"\\part");
    
    Str32 entryName; // entry label
    Str32 toc;
    vecStr32 changed, authors;

    if (!file_exist(path_data + U"changed.txt")) {
        write_vec_str(vecStr32(), path_data + U"changed.txt");
        write_vec_str(vecStr32(), path_data + U"authors.txt");
    }
    else {
        read_vec_str(changed, path_data + U"changed.txt");
        read_vec_str(authors, path_data + U"authors.txt");
        if (changed.size() != authors.size()) {
            throw Str32(U"内部错误： changed.txt 和 authors.txt 行数不同!");
        }
    }

    read(toc, path_out + "templates/changed_template.html"); // read html template
    CRLF_to_LF(toc);

    ind0 = toc.find(U"PhysWikiHTMLbody", ind0);
    if (changed.size() == 0) {
        replace(toc, U"PhysWikiHTMLbody", U"</div>");
        write(toc, path_out + "changed.html");
        return 0;
    }

    toc.erase(ind0, 16);
    ind0 = insert(toc, U"<p>\n", ind0);

    for (Long i = 0; i < size(changed); ++i) {
        Long ind = changed[i].rfind('.');
        if (ind < 0) {
            throw Str32(U"内部错误： changed.txt 中文件必须有后缀名!");
        }
        if (changed[i].substr(ind + 1) != U"tex")
            continue;
        entryName = changed[i].substr(0, ind);
        if (entryName == U"main") // ignore PhysWiki
            continue;
        // get chinese title and entry label
        ++N;
        // get Chinese title
        ind = search(entryName, entries);
        if (ind < 0) {
            changed.erase(changed.begin() + i);
            authors.erase(authors.begin() + i);
            continue;
        }
        // insert entry into html table of contents
        ind0 = insert(toc, U"<a href = \"" + entryName + ".html" + "\" target = \"_blank\">"
            + titles[ind] + U"（" + authors[i] + U"）" + U"</a><br>\n", ind0);
    }
    toc.insert(ind0, U"</p>\n</div>");
    write(toc, path_out + "changed.html");
    write_vec_str(changed, path_data + U"changed.txt");
    write_vec_str(authors, path_data + U"authors.txt");
    return N;
}

// add line numbers to the code (using table)
inline void code_table(Str32_O table_str, Str32_I code)
{
    Str32 line_nums;
    Long ind1 = 0, N = 0;
    while (true) {
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

// process Matlab code (\code command)
inline Long MatlabCode(Str32_IO str, Str32_I path_in, Bool_I show_title)
{
    Long N = 0, ind0 = 0;
    Str32 name; // code file name without extension
    Str32 code;
    Intvs intvIn, intvOut;
    // \code commands
    while (true) {
        if (show_title)
            ind0 = find_command(str, U"Code", ind0);
        else
            ind0 = find_command(str, U"code", ind0);
        if (ind0 < 0)
            return N;
        command_arg(name, str, ind0);
        // read file
        if (!file_exist(path_in + "codes/" + utf32to8(name) + ".m")) {
            throw Str32(U"代码文件 \"" + utf32to8(name) + ".m\" 未找到!");
        }
        read(code, path_in + "codes/" + utf32to8(name) + ".m");
        CRLF_to_LF(code);
        if (line_size_lim(code, 78) >= 0) {
            throw Str32(U"Matlab 单行代码过长！");
        }
        trim(code, U"\n ");
        Matlab_highlight(code);

        // insert code
        // for download button, use
        // U"<span class = \"icon\"><a href = \"" + name + U".m\" download> <i class = \"fa fa-caret-square-o-down\"></i></a></span>"
        Str32 title, code_tab_str;
        code_table(code_tab_str, code);

        if (show_title)
            title = U"<p><b>　　" + name + U".m</b></p>\n";
        str.replace(ind0, skip_command(str, ind0, 1) - ind0, title + code_tab_str);
        ++N;
    }
}

// process all lstlisting environments
// return the number of \Command{} processed
inline Long lstlisting(Str32_IO str, vecStr32_I str_verb)
{
    Long ind0 = 0;
    Intvs intvIn, intvOut;
    Str32 code, ind_str;
    find_env(intvIn, str, U"lstlisting", 'i');
    Long N = find_env(intvOut, str, U"lstlisting", 'o');
    Str32 lang = U""; // language
    Str32 code_tab_str;
    for (Long i = N - 1; i >= 0; --i) {
        // get language
        ind0 = expect(str, U"[", intvIn.L(i));
        if (ind0 > 0) {
            Long ind1 = pair_brace(str, ind0-1);
            ind0 = expect(str, U"language", ind0);
            if (ind0 < 0) {
                throw Str32(U"lstlisting 方括号中指定语言格式错误（[language=xxx]）!");
            }
            ind0 = expect(str, U"=", ind0);
            if (ind0 < 0) {
                throw Str32(U"lstlisting 方括号中指定语言格式错误（[language=xxx]）!");
            }
            lang = str.substr(ind0, ind1 - ind0); trim(lang);
            ind0 = ind1 + 1;
        }
        else {
            ind0 = intvIn.L(i);
        }
        ind_str = str.substr(ind0, intvIn.R(i) + 1 - ind0);
        trim(ind_str, U"\n ");
        code = str_verb[str2int(ind_str)];
        if (line_size_lim(code, 78) >= 0) {
            throw Str32(U"单行代码过长!");
        }
        
        // highlight
        if (lang == U"matlabC")
            Matlab_highlight(code);
        else if (lang == U"matlab")
            Matlab_highlight(code);
        else if (lang == U"cpp")
            cpp_highlight(code);
        else {
            // language not supported, no highlight
            if (!lang.empty())
                SLS_WARN("lstlisting 环境不支持 " + utf32to8(lang) + " 语言， 未添加高亮！");
        }

        code_table(code_tab_str, code);

        str.replace(intvOut.L(i), intvOut.R(i) - intvOut.L(i) + 1, code_tab_str);
    }
    return N;
}

// find \bra{}\ket{} and mark
inline Long OneFile4(Str32_I path)
{
    Long ind0{}, ind1{}, N{};
    Str32 str;
    read(str, path); // read file
    CRLF_to_LF(str);
    while (true) {
        ind0 = str.find(U"\\bra", ind0);
        if (ind0 < 0) break;
        ind1 = ind0 + 4;
        ind1 = expect(str, U"{", ind1);
        if (ind1 < 0) {
            ++ind0; continue;
        }
        ind1 = pair_brace(str, ind1 - 1);
        ind1 = expect(str, U"\\ket", ind1 + 1);
        if (ind1 > 0) {
            str.insert(ind0, U"删除标记"); ++N;
            ind0 = ind1 + 4;
        }
        else
            ++ind0;
    }
    if (N > 0)
        write(str, path);
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
    if (ind0 < 0) {
        throw Str32(U"没有注释标题!");
    }
    ind0++;
    ind0 = expect(str, U"%", ind0);
    if (ind0 < 0) {
        SLS_WARN("请在第二行注释关键词： 例如 \"%关键词1|关键词2|关键词3\"，至少两个！");
        return 0;
    }
    Str32 line; get_line(line, str, ind0);
    Long tmp = line.find(U"|", 0);
    if (tmp < 0) {
        SLS_WARN("请在第二行注释关键词： 例如 \"%关键词1|关键词2|关键词3\"，至少两个！");
        return 0;
    }

    ind0 = 0;
    while (true) {
        Long ind1 = line.find(U"|", ind0);
        if (ind1 < 0)
            break;
        word = line.substr(ind0, ind1 - ind0);
        trim(word);
        if (word.empty()) {
            throw Str32(U"关键词不能为空！");
        }
        keywords.push_back(word);
        ind0 = ind1 + 1;
    }
    word = line.substr(ind0);
    trim(word);
    if (word.empty()) {
        throw Str32(U"关键词不能为空！");
    }
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

// generate html from tex
// output the chinese title of the file, id-label pairs in the file
// output dependency info from \pentry{}, links[i][0] --> links[i][1]
// entryName does not include ".tex"
// path0 is the parent folder of entryName.tex, ending with '\\'
inline Long PhysWikiOnline1(vecStr32_IO ids, vecStr32_IO labels, vecLong_IO links,
    vecStr32_I entries, vecStr32_I titles, Long_I ind, vecStr32_I rules,
    Str32_I path_in, Str32_I path_out, Str32_I url)
{
    Str32 str;
    read(str, path_in + "contents/" + entries[ind] + ".tex"); // read tex file
    CRLF_to_LF(str);
    Str32 title;
    // read html template and \newcommand{}
    Str32 html;
    read(html, path_out + "templates/entry_template.html");
    CRLF_to_LF(html);

    // read title from first comment
    get_title(title, str);

    // add keyword meta to html
    vecStr32 keywords;
    if (get_keywords(keywords, str) > 0) {
        Str32 keywords_str = keywords[0];
        for (Long i = 1; i < size(keywords); ++i) {
            keywords_str += U"," + keywords[i];
        }
        replace(html, U"PhysWikiHTMLKeywords", keywords_str);
    }
    else {
        replace(html, U"PhysWikiHTMLKeywords", U"高中物理, 物理竞赛, 大学物理, 高等数学");
    }

    if (!titles[ind].empty() && title != titles[ind]) {
        throw Str32(U"中文标题不符: " + title + " | " + titles[ind]);
    }
    // insert HTML title
    if (replace(html, U"PhysWikiHTMLtitle", title) != 1) {
        throw Str32(U"entry_template.html 格式错误!");
    }
    // save and replace verbatim code with an index
    vecStr32 str_verb;
    verbatim(str_verb, str);
    limit_env(str);

    // remove comments
    Intvs intvComm;
    find_comments(intvComm, str, U"%");
    for (Long i = intvComm.size() - 1; i >= 0; --i) {
        str.erase(intvComm.L(i), intvComm.R(i) - intvComm.L(i) + 1);
    }
    // add spaces betwen chinese char and alphanumeric char
    chinese_alpha_num_space(str);
    // add spaces around inline equation
    inline_eq_space(str);
    // escape characters
    NormalTextEscape(str);
    // add paragraph tags
    paragraph_tag(str);
    // itemize and enumerate
    Itemize(str); Enumerate(str);
    // add html id for links
    EnvLabel(ids, labels, entries[ind], str);
    // process table environments
    Table(str);
    // ensure '<' and '>' has spaces around them
    EqOmitTag(str);
    // process example and exercise environments
    theorem_like_env(str);
    // process figure environments
    FigureEnvironment(str, path_out, path_in, url);
    // get dependent entries from \pentry{}
    depend_entry(links, str, entries, ind);
    // process \pentry{}
    pentry(str);
    // replace user defined commands
    while (newcommand(str, rules) > 0);
    linebreak(str); // line breaks '\\'
    // replace \name{} with html tags
    Command2Tag(U"subsection", U"<h2 class = \"w3-text-indigo\"><b>", U"</b></h2>", str);
    Command2Tag(U"subsubsection", U"<h3><b>", U"</b></h3>", str);
    Command2Tag(U"bb", U"<b>", U"</b>", str); Command2Tag(U"textbf", U"<b>", U"</b>", str);
    // replace \upref{} with link icon
    upref(str, path_in, url);
    href(str);
    // replace environments with html tags
    Env2Tag(U"equation", U"<div class=\"eq\"><div class = \"w3-cell\" style = \"width:710px\">\n\\begin{equation}",
                        U"\\end{equation}\n</div></div>", str);
    Env2Tag(U"gather", U"<div class=\"eq\"><div class = \"w3-cell\" style = \"width:710px\">\n\\begin{gather}",
        U"\\end{gather}\n</div></div>", str);
    Env2Tag(U"align", U"<div class=\"eq\"><div class = \"w3-cell\" style = \"width:710px\">\n\\begin{align}",
        U"\\end{align}\n</div></div>", str);
    
    // footnote
    footnote(str, entries[ind], url);
    // delete redundent commands
    replace(str, U"\\dfracH", U"");
    // Matlab code
    MatlabCode(str, path_in, false);
    MatlabCode(str, path_in, true);
    // verbatim recover (in inverse order of `verbatim()`)
    lstlisting(str, str_verb);
    lstinline(str, str_verb);
    verb(str, str_verb);

    Command2Tag(U"x", U"<code>", U"</code>", str);
    // insert body Title
    if (replace(html, U"PhysWikiTitle", title) != 1) {
        throw Str32(U"\"PhysWikiTitle\" 在 entry_template.html 中未找到!");
    }
    // insert HTML body
    if (replace(html, U"PhysWikiHTMLbody", str) != 1) {
        throw Str32(U"\"PhysWikiHTMLbody\" 在 entry_template.html 中未找到!");
    }
    // save html file
    write(html, path_out + entries[ind] + ".html");
    return 0;
}

// generate json file containing dependency tree
// empty elements of 'titles' will be ignored
inline Long dep_json(vecStr32_I entries, vecStr32_I titles, vecLong_I links, Str32_I path_out)
{
    Str32 str;
    // write entries
    str += U"{\n  \"nodes\": [\n";
    for (Long i = 0; i < size(titles); ++i) {
        if (titles[i].empty())
            continue;
        str += U"    {\"id\": \"" + titles[i] + U"\", \"group\": 1, \"url\": \"../online/" +
            entries[i] + ".html\"},\n";
    }
    str.pop_back(); str.pop_back();
    str += U"\n  ],\n";

    // report redundency
    vector<Node> tree;
    vecLong links1;
    tree_gen(tree, links);
    Long ret = tree_redundant(links1, tree, titles);
    if (ret < 0) {
        throw Str32(U"预备知识层数过多： " + titles[-ret - 1] + " (" + entries[-ret - 1] + ") 可能存在循环预备知识！");
    }
    if (size(links1) > 0) {
        cout << "\n" << endl;
        cout << "==============  多余的预备知识  ==============" << endl;
        for (Long i = 0; i < size(links1); i += 2) {
            Long ind1 = links1[i], ind2 = links1[i + 1];
            cout << titles[ind1] << " (" << entries[ind1] << ") -> "
                << titles[ind2] << " (" << entries[ind2] << ")" << endl;
        }
        cout << "=============================================\n" << endl;
    }

    // write links
    str += U"  \"links\": [\n";
    for (Long i = 0; i < size(links)/2; ++i) {
        if (titles[links[2*i]].empty() || titles[links[2*i+1]].empty())
            continue;
        str += U"  {\"source\": \"" + titles[links[2*i]] + "\", ";
        str += U"\"target\": \"" + titles[links[2*i+1]] + U"\", ";
        str += U"\"value\": 1},\n";
    }
    if (links.size() > 0)
        str.pop_back(); str.pop_back();
    str += U"\n  ]\n}\n";
    write(str, path_out + U"../tree/data/dep.json");
    return 0;
}

// convert PhysWiki/ folder to wuli.wiki/online folder
inline void PhysWikiOnline(Str32_I path_in, Str32_I path_out, Str32_I path_data, Str32_I url)
{
    vecStr32 entries; // name in \entry{}, also .tex file name
    vecStr32 titles; // Chinese titles in \entry{}
    vecStr32 rules;  // for newcommand()
    entries_titles(titles, entries, path_in);
    write_vec_str(titles, path_data + U"titles.txt");
    write_vec_str(entries, path_data + U"entries.txt");
    define_newcommands(rules);

    cout << u8"正在从 main.tex 生成目录 index.html ...\n" << endl;

    table_of_contents(entries, path_in, path_out);

    // dependency info from \pentry, entries[link[2*i]] is in \pentry{} of entries[link[2*i+1]]
    vecLong links;
    // html tag id and corresponding latex label (e.g. Idlist[i]: "eq5", "fig3")
    // the number in id is the n-th occurrence of the same type of environment
    vecStr32 labels, ids;

    // 1st loop through tex files
    // files are processed independently
    // `IdList` and `LabelList` are recorded for 2nd loop
    cout << u8"======  第 1 轮转换 ======\n" << endl;
    for (Long i = 0; i < size(entries); ++i) {
        cout    << std::setw(5)  << std::left << i
                << std::setw(10)  << std::left << entries[i]
                << std::setw(20) << std::left << titles[i] << endl;
        // main process
        PhysWikiOnline1(ids, labels, links, entries, titles, i, rules, path_in, path_out, url);
    }

    // save id and label data
    write_vec_str(labels, path_data + U"labels.txt");
    write_vec_str(ids, path_data + U"ids.txt");

    // generate dep.json
    if (file_exist(path_out + U"../tree/data/dep.json"))
        dep_json(entries, titles, links, path_out);

    // 2nd loop through tex files
    // deal with autoref
    // need `IdList` and `LabelList` from 1st loop
    cout << "\n\n\n\n" << u8"====== 第 2 轮转换 ======\n" << endl;
    Str32 html;
    for (Long i = 0; i < size(entries); ++i) {
        cout    << std::setw(5)  << std::left << i
                << std::setw(10)  << std::left << entries[i]
                << std::setw(20) << std::left << titles[i] << endl;
        read(html, path_out + entries[i] + ".html"); // read html file
        // process \autoref and \upref
        autoref(ids, labels, entries[i], html, url);
        write(html, path_out + entries[i] + ".html"); // save html file
    }
    cout << endl;
}

// like PhysWikiOnline, but convert only specified files
// requires ids.txt and labels.txt output from `PhysWikiOnline()`
inline Long PhysWikiOnlineN(vecStr32_I entryN, Str32_I path_in, Str32_I path_out, Str32_I path_data, Str32_I url)
{
    // html tag id and corresponding latex label (e.g. Idlist[i]: "eq5", "fig3")
    // the number in id is the n-th occurrence of the same type of environment
    vecStr32 labels, ids, entries, titles;
    if (!file_exist(path_data + U"labels.txt")) {
        throw Str32(U"内部错误： " + path_data + U"labels.txt 不存在!");
    }
    read_vec_str(labels, path_data + U"labels.txt");
    Long ind = find_repeat(labels);
    if (ind >= 0) {
        throw Str32(U"内部错误： labels.txt 存在重复：" + labels[ind]);
    }
    if (!file_exist(path_data + U"ids.txt")) {
        throw Str32(U"内部错误： " + path_data + U"ids.txt 不存在!");
    }
    read_vec_str(ids, path_data + U"ids.txt");
    if (!file_exist(path_data + U"entries.txt")) {
        throw Str32(U"内部错误： " + path_data + U"entries.txt 不存在!");
    }
    read_vec_str(entries, path_data + U"entries.txt");
    if (!file_exist(path_data + U"titles.txt")) {
        throw Str32(U"内部错误： " + path_data + U"titles.txt 不存在!");
    }
    read_vec_str(titles, path_data + U"titles.txt");
    if (labels.size() != ids.size()) {
        throw Str32(U"内部错误： labels.txt 与 ids.txt 长度不符");
    }
    if (entries.size() != titles.size()) {
        throw Str32(U"内部错误： entries.txt 与 titles.txt 长度不符");
    }

    vecStr32 rules;  // for newcommand()
    define_newcommands(rules);

    // 1st loop through tex files
    // files are processed independently
    // `IdList` and `LabelList` are recorded for 2nd loop
    cout << u8"\n\n======  第 1 轮转换 ======\n" << endl;

    // main process
    vecLong links;
    for (Long i = 0; i < size(entryN); ++i) {
        Long ind = search(entryN[i], entries);
        if (ind < 0) {
            throw Str32(U"entries.txt 中未找到该词条!");
        }

        cout << std::setw(5) << std::left << ind
            << std::setw(10) << std::left << entries[ind]
            << std::setw(20) << std::left << titles[ind] << endl;

        PhysWikiOnline1(ids, labels, links, entries, titles, ind, rules, path_in, path_out, url);
    }
    
    write_vec_str(labels, path_data + U"labels.txt");
    write_vec_str(ids, path_data + U"ids.txt");

    // 2nd loop through tex files
    // deal with autoref
    // need `IdList` and `LabelList` from 1st loop
    cout << "\n\n\n" << u8"====== 第 2 轮转换 ======\n" << endl;

    Str32 html;
    for (Long i = 0; i < size(entryN); ++i) {
        Long ind = search(entryN[i], entries);
        cout << std::setw(5) << std::left << ind
            << std::setw(10) << std::left << entries[ind]
            << std::setw(20) << std::left << titles[ind] << endl;

        read(html, path_out + entries[ind] + ".html"); // read html file
        // process \autoref and \upref
        autoref(ids, labels, entries[ind], html, url);
        write(html, path_out + entries[ind] + ".html"); // save html file
    }

    cout << "\n\n" << endl;
    return 0;
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
        while (true) {
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

// check format error of .tex files in path0
inline void PhysWikiCheck(Str32_I path0)
{
    Long ind0{};
    vecStr32 names;
    file_list_ext(names, path0, U"tex", false);
    //RemoveNoEntry(names);
    if (names.size() <= 0) return;
    //names.resize(0); names.push_back(U"Sample"));
    for (unsigned i{}; i < names.size(); ++i) {
        cout << i << " ";
        cout << names[i] << "...";
        // main process
        cout << OneFile4(path0 + names[i] + ".tex");
        cout << endl;
    }
}
