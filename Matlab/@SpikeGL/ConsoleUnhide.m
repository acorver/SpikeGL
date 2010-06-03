%    myobj = ConsoleUnhide(myobj)
%
%                Unhides the SpikeGL console window, causing it to be shown 
%                again.  See also ConsoleHide and IsConsoleHidden.
function [s] = ConsoleUnhide(s)

    s = DoSimpleCmd(s, 'CONSOLEUNHIDE');
