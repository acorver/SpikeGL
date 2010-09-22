%    daqData = GetDAQData(myObj, start_scan, scan_ct, channel_subset, downsample_factor)
%
%                Obtain a MxN matrix of int16s where M corresponds to
%                'scan_ct' number of scans requested (or fewer if fewer
%                were available to read, starting from 'start_scan'), N is
%                the number of channels currently being acquired and
%                optionally 'channel_subset' is the desired channel subset.
%                If channel_subset is not specified, the current channel
%                subset is used.
%                The downsample_factor is used to downsample the data by an
%                integer factor.  Default is 1 (no downsampling).
%                Note: make sure the Matlab data API facility is enabled in
%                options for this function to operate correctly.
%
function [ret] = GetDAQData(s, start_scan, scan_ct, varargin)

    ChkConn(s);
    
    if (~isnumeric(start_scan) | ~size(start_scan,1)),
        error('Invalid scan_start parameter received');
    end;
    if (~isnumeric(scan_ct) | ~size(scan_ct,1)),
        error('Invalid scan_ct parameter received');
    end;
    
    channel_subset = '';
    if (nargin >= 4),
        channel_subset = sprintf('%d#', varargin{1});
    end;
    downsample = 1;
    if (nargin >= 5),
        downsample = varargin{2};
        if (~isnumeric(downsample) | length(downsample) > 1),
            error('Downsample factor must be a single numeric value');
        end;
    end;
    CalinsNetMex('sendString', s.handle, sprintf('GETDAQDATA %d %d %s %d\n', start_scan, scan_ct, channel_subset, downsample));
    line = CalinsNetMex('readLine', s.handle);

    if (strfind(line, 'ERROR') == 1),
        error(line);
        return;
    end;
    
    [mat_dims] = sscanf(line, 'BINARY DATA %d %d', [1,2]);
    if (~isnumeric(mat_dims) | ~size(mat_dims,2)),
        warning('Invalid matrix dimensions');
        return;
    end;
    
    
    ret = CalinsNetMex('readMatrix', s.handle, 'int16', [mat_dims(2) mat_dims(1)]);
    ret = ret'; % need to flip the incoming matrix as per API spec..
    line = CalinsNetMex('readLine', s.handle);
    if (~(strfind(line, 'OK') == 1)),
        error('Did not get OK reply from GetDAQData');
    end;
    
    