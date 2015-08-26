/*
 * Insect Telemetry Receiver GUI for use with Intan 'Bug3' Chips
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

namespace Bug3
{

    

    /// <summary>
    /// The class stores data frames containing all data from Bug3 chip, plus TTL inputs,
    /// frame enumeration, and frame timing information from USB/FPGA board.
    /// </summary>
    public class USBData
    {
        // public variables
        public double[,] neuralData = new double[Constant.TotalNeuralChannels, Constant.NeuralSamplesPerFrame * Constant.FramesPerBlock];
        public UInt16[,] neuralData16 = new UInt16[Constant.TotalNeuralChannels, Constant.NeuralSamplesPerFrame * Constant.FramesPerBlock];
        public double[,] EMGData = new double[Constant.TotalEMGChannels, Constant.FramesPerBlock];
        public UInt16[,] EMGData16 = new UInt16[Constant.TotalEMGChannels, Constant.FramesPerBlock];
        public double[,] auxData = new double[Constant.TotalAuxChannels, Constant.FramesPerBlock];
        public UInt16[,] auxData16 = new UInt16[Constant.TotalAuxChannels, Constant.FramesPerBlock];
        public UInt16[] chipID = new UInt16[Constant.FramesPerBlock];
        public int[] chipFrameCounter = new int[Constant.FramesPerBlock];
        public UInt16[] TTLInputs = new UInt16[Constant.FramesPerBlock];
        public UInt16[] frameMarkerCorrelation = new UInt16[Constant.FramesPerBlock];
        public int[] boardFrameCounter = new int[Constant.FramesPerBlock];
        public int[] boardFrameTimer = new int[Constant.FramesPerBlock];
        public double BER;
        public double WER;
        public int missingFrameCount;
        public int falseFrameCount;
        public UInt16[] rawData = new UInt16[Constant.MaxFrameSize * Constant.FramesPerBlock];

        // the absolute time in nanoseconds that this data block was created, as returned by MainForm.GetAbsTimeNS()
        public long timeStampNanos; 

        // public methods

        /// <summary>
        /// USBData constructor.
        /// </summary>
        /// <param name="rawFrameBlock">Data block from USB interface.</param>
        /// <param name="dataRate">Data rate (Low, Medium, or High).</param>
        /// <param name="hammingDecoder">Hamming code decoder object.</param>
        public USBData(UInt16[] rawFrameBlock, DataRate dataRate, bool rawDataMode, HammingDecoder hammingDecoder)
        {
            int frame, channel, index, indexNeural, i;
            int numBitErrors;
            int bitErrorCount = 0;
            int wordErrorCount = 0;

            int minNeuralChannel = Constant.MinNeuralChannel(dataRate);
            int maxNeuralChannel = Constant.MaxNeuralChannel(dataRate);
            int frameSize = Constant.FrameSize(dataRate, rawDataMode);

            const double expectedFrameTimer = Constant.FPGAClockFreq * Constant.FramePeriod / 32.0; // = 960
            const int maxFrameTimer = (int)(expectedFrameTimer * 1.01);
            const int minFrameTimer = (int)(expectedFrameTimer / 1.01);

            missingFrameCount = 0;
            falseFrameCount = 0;

            timeStampNanos = MainForm.GetAbsTimeNS();

            for (i = 0; i < frameSize * Constant.FramesPerBlock; i++)
            {
                rawData[i] = rawFrameBlock[i];
            }

            index = 0;
            indexNeural = 0;
            double vOut;
            for (frame = 0; frame < Constant.FramesPerBlock; frame++)
            {
                for (i = 0; i < Constant.NeuralSamplesPerFrame; i++)
                {
                    for (channel = minNeuralChannel; channel <= maxNeuralChannel; channel++)
                    {
                        neuralData[channel, indexNeural + i] =
                            Constant.ADCStepNeural * ((neuralData16[channel, indexNeural+i]=hammingDecoder.DecodeDataCountErrors(rawFrameBlock[index], out numBitErrors, ref bitErrorCount, ref wordErrorCount)) - Constant.ADCOffset);
                        if (numBitErrors == 2 && (indexNeural + i) > 0)
                        {
                            neuralData[channel, indexNeural + i] = neuralData[channel, indexNeural + i - 1];
                            neuralData16[channel, indexNeural + i] = neuralData16[channel, indexNeural + i - 1];
                        }
                        index++;
                    }
                    for (channel = 0; channel < minNeuralChannel; channel++)
                    {
                        neuralData[channel, indexNeural + i] = 0.0;
                        neuralData16[channel, indexNeural + i] = (UInt16)Constant.ADCOffset;
                    }
                    for (channel = maxNeuralChannel + 1; channel < Constant.TotalNeuralChannels; channel++)
                    {
                        neuralData[channel, indexNeural + i] = 0.0;
                        neuralData16[channel, indexNeural + i] = (UInt16)Constant.ADCOffset;
                    }
                }
                indexNeural += Constant.NeuralSamplesPerFrame;
                
                for (channel = 0; channel < Constant.TotalEMGChannels; channel++)
                {
                    EMGData[channel, frame] =
                        Constant.ADCStepEMG * ((EMGData16[channel, frame]=hammingDecoder.DecodeDataCountErrors(rawFrameBlock[index], out numBitErrors, ref bitErrorCount, ref wordErrorCount)) - Constant.ADCOffset);
                    if (numBitErrors == 2 && frame > 0)
                    {
                        EMGData[channel, frame] = EMGData[channel, frame - 1];
                        EMGData16[channel, frame] = EMGData16[channel, frame - 1];
                    }
                    index++;
                }

                for (channel = 0; channel < Constant.TotalAuxChannels; channel++)
                {
                    vOut = Constant.ADCStep * (hammingDecoder.DecodeDataCountErrors(rawFrameBlock[index], out numBitErrors, ref bitErrorCount, ref wordErrorCount) - Constant.ADCOffset);

                    // Empirical equation modeling nonlinearity of auxiliary amplifier (see testing data, 6/25/11)
                    auxData[channel, frame] = 4.343 * vOut + 0.1 * Math.Exp(12.0 * (vOut - 0.82)) + 0.2 * Math.Exp(130.0 * (vOut - 1.0));

                    // Limit result to between 0 and 6.0 V
                    if (auxData[channel, frame] > 6.0)
                        auxData[channel, frame] = 6.0;
                    else if (auxData[channel, frame] < 0.0)
                        auxData[channel, frame] = 0.0;

                    if (numBitErrors == 2 && frame > 0)
                        auxData[channel, frame] = auxData[channel, frame - 1];

                    auxData16[channel, frame] = (UInt16)(auxData[channel, frame] / Constant.ADCStep + Constant.ADCOffset);

                    index++;
                }

                chipID[frame] = hammingDecoder.DecodeDataCountErrors(rawFrameBlock[index], out numBitErrors, ref bitErrorCount, ref wordErrorCount);
                index++;

                chipFrameCounter[frame] = hammingDecoder.DecodeDataCountErrors(rawFrameBlock[index], out numBitErrors, ref bitErrorCount, ref wordErrorCount);
                index++;

                boardFrameCounter[frame] = hammingDecoder.DecodeData(rawFrameBlock[index++]) + 2048 * hammingDecoder.DecodeData(rawFrameBlock[index++]);
                boardFrameTimer[frame] = hammingDecoder.DecodeData(rawFrameBlock[index++]) + 2048 * hammingDecoder.DecodeData(rawFrameBlock[index++]);
                TTLInputs[frame] = hammingDecoder.DecodeData(rawFrameBlock[index++]);
                frameMarkerCorrelation[frame] = hammingDecoder.DecodeData(rawFrameBlock[index++]);

                if (dataRate == DataRate.Low || dataRate == DataRate.Medium) index += 2;

                if (boardFrameTimer[frame] > maxFrameTimer)
                    missingFrameCount++;
                else if (boardFrameTimer[frame] < minFrameTimer)
                    falseFrameCount++;
            }

            BER = ((double)bitErrorCount) / ((double)(Constant.FramesPerBlock * Constant.BitsPerWord * (Constant.NeuralSamplesPerFrame * (maxNeuralChannel - minNeuralChannel + 1) + (Constant.TotalEMGChannels + Constant.TotalAuxChannels + 2))));
            WER = ((double)wordErrorCount) / ((double)(Constant.FramesPerBlock * (Constant.NeuralSamplesPerFrame * (maxNeuralChannel - minNeuralChannel + 1) + (Constant.TotalEMGChannels + Constant.TotalAuxChannels + 2))));
        }


        /// <summary>
        /// Copy neural data from data block into array.
        /// </summary>
        /// <param name="neuralDataArray">Array for neural data.</param>
        public void CopyNeuralDataToArray(double[,] neuralDataArray)
        {
            int i, channel;

            for (i = 0; i < Constant.NeuralSamplesPerFrame * Constant.FramesPerBlock; i++)
            {
                for (channel = 0; channel < Constant.TotalNeuralChannels; channel++)
                {
                    neuralDataArray[channel, i] = neuralData[channel, i];
                }
            }
        }

        /// <summary>
        /// Copy EMG data from data block into array.
        /// </summary>
        /// <param name="EMGDataArray">Array for EMG data.</param>
        public void CopyEMGDataToArray(double[,] EMGDataArray)
        {
            int i, channel;

            for (i = 0; i < Constant.FramesPerBlock; i++)
            {
                for (channel = 0; channel < Constant.TotalEMGChannels; channel++)
                {
                    EMGDataArray[channel, i] = EMGData[channel, i];
                }
            }
        }

        /// <summary>
        /// Copy auxiliary data from data block into array.
        /// </summary>
        /// <param name="auxDataArray">Array for auxiliary data.</param>
        public void CopyAuxDataToArray(double[,] auxDataArray)
        {
            int i, channel;

            for (i = 0; i < Constant.FramesPerBlock; i++)
            {
                for (channel = 0; channel < Constant.TotalAuxChannels; channel++)
                {
                    auxDataArray[channel, i] = auxData[channel, i];
                }
            }
        }

        /// <summary>
        /// Copy TTL data from data block into array.
        /// </summary>
        /// <param name="TTLDataArray">Array for TTL data.</param>
        public void CopyTTLDataToArray(bool[,] TTLDataArray)
        {
            int i, channel;

            for (i = 0; i < Constant.FramesPerBlock; i++)
            {
                for (channel = 0; channel < Constant.TotalTTLChannels; channel++)
                {
                    TTLDataArray[channel, i] = (TTLInputs[i] & (1 << channel)) > 0;
                }
            }
        }

        public void CopyTTLDataToArray(ref bool[,] array)
        {
            if (array == null) array = new bool[Constant.TotalTTLChannels, Constant.FramesPerBlock];
            this.CopyTTLDataToArray(array);
        }

        /// <summary>
        /// Copy raw data from data block into array.
        /// </summary>
        /// <param name="rawDataArray">Array for raw data.</param>
        /// <param name="dataRate">Data rate (High, Medium, or Low).</param>
        public void CopyRawDataToArray(ushort[] rawDataArray, DataRate dataRate, bool rawDataMode)
        {
            int i;

            for (i = 0; i < Constant.FrameSize(dataRate, rawDataMode) * Constant.FramesPerBlock; i++)
            {
                rawDataArray[i] = (ushort)(rawData[i]);
            }
        }

        public double GetBER()
        {
            return BER;
        }

        public double GetWER()
        {
            return WER;
        }

        public int GetMissingFrameCount()
        {
            return missingFrameCount;
        }

        public int GetFalseFrameCount()
        {
            return falseFrameCount;
        }

        public int GetChipID()
        {
            int ID;
            int code = chipID[0];

            switch (code)
            {
                case 7: { ID = 0; break;  }
                case 56: { ID = 1; break; }
                case 448: { ID = 2; break; }
                case 1536: { ID = 3; break; }
                default: { ID = -1; break; }
            }

            return ID;
        }

        public void InitNeuralFilterState(double[] neuralState, double[] neuralDelay1, double[] neuralDelay2, double[] neuralNotchDelay1, double[] neuralNotchDelay2)
        {
            for (int channel = 0; channel < Constant.TotalNeuralChannels; channel++)
            {
                neuralState[channel] = neuralData[channel, 0];
                neuralDelay1[channel] = neuralData[channel, 0];
                neuralDelay2[channel] = neuralData[channel, 0];
                neuralNotchDelay1[channel] = neuralData[channel, 0];
                neuralNotchDelay2[channel] = neuralData[channel, 0];
            }
        }


        public void InitEMGFilterState(double[] EMGState, double[] EMGDelay1, double[] EMGDelay2, double[] EMGNotchDelay1, double[] EMGNotchDelay2)
        {
            for (int channel = 0; channel < Constant.TotalEMGChannels; channel++)
            {
                EMGState[channel] = EMGData[channel, 0];
                EMGDelay1[channel] = EMGData[channel, 0];
                EMGDelay2[channel] = EMGData[channel, 0];
                EMGNotchDelay1[channel] = EMGData[channel, 0];
                EMGNotchDelay2[channel] = EMGData[channel, 0];
            }
        }


        public void NeuralFilterHPF(double[] neuralState, double fHPF, double fSample)
        {
            // Apply software first-order high-pass filter
            // (See RHA2000 series datasheet for desciption of this algorithm.)

            double filtB = 1.0 - Math.Exp(-2.0 * Math.PI * fHPF / fSample);
            double temp;

            for (int channel = 0; channel < Constant.TotalNeuralChannels; channel++)
            {
                temp = neuralData[channel, 0] - neuralState[channel];
                neuralState[channel] = neuralState[channel] + filtB * temp;
                neuralData[channel, 0] = temp;

                for (int i = 1; i < Constant.NeuralSamplesPerFrame * Constant.FramesPerBlock; i++)
                {
                    temp = neuralData[channel, i] - neuralState[channel];
                    neuralState[channel] = neuralState[channel] + filtB * temp;
                    neuralData[channel, i] = temp;
                }
            }
        }


        public void EMGFilterHPF(double[] EMGState, double fHPF, double fSample)
        {
            // Apply software first-order high-pass filter
            // (See RHA2000 series datasheet for desciption of this algorithm.)

            double filtB = 1.0 - Math.Exp(-2.0 * Math.PI * fHPF / fSample);
            double temp;

            for (int channel = 0; channel < Constant.TotalEMGChannels; channel++)
            {
                temp = EMGData[channel, 0] - EMGState[channel];
                EMGState[channel] = EMGState[channel] + filtB * temp;
                EMGData[channel, 0] = temp;

                for (int i = 1; i < Constant.FramesPerBlock; i++)
                {
                    temp = EMGData[channel, i] - EMGState[channel];
                    EMGState[channel] = EMGState[channel] + filtB * temp;
                    EMGData[channel, i] = temp;
                }
            }
        }


        public void NeuralFilterNotch(double[] neuralDelay1, double[] neuralDelay2, double[] neuralNotchDelay1, double[] neuralNotchDelay2, double fNotch, double fSample)
        {
            // Apply software notch filter
            // (See RHA2000 series datasheet for desciption of this algorithm.)

            double neuralDelay0;

            const double NotchBW = 10.0;  // notch filter bandwidth, in Hz
            // Note: Reducing the notch filter bandwidth will create a more frequency-selective filter, but this
            // can lead to a long settling time for the filter.  Selecting a bandwdith of 10 Hz (e.g., filtering
            // out frequencies between 55 Hz and 65 Hz for the 60 Hz notch filter setting) implements a fast-
            // settling filter.

            double d = Math.Exp(-1.0 * Math.PI * NotchBW / fSample);
            double b = (1.0 + d * d) * Math.Cos(2.0 * Math.PI * fNotch / fSample);
            double a = 0.5 * (1.0 + d * d);

            double b0 = a;
            double b1 = a * (-2.0) * Math.Cos(2.0 * Math.PI * fNotch / fSample);
            double b2 = a;
            double a1 = -b;
            double a2 = d * d;

            for (int channel = 0; channel < Constant.TotalNeuralChannels; channel++)
            {
                neuralDelay0 = neuralData[channel, 0];
                neuralData[channel, 0] = b2 * neuralDelay2[channel] + b1 * neuralDelay1[channel] + b0 * neuralData[channel, 0] - a2 * neuralNotchDelay2[channel] - a1 * neuralNotchDelay1[channel];
                neuralDelay2[channel] = neuralDelay1[channel];
                neuralDelay1[channel] = neuralDelay0;
                neuralDelay0 = neuralData[channel, 1];
                neuralData[channel, 1] = b2 * neuralDelay2[channel] + b1 * neuralDelay1[channel] + b0 * neuralData[channel, 1] - a2 * neuralNotchDelay1[channel] - a1 * neuralData[channel, 0];
                for (int i = 2; i < Constant.NeuralSamplesPerFrame * Constant.FramesPerBlock; i++)
                {
                    neuralDelay2[channel] = neuralDelay1[channel];
                    neuralDelay1[channel] = neuralDelay0;
                    neuralDelay0 = neuralData[channel, i];
                    neuralData[channel, i] = b2 * neuralDelay2[channel] + b1 * neuralDelay1[channel] + b0 * neuralData[channel, i] - a2 * neuralData[channel, i - 2] - a1 * neuralData[channel, i - 1];
                }
                neuralDelay2[channel] = neuralDelay1[channel];
                neuralDelay1[channel] = neuralDelay0;
                neuralNotchDelay2[channel] = neuralData[channel, Constant.NeuralSamplesPerFrame * Constant.FramesPerBlock - 2];
                neuralNotchDelay1[channel] = neuralData[channel, Constant.NeuralSamplesPerFrame * Constant.FramesPerBlock - 1];
            }
        }


        public void EMGFilterNotch(double[] EMGDelay1, double[] EMGDelay2, double[] EMGNotchDelay1, double[] EMGNotchDelay2, double fNotch, double fSample)
        {
            // Apply software notch filter
            // (See RHA2000 series datasheet for desciption of this algorithm.)

            double EMGDelay0;

            const double NotchBW = 10.0;  // notch filter bandwidth, in Hz
            // Note: Reducing the notch filter bandwidth will create a more frequency-selective filter, but this
            // can lead to a long settling time for the filter.  Selecting a bandwdith of 10 Hz (e.g., filtering
            // out frequencies between 55 Hz and 65 Hz for the 60 Hz notch filter setting) implements a fast-
            // settling filter.

            double d = Math.Exp(-1.0 * Math.PI * NotchBW / fSample);
            double b = (1.0 + d * d) * Math.Cos(2.0 * Math.PI * fNotch / fSample);
            double a = 0.5 * (1.0 + d * d);

            double b0 = a;
            double b1 = a * (-2.0) * Math.Cos(2.0 * Math.PI * fNotch / fSample);
            double b2 = a;
            double a1 = -b;
            double a2 = d * d;

            for (int channel = 0; channel < Constant.TotalEMGChannels; channel++)
            {
                EMGDelay0 = EMGData[channel, 0];
                EMGData[channel, 0] = b2 * EMGDelay2[channel] + b1 * EMGDelay1[channel] + b0 * EMGData[channel, 0] - a2 * EMGNotchDelay2[channel] - a1 * EMGNotchDelay1[channel];
                EMGDelay2[channel] = EMGDelay1[channel];
                EMGDelay1[channel] = EMGDelay0;
                EMGDelay0 = EMGData[channel, 1];
                EMGData[channel, 1] = b2 * EMGDelay2[channel] + b1 * EMGDelay1[channel] + b0 * EMGData[channel, 1] - a2 * EMGNotchDelay1[channel] - a1 * EMGData[channel, 0];
                for (int i = 2; i < Constant.FramesPerBlock; i++)
                {
                    EMGDelay2[channel] = EMGDelay1[channel];
                    EMGDelay1[channel] = EMGDelay0;
                    EMGDelay0 = EMGData[channel, i];
                    EMGData[channel, i] = b2 * EMGDelay2[channel] + b1 * EMGDelay1[channel] + b0 * EMGData[channel, i] - a2 * EMGData[channel, i - 2] - a1 * EMGData[channel, i - 1];
                }
                EMGDelay2[channel] = EMGDelay1[channel];
                EMGDelay1[channel] = EMGDelay0;
                EMGNotchDelay2[channel] = EMGData[channel, Constant.FramesPerBlock - 2];
                EMGNotchDelay1[channel] = EMGData[channel, Constant.FramesPerBlock - 1];
            }
        }

    }
}
