#pragma once

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
            throw scan_err(u8"图片必须有标签， 请使用上传图片按钮。");
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
            throw scan_err(U"图" + figNo + U"尺寸不能超过 14.25cm");

        // get file name of figure
        Long indName1 = str.find(U"figures/", ind0) + 8;
        Long indName2 = str.find(U"}", ind0) - 1;
        if (indName1 < 0 || indName2 < 0) {
            throw scan_err(u8"读取图片名错误");
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
            throw internal_err(U"图片格式不支持：" + figName);

        fname_in = gv::path_in + U"figures/" + figName + U"." + format;

        if (!file_exist(fname_in))
            throw internal_err(U"图片 \"" + fname_in + U"\" 未找到");

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
            throw scan_err(u8"图片标题未找到， 请使用上传图片按钮！");
        }
        command_arg(caption, str, ind0);
        if ((Long)caption.find(U"\\footnote") >= 0)
            throw scan_err(u8"图片标题中不能添加 \\footnote{}");
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

// issue environment
inline Long issuesEnv(Str32_IO str)
{
    Long Nenv = Env2Tag(U"issues", U"<div class = \"w3-panel w3-round-large w3-sand\"><ul>", U"</ul></div>", str);
    if (Nenv > 1)
        throw scan_err(u8"不支持多个 issues 环境， issues 必须放到最开始");
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
