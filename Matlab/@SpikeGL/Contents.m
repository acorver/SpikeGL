% SYNOPSIS
%
% The @SpikeGL class is a Matlab object with methods to access the
% 'SpikeGL' program via the network.  Typically, SpikeGL is run on the
% local machine, in which case all communication is via a network loopback
% socket.  However, true remote control of the SpikeGL program is 
% supported.
%
% This class provides nearly total control over a running SpikeGL
% process -- from starting and stopping an acquisition, to setting
% acquisition parameters to various functions such as the Par2 redundancy
% tool and the Sha1 verification tool.
%
% Instances of @SpikeGL are not very stateful.  Just about the only real
% state they maintain internally is a variable that is a handle to a
% network socket.  As such, it is ok to constantly create and destroy these
% objects.  In fact, the network connection is only kept open for a brief
% time (10 seconds max).
%
% The network socket handle is used with the 'CalinsNetMex' mexFunction, 
% which is a helper mexFunction that does all the actual socket 
% communications for this class (since Matlab lacks native network
% support).
%
% Users of this class merely need to construct an instance of a @SpikeGL
% object and all network communication with the SpikeGL process is handled
% automatically.
%
% EXAMPLES
%
% my_s = SpikeGL;     % connects to SpikeGL program running on
%                     % local machine
%
% stats = GetParams(my_s); % retrieves the acquisition params
%
%
% SetParams(my_s, struct('acqMode', 1, acqPdChan, 4 ... ));
%
% StartAcq(my_s);                  % starts the acquisition using last-set
%                                  % params
%
% StopAcq(my_s); % tell program to stop the acquisition
%
%
%
% FUNCTION REFERENCE
%
%     myobj = SpikeGL()
%     myobj = SpikeGL(host)
%     myobj = SpikeGL(host, port)
%
%                Constructor.  Constructs a new instance of a @SpikeGL 
%                object and immediately attempts to connect to the 
%                running process via the network. The default constructor
%                (no arguments) attempts to connect to 'localhost' port
%                4142.  Additional versions of this constructor support
%                specifying a host and port.
%
%    myobj = Close(myobj)
%
%                Closes the network connection to the SpikeGL process.
%                Useful to cleanup resources when you are done with a 
%                connection to SpikeGL.
%
%    myobj = ConsoleHide(myobj)
%
%                Hides the SpikeGL console window.  This may be useful in 
%                order to unclutter the desktop.
%
%    myobj = ConsoleUnhide(myobj)
%
%                Unhides the SpikeGL console window, causing it to be shown 
%                again.  See also ConsoleHide and IsConsoleHidden.
%
%    myobj = FastSettle(myobj)
%
%                If using the INTAN mux, this command sends a 'fast settle'
%                to the INTAN chip.  
%
%    params = GetDir(myobj)
%
%                Retrieve the listing of files in the 'save directory'.
%                Filename obtained from this list can later be passed to
%                VerifySha1 and other functions expecting a filename.
%                All files listed are relative to the save directory.
%                See also: GetSaveDir, SetSaveDir.
%
%    params = GetParams(myobj)
%
%                Retrieve the acquisition parameters (if any) used for 
%                the last acquisition that successfully ran (may be empty 
%                if no acquisition ever ran). Acquisition parameters are 
%                a struct of name/value pairs.   
%
%    dir = GetSaveDir(myobj)
%
%                Obtain the directory path to which data files will be
%                saved.  Data files are saved by plugins when they are
%                Stopped with the save_data_flag set to true.
%
%    filename = GetSaveFile(myobj)
%
%                Retrieve the save file name for the next time a save file is
%                opened.  Don't use this function.  Instead, use the
%                GetCurrentSaveFile.m function which is much faster
%                (<10ms response time)!
%
%    filename = GetCurrentSaveFile(myobj)
%
%                Retrieve the save file name for the currently running
%                acquisiton.  Returns an empty string if there is no
%                acquisition running.
%
%    time = GetTime(myobj)
%
%                Returns the number of seconds since SpikeGL was
%                started.  The returned value has a resolution in the
%                nanoseconds range, since it comes from the CPU's timestamp
%                counter.
%
%    version = GetVersion(myobj)
%
%                Obtain the version string associated with the
%                SpikeGL process we are connected to.
%
%    boolval = IsAcquiring(myobj)
%
%                Determine if the software is currently running an 
%                acquisition. 
%
%    boolval = IsConsoleHidden(myobj)
%
%                Determine if the console window is currently hidden or 
%                visible.  Returns true if the console window is hidden,
%                or false otherwise.  The console window may be
%                hidden/shown using the ConsoleHide() and ConsoleUnhide()
%                calls.
%
%    boolval = IsInitialized(myobj)
%
%                Determine if the application has finished its
%                initialization.  Initialization can take from several
%                seconds up to a minute, depending on the system hardware.
%                A new acquisition cannot be started until this function
%                returns 1.
%
%    boolval = IsSaving(myobj)
%
%                Determine if the software is currently saving data.  This
%                can only possibly be true if it is also running an
%                acquisition.
%
%    res = Par2(myobj, op, filename)
%
%                Create, Verify, or Repair Par2 redundancy files for
%                `filename'.  Arguments are:
%
%                op, a string that is either 'c', 'v', or 'r' for create,
%                verify or repair respectively.  
%
%                filename, the .par2 or .bin file to operate on (depending
%                on whether op is verify/repair or create).
%
%                Outputs the progress of the par2 command proccess as it
%                works.
%
%    myobj = SetParams(myobj, params_struct)
%
%                The inverse of GetParams (see GetParams.m).  Set the
%                acquisition parameters for the next acquisition to be run.
%                (Normally you don't call this, but call StartACQ instead,
%                passing it the parameters and StartACQ.m  in turn calls
%                this). Acquisition parameters are a struct of name/value
%                pairs that are used to configure the acquisition.  If an
%                acquisition is currently running, this call will fail with
%                an error.
%
%    myobj = SetSaveDir(myobj, dir)
%
%                Specify directory path to which data files will be
%                saved.  Data files are saved by plugins when they are
%                Stopped with the save_data_flag set to true.
%
%    myobj = SetSaveFile(myobj, 'filename')
%
%                Set the save file name for the next time a save file is
%                opened (either by calling SetSaving.m or by starting a new
%                acquisition).
%
%    myobj = SetSaving(myobj, bool_flag)
%
%                Toggle saving on/off for the acquisition.  Note that this
%                command has no effect if the acquisition is not running.
%                (Note: Toggling saving off then back on will overwrite the old
%                data file unless a call to SetSaveFile.m is made in between.)
%
%    myobj = StartACQ(myobj) 
%    myobj = StartACQ(myobj, params)
%    myobj = StartACQ(myobj, outfile)
%
%                Start an acquisition.  The optional second argument,
%                params, is a struct of acquisition parameters as returned
%                from GetParams.m.  Alternatively, the second argument can
%                be a string in which case it will denote the filename to
%                save data to, and the acquisition will implicitly use the
%                last set of parameters it has seen.  If there is no
%                second argument to this function, then the last set of
%                parameters that the application has seen are used.  If the 
%                acquisition was already running, StartACQ will *not*
%                restart it and will instead throw an error. Also, if the
%                parameters are invalid, an error is thrown as well.
%
%    myobj = StopACQ(myobj)
%   
%                Unconditionally stop the currently running acquisition (if
%                any), closing all data files immediately and returning the
%                app to the idle state. See IsAcquiring.m to determine if
%                an acquisition is running. 
%
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
%
%    daqData = GetDAQData(myObj, start_scan, scan_ct, channel_subset, downsample_factor)
%
%                Obtain a MxN matrix of int16s where M corresponds to
%                'scan_ct' number of scans requested (or fewer if fewer
%                were available to read, starting from 'start_scan'), N is
%                the number of channels currently being acquired and
%                optionally 'channel_subset' is the desired channel subset.
%                If channel_subset is not specified, the current channel
%                subset is used.
%                The downsample_factor is used to downsample the data by an
%                integer factor.  Default is 1 (no downsampling).
%                Note: make sure the Matlab data API facility is enabled in
%                options for this function to operate correctly.
%
%    daqData = GetLastNDAQData(myObj, NUM, channel_subset, downsample_ratio)
%
%                Obtain a M x N matrix of the most recent NUM samples.  The
%                N dimension of the returned matrix is the number of
%                channels currently being acquired and optionally
%                'channel_subset' is the desired channel subset. If
%                channel_subset is not specified, the current channel
%                subset is used.
%                The downsample_factor is used to downsample the data by an
%                integer factor.  Default is 1 (no downsampling).
%                Note: make sure the Matlab data API facility is enabled in
%                options for this function to operate correctly.
%
%    channelSubset = GetChannelSubset(myobj)
%
%                Returns a vector of channel id's representing the channel
%                subset.  That is, the channels that are currently set to
%                'save'.  This vector may sometimes be empty if no
%                acquisition has ever run.
%
%    scanCount = GetScanCount(myobj)
%
%                Obtains the current scan count.  This is useful for
%                calling GetDAQData, as the parameter that GetDAQData
%                expects is a scan count.  If the Matlab data API facility
%                is not enabled, then 0 is always returned.
