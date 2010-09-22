%    channelSubset = GetChannelSubset(myobj)
%
%                Returns a vector of channel id's representing the channel
%                subset.  That is, the channels that are currently set to
%                'save'.  This vector may sometimes be empty if no
%                acquisition has ever run.
function [ret] = GetChannelSubset(s)

    ret = str2num(DoQueryCmd(s, 'GETCHANNELSUBSET'));
    