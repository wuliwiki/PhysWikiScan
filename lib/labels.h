#pragma once

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
                throw scan_err(u8"该环境不支持 label");
            }
        }
        
        // check label format and save label
        ind0 = expect(str, U"{", ind5 + 6);
        ind3 = expect(str, type + U'_' + entry, ind0);
        if (ind3 < 0) {
            throw scan_err(U"label " + str.substr(ind0, 20) + U"... 格式错误， 是否为 \"" + type + U'_' + entry + U"\"？");
        }
        ind3 = str.find(U"}", ind3);
        
        label = str.substr(ind0, ind3 - ind0);
        trim(label);
        Long ind = search(label, labels);
        if (ind < 0)
            labels.push_back(label);
        else
            throw scan_err(U"标签多次定义： " + labels[ind]);
        
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


// replace autoref with html link
// no comment allowed
// does not add link for \autoref inside eq environment (equation, align, gather)
// return number of autoref replaced, or -1 if failed
inline Long autoref(Str32_IO str, Str32_I entry, SQLite::Database &db_rw)
{
    Long ind0{}, ind1{}, ind2{}, ind3{}, N{}, ienv{};
    Bool inEq;
    Str32 entry1, label0, type, kind, newtab, file;
    vecStr32 envNames{U"equation", U"align", U"gather"};
    Str32 db_ref_by_str;
    Str ref_by_str;
    set<Str> ref_by;

    SQLite::Statement stmt_select(db_rw,
                                   R"(SELECT "order", "ref_by" FROM "labels" WHERE "id"=?;)");
    SQLite::Statement stmt_select_fig(db_rw,
                                      R"(SELECT "order", "ref_by" FROM "figures" WHERE "id"=?;)");

    SQLite::Statement stmt_update_ref_by(db_rw,
                                         R"(UPDATE "labels" SET "ref_by"=? WHERE "id"=?;)");
    SQLite::Statement stmt_update_ref_by_fig(db_rw,
                                             R"(UPDATE "figures" SET "ref_by"=? WHERE "id"=?;)");

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
            throw scan_err(u8"\\autoref 变量不能为空");
        ind1 = NextNoSpace(entry1, str, ind1);
        ind2 = str.find('_', ind1);
        if (ind2 < 0)
            throw scan_err(u8"\\autoref 格式错误");
        ind3 = find_num(str, ind2);
        if (ind3 < 0)
            throw scan_err(u8"autoref 格式错误");
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
                throw scan_err(u8"\\label 类型错误， 必须为 eq/fig/def/lem/the/cor/ex/exe/tab/sub/lst 之一");
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
                throw scan_err(u8"\\label 类型错误， 必须为 eq/fig/def/lem/the/cor/ex/exe/tab/sub/lst 之一");
        }
        ind3 = str.find('}', ind3);
        label0 = str.substr(ind1, ind3 - ind1); trim(label0);
        Long db_label_order;
        if (type == U"fig") {
            stmt_select_fig.bind(1, u8(label_id(label0)));
            if (!stmt_select_fig.executeStep())
                throw scan_err(U"\\autoref{} 中标签未找到： " + label0);
            db_label_order = int(stmt_select_fig.getColumn(0));
            parse(ref_by, stmt_select_fig.getColumn(1));
            stmt_select_fig.reset();

            if (ref_by.insert(u8(entry)).second) {
                // inserted, update db
                join(ref_by_str, ref_by);
                stmt_update_ref_by_fig.bind(1, ref_by_str);
                stmt_update_ref_by_fig.bind(2, u8(label0));
                stmt_update_ref_by_fig.exec(); stmt_update_ref_by.reset();
            }
        } else {
            stmt_select.bind(1, u8(label0));
            if (!stmt_select.executeStep())
                throw scan_err(U"\\autoref{} 中标签未找到： " + label0);
            db_label_order = int(stmt_select.getColumn(0));
            parse(ref_by, stmt_select.getColumn(1));
            stmt_select.reset();

            if (ref_by.insert(u8(entry)).second) {
                // inserted, update db
                join(ref_by_str, ref_by);
                stmt_update_ref_by.bind(1, ref_by_str);
                stmt_update_ref_by.bind(2, u8(label0));
                stmt_update_ref_by.exec(); stmt_update_ref_by.reset();
            }
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
        throw scan_err(u8"每个图片上传后都会自动创建 label， 如果没有请手动在 \\caption{} 后面添加。");

    // insert \label{} command
    Str32 full_name = gv::path_in + "/contents/" + entry + ".tex";
    if (!file_exist(full_name))
        throw scan_err(U"文件不存在： " + entry + ".tex");
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
        throw scan_err(u8"\\label 类型错误， 必须为 eq/fig/def/lem/the/cor/ex/exe/tab/sub 之一");
    
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
                throw scan_err(u8"被引用公式不存在");
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
                throw scan_err(u8"暂不支持引用含有 align 和 gather 环境的文件，请手动插入 label 并引用。");
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
                throw scan_err(u8"被引用对象不存在");
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
            throw scan_err(u8"被引用对象不存在");

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
            throw scan_err(U"不允许 \\upref{" + entry1 + U"} 本词条");
        trim(entry1);
        if (!file_exist(gv::path_in + U"contents/" + entry1 + U".tex")) {
            throw scan_err(U"\\upref 引用的文件未找到： " + entry1 + U".tex");
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
