%    params = GetParams(myobj)
%
%                Retrieve the acquisition parameters (if any) used for 
%                the last acquisition that successfully ran (may be empty 
%                if no acquisition ever ran). Acquisition parameters are 
%                a struct of name/value pairs.   
function [ret] = GetParams(s)

   
    ret = struct();
    res = DoGetResultsCmd(s, 'GETPARAMS');
    for i=1:length(res),
        [toks] = regexp(res{i}, '(\w+)\s*=\s*(\S+)', 'tokens');
        if (size(toks,1)),
            a=toks{1};
            % optionally convert to numeric
            [matches] = regexp(a{2}, '^([0-9.e-]+)$', 'match');
            if (~isempty(matches)),
                scn = sscanf(matches{1}, '%g');
                if (~isempty(scn)), a{2} = scn; end;
            end; 
%            ret = setfield(ret, a{1}, a{2});
%            Use dynamic fieldnames instead
             ret.(a{1}) = a{2};
        end;
    end;
    
    