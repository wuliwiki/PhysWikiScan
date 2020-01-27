% process PhysWiki/backup/xxx.tex
[fnames,path] = uigetfile('*.tex', 'multiselect', 'on');
cd(path);
Nf = numel(fnames);
entry = cell(1, Nf);
time = uint64(zeros(1, Nf));
author = cell(1, Nf);
xiaoshi = char([23567   26102]); % '??'
for i = 1 : Nf
    ind = strfind(fnames{i}, '_20');
    entry{i} = fnames{i}(1:ind-1);
    time(i) = int64(str2double(fnames{i}(ind+1:ind+12)));
    author{i} = fnames{i}(ind+14:end-4);
    if strcmp(author{i}, xiaoshi) || strcmp(author{i}, 'addiszx')
        author{i} = 'admin';
        dest = [entry{i} '_' num2str(time(i)) '_' author{i} '.tex'];
        disp([fnames{i} '   --->   ' dest]);
        movefile(fnames{i}, dest);
        fnames{i} = dest;
    end
    copyfile(fnames{i}, [num2str(time(i)) '_' author{i} '_' entry{i} '.tex']);
end
