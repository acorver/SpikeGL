function [ ret ] = ParseBug3FileFromSpikeGL( filename )
%PARSEBUG3FILEFROMSPIKEGL Pass a .bug3 file to parse, returns an array
% of structs which is the per-block data
fid = fopen(filename);
if (fid == -1),
    error('File not found');
end;

ret = [];
curr = struct();

tline = fgetl(fid);

while ischar(tline),
    a = textscan(tline, '[ block %d ]', 1);
    if (~isempty(a{1}) & isnumeric(a{1})), 
        %disp(sprintf('Found block %d',a{1}));
        if (length(fieldnames(curr)) > 0), ret = [ ret; curr]; end;
        curr = struct();
        curr.blockNum = a{1};
    else
        a = textscan(tline, '%s = %s', 2);
        n=cell2mat(a{1});
        v=cell2mat(a{2});
        if (length(n) && length(v) && ischar(n) && ischar(v)),
            %disp(sprintf('%s ... = ... %s',n,v));
            v = sscanf(v,'%f,');
            curr = setfield(curr, n, v);
        else
            % disp(sprintf('Invalid: %s', tline));
        end;
    end;
    tline = fgetl(fid);
end;

end

