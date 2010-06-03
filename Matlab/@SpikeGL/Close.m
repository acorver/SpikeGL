%    myobj = Close(myobj)
%
%                Closes the network connection to the SpikeGL process.
%                Useful to cleanup resources when you are done with a 
%                connection to SpikeGL.
function [s] = Close(s)
    CalinsNetMex('disconnect', s.handle);
%    CalinsNetMex('destroy', s.handle);
    s.handle = -1;
