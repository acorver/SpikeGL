%    daqData = GetLastNDAQData(myObj, NUM, channel_subset, downsample_ratio)
%
%                Obtain a M x N matrix of the most recent NUM samples.  The
%                N dimension of the returned matrix is the number of
%                channels currently being acquired and optionally
%                'channel_subset' is the desired channel subset. If
%                channel_subset is not specified, the current channel
%                subset is used.
%                The downsample_factor is used to downsample the data by an
%                integer factor.  Default is 1 (no downsampling).
%                Note: make sure the Matlab data API facility is enabled in
%                options for this function to operate correctly.
%
function [ret] = GetLastNDAQData(s, num, varargin)

    channel_subset = [];
    
    if (nargin >= 3),
        channel_subset = varargin{1};
    else
        channel_subset = GetChannelSubset(s);
    end;
    
    downsample = 1;
    if (nargin >= 4),
        downsample = varargin{2};
        if (~isnumeric(downsample) | length(downsample) > 1),
            error('Downsample factor must be a single numeric value');
        end;
    end;
    
    scanCt = GetScanCount(s);
    if (num > scanCt),
        num = scanCt;
    end;
    
    ret = GetDAQData(s, scanCt-num, num, channel_subset, downsample);
    
    