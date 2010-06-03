%    res = Par2(myobj, op, filename)
%
%                Create, Verify, or Repair Par2 redundancy files for
%                `filename'.  Arguments are:
%
%                op, a string that is either 'c', 'v', or 'r' for create,
%                verify or repair respectively.  
%
%                filename, the .par2 or .bin file to operate on (depending
%                on whether op is verify/repair or create).
%
%                Outputs the progress of the par2 command proccess as it
%                works.
function [res] = Par2(s, op, file)
    res = 0;
 
    ChkConn(s);
    
    if (IsAcquiring(s)),
        error('Due to performance considerations, cannot run this command while the acquisition is in progress.  Try again when the acquisition is not running.');
        return;
    end;
    
    if (~strcmp(op, 'v') & ~strcmp(op, 'r') & ~strcmp(op, 'c')),
        error('Op must be one of ''v'', ''r'' or ''c''!');
        return;
    end;
    CalinsNetMex('sendString', s.handle, sprintf('PAR2 %s %s\n', op, file));
    line = [];
    i = 0;
    while ( 1 ),        
        line = CalinsNetMex('readLine', s.handle);
        if (i == 0 & strfind(line, 'ERROR') == 1),
            disp(sprintf('Par2 Error: %s', line(7:length(line))));
            return;
        end;
        if (isempty(line)), continue; end;
        if (strcmp(line,'OK')), 
            res = 1;
            break; 
        end;
        disp(sprintf('%s',line));
    end;
    
