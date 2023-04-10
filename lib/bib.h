#pragma once

// replace "\cite{}" with `[?]` cytation linke
inline Long cite(Str_IO str, SQLite::Database &db_read)
{
    SQLite::Statement stmt_select(db_read,
                                  R"(SELECT "order", "details" from "bibliography" WHERE "id"=?;)");
    Long ind0 = 0, N = 0;
    Str bib_label;
    while (1) {
        ind0 = find_command(str, u8"cite", ind0);
        if (ind0 < 0)
            return N;
        ++N;
        command_arg(bib_label, str, ind0);
        stmt_select.bind(1, bib_label);
        if (!stmt_select.executeStep())
            throw scan_err(u8"文献 label 未找到（请检查并编译 bibliography.tex）：" + bib_label);
        Long ibib = (int)stmt_select.getColumn(0);
        // bib_detail = (const char*)stmt_select.getColumn(1);
        stmt_select.reset();
        Long ind1 = skip_command(str, ind0, 1);
        str.replace(ind0, ind1 - ind0, u8" <a href=\"" + gv::url + "bibliography.html#"
            + num2str(ibib+1) + "\">[" + num2str(ibib+1) + "]</a> ");
    }
}

inline Long bibliography(vecStr_O bib_labels, vecStr_O bib_details)
{
    Long N = 0;
    Str str, bib_list, bib_label;
    bib_labels.clear(); bib_details.clear();
    read(str, gv::path_in + u8"bibliography.tex");
    CRLF_to_LF(str);
    Long ind0 = 0;
    while (1) {
        ind0 = find_command(str, u8"bibitem", ind0);
        if (ind0 < 0) break;
        command_arg(bib_label, str, ind0);
        bib_labels.push_back(bib_label);
        ind0 = skip_command(str, ind0, 1);
        ind0 = expect(str, u8"\n", ind0);
        bib_details.push_back(getline(str, ind0));
        href(bib_details.back()); Command2Tag(u8"textsl", u8"<i>", u8"</i>", bib_details.back());
        ++N;
        bib_list += u8"<span id = \"" + num2str(N) + "\"></span>[" + num2str(N) + u8"] " + bib_details.back() + u8"<br>\n";
    }
    Long ret = find_repeat(bib_labels);
    if (ret > 0)
        throw scan_err(u8"文献 label 重复：" + bib_labels[ret]);
    Str html;
    read(html, gv::path_out + u8"templates/bib_template.html");
    replace(html, u8"PhysWikiBibList", bib_list);
    write(html, gv::path_out + u8"bibliography.html");
    return N;
}
