%    myobj = SetParams(myobj, params_struct)
%
%                The inverse of GetParams (see GetParams.m).  Set the
%                acquisition parameters for the next acquisition to be run.
%                (Normally you don't call this, but call StartACQ instead,
%                passing it the parameters and StartACQ.m  in turn calls
%                this). Acquisition parameters are a struct of name/value
%                pairs that are used to configure the acquisition.  If an
%                acquisition is currently running, this call will fail with
%                an error.
function [s] = SetParams(s, params)
    if (~isstruct(params)),
        error('Argument to SetParams must be a struct');
    end;
    ChkConn(s);
    running = IsAcquiring(s);
    if (running),
        error('Cannot set params while an acquisition is running!  Stop() it first!');
    end;
    CalinsNetMex('sendString', s.handle, sprintf('SETPARAMS\n'));
    ReceiveREADY(s, 'SETPARAMS');
    names = fieldnames(params);
    for i=1:length(names),
        f = params.(names{i});
        if (isnumeric(f) & isscalar(f)),
            line = sprintf('%s = %g\n', names{i}, f);
        elseif (ischar(f)),
            line = sprintf('%s = %s\n', names{i}, f);
        else 
            error('Field %s must be numeric scalar or a string', names{i});
        end;
        CalinsNetMex('sendString', s.handle, line);
    end;
    % end with blank line
    CalinsNetMex('sendString', s.handle, sprintf('\n'));
    ReceiveOK(s, 'SETPARAMS');
    
    