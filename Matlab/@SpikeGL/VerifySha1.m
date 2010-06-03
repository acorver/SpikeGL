%    res = VerifySha1(myobj, filename)
%
%                Verifies the SHA1 sum of the file specified by filename.
%                If filename is relative, it is interpreted as being
%                relative to the save dir.  Absolute filenames (starting
%                with a '/') are supported as well.  Since this is a long
%                operation, this functions uses the `disp' command to print
%                progress information to the matlab console.  On
%                completion, the sha1 sum either verifies ok or it doesn't.
%                The `res' return value is 1 if the verification passed,
%                zero otherwise.
function [res] = VerifySha1(s, file)

    ChkConn(s);
    CalinsNetMex('sendString', s.handle, sprintf('VERIFYSHA1 %s\n', file));
    line = [];
    res = cell(0,1);
    i = 0;
    res = 0;
    while ( 1 ),        
        line = CalinsNetMex('readLine', s.handle);
        if (i == 0 & strfind(line, 'ERROR') == 1),
            disp(sprintf('Verify not ok: %s', line(7:length(line))));
            return;
        end;
        if (isempty(line)), continue; end;
        if (strcmp(line,'OK')), 
            res = 1;
            break; 
        end;
        disp(sprintf('Verify progress: %s%%',line));
    end;

