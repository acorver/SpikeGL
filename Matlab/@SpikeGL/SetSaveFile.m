%    myobj = SetSaveFile(myobj, 'filename')
%
%                Set the save file name for the next time a save file is
%                opened (either by calling SetSaving.m or by starting a new
%                acquisition).
function [s] = SetSaveFile(s, filename)
    if (~ischar(filename)),
        error('Argument to SetSaveFile must be a string');
        return;
    end;
    ChkConn(s);
    DoSimpleCmd(s, sprintf('SETSAVEFILE %s',filename));
    
    