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
    /// <summary>
    /// This class provides access to and control of the Opal Kelly XEM3010 USB/FPGA interface board
    /// connected to an InsectChip receiver.
    /// </summary>
    public class USBSource
    {
        // private variables

        private okCFrontPanel myXEM;

        private double[] neuralState = new double[Constant.TotalNeuralChannels];
        private double[] neuralDelay1 = new double[Constant.TotalNeuralChannels];
        private double[] neuralDelay2 = new double[Constant.TotalNeuralChannels];
        private double[] neuralNotchDelay1 = new double[Constant.TotalNeuralChannels];
        private double[] neuralNotchDelay2 = new double[Constant.TotalNeuralChannels];

        private double[] EMGState = new double[Constant.TotalEMGChannels];
        private double[] EMGDelay1 = new double[Constant.TotalEMGChannels];
        private double[] EMGDelay2 = new double[Constant.TotalEMGChannels];
        private double[] EMGNotchDelay1 = new double[Constant.TotalEMGChannels];
        private double[] EMGNotchDelay2 = new double[Constant.TotalEMGChannels];

        private bool dataJustStarted;
        private bool synthDataMode;
        private bool newSynthDataReady;

        private double[] synthSpikeOffset = { 52, -44, 13, 0, 33, -35, -21, 23, -24, -5 };

        private double[] synthSpikeAmp1 = { -100, -500, -350, 0, -70, -240, 0, -180, -30, -400 };
        private double[] synthSpikeAmp2 = { -160, -80, -100, -120, 0, -340, 0, -80, -30, -180 };

        private double[] synthSpikeWidth1 = { 25, 15, 30, 10, 22, 18, 12, 21, 28, 15 };
        private double[] synthSpikeWidth2 = { 39, 30, 25, 20, 24, 29, 30, 24, 35, 20 };
        
        private double[] synthEMGOffset = { 0.1, -0.3, 0.05, 0.2 };
 
        private double[] synthEMGAmp1 = { 5.0, 3.5, 1.0, 4.0 };
        private double[] synthEMGAmp2 = { 0.8, 1.0, 0.8, 2.6 };

        private double[] synthEMGWidth1 = { 10, 8, 9, 7 };
        private double[] synthEMGWidth2 = { 7, 9, 6, 11 };

        private double[,] neuralSynthData = new double[Constant.TotalNeuralChannels, Constant.NeuralSamplesPerFrame * Constant.FramesPerBlock];
        private double[,] EMGSynthData = new double[Constant.TotalEMGChannels, Constant.FramesPerBlock];

        private bool enableHPF;
        private double fHPF;
        private bool enableNotch;
        private const double fNotch = 60.0;  // 60 Hz notch filter; change to 50 for Europe

        private int chipCounter = 0;
        private int frameCounterLow = 0;
        private int frameCounterHigh = 0;

        private const int USBBufSize = (4 * Constant.MaxFrameSize / 3) * Constant.BytesPerWord * Constant.FramesPerBlock;
        private byte[] USBBuf = new byte[USBBufSize];
        private UInt16[] rawFrameBlock = new UInt16[Constant.MaxFrameSize * Constant.FramesPerBlock];

        // USB FPGA board I/O endpoint addresses
        private const int wireInResetReadWrite = 0x00;
        private const int wireInCorrelationThreshold = 0x01;
        private const int wireInDataRate = 0x02;
        private const int wireInClockEdge = 0x03;
        private const int wireInLEDs = 0x04;
        private const int wireInRawDataMode = 0x05;
        private const int wireOutNumPages = 0x20;
        private const int wireOutBoardID = 0x3E;
        private const int wireOutBoardVersion = 0x3F;
        private const int pipeOutData = 0xA0;

        // properties

        /// <summary>
        /// Is software high-pass filter enabled?
        /// </summary>
        public bool EnableHPF
        {
            get
            {
                return enableHPF;
            }
            set
            {
                enableHPF = value;
            }
        }

        /// <summary>
        /// Cutoff frequency of software high-pass filter
        /// </summary>
        public double FHPF
        {
            get
            {
                return fHPF;
            }
            set
            {
                fHPF = value;
            }
        }

        /// <summary>
        /// Is software notch filter enabled?
        /// </summary>
        public bool EnableNotch
        {
            get
            {
                return enableNotch;
            }
            set
            {
                enableNotch = value;
            }
        }


        /// <summary>
        /// Are we generating synthetic neural data for demo purposes (if USB/FPGA board is not connected)?
        /// </summary>
        public bool SynthDataMode
        {
            get
            {
                return synthDataMode;
            }
            set
            {
                synthDataMode = value;
            }
        }

        /// <summary>
        /// Flag for timer to trip every 25 ms to generate more synthetic spike data
        /// (if USB/FPGA board is not connected)
        /// </summary>
        public bool NewSynthDataReady
        {
            get
            {
                return newSynthDataReady;
            }
            set
            {
                newSynthDataReady = value;
            }
        }

        // public methods

        /// <summary>
        /// Attempt to open Opal Kelly XEM3010 USB FPGA board.
        /// </summary>
        /// <param name="dataRate">Data rate (High, Medium, or Low).</param>
        /// <param name="boardID">Board ID number.</param>
        /// <param name="boardVersion">Board version number.</param>
        /// <returns>Board name.</returns>
        public string Open(DataRate dataRate, out int boardID, out int boardVersion)
        {
            boardID = -1;
            boardVersion = -1;

            // Open Opal Kelly XEM3010 board
            myXEM = new okCFrontPanel();

            if (myXEM.OpenBySerial("") != okCFrontPanel.ErrorCode.NoError)
            {
                UsbException e = new UsbException("USB Setup Error: Could not find USB board.");
                throw e;
            }

            // Setup the PLL from the stored configuration.
            myXEM.LoadDefaultPLLConfiguration();

            // Read back the CY22393 PLL configuation (6/3/11)
            okCPLL22393 myPLL = new okCPLL22393();
            myXEM.GetEepromPLL22393Configuration(myPLL);

            // Modify PLL settings from default
            myPLL.SetOutputDivider(0, 5); // set clk1 to 80 MHz (normally 4 = 100 MHz)
            myPLL.SetOutputDivider(1, 8); // set clk2 to 50 MHz (normally 4 = 100 MHz)
            // ... 8 works at 30 MHz with state 7 present (BER = 5.3e-4), zero BER at 20 MHz
            myXEM.SetPLL22393Configuration(myPLL);

            // Download the configuration file.
            if (okCFrontPanel.ErrorCode.NoError != myXEM.ConfigureFPGA("bug3a_receiver_1.bit"))
            {
                UsbException e = new UsbException("USB Setup Error: FPGA configuration failed.");
                throw e;
            }

            Debug.WriteLine("FPGA configuration complete.");

            // Check for FrontPanel support in the FPGA configuration.
            if (false == myXEM.IsFrontPanelEnabled())
            {
                UsbException e = new UsbException("USB Setup Error: FrontPanel support is not available.");
                throw e;
            }

            // Reset FIFOs
            myXEM.SetWireInValue(wireInResetReadWrite, 0x04);
            myXEM.UpdateWireIns();
            myXEM.SetWireInValue(wireInResetReadWrite, 0x00);
            myXEM.UpdateWireIns();

            // Turn raw data mode off by default
            myXEM.SetWireInValue(wireInRawDataMode, 0x00);
            myXEM.UpdateWireIns();

            myXEM.UpdateWireOuts();
            boardID = (int) myXEM.GetWireOutValue(wireOutBoardID);
            boardVersion = (int) myXEM.GetWireOutValue(wireOutBoardVersion);

            string boardName;
            boardName = myXEM.GetDeviceID();
            // Debug.WriteLine("Board ID: " + boardID);
            return boardName;
        }

        /// <summary>
        /// Start streaming USB data.
        /// </summary>
        public void Start()
        {
            if (!synthDataMode)
            {
                // Reset FIFOs
                myXEM.SetWireInValue(wireInResetReadWrite, 0x04);
                myXEM.UpdateWireIns();
                myXEM.SetWireInValue(wireInResetReadWrite, 0x00);
                myXEM.UpdateWireIns();

                // Enable data transfers
                myXEM.SetWireInValue(wireInResetReadWrite, 0x03); // read and write
                myXEM.UpdateWireIns();
            }

            dataJustStarted = true;
        }

        /// <summary>
        /// Stop streaming USB data.
        /// </summary>
        public void Stop()
        {
            if (!synthDataMode)
            {
                // Stop read and write
                myXEM.SetWireInValue(wireInResetReadWrite, 0x00);
                // Turn off LEDs
                myXEM.SetWireInValue(wireInLEDs, 0x00);
                myXEM.UpdateWireIns();

                // Reset FIFOs
                myXEM.SetWireInValue(wireInResetReadWrite, 0x04);
                myXEM.UpdateWireIns();
                myXEM.SetWireInValue(wireInResetReadWrite, 0x00);
                myXEM.UpdateWireIns();
            }

        }

        /// <summary>
        /// Check to see if there is at least 25 msec worth of data (40 frames) in the USB read buffer.
        /// </summary>
        /// <param name="plotQueue">Queue used for plotting data to screen.</param>
        /// <param name="saveQueue">Queue used for saving data to disk.</param>
        /// <param name="dataRate">Data rate (High, Medium, or Low).</param>
        /// <param name="hammingDecoder">Hamming decoder object.</param>
        /// <returns>Number of pages remaining in FPGA board RAM buffer.</returns>
        public int CheckForUSBData(Queue<USBData> plotQueue, Queue<USBData> saveQueue, DataRate dataRate, bool rawDataMode, HammingDecoder hammingDecoder)
        {
            bool haveEnoughData = false;
            int i, numPagesInRAM, numPagesLeftInRAM, numBytesToRead, numBytesRead, indexUSB, indexFrame;
            int word1, word2, word3, word4;
            int pageThreshold = 20;

            numPagesLeftInRAM = 0;

            if (rawDataMode)
            {
                pageThreshold = 24;
            }
            else
            {
                switch (dataRate)
                {
                    case DataRate.High:
                        pageThreshold = 20;
                        break;
                    case DataRate.Medium:
                        pageThreshold = 11;
                        break;
                    case DataRate.Low:
                        pageThreshold = 6;
                        break;
                }
            }

            if (synthDataMode)
            {
                if (newSynthDataReady)
                {
                    //HACK XXX TODO FIXME TESTING -- comment-in the following conditional to test the "no data from USB condition"
                    //long sec = System.DateTime.Now.Second;
                    //if ((sec / 4) % 2 == 0)
                    //{
                        haveEnoughData = true;
                        newSynthDataReady = false;
                    //}
                }
            }
            else
            {
                myXEM.UpdateWireOuts();
                numPagesInRAM = (int) myXEM.GetWireOutValue(wireOutNumPages);
                //Debug.WriteLine("numPagesInRAM = " + numPagesInRAM);
                if (numPagesInRAM > pageThreshold) haveEnoughData = true;
            }

            if (haveEnoughData)
            {
                USBData USBDataBlock;
                long tsNow = MainForm.GetAbsTimeNS();

                if (synthDataMode)
                {
                    int channel, j;
                    Random myRand = new Random(unchecked((int)DateTime.Now.Ticks));

                    for (channel = 0; channel < Constant.TotalNeuralChannels; channel++)
                    {
                        for (i = 0; i < Constant.NeuralSamplesPerFrame * Constant.FramesPerBlock; i++)
                        {
                            neuralSynthData[channel, i] = synthSpikeOffset[channel] + 5.7 * gaussian(myRand);  // create realistic offset and background of 5.7 uV rms noise
                        }
                        i = 0;
                        while (i < Constant.NeuralSamplesPerFrame * Constant.FramesPerBlock - 52)   // 52 samples = 2 msec (refractory period)
                        {
                            if (myRand.NextDouble() < 0.01)
                            {
                                for (j = 0; j < synthSpikeWidth1[channel]; j++)
                                {
                                    neuralSynthData[channel, i + j] += (synthSpikeAmp1[channel] * Math.Exp(-1 * (double)j / 12.5) * Math.Sin(6.28 * (double)j / synthSpikeWidth1[channel]));
                                }
                                i += 40 + 52;    // advance by 2 msec (refractory period)
                            }
                            else if (myRand.NextDouble() < 0.02)
                            {
                                for (j = 0; j < synthSpikeWidth2[channel]; j++)
                                {
                                    neuralSynthData[channel, i + j] += (synthSpikeAmp2[channel] * Math.Exp(-1 * (double)j / 12.5) * Math.Sin(6.28 * (double)j / synthSpikeWidth2[channel]));
                                }
                                i += 40 + 52;    // advance by 2 msec (refractory period)
                            }
                            else
                            {
                                i += 26;    // advance by 1 msec
                            }
                        }
                    }

                    for (channel = 0; channel < Constant.TotalEMGChannels; channel++)
                    {
                        for (i = 0; i < Constant.FramesPerBlock; i++)
                        {
                            EMGSynthData[channel, i] = synthEMGOffset[channel] + 0.043 * gaussian(myRand);  // create realistic offset and background of 43 uV rms noise
                        }
                        i = 0;
                        while (i < Constant.FramesPerBlock - 10)   // 10 = max 'spike' width
                        {
                            if (myRand.NextDouble() < 0.004)
                            {
                                for (j = 0; j < synthEMGWidth1[channel]; j++)
                                {
                                    EMGSynthData[channel, i + j] += (synthEMGAmp1[channel] * Math.Exp(-1 * (double)j / 12.5) * Math.Sin(6.28 * (double)j / synthEMGWidth1[channel]));
                                }
                                i += 10 + 3;    // advance by 2 msec (refractory period)
                            }
                            else if (myRand.NextDouble() < 0.008)
                            {
                                for (j = 0; j < synthEMGWidth2[channel]; j++)
                                {
                                    EMGSynthData[channel, i + j] += (synthEMGAmp2[channel] * Math.Exp(-1 * (double)j / 12.5) * Math.Sin(6.28 * (double)j / synthEMGWidth2[channel]));
                                }
                                i += 10 + 3;    // advance by 2 msec (refractory period)
                            }
                            else
                            {
                                i += 2;    // advance by 1 msec
                            }
                        }
                    }

                    // USBDataBlock = new USBData(neuralSynthData, EMGSynthData);

                    USBDataFrameCreate(neuralSynthData, EMGSynthData, rawFrameBlock, dataRate, hammingDecoder, 0.001);
                    USBDataBlock = new USBData(rawFrameBlock, dataRate, rawDataMode, hammingDecoder);
                    numPagesLeftInRAM = 1;
                }
                else
                {
                    numBytesToRead = (4 * Constant.FrameSize(dataRate, rawDataMode) / 3) * Constant.BytesPerWord * Constant.FramesPerBlock;
                    numBytesRead = myXEM.ReadFromPipeOut(pipeOutData, numBytesToRead, USBBuf);

                    if (numBytesRead != numBytesToRead)
                    {
                        UsbException e = new UsbException("USB read error; not enough bytes read");
                        throw e;
                    }
                    
                    if ((USBBuf[1] & 0xe0) != 0xe0) // USBBuf[1] = high byte of first word; should have the form 111xxxxx if we are in sync
                    {
                        Debug.WriteLine("Resync");

                        // Reset FIFOs
                        myXEM.SetWireInValue(wireInResetReadWrite, 0x04);
                        myXEM.UpdateWireIns();
                        myXEM.SetWireInValue(wireInResetReadWrite, 0x00);
                        myXEM.UpdateWireIns();

                        // Enable data transfers
                        myXEM.SetWireInValue(wireInResetReadWrite, 0x03); // read and write
                        myXEM.UpdateWireIns();
 
                        return 0;     // abandon corrupted frame; get out of here

                        // UsbException e = new UsbException("USB sync error");
                        // throw e;
                    }

                    indexUSB = 0;
                    indexFrame = 0;
                    for (i = 0; i < (Constant.FrameSize(dataRate, rawDataMode) / 3) * Constant.FramesPerBlock; i++)
                    {
                        word1 = 256 * Convert.ToInt32(USBBuf[indexUSB + 1]) + Convert.ToInt32(USBBuf[indexUSB]);
                        indexUSB += 2;
                        word2 = 256 * Convert.ToInt32(USBBuf[indexUSB + 1]) + Convert.ToInt32(USBBuf[indexUSB]);
                        indexUSB += 2;
                        word3 = 256 * Convert.ToInt32(USBBuf[indexUSB + 1]) + Convert.ToInt32(USBBuf[indexUSB]);
                        indexUSB += 2;
                        word4 = 256 * Convert.ToInt32(USBBuf[indexUSB + 1]) + Convert.ToInt32(USBBuf[indexUSB]);
                        indexUSB += 2;

                        rawFrameBlock[indexFrame++] = Convert.ToUInt16(((word1 & 0x0f00) << 4) + (word2 & 0x0fff));
                        rawFrameBlock[indexFrame++] = Convert.ToUInt16(((word1 & 0x00f0) << 8) + (word3 & 0x0fff));
                        rawFrameBlock[indexFrame++] = Convert.ToUInt16(((word1 & 0x000f) << 12) + (word4 & 0x0fff));
                    }

                    USBDataBlock = new USBData(rawFrameBlock, dataRate, rawDataMode, hammingDecoder);

                    myXEM.UpdateWireOuts();
                    numPagesLeftInRAM = (int) myXEM.GetWireOutValue(wireOutNumPages);
                }
                
                if (dataJustStarted)
                {
                    USBDataBlock.InitNeuralFilterState(neuralState, neuralDelay1, neuralDelay2, neuralNotchDelay1, neuralNotchDelay2);
                    USBDataBlock.InitEMGFilterState(EMGState, EMGDelay1, EMGDelay2, EMGNotchDelay1, EMGNotchDelay2);
                    dataJustStarted = false;
                }
                
                if (enableHPF)
                {
                    USBDataBlock.NeuralFilterHPF(neuralState, fHPF, Constant.NeuralSampleRate);
                    USBDataBlock.EMGFilterHPF(EMGState, fHPF, Constant.EMGSampleRate);
                }
                
                if (enableNotch)
                {
                    USBDataBlock.NeuralFilterNotch(neuralDelay1, neuralDelay2, neuralNotchDelay1, neuralNotchDelay2, fNotch, Constant.NeuralSampleRate);
                    USBDataBlock.EMGFilterNotch(EMGDelay1, EMGDelay2, EMGNotchDelay1, EMGNotchDelay2, fNotch, Constant.EMGSampleRate);
                }

                USBDataBlock.timeStampNanos = tsNow;

                plotQueue.Enqueue(USBDataBlock);
                saveQueue.Enqueue(USBDataBlock);
            }

            return numPagesLeftInRAM;
        }

        /// <summary>
        /// Create synthetic USB data frames.
        /// </summary>
        /// <param name="neuralSynthData">Neural waveforms.</param>
        /// <param name="EMGSynthData">EMG waveforms.</param>
        /// <param name="rawFrameBlock">Array for raw USB data.</param>
        /// <param name="dataRate">Data rate (High, Medium, or Low).</param>
        /// <param name="hammingDecoder">Hamming decoder object.</param>
        /// <param name="BER">Bit error rate (set to zero to disable).</param>
        private void USBDataFrameCreate(double[,] neuralSynthData, double[,] EMGSynthData, UInt16[] rawFrameBlock, DataRate dataRate, HammingDecoder hammingDecoder, double BER)
        {
            int frame, i, channel, index, indexNeural;
            int dataWord;
            Random myRand = new Random(unchecked((int)DateTime.Now.Ticks));

            index = 0;
            indexNeural = 0;
            for (frame = 0; frame < Constant.FramesPerBlock; frame++)
            {
                for (i = 0; i < Constant.NeuralSamplesPerFrame; i++)
                {
                    for (channel = Constant.MinNeuralChannel(dataRate); channel <= Constant.MaxNeuralChannel(dataRate); channel++)
                    {
                        dataWord = (int)((neuralSynthData[channel, indexNeural] / Constant.ADCStepNeural) + Constant.ADCOffset);
                        rawFrameBlock[index++] = hammingDecoder.EncodeData(dataWord, myRand, BER);
                    }
                    indexNeural++;
                }
                for (channel = 0; channel < Constant.TotalEMGChannels; channel++)
                {
                    dataWord = (int)((EMGSynthData[channel, frame] / Constant.ADCStepEMG) + Constant.ADCOffset);
                    rawFrameBlock[index++] = hammingDecoder.EncodeData(dataWord, myRand, BER);
                }
                dataWord = 0;   // AUX 1
                rawFrameBlock[index++] = hammingDecoder.EncodeData(dataWord, myRand, BER);

                dataWord = (int)(((1.8 + 0.1 * gaussian(myRand)) / Constant.ADCStepAux) + Constant.ADCOffset);
                rawFrameBlock[index++] = hammingDecoder.EncodeData(dataWord, myRand, BER);

                dataWord = 1536;   // Chip ID
                rawFrameBlock[index++] = hammingDecoder.EncodeData(dataWord, myRand, BER);

                dataWord = chipCounter;   // Chip Counter
                rawFrameBlock[index++] = hammingDecoder.EncodeData(dataWord, myRand, BER);

                dataWord = frameCounterLow;   // Frame Counter Low
                rawFrameBlock[index++] = hammingDecoder.EncodeData(dataWord, myRand, 0.0);

                dataWord = frameCounterHigh;   // Frame Counter High
                rawFrameBlock[index++] = hammingDecoder.EncodeData(dataWord, myRand, 0.0);

                dataWord = 960;   // Frame Timer Low (= 960)
                rawFrameBlock[index++] = hammingDecoder.EncodeData(dataWord, myRand, 0.0);

                dataWord = 0;   // Frame Timer High (= 960)
                rawFrameBlock[index++] = hammingDecoder.EncodeData(dataWord, myRand, 0.0);

                dataWord = 0;   // TTL Inputs
                rawFrameBlock[index++] = hammingDecoder.EncodeData(dataWord, myRand, 0.0);

                dataWord = 144;   // Frame Marker Correlation
                rawFrameBlock[index++] = hammingDecoder.EncodeData(dataWord, myRand, 0.0);

                if (dataRate == DataRate.Medium || dataRate == DataRate.Low)
                {
                    dataWord = 0;
                    rawFrameBlock[index++] = hammingDecoder.EncodeData(dataWord, myRand, 0.0);
                    rawFrameBlock[index++] = hammingDecoder.EncodeData(dataWord, myRand, 0.0);
                }

                chipCounter++;
                if (chipCounter > 2047) chipCounter = 0;

                frameCounterLow++;
                if (frameCounterLow > 2047)
                {
                    frameCounterLow = 0;
                    frameCounterHigh++;
                    if (frameCounterHigh > 2047) frameCounterHigh = 0;
                }
            }
            
        }


        /// <summary>
        /// Return a random number from a Gaussian distribution with variance = 1.
        /// </summary>
        /// <param name="rand">Pseudo-random number generator object.</param>
        /// <returns>Random number picked from Gaussian distribution.</returns>
        private static float gaussian(Random rand)
        {
            double r = 0.0;
            const double Sqrt3 = 1.73205080757;
            const int N = 8;   // making N larger increases accuracy at the expense of speed

            for (int i = 0; i < N; i++)
            {
                r += (Sqrt3 * 2.0 * rand.NextDouble()) - 1.0;
            }

            r /= Math.Sqrt(N);

            return ((float)r);
        }

        /// <summary>
        /// Configure FPGA board to receive data of certain rate.
        /// </summary>
        /// <param name="dataRate">Data rate (High, Medium, or Low).</param>
        public void SetDataRate(DataRate dataRate)
        {
            if (myXEM != null)
            {
                if (dataRate == DataRate.Low)
                    myXEM.SetWireInValue(wireInDataRate, 3);
                else if (dataRate == DataRate.Medium)
                    myXEM.SetWireInValue(wireInDataRate, 1);
                else if (dataRate == DataRate.High)
                    myXEM.SetWireInValue(wireInDataRate, 0);
                myXEM.UpdateWireIns();
            }
            Debug.WriteLine("XEM data rate changed to dataRate = " + dataRate);
        }

        /// <summary>
        /// Set frame marker correlation threshold level on FPGA board.
        /// </summary>
        /// <param name="threshold">Threshold.</param>
        public void SetCorrelationThreshold(int threshold)
        {
            if (threshold < 0)
                threshold = 0;
            else if (threshold > Constant.MaxFrameCorrelation)
                threshold = Constant.MaxFrameCorrelation;
            if (myXEM != null)
            {
                myXEM.SetWireInValue(wireInCorrelationThreshold, (uint)threshold);
                myXEM.UpdateWireIns();
            }
        }

        /// <summary>
        /// Set data clock edge on FGPA board.
        /// </summary>
        /// <param name="polarity">Clock polarity.</param>
        public void SetClockEdge(int polarity)
        {
            if (polarity < 0)
                polarity = 0;
            else if (polarity > 1)
                polarity = 1;
            if (myXEM != null)
            {
                myXEM.SetWireInValue(wireInClockEdge, (uint)(1 - polarity));
                myXEM.UpdateWireIns();
            }
        }

        /// <summary>
        /// Turn on selected LEDs.
        /// </summary>
        /// <param name="LEDcode">7-bit number (valid range 0-63) to control seven LEDs on USB/FPGA board.</param>
        public void LEDControl(int LEDcode)
        {
            if (myXEM != null)
            {
                myXEM.SetWireInValue(wireInLEDs, (uint)LEDcode);
                myXEM.UpdateWireIns();
            }
        }

        /// <summary>
        /// Enable or disable raw data mode.
        /// </summary>
        /// <param name="rawDataMode">Enable/disable raw data mode.</param>
        public void EnableRawMode(bool rawDataMode)
        {
            if (myXEM != null)
            {
                if (rawDataMode)
                {
                    myXEM.SetWireInValue(wireInRawDataMode, 0x01);
                }
                else
                {
                    myXEM.SetWireInValue(wireInRawDataMode, 0x00);
                }
                myXEM.UpdateWireIns();
            }
        }

        /// <summary>
        /// USBSource constructor.
        /// </summary>
        public USBSource()
        {
            dataJustStarted = true;
            enableHPF = false;
            fHPF = 1.0;
            enableNotch = false;
            synthDataMode = false;
        }
    }


    public class UsbException : System.Exception
    {
        public UsbException(string message) :
            base(message) // pass the message up to the base class
        {
        }
    }

}
