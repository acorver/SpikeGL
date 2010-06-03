%    myobj = ConsoleHide(myobj)
%
%                Hides the SpikeGL console window.  This may be useful in 
%                order to unclutter the desktop.
function [s] = ConsoleHide(s)

    s = DoSimpleCmd(s, 'CONSOLEHIDE');
