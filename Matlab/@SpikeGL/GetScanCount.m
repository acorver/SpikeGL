%    scanCount = GetScanCount(myobj)
%
%                Obtains the current scan count.  This is useful for
%                calling GetDAQData, as the parameter that GetDAQData
%                expects is a scan count.  If the Matlab data API facility
%                is not enabled, then an error is returned;
function [ret] = GetScanCount(s)
    
    ret = str2num(DoQueryCmd(s, 'GETSCANCOUNT'));
    