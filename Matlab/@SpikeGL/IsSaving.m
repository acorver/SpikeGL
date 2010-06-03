%    boolval = IsSaving(myobj)
%
%                Determine if the software is currently saving data.  This
%                can only possibly be true if it is also running an
%                acquisition.
function [ret] = IsSaving(s)

    ret = sscanf(DoQueryCmd(s, 'ISSAVING'), '%d');
