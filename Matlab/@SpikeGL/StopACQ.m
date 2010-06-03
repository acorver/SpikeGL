%    myobj = StopACQ(myobj)
%   
%                Unconditionally stop the currently running acquisition (if
%                any), closing all data files immediately and returning the
%                app to the idle state. See IsAcquiring.m to determine if
%                an acquisition is running. 
function [s] = Stop(varargin)
    s = varargin{1};
    d = DoSimpleCmd(s, 'STOPACQ');
