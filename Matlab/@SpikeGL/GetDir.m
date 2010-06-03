%    params = GetDir(myobj)
%
%                Retrieve the listing of files in the 'save directory'.
%                Filename obtained from this list can later be passed to
%                VerifySha1 and other functions expecting a filename.
%                All files listed are relative to the save directory.
%                See also: GetSaveDir, SetSaveDir.
function [ret] = GetDir(s)

    ret = DoGetResultsCmd(s, 'GETDIR');
   
    
    