%    boolval = IsInitialized(myobj)
%
%                Determine if the application has finished its
%                initialization.  Initialization can take from several
%                seconds up to a minute, depending on the system hardware.
%                A new acquisition cannot be started until this function
%                returns 1.
function ret = IsInitialized(s)
    ret = sscanf(DoQueryCmd(s, 'ISINITIALIZED'), '%d');
