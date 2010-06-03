%    boolval = IsAcquiring(myobj)
%
%                Determine if the software is currently running an 
%                acquisition. 
function [ret] = IsAcquiring(s)

    ret = sscanf(DoQueryCmd(s, 'ISACQ'), '%d');
