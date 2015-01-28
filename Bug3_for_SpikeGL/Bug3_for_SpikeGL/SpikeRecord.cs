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


namespace Bug3
{
    /// <summary>
    /// This class assists with threshold-based spike detection for the
    /// Spike Scope tool.
    /// </summary>
    public class SpikeRecord
    {
        // private variables
        private double[,] spikeData = new double[Constant.MaxDepth, Constant.SnippetLength];
        private double[] findSpikeBuffer = new double[Constant.SnippetLength + Constant.NeuralSamplesPerFrame * Constant.FramesPerBlock];
        private int spikeDataIndex;
        private int spikeCount;
        private double[] spikeThreshold = new double[Constant.TotalNeuralChannels];
        private bool changedChannel = true;
        private bool audioEnabled = false;
        private int audioVolume = 5;

        private int channel;
        private int numSpikesToShow = 1;

        // properties

        /// <summary>
        /// Amplifier channel
        /// </summary>
        public int Channel
        {
            get
            {
                return channel;
            }
            set
            {
                if (value < 0)
                    channel = 0;
                else if (value > (Constant.TotalNeuralChannels - 1))
                    channel = (Constant.TotalNeuralChannels - 1);
                else
                {
                    channel = value;
                    changedChannel = true;
                }
            }
        }

        /// <summary>
        /// Number of spikes found
        /// </summary>
        public int SpikeCount
        {
            get
            {
                return spikeCount;
            }
        }

        /// <summary>
        /// Number of spikes to display in Spike Scope
        /// </summary>
        public int NumSpikesToShow
        {
            get
            {
                return numSpikesToShow;
            }
            set
            {
                numSpikesToShow = value;
            }
        }

        /// <summary>
        /// Enable audio output of spikes?
        /// </summary>
        public bool AudioEnabled
        {
            get
            {
                return audioEnabled;
            }
            set
            {
                audioEnabled = value;
            }
        }

        /// <summary>
        /// Audio output volume
        /// </summary>
        public int AudioVolume
        {
            get
            {
                return audioVolume;
            }
            set
            {
                audioVolume = value;
            }
        }


        // public methods

        /// <summary>
        /// Set spike detection threshold.
        /// </summary>
        /// <param name="threshold">Amplifier channel.</param>
        public void SetThreshold(float threshold)
        {
            spikeThreshold[channel] = threshold;
        }

        /// <summary>
        /// Get spike detection threshold of current channel.
        /// </summary>
        /// <returns>Spike detection threshold.</returns>
        public double GetThreshold()
        {
            return spikeThreshold[channel];
        }

        /// <summary>
        /// Search for spikes that exceed threshold.
        /// </summary>
        /// <param name="data">Amplifier waveform.</param>
        /// <returns>Number of spikes found in waveform.</returns>
        public int FindSpikes(double[,] data)
        {
            int i, j;
            int numSpikesFound = 0;
            double threshold = spikeThreshold[channel];

            for (i = Constant.SnippetLength; i < Constant.SnippetLength + Constant.NeuralSamplesPerFrame * Constant.FramesPerBlock; i++)
            {
                findSpikeBuffer[i] = data[channel, i - Constant.SnippetLength];
            }

            if (changedChannel)
                i = Constant.SnippetLength + Constant.TriggerDelay;
            else
                i = Constant.TriggerDelay;

            while (i < Constant.NeuralSamplesPerFrame * Constant.FramesPerBlock + Constant.TriggerDelay)
            {
                if (CheckThreshold(findSpikeBuffer[i - 1], findSpikeBuffer[i], threshold))
                {
                    numSpikesFound++;

                    spikeDataIndex++;
                    if (spikeDataIndex == Constant.MaxDepth)
                        spikeDataIndex = 0;

                    spikeCount++;
                    if (spikeCount > Constant.MaxDepth)
                        spikeCount = Constant.MaxDepth;

                    int count = 0;
                    for (j = i - Constant.TriggerDelay; j < i + (Constant.SnippetLength - Constant.TriggerDelay); j++)
                    {
                        spikeData[spikeDataIndex, count++] = findSpikeBuffer[j];
                    }

                    i += Constant.SnippetLength;
                }
                else
                    i++;
            }

            for (i = 0; i < Constant.SnippetLength; i++)
            {
                findSpikeBuffer[i] = data[channel, Constant.NeuralSamplesPerFrame * Constant.FramesPerBlock - Constant.SnippetLength + i];
            }

            changedChannel = false;
            return numSpikesFound;
        }

        /// <summary>
        /// Look for positive or negative crossings of spike detection threshold.
        /// </summary>
        /// <param name="d1">Waveform sample at time t-1.</param>
        /// <param name="d2">Waveform sample at time t.</param>
        /// <param name="threshold">Spike detection threshold.</param>
        /// <returns>Did we find a spike?</returns>
        private bool CheckThreshold(double d1, double d2, double threshold)
        {
            bool foundSpike = false;

            if (threshold < 0)
            {
                if (d1 > threshold & d2 <= threshold)
                    foundSpike = true;
            }
            else
            {
                if (d1 < threshold & d2 >= threshold)
                    foundSpike = true;
            }

            return foundSpike;
        }

        /// <summary>
        /// Clear all detected spikes from memory.
        /// </summary>
        public void ClearSpikes()
        {
            int i, j;

            for (i = 0; i < Constant.MaxDepth; i++)
            {
                for (j = 0; j < Constant.SnippetLength; j++)
                    spikeData[i, j] = 0.0;
            }
            spikeDataIndex = 0;
            spikeCount = 0;
        }

        /// <summary>
        /// Copy spike waveform to array.
        /// </summary>
        /// <param name="dataOut">Array to receive waveform data.</param>
        /// <param name="index">Spike index: 0 = returned newest value; (maxDepth - 1) returns oldest spike.</param>
        public void CopySpikeToArray(double[] dataOut, int index)
        {
            int adjIndex;

            adjIndex = spikeDataIndex - index;
            if (adjIndex < 0)
                adjIndex += Constant.MaxDepth;

            for (int i = 0; i < Constant.SnippetLength; i++)
            {
                dataOut[i] = spikeData[adjIndex, i];
            }
        }

        /// <summary>
        /// SpikeRecord constructor.
        /// </summary>
        public SpikeRecord()
        {
            channel = 0;
            for (int i = 0; i < Constant.TotalNeuralChannels; i++)
                spikeThreshold[i] = 0.0;
            spikeDataIndex = 0;
            spikeCount = 0;
        }
    }

}
