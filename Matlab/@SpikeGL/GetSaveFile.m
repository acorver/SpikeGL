%    filename = GetSaveFile(myobj)
%
%                Retrieve the save file name for the next time a save file is
%                opened.
function [filename] = GetSaveFile(s)
    p = GetParams(s);
    filename = p.outputFile;
    
    