%    filename = GetSaveFile(myobj)
%
%                Retrieve the save file name for the next time a save file is
%                opened.  Don't use this function.  Instead, use the
%                GetCurrentSaveFile.m function which is much faster
%                (<10ms response time)!
function [filename] = GetSaveFile(s)
    p = GetParams(s);
    filename = p.outputFile;
    
    