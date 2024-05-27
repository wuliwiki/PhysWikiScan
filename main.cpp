#define VERSION_MAJOR 0
#define VERSION_MINOR 12
#define VERSION_PATCH 10

#ifdef _MSC_VER
#define SLS_HAS_FILESYSTEM
#endif
#include "lib/PhysWikiScan.h"

// get arguments
void get_args(vecStr_O args, Int_I argc, const char *argv[])
{
	args.clear();
	if (argc > 1) {
		// convert argv to args
		Str temp;
		for (Int i = 1; i < argc; ++i) {
			temp = argv[i];
			args.push_back(temp);
		}
	}
	else {
		// input args
		cout << "#===========================#" << endl;
		cout << "#     PhysWikiScan          #" << endl;
		cout << "#===========================#\n" << endl;

		cout << u8"请输入 arguments" << endl;
		Str temp; getline(std::cin, temp);
		Long ind0, ind1 = 0;
		ind0 = temp.find_first_not_of(' ', ind1);
		for (Int i = 0; i < 100; ++i) {
			ind1 = find(temp, ' ', ind0);
			if (ind1 < 0) {
				if (size(temp) > ind0)
					args.push_back(temp.substr(ind0));
				break;
			}

			args.push_back(temp.substr(ind0, ind1 - ind0));
			ind0 = temp.find_first_not_of(' ', ind1);
		}
	}
}

// read set_path.txt
// return paths_in.size()
Long read_path_file(vecStr_O paths_in, vecStr_O paths_out, vecStr_O paths_data, vecStr_O urls)
{
	Str temp, line;
	if (!file_exist("set_path.txt")) {
		throw internal_err(u8"set_path.txt 不存在!");
	}
	read(temp, "set_path.txt");
	CRLF_to_LF(temp);

	Long ind0 = 0;
	for (Long i = 0; i < 100; ++i) {
		// paths_in
		ind0 = skip_line(temp, ind0);
		if (ind0 < 0)
			throw internal_err(u8"path.txt 格式 (1)");
		ind0 = get_line(line, temp, ind0);
		if (ind0 < 0)
			throw internal_err(u8"path.txt 格式 (2)");
		paths_in.push_back(line); trim(paths_in.back());

		// paths_out
		ind0 = skip_line(temp, ind0);
		if (ind0 < 0)
			throw internal_err(u8"path.txt 格式 (3)");
		ind0 = get_line(line, temp, ind0);
		if (ind0 < 0)
			throw internal_err(u8"path.txt 格式 (4)");
		paths_out.push_back(line); trim(paths_out.back());

		// paths_data
		ind0 = skip_line(temp, ind0);
		if (ind0 < 0)
			throw internal_err(u8"path.txt 格式 (5)");
		ind0 = get_line(line, temp, ind0);
		if (ind0 < 0)
			throw internal_err(u8"path.txt 格式 (6)");
		paths_data.push_back(line); trim(paths_out.back());

		// urls
		ind0 = skip_line(temp, ind0);
		if (ind0 < 0)
			throw internal_err(u8"path.txt 格式 (7)");
		ind0 = get_line(line, temp, ind0);
		urls.push_back(line); trim(paths_out.back());

		if (ind0 < 0)
			break;
	}
	return size(paths_in);
}

// get path and remove --path options from args
void get_path(Str_O path_in, Str_O path_out, Str_O path_data, Str_O url, vecStr_IO args)
{
	Long N = size(args);
	path_in.clear(); path_out.clear(); path_data.clear(); url.clear();

	// directly specify path (depricated)
	if (N > 3 && args[N - 4] == "--path-in-out-data") {
		path_in = args[N - 3];
		path_out = args[N - 2];
		path_data = args[N - 1];
		url = "";
		args.resize(args.size()-4);
		return;
	}

	// directly specify paths
	if (N > 5 && args[N - 5] == "--path-in-out-data-url") {
		path_in = args[N - 4];
		path_out = args[N - 3];
		path_data = args[N - 2];
		url = args[N - 1];
		args.resize(args.size()-5);
		return;
	}

	// paths for user note
	if (N > 1 && args[N - 2] == "--path-user") {
		Str sub_folder;
		Str &id = args.back(); // authors.id
		if (id.back() != '/') id += '/';
		if (size(id) > 8 && id.substr(id.size()-8) == "/online/") {
			id.erase(id.size()-7); sub_folder = "online/";
		}
		else if (size(id) > 9 && id.substr(id.size()-9) == "/changed/") {
			id.erase(id.size()-8); sub_folder = "changed/";
		}
		else
			sub_folder = "changed/";

		path_in << "../user-notes/" << id;
		path_out << "../user-notes/" << id << sub_folder;
		path_data << "../user-notes/" << id << "cmd_data/";
		url << "https://wuli.wiki/user/" << id << sub_folder;
		args.resize(args.size()-2);
		return;
	}

	// use path number in set_path.txt
	vecStr paths_in, paths_out, paths_data, urls;
	read_path_file(paths_in, paths_out, paths_data, urls);

	if (N > 1 && args[N - 2] == "--path") {
		Llong i_path;
		if (str2int(i_path, args.back()) == size(args.back())
			&& i_path < size(paths_in)) {
			path_in = paths_in[i_path];
			path_out = paths_out[i_path];
			path_data = paths_data[i_path];
			url = urls[i_path];
		}
		else
			throw internal_err(u8"非法的 --path 参数");
		args.resize(args.size()-2);
		return;
	}

	// default path
	path_in = paths_in[0];
	path_out = paths_out[0];
	path_data = paths_data[0];
	url = urls[0];
}

inline void replace_eng_punc_to_chinese(Str_I path_in)
{
	vecStr names, str_verb;
	Str fname, str;
	Intvs intv;
	file_list_ext(names, path_in + "contents/", "tex", false);

	//RemoveNoEntry(names);
	if (names.empty()) return;
	//names.resize(0); names.push_back("Sample"));

	vecStr skip_list = { "Sample", "edTODO" };
	for (Long i = 0; i < size(names); ++i) {
		cout << i << " ";
		cout << names[i] << "...";
		if (search(names[i], skip_list) >= 0)
			continue;
		// main process
		fname = path_in + "contents/" + names[i] + ".tex";
		read(str, fname);
		CRLF_to_LF(str);
		// verbatim(str_verb, str);
		// TODO: hide comments then recover at the end
		// find_comments(intv, str, "%");
		Long N;
		try { N = check_normal_text_punc(str, false, true); }
		catch (...) { continue; }

		// verb_recover(str, str_verb);
		if (N > 0)
			write(str, fname);
		cout << endl;
	}
}

int main(int argc, const char *argv[]) {
	using namespace slisc;

	// write command to log
	try {
		scan_log_limit();
		sb = "\n参数 ";
		for (int i = 1; i < argc; ++i)
			sb << " [" << argv[i] << ']';
		scan_log(sb, true);

		Timer timer;
		vecStr args;
		get_args(args, argc, argv);
		timer.tic();

		get_path(gv::path_in, gv::path_out, gv::path_data, gv::url, args);
		if (find(gv::path_in, "/PhysWiki/") >= 0)
			gv::is_wiki = true;
		else
			gv::is_wiki = false;

		ensure_dir(gv::path_out +"code/matlab/");

		// === parse arguments ===
		clear(sb) << gv::path_data << "scan.db";
		if (!file_exist(sb)) throw internal_err(u8"数据库文件不存在：" + sb);
		SQLite::Database db_rw(sb, SQLite::OPEN_READWRITE);
		db_rw.exec("PRAGMA busy_timeout = 3000;");
		
		unique_ptr<SQLite::Database> db_read_wiki;
		if (!gv::is_wiki)
			db_read_wiki = unique_ptr<SQLite::Database>(
				new SQLite::Database("data/scan.db", SQLite::OPEN_READONLY));

		if (args[0] == "." && args.size() == 1) {
			PhysWikiOnline(db_rw, db_read_wiki);
		}
		else if (args[0] == "--toc" && args.size() == 1) {
			SQLite::Transaction transaction(db_rw);
			arg_toc(db_rw);
			transaction.commit();
		}
		else if (args[0] == "--toc-all" && args.size() == 1) {
			// get entries from folder
			SQLite::Transaction transaction(db_rw);
			vecStr entries, titles;
			entries_titles(entries, titles, db_rw);
			arg_toc(db_rw);
			transaction.commit();
		}
		else if (args[0] == "--wc" && args.size() == 1)
			word_count(db_rw);
		else if (args[0] == "--inline-eq-space")
			// check format and auto correct .tex files
			add_space_around_inline_eq(gv::path_in);
		else if (args[0] == ",")
			// check format and auto correct .tex files
			replace_eng_punc_to_chinese(gv::path_in);
		else if (args[0] == "--autoref" && args.size() == 4) {
			// check a label, add one if necessary
			// args: [1]: entry, [2]: eq/fig/etc, [3]: disp_num
			Str label;
			Str fname = gv::path_data + "autoref.txt";
			file_remove(fname);
			SQLite::Transaction transaction(db_rw);
			Long ret = check_add_label(label, args[1], args[2], str2Llong(args[3]), db_rw);
			transaction.commit();
			vecStr output;
			if (ret == 0) { // added
				Str id = args[2] + args[3];
				output = {label, "added"};
			}
			else // ret == 1, already exist
				output = {label, "exist"};
			cout << output[0] << endl;
			cout << output[1] << endl;
			write_vec_str(output, fname);
		}
		else if (args[0] == "--autoref-dry" && args.size() == 4) {
			// check a label only, without adding
			Str label;
			Str fname = gv::path_data + "autoref.txt";
			file_remove(fname);
			SQLite::Transaction transaction(db_rw);
			Long ret = check_add_label(label, args[1], args[2], str2Llong(args[3]), db_rw, true);
			transaction.commit();
			vecStr output;
			if (ret == 0) // added
				output = {label, "added"};
			else // ret == 1, already exist
				output = {label, "exist"};
			cout << output[0] << endl;
			cout << output[1] << endl;
			write_vec_str(output, fname);
		}
		else if (args[0] == "--entry" && args.size() > 1) {
			// process specified entries
			vecStr entries;
			Str arg;
			for (Long i = 1; i < size(args); ++i) {
				arg = args[i];
				if (arg[0] == '-' && arg[1] == '-')
					break;
				entries.push_back(arg);
			}
			SQLite::Transaction transaction(db_rw);
			PhysWikiOnlineN(entries, false, db_rw, db_read_wiki);
			transaction.commit();
		}
		else if (args[0] == "--tree" && args.size() == 1) {
			dep_json(db_rw);
		}
		else if (args[0] == "--delete" && args.size() > 1) {
			vecStr entries;
			Str arg;
			for (Long i = 1; i < size(args); ++i) {
				arg = args[i];
				if (arg[0] == '-' && arg[1] == '-')
					break;
				entries.push_back(arg);
			}
			SQLite::Transaction transaction(db_rw);
			arg_delete(entries, db_rw);
			transaction.commit();
		}
		else if (args[0] == "--delete-cleanup" && args.size() == 1) {
			arg_delete_cleanup(db_rw);
		}
		else if (args[0] == "--delete-hard" && args.size() > 1) {
			vecStr entries;
			Str arg;
			for (Long i = 1; i < size(args); ++i) {
				arg = args[i];
				if (arg[0] == '-' && arg[1] == '-')
					break;
				entries.push_back(arg);
			}
			SQLite::Transaction transaction(db_rw);
			arg_delete_hard(entries, db_rw);
			transaction.commit();
		}
		else if (args[0] == "--delete-figure" && args.size() > 1) {
			vecStr figures;
			Str arg;
			for (Long i = 1; i < size(args); ++i) {
				arg = args[i];
				if (arg[0] == '-' && arg[1] == '-')
					break;
				figures.push_back(arg);
			}
			SQLite::Transaction transaction(db_rw);
			arg_delete_figs_hard(figures, db_rw);
			transaction.commit();
		}
		else if (args[0] == "--delete-image" && args.size() > 1) {
			vecStr images;
			Str arg;
			for (Long i = 1; i < size(args); ++i) {
				arg = args[i];
				if (arg[0] == '-' && arg[1] == '-')
					break;
				images.push_back(arg);
			}
			SQLite::Transaction transaction(db_rw);
			db_delete_images(images, db_rw);
			transaction.commit();
		}
		else if (args[0] == "--bib") {
			SQLite::Transaction transaction(db_rw);
			arg_bib(db_rw);
			transaction.commit();
		}
		else if (args[0] == "--history-all" && args.size() <= 2) {
			db_rw.exec("BEGIN EXCLUSIVE");
			arg_history(db_rw);
			db_rw.exec("COMMIT");

			db_rw.exec("BEGIN EXCLUSIVE");
			db_update_history_last(db_rw);
			db_rw.exec("COMMIT");

			db_rw.exec("BEGIN EXCLUSIVE");
			if (args.size() == 2)
				history_add_del_all(db_rw, true);
			else
				history_add_del_all(db_rw, false);
			db_rw.exec("COMMIT");
		}
		else if (args[0] == "--history-normalize" && args.size() == 1) {
			SQLite::Transaction transaction(db_rw);
			history_normalize(db_rw);
			transaction.commit();
		}
		else if (args[0] == "--backup" && args.size() == 3) {
			SQLite::Transaction transaction(db_rw);
			arg_backup(args[1], str2Int(args[2]), db_rw);
			transaction.commit();
		}
		else if (args[0] == "--stat" && args.size() == 3) {
			SLS_ERR("not finished!"); // arg_stat(args[1], args[2]);
		}
		else if (args[0] == "--author-char-stat" && args.size() == 4) {
			author_char_stat(args[1], args[2], args[3], db_rw);
		}
		else if (args[0] == "--fix-db" && size(args) == 1) {
			arg_fix_db(db_rw);
		}
		else if (args[0] == "--fix-foreign-keys" && size(args) == 1) {
			SQLite::Transaction transaction(db_rw);
			fix_foreign_keys(db_rw);
			transaction.commit();
		}
		else if (args[0] == "--migrate-db" && size(args) == 3) {
			// copy old database to a new database with different schema
			migrate_db(args[2], args[1]);
		}
		else if (args[0] == "--migrate-user-db" && size(args) == 1) {
			migrate_user_db();
		}
		else if (args[0] == "--all-users" && size(args) == 1) {
			all_users("changed/");
		}
		else if (args[0] == "--all-users-online" && size(args) == 1) {
			all_users("online/");
		}
		else if (args[0] == "--hide" && args.size() > 1) {
			Str str, fname = gv::path_in + "contents/" + args[1] + ".tex";
			read(str, fname);
			CRLF_to_LF(str);
			hide_eq_verb(str);
			write(str, fname);
		}
		else if (args[0] == "--unhide" && args.size() > 1) {
			Str str, fname = gv::path_in + "contents/" + args[1] + ".tex";
			read(str, fname);
			CRLF_to_LF(str);
			unhide_eq_verb(str);
			write(str, fname);
		}
		else if (args[0] == "--all-commands") {
			vecStr commands;
			all_commands(commands, gv::path_in + "contents/");
			write_vec_str(commands, gv::path_data + "commands.txt");
		}
		else if (args[0] == "--check-url") {
			vecStr entries;
			if (args.size() == 1)
				check_url(entries);
			for (Long i = 1; i < size(args); ++i)
				entries.push_back(args[i]);
			check_url(entries);
		}
		else if (args[0] == "--check-url-from" && args.size() == 2) {
			vecStr entries;
			check_url(entries, args[1]);
		}
		else if (args[0] == "--version" && args.size() == 1) {
			cout << VERSION_MAJOR << '.' << VERSION_MINOR << '.' << VERSION_PATCH << endl;
			return 0;
		}
		else if (args[0] == "--batch-mod" && args.size() == 1) {
			arg_batch_mod();
		}
		else {
			throw scan_err(u8"scan 内部错误： 命令不合法");
		}
		stringstream time_str;
		time_str << std::fixed << std::setprecision(3) << timer.toc();
		clear(sb) << "all done! time (s): " << time_str.str();
		cout << sb << endl;
		scan_log(sb);
		if (argc <= 1) {
			cout << u8"按任意键退出..." << endl;
			char c = (char)getchar();
			++c;
		}
	}
	catch (const std::exception &e) {
		if (e.what() == Str("database is locked")) {
			scan_cerr(u8"scan 错误：数据库被占用，请稍后重试。如果该错误持超过 5 分钟，请联系管理员。");
		}
		else {
			clear(sb) << u8"scan 错误：" << e.what();
			scan_cerr(sb);
		}
		exit(1);
	}
	catch (...) {
		scan_cerr(u8"scan 内部错误： 不要 throw 除了 scan_err 和 internal_err 之外的东西！");
		exit(1);
	}

	return 0;
}
