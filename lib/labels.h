#pragma once

// get label type (eq, fig, ...)
inline Str label_type(Str_I label)
{
    assert(label[0] != ' '); assert(label.back() != ' ');
    Long ind1 = label.find('_');
    return label.substr(0, ind1);
}

// get label id
inline Str label_id(Str_I label)
{
    assert(label[0] != ' '); assert(label.back() != ' ');
    Long ind1 = label.find('_');
    return label.substr(ind1+1);
}

// get entry from label (old format)
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
inline Long EnvLabel(vecStr_O fig_ids, vecLong_O fig_orders,
                     vecStr_O labels, vecLong_O label_orders, Str_I entry, Str_IO str)
{
    Long ind0{}, ind2{}, ind3{}, ind4{}, N{},
        Ngather{}, Nalign{}, i{}, j{};
    Str type; // "eq", "fig", "ex", "sub"...
    Str envName; // "equation" or "figure" or "example"...
    Long idN{}; // convert to idNum
    Str label, fig_id;
    fig_ids.clear(); fig_orders.clear();
    labels.clear(); label_orders.clear();
    while (1) {
        const Long ind5 = find_command(str, "label", ind4);
        if (ind5 < 0) return N;
        // detect environment kind
        ind2 = str.rfind("\\end", ind5);
        ind4 = str.rfind("\\begin", ind5);
        if (ind4 < 0 || (ind4 >= 0 && ind2 > ind4)) {
            // label not in environment, must be a subsection label
            Long ind7 = str.rfind("\\subsection", ind5);
            Long ind8 = str.rfind("\n", ind5);
            if (ind7 < 0 || ind8 > ind7)
                throw scan_err(u8"环境外的 \\label 视为 \\subsection{} 的标签， 必须紧接在后面（不能换行）： " + str.substr(ind5, 22));
            type = "sub"; envName = "subsection";
        }
        else {
            Long ind1 = expect(str, "{", ind4 + 6);
            if (ind1 < 0)
                throw scan_err(u8"\\begin 后面没有 {： " + str.substr(ind4, 22));
            if (expect(str, "equation", ind1) > 0) {
                type = "eq"; envName = "equation";
            }
            else if (expect(str, "figure", ind1) > 0) {
                type = "fig"; envName = "figure";
                Long ind7 = str.rfind("\\caption", ind5);
                Long ind8 = str.rfind("\n", ind5);
                if (ind7 < 0 || ind8 > ind7)
                    throw scan_err(u8"figure 环境中的 \\label 必须紧接在 \\caption{} 后面（不能换行）： " + str.substr(ind5, 22));
            }
            else if (expect(str, "definition", ind1) > 0) {
                type = "def"; envName = "definition";
            }
            else if (expect(str, "lemma", ind1) > 0) {
                type = "lem"; envName = "lemma";
            }
            else if (expect(str, "theorem", ind1) > 0) {
                type = "the"; envName = "theorem";
            }
            else if (expect(str, "corollary", ind1) > 0) {
                type = "cor"; envName = "corollary";
            }
            else if (expect(str, "example", ind1) > 0) {
                type = "ex"; envName = "example";
            }
            else if (expect(str, "exercise", ind1) > 0) {
                type = "exe"; envName = "exercise";
            }
            else if (expect(str, "table", ind1) > 0) {
                type = "tab"; envName = "table";
            }
            else if (expect(str, "gather", ind1) > 0) {
                type = "eq"; envName = "gather";
            }
            else if (expect(str, "align", ind1) > 0) {
                type = "eq"; envName = "align";
            }
            else {
                throw scan_err(u8"该环境不支持 label");
            }
        }
        
        // check label format and save label
        ind0 = expect(str, "{", ind5 + 6);
        ind3 = expect(str, type + '_' + entry, ind0);
        if (ind3 < 0)
            throw scan_err("label " + str.substr(ind0, 20)
                + u8"... 格式错误， 是否为 \"" + type + '_' + entry + u8"\"？");
        ind3 = str.find("}", ind3);
        
        label = str.substr(ind0, ind3 - ind0);
        trim(label);
        if (type == "fig") {
            fig_id = label_id(label);
            Long ind = search(fig_id, fig_ids);
            if (ind < 0)
                fig_ids.push_back(fig_id);
            else
                throw scan_err(u8"标签多次定义： " + label);
        }
        else {
            Long ind = search(label, labels);
            if (ind < 0)
                labels.push_back(label);
            else
                throw scan_err(u8"标签多次定义： " + label);
        }
        
        // count idNum, insert html id tag, delete label
        Intvs intvEnv;
        if (type == "eq") { // count equations
            idN = find_env(intvEnv, str.substr(0,ind4), "equation");
            Ngather = find_env(intvEnv, str.substr(0,ind4), "gather");
            if (Ngather > 0) {
                for (i = 0; i < Ngather; ++i) {
                    for (j = intvEnv.L(i); j < intvEnv.R(i); ++j) {
                        if (str.at(j) == '\\' && str.at(j+1) == '\\')
                            ++idN;
                    }
                    ++idN;
                }
            }
            Nalign = find_env(intvEnv, str.substr(0,ind4), "align");
            if (Nalign > 0) {
                for (i = 0; i < Nalign; ++i) {
                    for (j = intvEnv.L(i); j < intvEnv.R(i); ++j) {
                        if (str.at(j) == '\\' && str.at(j + 1) == '\\')
                            ++idN;
                    }
                    ++idN;
                }
            }
            if (envName == "gather" || envName == "align") {
                for (i = ind4; i < ind5; ++i) {
                    if (str.at(i) == '\\' && str.at(i + 1) == '\\')
                        ++idN;
                }
            }
            ++idN;
        }
        else if (type == "sub") { // count \subsection number
            Long ind = -1; idN = 0; ind4 = -1;
            while (1) {
                ind = find_command(str, "subsection", ind + 1);
                if (ind > ind5 || ind < 0)
                    break;
                ind4 = ind; ++idN;
            }
        }
        else {
            idN = find_env(intvEnv, str.substr(0, ind4), envName) + 1;
        }
        if (type == "fig") {
            if (search(idN, fig_orders) >= 0)
                throw scan_err(u8"图片似乎定义了多个 \\label{}： fig_" + fig_ids.back());
            if (fig_orders.empty()) {
                if (idN != 1)
                    throw scan_err(u8"似乎第一个图片环境没有定义 \\label{}");
            }
            else if (idN != fig_orders.back()+1)
                throw scan_err(u8"似乎第 " + to_string(fig_orders.back()+1) + u8" 个图片环境没有定义 \\label{}");
            fig_orders.push_back(idN);
        }
        else
            label_orders.push_back(idN);
        str.erase(ind5, ind3 - ind5 + 1);
        str.insert(ind4, "<span id = \"" + label + "\"></span>");
    }
}

// replace autoref with html link
// no comment allowed
// does not add link for \autoref inside eq environment (equation, align, gather)
// return number of autoref replaced, or -1 if failed
// new_label_ref_by: in database, these labels should append `entry` to "ref_by"
// new_fig_ref_by: in database, these figures should append `entry` to "ref_by"
inline Long autoref(unordered_set<Str> &add_refs, // labels to add to entry.refs
                    unordered_set<Str> &del_refs, // labels to delete from entry.refs
                    Str_IO str, Str_I entry, SQLite::Database &db_read)
{
    Long ind0{}, ind1{}, ind2{}, ind3{}, N{}, ienv{};
    Bool inEq;
    Str entry1, label0, fig_id, type, kind, newtab, file;
    vecStr envNames{"equation", "align", "gather"};
    Str db_ref_by_str;
    Str ref_by_str;
    set<Str> ref_by;

    SQLite::Statement stmt_select(db_read,
        R"(SELECT "order", "ref_by" FROM "labels" WHERE "id"=?;)");
    SQLite::Statement stmt_select_fig(db_read,
        R"(SELECT "order", "ref_by" FROM "figures" WHERE "id"=?;)");
    SQLite::Statement stmt_select0(db_read,
        R"(SELECT "refs" FROM "entries" WHERE "id"=')" + entry + "';");

    if (!stmt_select0.executeStep())
        throw scan_err(u8"词条不存在： " + entry);
    parse(del_refs, stmt_select0.getColumn(0));

    while (1) {
        newtab.clear(); file.clear();
        ind0 = find_command(str, "autoref", ind0);
        if (is_in_tag(str, "code", ind0)) {
            ++ind0; continue;
        }
        if (ind0 < 0)
            break;
        inEq = index_in_env(ienv, ind0, envNames, str);
        ind1 = expect(str, "{", ind0 + 8);
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
            if (type == "eq") kind = u8"式";
            else if (type == "fig") kind = u8"图";
            else if (type == "def") kind = u8"定义";
            else if (type == "lem") kind = u8"引理";
            else if (type == "the") kind = u8"定理";
            else if (type == "cor") kind = u8"推论";
            else if (type == "ex") kind = u8"例";
            else if (type == "exe") kind = u8"习题";
            else if (type == "tab") kind = u8"表";
            else if (type == "sub") kind = u8"子节";
            else if (type == "lst") {
                kind = u8"代码";
                SLS_WARN(u8"autoref lstlisting 功能未完成！");
                ++ind0; continue;
            }
            else
                throw scan_err(u8"\\label 类型错误， 必须为 eq/fig/def/lem/the/cor/ex/exe/tab/sub/lst 之一");
        }
        else {
            if      (type == "eq")  kind = "eq. ";
            else if (type == "fig") kind = "fig. ";
            else if (type == "def") kind = "def. ";
            else if (type == "lem") kind = "lem. ";
            else if (type == "the") kind = "thm. ";
            else if (type == "cor") kind = "cor. ";
            else if (type == "ex")  kind = "ex. ";
            else if (type == "exe") kind = "exer. ";
            else if (type == "tab") kind = "tab. ";
            else if (type == "sub") kind = "sub. ";
            else if (type == "lst") {
                kind = "code. ";
                SLS_WARN(u8"autoref lstlisting 功能未完成！");
                ++ind0; continue;
            }
            else
                throw scan_err(u8"\\label 类型错误， 必须为 eq/fig/def/lem/the/cor/ex/exe/tab/sub/lst 之一");
        }
        ind3 = str.find('}', ind3);
        label0 = str.substr(ind1, ind3 - ind1); trim(label0);
        Long db_label_order;
        if (type == "fig") {
            fig_id = label_id(label0);
            del_refs.erase("fig_" + fig_id);
            stmt_select_fig.bind(1, fig_id);
            if (!stmt_select_fig.executeStep())
                throw scan_err(u8"\\autoref{} 中标签未找到： " + label0);
            db_label_order = int(stmt_select_fig.getColumn(0));
            parse(ref_by, stmt_select_fig.getColumn(1));
            stmt_select_fig.reset();

            if (!ref_by.count(entry))
                add_refs.insert("fig_" + fig_id);
        }
        else {
            del_refs.erase(label0);
            stmt_select.bind(1, label0);
            if (!stmt_select.executeStep())
                throw scan_err(u8"\\autoref{} 中标签未找到： " + label0);
            db_label_order = int(stmt_select.getColumn(0));
            parse(ref_by, stmt_select.getColumn(1));
            stmt_select.reset();

            if (!ref_by.count(entry))
                add_refs.insert(label0);
        }

        file = gv::url + entry1 + ".html";
        if (entry1 != entry) // reference another entry1
            newtab = "target = \"_blank\"";
        if (!inEq)
            str.insert(ind3 + 1, " </a>");
        str.insert(ind3 + 1, kind + ' ' + num2str32(db_label_order));
        if (!inEq)
            str.insert(ind3 + 1, "<a href = \"" + file + "#" + label0 + "\" " + newtab + ">");
        str.erase(ind0, ind3 - ind0 + 1);
        ++N;
    }
    return N;
}

// get a new label name for an environment for an entry
inline void new_label_name(Str_O label, Str_I envName, Str_I entry, Str_I str)
{
    Str label0;
    for (Long num = 1; ; ++num) {
        Long ind0 = 0;
        while (1) {
            label = envName + "_" + entry + "_" + num2str(num);
            ind0 = find_command(str, "label", ind0);
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
// order is not the label number, but the displayed number
// if exist, return 1, output label
// if doesn't exist, return 0
// dry_run : don't actually modify tex file
inline Long check_add_label(Str_O label, Str_I entry, Str_I type, Long_I order, Bool_I dry_run = false)
{
    Long ind0 = 0;
    Str label0, newtab;


    SQLite::Database db_read(gv::path_data + "scan.db", SQLite::OPEN_READONLY);

    // check if label exist
    if (type == "fig") {
        SQLite::Statement stmt_select_fig(db_read,
            R"(SELECT "id" FROM "figures" WHERE "entry"=? AND "order"=?;)");
        stmt_select_fig.bind(1, entry);
        stmt_select_fig.bind(2, (int)order);
        if (stmt_select_fig.executeStep()) {
            label = "fig_";
            label += (const char *) stmt_select_fig.getColumn(0);
            stmt_select_fig.reset();
            return 1;
        }
        else
            throw scan_err(u8"找不到图片， 请确认图片存在且成功编译， 且用 \\label{} 定义了标签。 要上传图片请使用菜单栏的【上传图片】按钮， 成功后会自动插入图片环境并创建 label。");
    }
    else {
        SQLite::Statement stmt_select_label(db_read,
            R"(SELECT "id" FROM "labels" WHERE "type"=? AND "entry"=? AND "order"=?;)");
        stmt_select_label.bind(1, type);
        stmt_select_label.bind(2, entry);
        stmt_select_label.bind(3, (int) order);
        if (stmt_select_label.executeStep()) {
            label = (const char *) stmt_select_label.getColumn(0);
            stmt_select_label.reset();
            return 1;
        }
    }
    // label does not exist

    // insert \label{} command
    Str full_name = gv::path_in + "/contents/" + entry + ".tex";
    if (!file_exist(full_name))
        throw scan_err(u8"文件不存在： " + entry + ".tex");
    Str str;
    read(str, full_name);

    // find comments
    Intvs intvComm;
    find_comments(intvComm, str, "%");

    vecStr types = { "eq", "fig", "def", "lem",
        "the", "cor", "ex", "exe", "tab", "sub" };
    vecStr envNames = { "equation", "figure", "definition", "lemma",
        "theorem", "corollary", "example", "exercise", "table"};

    Long idNum = search(type, types);
    if (idNum < 0)
        throw scan_err(u8"\\label 类型错误， 必须为 eq/fig/def/lem/the/cor/ex/exe/tab/sub 之一");
    
    // count environment display number starting at ind4
    Intvs intvEnv;
    if (type == "eq") { // add equation labels
        // count equations
        ind0 = 0;
        Long idN = 0;
        vecStr eq_envs = { "equation", "gather", "align" };
        Str env0;
        while (1) {
            ind0 = find_command(str, "begin", ind0);
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
            if (idN == order) {
                new_label_name(label, type, entry, str);
                ind0 = skip_command(str, ind0, 1);
                if (!dry_run) {
                    str.insert(ind0, "\\label{" + label + "}");
                    write(str, full_name);
                }
                break;
            }
            if (ienv > 0) {// found gather or align
                throw scan_err(u8"暂不支持引用含有 align 和 gather 环境的文件，请手动插入 label 并引用。");
                Long ind1 = skip_env(str, ind0);
                for (Long i = ind0; i < ind1; ++i) {
                    if (str.substr(i, 2) == "\\\\" && current_env(i, str) == eq_envs[ienv]) {
                        ++idN;
                        if (idN == order) {
                            new_label_name(label, type, entry, str);
                            ind0 = skip_command(str, ind0, 1);
                            if (!dry_run) {
                                str.insert(i + 2, "\n\\label{" + label + "}");
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
    else if (type == "sub") { // add subsection labels
        Long ind0 = -1;
        for (Long i = 0; i < order; ++i) {
            ind0 = find_command(str, "subsection", ind0+1);
            if (ind0 < 0)
                throw scan_err(u8"被引用对象不存在");
        }
        ind0 = skip_command(str, ind0, 1);
        new_label_name(label, type, entry, str);
        if (!dry_run) {
            str.insert(ind0, "\\label{" + label + "}");
            write(str, full_name);
        }
    }
    else { // add environment labels
        Long Nid = find_env(intvEnv, str, envNames[idNum], 'i');
        if (order > Nid)
            throw scan_err(u8"被引用对象不存在");

        new_label_name(label, type, entry, str);
        // this doesn't work for figure invironment, since there is an [ht] option
        if (!dry_run) {
            str.insert(intvEnv.L(order - 1), "\\label{" + label + "}");
            write(str, full_name);
        }
    }

    if (!dry_run) {
        // insert into "labels"
        SQLite::Database db_rw(gv::path_data + "scan.db", SQLite::OPEN_READWRITE);
        SQLite::Transaction transaction(db_rw);
        SQLite::Statement stmt_insert(db_rw,
            R"(INSERT INTO "labels" ("id", "type", "entry", "order") VALUES (?, ?, ?, ?);)");
        stmt_insert.bind(1, label);
        stmt_insert.bind(2, type);
        stmt_insert.bind(3, entry);
        stmt_insert.bind(4, (int)order);
        stmt_insert.exec(); stmt_insert.reset();

        // update "entries.refs"
        set<Str> labels;
        parse(labels, get_text("entries", "id", entry, "labels", db_rw));
        labels.insert(label);
        Str labels_str; join(labels_str, labels);
        SQLite::Statement stmt_update(db_rw,
            R"(UPDATE "entries" SET "labels"=? WHERE "id"=?;)");
        stmt_update.bind(1, labels_str);
        stmt_update.bind(2, entry);
        stmt_update.exec(); stmt_update.reset();
        transaction.commit();
    }
    return 0;
}

// process upref
// path must end with '\\'
inline Long upref(unordered_map<Str, Bool> &uprefs_change, // entry -> [1]add/[0]del
     Str_IO str, Str_I entry, SQLite::Database &db_read)
{
    uprefs_change.clear();
    SQLite::Statement stmt_select(db_read,
        R"(SELECT "uprefs" FROM "entries" WHERE "id"=?;)");
    stmt_select.bind(1, entry);
    if (!stmt_select.executeStep())
        throw internal_err(SLS_WHERE);
    unordered_set<Str> db_uprefs;
    parse(db_uprefs, stmt_select.getColumn(0));

    Long ind0 = 0, right, N = 0;
    Str entry1;
    while (1) {
        ind0 = find_command(str, "upref", ind0);
        if (ind0 < 0)
            return N;
        command_arg(entry1, str, ind0);
        if (entry1 == entry)
            throw scan_err(u8"不允许 \\upref{" + entry1 + u8"} 本词条");
        trim(entry1);
        if (!file_exist(gv::path_in + "contents/" + entry1 + ".tex")) {
            throw scan_err(u8"\\upref 引用的文件未找到： " + entry1 + ".tex");
        }
        right = skip_command(str, ind0, 1);
        str.replace(ind0, right - ind0,
                    R"(<span class = "icon"><a href = ")"
                    + gv::url + entry1 +
                    R"(.html" target = "_blank"><i class = "fa fa-external-link"></i></a></span>)");
        ++N;

        // db
        if (!db_uprefs.count(entry1)) {
            SLS_WARN(u8"检测到新增的 upref（将添加）：" + entry1);
            uprefs_change[entry1] = true;
        }
        else
            db_uprefs.erase(entry1);
    }
    for (auto &e : db_uprefs) {
        SLS_WARN(u8"检测到删除的 upref（将删除）：" + e);
        uprefs_change[e] = false;
    }
    return N;
}

// add equation tags
inline Long equation_tag(Str_IO str, Str_I nameEnv)
{
    Long page_width = 900;
    Long i{}, N{}, Nenv;
    Intvs intvEnvOut, intvEnvIn;
    Nenv = find_env(intvEnvIn, str, nameEnv, 'i');
    find_env(intvEnvOut, str, nameEnv, 'o');

    for (i = Nenv - 1; i >= 0; --i) {
        Long iname, width = page_width - 35;
        if (index_in_env(iname, intvEnvOut.L(i), { "example", "exercise", "definition", "theorem", "lemma", "corollary"}, str))
            width -= 40;
        if (index_in_env(iname, intvEnvOut.L(i), { "itemize", "enumerate" }, str))
            width -= 40;
        Str strLeft = "<div class=\"eq\"><div class = \"w3-cell\" style = \"width:" + num2str32(width) + "px\">\n\\begin{" + nameEnv + "}";
        Str strRight = "\\end{" + nameEnv + "}\n</div></div>";
        str.replace(intvEnvIn.R(i) + 1, intvEnvOut.R(i) - intvEnvIn.R(i), strRight);
        str.replace(intvEnvOut.L(i), intvEnvIn.L(i) - intvEnvOut.L(i), strLeft);
    }
    return N;
}
