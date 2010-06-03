%    myobj = FastSettle(myobj)
%
%                If using the INTAN mux, this command sends a 'fast settle'
%                to the INTAN chip.  
function [s] = FastSettle(s)

    if (~IsAcquiring(s)),
        warning('The acquisition was not running, FastSettle command ignored.');
        return;
    end;
    
    DoSimpleCmd(s, 'FASTSETTLE');
