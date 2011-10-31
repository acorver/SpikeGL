function ret = GetParams(s)
% s a SpikeGL object

%    params = GetParams(myobj)
%
%                Retrieve the acquisition parameters (if any) used for 
%                the last acquisition that successfully ran (may be empty 
%                if no acquisition ever ran). Acquisition parameters are 
%                a struct of name/value pairs.   

% this version fixes a bug wherein ret.outputFile was wrong if there
% were spaces in the the file name
   
    ret = struct();
    res = DoGetResultsCmd(s, 'GETPARAMS');
      % res a cell array, each cell containing a string of the form
      % '<parameter name> = <parameter value>'
      % parameter names are sequences of word characters [a-z_A-Z0-9]
      % some examples of parameter values:
      %   3
      %   0.5
      %   -1
      %   false
      %   0:1
      %   0=1,1=2
      %   Dev1
      %   C:/Documents and Settings/labadmin/My Documents/data.bin
    for i=1:length(res)
        %[toks] = regexp(res{i}, '(\w+)\s*=\s*(\S+)', 'tokens');
        pair = ...
            regexp(res{i},...
                   '^\s*(?<name>\w+)\s*=\s*(?<value>.*)\s*$', 'names');
        % pair is a struct array with at most one element.
        % If there's an element, then pair.name is the param name, a
        % string, and pair.value is the value, a string.
        if ~isempty(pair)
            % If the value has the format of a double, convert it to
            % a double.
            i_match=...
              regexp(pair.value,...
                     '^\s*[-+]?[0-9]*\.?[0-9]+([eE][-+]?[0-9]+)?\s*$',...
                     'once');
            % i_match==1 if the string represents a float (possibly with
            % leading and/or trailing whitespace), and isempty(i_match)
            % otherwise.
            if isempty(i_match)
              % store the value as a string
              ret.(pair.name) = pair.value;
            else
              % convert to double
              ret.(pair.name) = str2double(pair.value); 
            end
              
        end
    end
end  % function

