%    myobj = StartACQ(myobj) 
%    myobj = StartACQ(myobj, params)
%    myobj = StartACQ(myobj, outfile)
%
%                Start an acquisition.  The optional second argument,
%                params, is a struct of acquisition parameters as returned
%                from GetParams.m.  Alternatively, the second argument can
%                be a string in which case it will denote the filename to
%                save data to, and the acquisition will implicitly use the
%                last set of parameters it has seen.  If there is no
%                second argument to this function, then the last set of
%                parameters that the application has seen are used.  If the 
%                acquisition was already running, StartACQ will *not*
%                restart it and will instead throw an error. Also, if the
%                parameters are invalid, an error is thrown as well.
function [s] = StartACQ(varargin)
    s = varargin{1};
    fname=[];
    params=[];
    if (nargin > 1), 
        arg2 = varargin{2};
        if (ischar(arg2)),
            fname = arg2;
        elseif (isstruct(arg2)),
            params = arg2;
        else
            error('Invalid argument type for second argument.  Must be a string or struct.');
            return;
        end;
    end;

    if (IsAcquiring(s)),
        error('Acquisition is already running!');
        return;
    end;
    if (isempty(params)),
        params = GetParams(s);
    end;
    if (~isempty(fname)),
        params.lastOutFile = fname;
    end;
    SetParams(s, params); % NB: always send params as that does some important setup!
    d = DoSimpleCmd(s, sprintf('STARTACQ\n'));
    
