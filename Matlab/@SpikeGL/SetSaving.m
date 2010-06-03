%    myobj = SetSaving(myobj, bool_flag)
%
%                Toggle saving on/off for the acquisition.  Note that this
%                command has no effect if the acquisition is not running.
%                (Note: Toggling saving off then back on will overwrite the old
%                data file unless a call to SetSaveFile.m is made in between.)
function [s] = SetSaving(s,b)

    if (~IsAcquiring(s)),
        warning('The acquisition was not running, SetSaving command ignored.');
        return;
    end;
    
    if (~isnumeric(b)),
        error('Second argument should be a boolean (numeric) flag');
    end;
    DoSimpleCmd(s, sprintf('SETSAVING %d',b));
