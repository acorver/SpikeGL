%    version = GetVersion(myobj)
%
%                Obtain the version string associated with the
%                SpikeGL process we are connected to.
function [ret] = GetVersion(s)

    ret = DoQueryCmd(s, 'GETVERSION');
