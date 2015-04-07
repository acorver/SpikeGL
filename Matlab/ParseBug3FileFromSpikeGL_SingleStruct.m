function [ ret ] = ParseBug3FileFromSpikeGL_SingleStruct( filename )
%PARSEBUG3FILEFROMSPIKEGL_SingleStruct 
%
% Pass a .bug3 file to parse, returns a struct containing fields which are
% arrays, elements of which are the per-block or per-frame bug3 meta-data.
%
% Here is an example of what may be returned:
%
%                        blockNum: [93x1 int32]
%                 framesThisBlock: [93x1 double]
%      spikeGL_DataFile_ScanCount: [93x1 double]
%    spikeGL_DataFile_SampleCount: [93x1 double]
%            spikeGL_ScansInBlock: [93x1 double]
%               boardFrameCounter: [3720x1 double]
%                 boardFrameTimer: [3720x1 double]
%                chipFrameCounter: [3720x1 double]
%                          chipID: [3720x1 double]
%          frameMarkerCorrelation: [3720x1 double]
%               missingFrameCount: [93x1 double]
%                 falseFrameCount: [93x1 double]
%                             BER: [93x1 double]
%                             WER: [93x1 double]
%                       avgVunreg: [93x1 double]
%
% The above example has 93 total blocks, with about 40 frames per block.
% Some of the fields are *per-block* fields, whereas others are
% *per-frame*.
%
% In the above example all fields with '93' as their length are
% *per-block*.
%
% All fields with '3270' as their length are *per-frame* 
%
% (To state it another way: a block may contain multiple frames. To find
% out how many frames are in a particular block, look at the corresponding
% element for that block in the framesThisBlock field).
%
fid = fopen(filename);
if (fid == -1),
    error('File not found');
end;

ret = struct();

tline = fgetl(fid);

while ischar(tline),
    a = textscan(tline, '[ block %d ]', 1);
    if (~isempty(a{1}) & isnumeric(a{1})), 
        %disp(sprintf('Found block %d',a{1}));
        f = [];
        if (isfield(ret,'blockNum')), f = ret.blockNum; end;
        ret = setfield(ret, 'blockNum', [f; a{1}]);
    else
        a = textscan(tline, '%s = %s', 2);
        n=cell2mat(a{1});
        v=cell2mat(a{2});
        if (length(n) && length(v) && ischar(n) && ischar(v)),
            %disp(sprintf('%s ... = ... %s',n,v));
            v = sscanf(v,'%f,');
            f = [];
            if (isfield(ret, n)),
                f = getfield(ret, n);
            end;
            ret = setfield(ret, n, [f; v]);
        else
            % disp(sprintf('Invalid: %s', tline));
        end;
    end;
    tline = fgetl(fid);
end;

end

