/*
 * Intan Insect Telemetry Receiver GUI for use with Intan 'Bug3' Chips
 * Copyright (c) 2011 Intan Technologies, LLC  http://www.intantech.com
 * 
 * This software is provided 'as-is', without any express or implied 
 * warranty.  In no event will the authors be held liable for any damages 
 * arising from the use of this software. 
 * 
 * Permission is granted to anyone to use this software for any applications that use
 * Intan Technologies integrated circuits, and to alter it and redistribute it freely,
 * subject to the following restrictions: 
 * 
 * 1. The application must require the use of Intan Technologies integrated circuits.
 *
 * 2. The origin of this software must not be misrepresented; you must not 
 *    claim that you wrote the original software. If you use this software 
 *    in a product, an acknowledgment in the product documentation is required.
 * 
 * 3. Altered source versions must be plainly marked as such, and must not be 
 *    misrepresented as being the original software.
 * 
 * 4. This notice may not be removed or altered from any source distribution.
 * 
 */

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading;
using System.Windows.Forms;
using System.Diagnostics;
using System.IO;
using System.Media;
using OpalKelly.FrontPanel;

namespace Bug3
{
    public enum DataRate { Low = 0, Medium = 1, High = 2 };

    public static class Constant
    {
        public const int FramesPerBlock = 40;       // number of data frames to read over USB at a time

        public const int TotalNeuralChannels = 10;
        public const int TotalEMGChannels = 4;
        public const int TotalAuxChannels = 2;
        public const int TotalTTLChannels = 11;
        public const int NeuralSamplesPerFrame = 16;

        public const int MaxFrameCorrelation = 144;

        public const double FramePeriod = 0.0006144;    // frame time length, in seconds
        public const double NeuralSampleRate = (double)NeuralSamplesPerFrame / FramePeriod;
        public const double EMGSampleRate = 1.0 / FramePeriod;
        public const double FPGAClockFreq = 50000000.0; // FPGA 'clk2' clock frequency, in Hz

        public const int MaxFrameSize = 198;        // maximum data frame size, in 16-bit words
        public const int BytesPerWord = 2;
        public const int BitsPerWord = 16;

        // USB Data constants
        public const double ADCStep = 1.02 * 1.225 / 1024.0;    // units = V
        public const double ADCStepNeural = 2.5;                // units = uV
        public const double ADCStepEMG = 0.025;                 // units = mV
        public const double ADCStepAux = 0.0052;                // units = V
        public const double ADCOffset = 1023.0;

        // Spike Scope constants
        public const int MaxDepth = 30;        // maximum number of spikes to save for each channel
        public const int SnippetLength = 52;   // spike snapshot length (number of samples)
        public const int TriggerDelay = 13;    // numbers of samples before spike threshold crossing to save

        public static int FrameSize(DataRate dataRate, bool rawDataMode)
        {
            if (rawDataMode)
            {
                if (dataRate == DataRate.High)
                    return 198;
                else if (dataRate == DataRate.Medium)
                    return 102;
                else if (dataRate == DataRate.Low)
                    return 54;
                else return -1;
            }
            else
            {
                if (dataRate == DataRate.High)
                    return 174;
                else if (dataRate == DataRate.Medium)
                    return 96;
                else if (dataRate == DataRate.Low)
                    return 48;
                else return -1;
            }
        }

        public static int MinNeuralChannel(DataRate dataRate)
        {
            if (dataRate == DataRate.High)
                return 0;
            else if (dataRate == DataRate.Medium)
                return 2;
            else if (dataRate == DataRate.Low)
                return 4;
            else return -1;
        }

        public static int MaxNeuralChannel(DataRate dataRate)
        {
            if (dataRate == DataRate.High)
                return 9;
            else if (dataRate == DataRate.Medium)
                return 6;
            else if (dataRate == DataRate.Low)
                return 5;
            else return -1;
        }

        public static double MsecPerRAMPage(DataRate dataRate)
        {
            if (dataRate == DataRate.High)
                return 1000.0 * Constant.FramePeriod * 2.2069;
            else if (dataRate == DataRate.Medium)
                return 1000.0 * Constant.FramePeriod * 4.0000;
            else if (dataRate == DataRate.Low)
                return 1000.0 * Constant.FramePeriod * 8.0000;
            else return 0.0;
        }
    }

}
