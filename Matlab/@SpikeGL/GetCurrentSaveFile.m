%    filename = GetCurrentSaveFile(myobj)
%
%                Retrieve the save file name for the currently running
%                acquisiton.  Returns an empty string if there is no
%                acquisition running.
function [filename] = GetCurrentSaveFile(s)
    dosrvrcmd = 1;
    filename = '';
    
    if ( ispc && ( strcmp(s.host,'localhost') || strcmp(s.host, '127.0.0.1') )),
        dosrvrcmd = 0;
        filename = CalinsNetMex('getSpikeGLFileNameFromShm', s.handle);
        if (strcmp('NOT ATTACHED TO SHM!', filename)),
            dosrvrcmd = 1;
        end;
    end;
    
    
    if (dosrvrcmd), 
        
        filename = DoGetResultsCmd(s, 'GETCURRENTSAVEFILE');
        if (isempty(filename)), filename = ''; end;
        if (iscell(filename)),
            filename = filename(1,1);
        end;
    

    end;
    