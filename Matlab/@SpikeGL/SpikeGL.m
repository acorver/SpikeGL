%    myobj = SpikeGL()
%    myobj = SpikeGL(host)
%    myobj = SpikeGL(host, port)
%
%                Constructor.  Constructs a new instance of a @SpikeGL 
%                object and immediately attempts to connect to the 
%                running process via the network. The default constructor
%                (no arguments) attempts to connect to 'localhost' port
%                4142.  Additional versions of this constructor support
%                specifying a host and port.
function [s] = SpikeGL(varargin) 
    host = 'localhost';
    port = 4142;
    if (nargin >= 1), host = varargin{1}; end;
    if (nargin >= 2), port = varargin{2}; end;
    if (~ischar(host) | ~isnumeric(port)),
        error('Host must be a string and port must be a number');
    end;
    %if (strcmp(host, 'localhost')), 
    %    OSFuncs('ensureProgramIsRunning', 'SpikeGL');
    %end;
    s=struct;
    s.host = host;
    s.port = port;
    s.in_chkconn = 0;
    s.handle = CalinsNetMex('create', host, port);
    s.ver = '';
    s = class(s, 'SpikeGL');
    CalinsNetMex('connect', s.handle);
    ChkConn(s);
    s.ver = DoQueryCmd(s, 'GETVERSION');
    