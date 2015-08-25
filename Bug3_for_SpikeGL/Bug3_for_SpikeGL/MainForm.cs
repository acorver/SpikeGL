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
using System.Windows.Forms;
using System.Diagnostics;
using System.IO;
using System.Media;
using System.Threading;
using OpalKelly.FrontPanel;

namespace Bug3
{
    /// <summary>
    /// Main window for Intan Insect Telemetry Receiver GUI
    /// </summary>
    public partial class MainForm : Form
    {
        private BufferedGraphicsContext myContext;
        private BufferedGraphics myBuffer;

        private HammingDecoder myHammingDecoder;
        private USBSource myUSBSource = new USBSource();
        private DataRate dataRate;

        private Queue<USBData> plotQueue = new Queue<USBData>();
        private Queue<USBData> saveQueue = new Queue<USBData>();

        private double[,] neuralData = new double[Constant.TotalNeuralChannels, Constant.NeuralSamplesPerFrame * Constant.FramesPerBlock];
        private double[,] EMGData = new double[Constant.TotalEMGChannels, Constant.FramesPerBlock];
        private double[] EMGDataPrev = new double[Constant.TotalEMGChannels];
        private double[,] auxData = new double[Constant.TotalAuxChannels, Constant.FramesPerBlock];
        private bool[,] TTLData = new bool[Constant.TotalTTLChannels, Constant.FramesPerBlock];

        private bool[] displayNeuralChannel = new bool[Constant.TotalNeuralChannels];
        private bool[] displayEMGChannel = new bool[Constant.TotalEMGChannels];

        private int xSlowPos = 0;
        private int xSlowPosBER = 0;
        private const int XPlotOffset = 64;
        private int noDataCounter = 0;
        private int noDataCounter2 = 0;

        private const int numAvg = 10;
        private int avgStatCounter = 0;
        private double avgBER = 0.0;
        private double logAvgBER = 0.0;
        private double avgPower = 0.0;
        private int totalMissingFrames = 0;
        private int totalFalseFrames = 0;

        // voltage axis scales and labels
        private const int NumYScales = 7;
        private float[] YScaleFactors = new float[NumYScales] { 0.8F, 0.4F, 0.2F, 0.08F, 0.04F, 0.02F, 0.008F };
        private string[] YScaleText = new string[NumYScales] { "50 " + '\u03bc' + "V", "100 " + '\u03bc' + "V", "200 " + '\u03bc' + "V", "500 " + '\u03bc' + "V", "1.0 mV", "2.0 mV", "5.0 mV" };
        private int yScaleIndex = 3;

        private const int NumYScalesEMG = 7;
        private float[] YScaleFactorsEMG = new float[NumYScales] { 80.0F, 40.0F, 20.0F, 8.0F, 4.0F, 2.0F, 0.8F };
        private string[] YScaleTextEMG = new string[NumYScales] { "500 " + '\u03bc' + "V", "1.0 mV", "2.0 mV", "5.0 mV", "10 mV", "20 mV", "50 mV" };
        private int yScaleIndexEMG = 3;

        // time axis scales and labels
        private const int NumXScales = 8;
        private string[] XScaleText = new string[NumXScales] { "25 msec", "50 msec", "100 msec", "200 msec", "400 msec", "800 msec", "1.6 sec", "3.2 sec" };
        private int xScaleIndex = 5;

        private const int XVoltagePlotCorner = 945;
        private const int YVoltagePlotCorner = 310;
        private const int XVoltagePlotSize = 120;
        private const int YVoltagePlotSize = 260;
        private const double MaxVoltagePlot = 5.0;
        private const double MinVoltagePlot = 1.0;
        private const int YellowZoneTop = (int)((MaxVoltagePlot - 3.60) / (MaxVoltagePlot - MinVoltagePlot) * (double)YVoltagePlotSize);
        private const int YellowZone = (int)((1.60 - MinVoltagePlot) / (MaxVoltagePlot - MinVoltagePlot) * (double)YVoltagePlotSize);
        private const int RedZone = (int)((1.35 - MinVoltagePlot) / (MaxVoltagePlot - MinVoltagePlot) * (double)YVoltagePlotSize);

        private const int XBERPlotCorner = 788;
        private const int YBERPlotCorner = 310;
        private const int XBERPlotSize = 120;
        private const int YBERPlotSize = 260;
        private const int YBERIndex1 = (int)(0.25 * (double)YBERPlotSize);
        private const double MaxBER = -1.0;
        private const double MinBER = -5.0;

        private bool readMode = false;
        private bool saveMode = false;
        private bool rawMode = false;

        private BinaryWriter binWriter;
        private FileStream fs;
        private string saveFileName;
        private double fileSize;
        private double fileSaveTime;
        private ushort[] rawData = new ushort[Constant.MaxFrameSize * Constant.FramesPerBlock];

        // pen colors for auxiliary TTL inputs on FPGA board
        private Pen[] TTLPens = new Pen[11] { Pens.Red, Pens.Orange, Pens.Yellow, Pens.Green, Pens.Blue, Pens.Purple,
                                              Pens.Red, Pens.Orange, Pens.Yellow, Pens.Green, Pens.Blue };

        // Spike Scope window
        private MagnifyForm frmMagnifyForm;
        private SpikeRecord mySpikeRecord = new SpikeRecord();
        private bool spikeWindowVisible = false;
        private Point spikeWindowOffset = new Point(980, 150);

        public class ConfigParams
        {
            public bool guiHidden = false;
            public bool consoleData = false;
            public bool autoStart = false;
            public DataRate rate = DataRate.High;
            public int clockEdgePolarity = 0; // rising=0, falling=1
            public int errTolerance = 6; // out of 144?
            public int hpfCutoff = 0; // <= 0 == off, otherwise value is in Hz
            public bool notchFilter = false; // if true, enable software notch filter on incoming data
        }

        public ConfigParams Params = new ConfigParams();

        private void ParseParams()
        {
            Console.WriteLine("Parsing Parameters...");
            string s;

            if (null != (s = Environment.GetEnvironmentVariable("BUG3_SPIKEGL_MODE")))
            {
                Console.WriteLine("SpikeGL mode passed in env: GUI hidden, data will fork to console, will autostart reading data!");
                Params.guiHidden = true;
                Params.consoleData = true;
                Params.autoStart = true;
            }
            if (null != (s = Environment.GetEnvironmentVariable("BUG3_DATA_RATE"))) // low, medium, high
            {
                s = s.ToLower();
                string ratename = "HIGH";
                if (s[0] == 'l') { Params.rate = DataRate.Low; ratename = "LOW"; }
                else if (s[0] == 'm') { Params.rate = DataRate.Medium; ratename = "MEDIUM"; }
                else Params.rate = DataRate.High;
                Console.WriteLine("Data rate set to '" + ratename + "'");
            }
            if (null != (s = Environment.GetEnvironmentVariable("BUG3_CHANGES_ON_CLOCK"))) // rising/falling
            {
                int pol = -1;
                try { pol = Convert.ToInt32(s, 10); }
                catch { pol = -1; }
                if (pol > -1)
                {
                    Params.clockEdgePolarity = pol;
                    string name = pol == 0 ? "RISING" : "FALLING";
                    Console.WriteLine("Changes on clock set to: " + name);
                }
            }
            if (null != (s = Environment.GetEnvironmentVariable("BUG3_ERR_TOLERANCE"))) // default 6 out of 144
            {
                int tol = -1;
                try { tol = Convert.ToInt32(s, 10); }
                catch { tol = -1; }
                if (tol > 0)
                {
                    Console.WriteLine("Error tolerance={0:D}", tol);
                    Params.errTolerance = tol;
                }
            }
            if (null != (s = Environment.GetEnvironmentVariable("BUG3_HIGHPASS_FILTER_CUTOFF"))) // in Hz
            {
                int filt = -1;
                try { filt = Convert.ToInt32(s, 10); }
                catch { filt = -1; }
                if (filt > 0)
                {
                    Console.WriteLine("Highpass filter cutoff={0:D}", filt);
                    Params.hpfCutoff = filt;
                }
            }
            if (null != (s = Environment.GetEnvironmentVariable("BUG3_60HZ_NOTCH_FILTER"))) // if set, enable
            {
                Console.WriteLine("Software notch filter enabled");
                Params.notchFilter = true;

            }
        }

        public MainForm()
        {
            InitializeComponent();
        }

        private void MainForm_Load(object sender, EventArgs e)
        {

            ParseParams();

            myContext = new BufferedGraphicsContext();
            myBuffer = myContext.Allocate(this.CreateGraphics(), this.DisplayRectangle);
            myBuffer.Graphics.Clear(SystemColors.Control);

            // Draw amplifier vertical scale bars
            myBuffer.Graphics.DrawLine(Pens.Black, 715, 220, 715, 259);
            myBuffer.Graphics.DrawLine(Pens.Black, 716, 220, 716, 259);
            myBuffer.Graphics.DrawLine(Pens.Black, 715, 540, 715, 579);
            myBuffer.Graphics.DrawLine(Pens.Black, 716, 540, 716, 579);

            // Draw BER/WER plot background
            Rectangle rectA = new Rectangle(XBERPlotCorner, YBERPlotCorner, XBERPlotSize, YBERPlotSize);
            myBuffer.Graphics.FillRectangle(Brushes.LightGray, rectA);
            Rectangle rectB = new Rectangle(XBERPlotCorner, YBERPlotCorner + YBERIndex1, XBERPlotSize, (int)((double)YBERPlotSize / 4.0));
            myBuffer.Graphics.FillRectangle(Brushes.WhiteSmoke, rectB);
            Rectangle rectC = new Rectangle(XBERPlotCorner, YBERPlotCorner + YBERPlotSize - YBERIndex1, XBERPlotSize, (int)((double)YBERPlotSize / 4.0));
            myBuffer.Graphics.FillRectangle(Brushes.WhiteSmoke, rectC);
            Rectangle rectD = new Rectangle(XBERPlotCorner, YBERPlotCorner + YBERPlotSize, XBERPlotSize, 8);
            myBuffer.Graphics.FillRectangle(SystemBrushes.Control, rectD);

            // Draw unregulated voltage plot background
            Rectangle rect1 = new Rectangle(XVoltagePlotCorner, YVoltagePlotCorner, XVoltagePlotSize, YVoltagePlotSize);
            myBuffer.Graphics.FillRectangle(Brushes.LightGreen, rect1);
            Rectangle rect2 = new Rectangle(XVoltagePlotCorner, YVoltagePlotCorner, XVoltagePlotSize, YellowZoneTop);
            myBuffer.Graphics.FillRectangle(Brushes.LightYellow, rect2);
            Rectangle rect3 = new Rectangle(XVoltagePlotCorner, YVoltagePlotCorner + YVoltagePlotSize - YellowZone, XVoltagePlotSize, YellowZone);
            myBuffer.Graphics.FillRectangle(Brushes.LightYellow, rect3);
            Rectangle rect4 = new Rectangle(XVoltagePlotCorner, YVoltagePlotCorner + YVoltagePlotSize - RedZone, XVoltagePlotSize, RedZone);
            myBuffer.Graphics.FillRectangle(Brushes.LightSalmon, rect4);

            lblYScale.Text = YScaleText[yScaleIndex];
            lblEMGYScale.Text = YScaleTextEMG[yScaleIndexEMG];
            lblXScale.Text = XScaleText[xScaleIndex];

            myBuffer.Render();

            myHammingDecoder = new HammingDecoder();

            string boardName = "";
            int boardID = 0, boardVersion = 0;

            try
            {
                boardName = myUSBSource.Open(dataRate, out boardID, out boardVersion);
            }
            catch
            {
                this.Text = "Intan Technologies Insect Telemetry Receiver (SIMULATED DATA: Connect board and restart program to record live data)";
                myUSBSource.SynthDataMode = true;
                tmrSynthData.Enabled = true;
                yScaleIndex = 2;
                lblYScale.Text = YScaleText[yScaleIndex];
                yScaleIndexEMG = 2;
                lblEMGYScale.Text = YScaleTextEMG[yScaleIndexEMG];

                if (Params.guiHidden)
                {
                    Console.WriteLine("USRMSG: " + this.Text);
                }
                else if (MessageBox.Show("Intan Technologies USB device not found.  Click OK to run application with synthesized neural data for demonstration purposes.\n\nTo use the USB FPGA board click Cancel, load correct drivers and/or connect device to USB port, then restart application.",
                  "Intan USB Device Not Found", MessageBoxButtons.OKCancel, MessageBoxIcon.Information) == DialogResult.Cancel)
                    this.Close();
            }

            if (myUSBSource.SynthDataMode)
            {
                lblStatus.Text = "No USB board connected.  Ready to run with synthesized neural data.";
            }
            else
            {
                lblStatus.Text = String.Concat("Connected to " + boardName + ", type " + boardID + ", version " + boardVersion + ".");
            }

            // apply params to UI, even in SpikeGL mode the UI sate affects the acquisition...
            cmbDataRate.SelectedIndex = (int)Params.rate;
            cmbClockEdge.SelectedIndex = ((int)Params.clockEdgePolarity == 0) ? 0 : 1;
            numFrameErrorTolerance.Value = Params.errTolerance;
            if (Params.hpfCutoff > 0)
            {
                chkEnableHPF.Checked = true;
                txtHPF.Text = Convert.ToString(Params.hpfCutoff);
            }
            if (Params.notchFilter)
            {
                btnNotchFilterDisable.Checked = false;
                btnNotchFilter60Hz.Checked = true;
            }
            if (Params.guiHidden)
            {
                this.WindowState = System.Windows.Forms.FormWindowState.Minimized;
            }
            if (Params.consoleData) doStdinHandler();
        }

        private void doStdinHandler()
        {
            BackgroundWorker bw = new BackgroundWorker();

            // this allows our worker to report progress during work
            bw.WorkerReportsProgress = true;

            // what to do in the background thread
            bw.DoWork += new DoWorkEventHandler(
            delegate(object o, DoWorkEventArgs args)
            {
                BackgroundWorker b = o as BackgroundWorker;

                // process stdin
                string line = null;
                try
                {
                    while ((line = Console.ReadLine()) != null)
                    {
                        if (line.Length <= 0) continue;
                        line = line.ToLower();
                        // handle turn on/off software notch filter
                        if (line.StartsWith("snf="))
                        {
                            int snf = -1;
                            try { snf = Convert.ToInt32(line.Substring(4), System.Globalization.CultureInfo.InvariantCulture); }
                            catch { snf = -1; }
                            if (snf >= 0) b.ReportProgress(1, snf);
                            else b.ReportProgress(0, "PASS A POSITIVE INT: " + line);
                        }
                        // handle turn on/off high pass filter
                        else if (line.StartsWith("hpf="))
                        {
                            int hpf = -1;
                            try { hpf = Convert.ToInt32(line.Substring(4), System.Globalization.CultureInfo.InvariantCulture); }
                            catch { hpf = -1; }
                            if (hpf >= 0) b.ReportProgress(2, hpf);
                            else b.ReportProgress(0, "PASS A POSITIVE INT: " + line);
                        }
                        else if (line.StartsWith("quit") || line.StartsWith("exit"))
                            return;
                        else
                            b.ReportProgress(0, "UNRECOGNIZED: '" + line + "'");
                    }
                }
                catch
                {
                    // ...
                }

            });

            // what to do when progress changed (update the progress bar for example)
            bw.ProgressChanged += new ProgressChangedEventHandler(
            delegate(object o, ProgressChangedEventArgs args)
            {
                if (args.ProgressPercentage <= 0)
                    Console.WriteLine(args.UserState as string);
                else if (args.ProgressPercentage == 1) //snf
                {
                    if ((int)args.UserState != 0)
                    {
                        btnNotchFilterDisable.Checked = false;
                        btnNotchFilter60Hz.Checked = true;
                    }
                    else
                    {
                        btnNotchFilter60Hz.Checked = false;
                        btnNotchFilterDisable.Checked = true;
                    }
                    //Console.WriteLine("USRMSG: snf set to: " + (int)args.UserState);
                }
                else if (args.ProgressPercentage == 2) // hpf
                {
                    int hpf = (int)args.UserState;
                    if (hpf <= 0)
                    { // off
                        chkEnableHPF.Checked = false;
                    }
                    else
                    { // on
                        chkEnableHPF.Checked = true;
                        txtHPF.Text = Convert.ToString(hpf);
                    }
                    //Console.WriteLine("USRMSG: hpf set to: " + hpf);
                }
            });

            // what to do when worker completes its task (notify the user)
            bw.RunWorkerCompleted += new RunWorkerCompletedEventHandler(
            delegate(object o, RunWorkerCompletedEventArgs args)
            {
                Console.WriteLine("QUITTING...");
                Application.Exit();
            });

            bw.RunWorkerAsync();
        }

        private void MainForm_Shown(object sender, EventArgs e)
        {
            if (Params.autoStart) this.btnStart.PerformClick();
            if (Params.guiHidden) this.Visible = false;
        }

        private void MainForm_Paint(object sender, PaintEventArgs e)
        {
            myBuffer.Render();
        }

        private void MainForm_FormClosed(object sender, FormClosedEventArgs e)
        {
            myUSBSource.LEDControl(0);  // turn off LEDs
            myUSBSource.Stop();

            if (saveMode == true)
            {
                binWriter.Flush();
                binWriter.Close();
                fs.Close();
                saveMode = false;
            }

            myBuffer.Dispose();
            myContext.Dispose();
        }

        private void MainForm_MouseClick(object sender, MouseEventArgs e)
        {
            if (e.X > 64 && e.X < 710 && e.Y > 10 && e.Y < 470)
            {
                if (spikeWindowVisible)
                {
                    double y = (double)e.Y;
                    y = (58.0 - y) / -40.0;
                    y = Math.Round(y);
                    int ch = (int)y;
                    if (ch < 0)
                        ch = 0;
                    else if (ch > (Constant.TotalNeuralChannels - 1))
                        ch = (Constant.TotalNeuralChannels - 1);
                    mySpikeRecord.Channel = ch;
                    frmMagnifyForm.ChangeChannel(ch);
                }
            }
        }

        private void doPlot(int numPagesLeftInRAM)
        {
            if (plotQueue.Count() == 0) return;

            int barLength;
            double msecInRAM;

            Rectangle barGraph = new Rectangle(765, 84, 80, 5);
            myBuffer.Graphics.FillRectangle(SystemBrushes.Control, barGraph);
            msecInRAM = Constant.MsecPerRAMPage(dataRate) * (double)numPagesLeftInRAM;
            barLength = (int)(80.0 * msecInRAM / 1000.0);
            if (barLength > 80)
            {
                barLength = 80;
                barGraph.Width = barLength;
                myBuffer.Graphics.FillRectangle(Brushes.Red, barGraph);
            }
            else if (barLength > 40)
            {
                barGraph.Width = barLength;
                myBuffer.Graphics.FillRectangle(Brushes.Yellow, barGraph);
            }
            else
            {
                barGraph.Width = barLength;
                myBuffer.Graphics.FillRectangle(Brushes.Green, barGraph);
            }

            USBData plotData;

            plotData = plotQueue.Last();
            if (plotQueue.Count != 1)
                Debug.WriteLine("Plot falling behind!");
            plotData.CopyNeuralDataToArray(neuralData);
            plotData.CopyEMGDataToArray(EMGData);
            plotData.CopyTTLDataToArray(TTLData);

            if (spikeWindowVisible)
            {
                if (mySpikeRecord.FindSpikes(neuralData) > 0)
                {
                    frmMagnifyForm.DrawSpikes();
                }
            }

            if (saveMode)
            {
                lblStatus.Text = String.Concat("Saving data to file ", saveFileName, ".  Estimated file size = ", fileSize.ToString("F01"), " MB.");
            }

            int channel, x;
            float yOffset;
            Rectangle rectBounds;

            // Which channels should we plot on the screen?
            displayNeuralChannel[0] = chkNeural1.Checked;
            displayNeuralChannel[1] = chkNeural2.Checked;
            displayNeuralChannel[2] = chkNeural3.Checked;
            displayNeuralChannel[3] = chkNeural4.Checked;
            displayNeuralChannel[4] = chkNeural5.Checked;
            displayNeuralChannel[5] = chkNeural6.Checked;
            displayNeuralChannel[6] = chkNeural7.Checked;
            displayNeuralChannel[7] = chkNeural8.Checked;
            displayNeuralChannel[8] = chkNeural9.Checked;
            displayNeuralChannel[9] = chkNeural10.Checked;

            displayEMGChannel[0] = chkEMG1.Checked;
            displayEMGChannel[1] = chkEMG2.Checked;
            displayEMGChannel[2] = chkEMG3.Checked;
            displayEMGChannel[3] = chkEMG4.Checked;

            float yNeuralScale = YScaleFactors[yScaleIndex];
            float yEMGScale = YScaleFactorsEMG[yScaleIndexEMG];

            if (xScaleIndex == 0)
            {
                rectBounds = new Rectangle(XPlotOffset, 0, 641, 800);
                myBuffer.Graphics.FillRectangle(SystemBrushes.Control, rectBounds);

                // Plot neural amplifier waveforms on screen
                for (channel = 0; channel < Constant.TotalNeuralChannels; channel++)
                {
                    if (displayNeuralChannel[channel] == true)
                    {
                        yOffset = (float)channel * -40.0F + 582.0F;
                        for (x = 1; x < Constant.NeuralSamplesPerFrame * Constant.FramesPerBlock; x++)
                        {
                            if (spikeWindowVisible == true && channel == mySpikeRecord.Channel)
                            {
                                myBuffer.Graphics.DrawLine(Pens.Blue, (float)x + (float)XPlotOffset,
                                    640.0F - (yOffset + yNeuralScale * (float)neuralData[channel, x - 1]), (float)x + (float)XPlotOffset + 1.0F,
                                    640.0F - (yOffset + yNeuralScale * (float)neuralData[channel, x]));
                            }
                            else
                            {
                                myBuffer.Graphics.DrawLine(Pens.Black, (float)x + (float)XPlotOffset,
                                    640.0F - (yOffset + yNeuralScale * (float)neuralData[channel, x - 1]), (float)x + (float)XPlotOffset + 1.0F,
                                    640.0F - (yOffset + yNeuralScale * (float)neuralData[channel, x]));
                            }
                        }
                    }
                }

                // Plot EMG amplifier waveforms on screen
                for (channel = 0; channel < Constant.TotalEMGChannels; channel++)
                {
                    if (displayEMGChannel[channel] == true)
                    {
                        yOffset = (float)(channel + 11) * -40.0F + 582.0F;
                        for (x = 1; x < Constant.FramesPerBlock; x++)
                        {
                            myBuffer.Graphics.DrawLine(Pens.Black, (float)(16 * x) + (float)XPlotOffset,
                                640.0F - (yOffset + yEMGScale * (float)EMGData[channel, x - 1]), (float)(16 * x) + (float)XPlotOffset + 16.0F,
                                640.0F - (yOffset + yEMGScale * (float)EMGData[channel, x]));
                        }
                    }
                }

                // Plot TTL input rasters on screen
                for (channel = 0; channel < Constant.TotalTTLChannels; channel++)
                {
                    yOffset = 688.0F + 3.0F * (float)channel;
                    for (x = 0; x < Constant.FramesPerBlock; x++)
                    {
                        if (TTLData[channel, x] == true)
                        {
                            myBuffer.Graphics.DrawLine(TTLPens[channel], (float)(16 * x) + (float)XPlotOffset,
                                    yOffset, (float)(16 * x) + (float)XPlotOffset + 15.0F, yOffset);
                        }
                    }
                }
            }
            else
            {
                int drawWidth, neuralChunk, EMGChunk, EMGChunkInv, xSlowPos1;
                bool TTLHigh;

                switch (xScaleIndex)
                {
                    case 1:
                        drawWidth = 320;
                        break;
                    case 2:
                        drawWidth = 160;
                        break;
                    case 3:
                        drawWidth = 80;
                        break;
                    case 4:
                        drawWidth = 40;
                        break;
                    case 5:
                        drawWidth = 20;
                        break;
                    case 6:
                        drawWidth = 10;
                        break;
                    case 7:
                        drawWidth = 5;
                        break;
                    default:
                        drawWidth = 5;
                        break;
                }
                neuralChunk = (Constant.NeuralSamplesPerFrame * Constant.FramesPerBlock) / drawWidth;
                EMGChunk = Constant.FramesPerBlock / drawWidth;
                EMGChunkInv = drawWidth / Constant.FramesPerBlock;

                float maxValue, minValue;

                rectBounds = new Rectangle(xSlowPos + XPlotOffset, 0, drawWidth, 800);
                myBuffer.Graphics.FillRectangle(SystemBrushes.Control, rectBounds);

                if (xSlowPos == 0)
                {
                    rectBounds = new Rectangle(640 + XPlotOffset, 0, 1, 800);
                    myBuffer.Graphics.FillRectangle(SystemBrushes.Control, rectBounds);
                }

                xSlowPos1 = xSlowPos;
                for (int x1 = 0; x1 < drawWidth; x1++)
                {
                    // Plot neural amplifier waveforms on screen
                    for (channel = 0; channel < Constant.TotalNeuralChannels; channel++)
                    {
                        if (displayNeuralChannel[channel] == true)
                        {
                            maxValue = -999999.0F;
                            minValue = 999999.0F;
                            for (int x2 = 0; x2 < neuralChunk; x2++)
                            {
                                if (neuralData[channel, neuralChunk * x1 + x2] > maxValue)
                                {
                                    maxValue = (float)neuralData[channel, neuralChunk * x1 + x2];
                                }
                                if (neuralData[channel, neuralChunk * x1 + x2] < minValue)
                                {
                                    minValue = (float)neuralData[channel, neuralChunk * x1 + x2];
                                }
                            }

                            yOffset = (float)channel * -40.0F + 582.0F;

                            // DrawLine will not draw anything if the specified line is too short, so we must set a
                            // lower bound on its length.
                            if (yNeuralScale * (maxValue - minValue) < 0.10F)
                            {
                                maxValue += 0.1F / yNeuralScale;
                            }

                            if (spikeWindowVisible == true && channel == mySpikeRecord.Channel)
                            {
                                myBuffer.Graphics.DrawLine(Pens.Blue, (float)xSlowPos + (float)XPlotOffset,
                                     640.0F - (yOffset + yNeuralScale * minValue), (float)xSlowPos + (float)XPlotOffset,
                                     640.0F - (yOffset + yNeuralScale * maxValue));
                            }
                            else
                            {
                                myBuffer.Graphics.DrawLine(Pens.Black, (float)xSlowPos + (float)XPlotOffset,
                                     640.0F - (yOffset + yNeuralScale * minValue), (float)xSlowPos + (float)XPlotOffset,
                                     640.0F - (yOffset + yNeuralScale * maxValue));
                            }
                        }
                    }

                    // Plot EMG amplifier waveforms on screen
                    for (channel = 0; channel < Constant.TotalEMGChannels; channel++)
                    {
                        if (displayEMGChannel[channel] == true)
                        {
                            yOffset = (float)(channel + 11) * -40.0F + 582.0F;

                            if (EMGChunk > 1)
                            {
                                maxValue = -999999.0F;
                                minValue = 999999.0F;
                                for (int x2 = 0; x2 < EMGChunk; x2++)
                                {
                                    if (EMGData[channel, EMGChunk * x1 + x2] > maxValue)
                                    {
                                        maxValue = (float)EMGData[channel, EMGChunk * x1 + x2];
                                    }
                                    if (EMGData[channel, EMGChunk * x1 + x2] < minValue)
                                    {
                                        minValue = (float)EMGData[channel, EMGChunk * x1 + x2];
                                    }
                                }

                                // DrawLine will not draw anything if the specified line is too short, so we must set a
                                // lower bound on its length.
                                if (yEMGScale * (maxValue - minValue) < 0.10F)
                                {
                                    maxValue += 0.1F / yEMGScale;
                                }

                                myBuffer.Graphics.DrawLine(Pens.Black, (float)xSlowPos + (float)XPlotOffset,
                                     640.0F - (yOffset + yEMGScale * minValue), (float)xSlowPos + (float)XPlotOffset,
                                     640.0F - (yOffset + yEMGScale * maxValue));

                            }
                            else
                            {
                                if (x1 == 0)
                                {
                                    myBuffer.Graphics.DrawLine(Pens.Black, (float)xSlowPos1 + (float)XPlotOffset,
                                        640.0F - (yOffset + yEMGScale * (float)EMGDataPrev[channel]), (float)xSlowPos1 + (float)EMGChunkInv + (float)XPlotOffset,
                                        640.0F - (yOffset + yEMGScale * (float)EMGData[channel, 0]));

                                }
                                else if (x1 > 0 && x1 < Constant.FramesPerBlock)
                                {
                                    myBuffer.Graphics.DrawLine(Pens.Black, (float)xSlowPos1 + (float)(EMGChunkInv * x1) + (float)XPlotOffset,
                                        640.0F - (yOffset + yEMGScale * (float)EMGData[channel, x1 - 1]), (float)xSlowPos1 + (float)(EMGChunkInv * (x1 + 1)) + (float)XPlotOffset,
                                        640.0F - (yOffset + yEMGScale * (float)EMGData[channel, x1]));
                                }
                            }
                        }
                    }

                    // Plot TTL input rasters on screen
                    for (channel = 0; channel < Constant.TotalTTLChannels; channel++)
                    {
                        yOffset = 688.0F + 3.0F * (float)channel;
                        TTLHigh = false;
                        if (EMGChunk > 1)
                        {
                            for (int x2 = 0; x2 < EMGChunk; x2++)
                            {
                                if (TTLData[channel, EMGChunk * x1 + x2] == true)
                                {
                                    TTLHigh = true;
                                    x2 = EMGChunk;
                                }
                            }
                            if (TTLHigh)
                            {
                                myBuffer.Graphics.DrawLine(TTLPens[channel], (float)xSlowPos + (float)XPlotOffset,
                                    yOffset, (float)xSlowPos + (float)XPlotOffset, yOffset + 0.1F);
                            }
                        }
                        else
                        {
                            if (x1 < Constant.FramesPerBlock)
                            {
                                if (TTLData[channel, x1] == true)
                                {
                                    myBuffer.Graphics.DrawLine(TTLPens[channel], (float)xSlowPos1 + (float)(EMGChunkInv * x1) + (float)XPlotOffset,
                                        yOffset, (float)xSlowPos1 + (float)(EMGChunkInv * (x1 + 1)) + (float)XPlotOffset, yOffset);
                                }
                            }
                        }

                    }
                    xSlowPos++;
                }

                if (xScaleIndex > 4)
                    myBuffer.Graphics.DrawLine(Pens.Red, xSlowPos + XPlotOffset, 0, xSlowPos + XPlotOffset, 800);

                if (xSlowPos >= 640)
                    xSlowPos = 0;
            }

            for (channel = 0; channel < Constant.TotalEMGChannels; channel++)
            {
                EMGDataPrev[channel] = EMGData[channel, Constant.FramesPerBlock - 1];
            }

            // Display chip ID number

            int ID = plotData.GetChipID();
            if (ID == -1)
                lblChipID.Text = "?";
            else
                lblChipID.Text = Convert.ToString(ID);

            // Plot unregulated voltage level

            Rectangle rect1 = new Rectangle(xSlowPosBER + XVoltagePlotCorner, YVoltagePlotCorner, 1, YVoltagePlotSize);
            myBuffer.Graphics.FillRectangle(Brushes.LightGreen, rect1);
            Rectangle rect2 = new Rectangle(xSlowPosBER + XVoltagePlotCorner, YVoltagePlotCorner, 1, YellowZoneTop);
            myBuffer.Graphics.FillRectangle(Brushes.LightYellow, rect2);
            Rectangle rect3 = new Rectangle(xSlowPosBER + XVoltagePlotCorner, YVoltagePlotCorner + YVoltagePlotSize - YellowZone, 1, YellowZone);
            myBuffer.Graphics.FillRectangle(Brushes.LightYellow, rect3);
            Rectangle rect4 = new Rectangle(xSlowPosBER + XVoltagePlotCorner, YVoltagePlotCorner + YVoltagePlotSize - RedZone, 1, RedZone);
            myBuffer.Graphics.FillRectangle(Brushes.LightSalmon, rect4);
            double avgVunreg = 0.0;

            plotData.CopyAuxDataToArray(auxData);
            for (int i = 0; i < Constant.FramesPerBlock; i++)
            {
                avgVunreg += auxData[1, i];
            }
            avgVunreg /= (double)Constant.FramesPerBlock;
            if (avgVunreg > MaxVoltagePlot)
                avgVunreg = MaxVoltagePlot;
            else if (avgVunreg < MinVoltagePlot)
                avgVunreg = MinVoltagePlot;

            float yVunreg = (float)(YVoltagePlotCorner + YVoltagePlotSize) - (float)((double)YVoltagePlotSize * (avgVunreg - MinVoltagePlot) / (MaxVoltagePlot - MinVoltagePlot));
            if (yVunreg > (YVoltagePlotCorner + YVoltagePlotSize - 1.0F))
                yVunreg = (YVoltagePlotCorner + YVoltagePlotSize - 1.0F);

            myBuffer.Graphics.DrawLine(Pens.Black, (float)(xSlowPosBER + XVoltagePlotCorner), yVunreg,
                (float)(xSlowPosBER + XVoltagePlotCorner) + 0.1F, yVunreg + 0.1F);

            // Plot BER and WER

            int missingFrameCount = plotData.GetMissingFrameCount();
            int falseFrameCount = plotData.GetFalseFrameCount();
            // lblStatus.Text = String.Concat("Missing frames count = " + missingFrameCount.ToString());

            if (missingFrameCount == 0 && falseFrameCount == 0)
            {
                Rectangle rectA = new Rectangle(xSlowPosBER + XBERPlotCorner, YBERPlotCorner, 1, YBERPlotSize);
                myBuffer.Graphics.FillRectangle(Brushes.LightGray, rectA);
                Rectangle rectB = new Rectangle(xSlowPosBER + XBERPlotCorner, YBERPlotCorner + YBERIndex1, 1, (int)((double)YBERPlotSize / 4.0));
                myBuffer.Graphics.FillRectangle(Brushes.WhiteSmoke, rectB);
                Rectangle rectC = new Rectangle(xSlowPosBER + XBERPlotCorner, YBERPlotCorner + YBERPlotSize - YBERIndex1, 1, (int)((double)YBERPlotSize / 4.0));
                myBuffer.Graphics.FillRectangle(Brushes.WhiteSmoke, rectC);
                Rectangle rectD = new Rectangle(xSlowPosBER + XBERPlotCorner, YBERPlotCorner + YBERPlotSize, 1, 8);
                myBuffer.Graphics.FillRectangle(SystemBrushes.Control, rectD);
            }
            else if (missingFrameCount > 0)
            {
                Rectangle rectA = new Rectangle(xSlowPosBER + XBERPlotCorner, YBERPlotCorner, 1, YBERPlotSize);
                myBuffer.Graphics.FillRectangle(Brushes.LightBlue, rectA);
                Rectangle rectB = new Rectangle(xSlowPosBER + XBERPlotCorner, YBERPlotCorner + YBERIndex1, 1, (int)((double)YBERPlotSize / 4.0));
                myBuffer.Graphics.FillRectangle(Brushes.PowderBlue, rectB);
                Rectangle rectC = new Rectangle(xSlowPosBER + XBERPlotCorner, YBERPlotCorner + YBERPlotSize - YBERIndex1, 1, (int)((double)YBERPlotSize / 4.0));
                myBuffer.Graphics.FillRectangle(Brushes.PowderBlue, rectC);
                Rectangle rectD = new Rectangle(xSlowPosBER + XBERPlotCorner, YBERPlotCorner + YBERPlotSize, 1, 8);
                myBuffer.Graphics.FillRectangle(SystemBrushes.Control, rectD);
            }
            else if (falseFrameCount > 0)
            {
                Rectangle rectA = new Rectangle(xSlowPosBER + XBERPlotCorner, YBERPlotCorner, 1, YBERPlotSize);
                myBuffer.Graphics.FillRectangle(Brushes.LightPink, rectA);
                Rectangle rectB = new Rectangle(xSlowPosBER + XBERPlotCorner, YBERPlotCorner + YBERIndex1, 1, (int)((double)YBERPlotSize / 4.0));
                myBuffer.Graphics.FillRectangle(Brushes.Pink, rectB);
                Rectangle rectC = new Rectangle(xSlowPosBER + XBERPlotCorner, YBERPlotCorner + YBERPlotSize - YBERIndex1, 1, (int)((double)YBERPlotSize / 4.0));
                myBuffer.Graphics.FillRectangle(Brushes.Pink, rectC);
                Rectangle rectD = new Rectangle(xSlowPosBER + XBERPlotCorner, YBERPlotCorner + YBERPlotSize, 1, 8);
                myBuffer.Graphics.FillRectangle(SystemBrushes.Control, rectD);
            }

            double BER, WER, logBER, logWER;

            BER = plotData.GetBER();
            WER = plotData.GetWER();



            if (BER > 0.0)
            {
                logBER = Math.Log10(BER);
                if (logBER > MaxBER)
                    logBER = MaxBER;
                else if (logBER < MinBER)
                    logBER = MinBER;

                float yBER = (float)(YBERPlotCorner + YBERPlotSize) - (float)((double)YBERPlotSize * (logBER - MinBER) / (MaxBER - MinBER));
                if (yBER > (YBERPlotCorner + YBERPlotSize - 1.0F))
                    yBER = (YBERPlotCorner + YBERPlotSize - 1.0F);

                myBuffer.Graphics.DrawLine(Pens.Black, (float)(xSlowPosBER + XBERPlotCorner), yBER,
                    (float)(xSlowPosBER + XBERPlotCorner) + 0.1F, yBER + 0.1F);
            }
            else
            {
                logBER = -100.0;
                myBuffer.Graphics.DrawLine(Pens.Black, (float)(xSlowPosBER + XBERPlotCorner), (float)(YBERPlotCorner + YBERPlotSize + 3),
                    (float)(xSlowPosBER + XBERPlotCorner) + 0.1F, (float)(YBERPlotCorner + YBERPlotSize + 3) + 0.1F);
            }

            if (WER > 0.0)
            {
                logWER = Math.Log10(WER);
                if (logWER > MaxBER)
                    logWER = MaxBER;
                else if (logWER < MinBER)
                    logWER = MinBER;

                float yWER = (float)(YBERPlotCorner + YBERPlotSize) - (float)((double)YBERPlotSize * (logWER - MinBER) / (MaxBER - MinBER));
                if (yWER > (YBERPlotCorner + YBERPlotSize - 1.0F))
                    yWER = (YBERPlotCorner + YBERPlotSize - 1.0F);

                myBuffer.Graphics.DrawLine(Pens.Red, (float)(xSlowPosBER + XBERPlotCorner), yWER,
                    (float)(xSlowPosBER + XBERPlotCorner) + 0.1F, yWER + 0.1F);
            }
            else
            {
                logWER = -100.0;
                myBuffer.Graphics.DrawLine(Pens.Red, (float)(xSlowPosBER + XBERPlotCorner), (float)(YBERPlotCorner + YBERPlotSize + 5),
                    (float)(xSlowPosBER + XBERPlotCorner) + 0.1F, (float)(YBERPlotCorner + YBERPlotSize + 5) + 0.1F);
            }


            // Display BER on FPGA board LED bar graph

            int LEDcntl = 0;
            if (logBER < -1.5) LEDcntl += 1;
            if (logBER < -2.0) LEDcntl += 2;
            if (logBER < -2.5) LEDcntl += 4;
            if (logBER < -3.0) LEDcntl += 8;
            if (logBER < -3.5) LEDcntl += 16;
            if (logBER < -4.0) LEDcntl += 32;
            if (BER == 0.0) LEDcntl += 64;

            myUSBSource.LEDControl(LEDcntl);

            lblFrameFound.Text = "Yes";
            lblFrameFound.ForeColor = Color.Green;

            xSlowPosBER++;
            if (xSlowPosBER >= 120)
                xSlowPosBER = 0;

            myBuffer.Render();
            noDataCounter = 0;

            // Display averaged statistics in status bar

            if (avgStatCounter == numAvg)
            {
                avgStatCounter = 0;
                avgBER = avgBER / numAvg;
                logAvgBER = Math.Log10(avgBER);
                avgPower = avgPower / numAvg;

                lblStatus.Text = String.Concat("log10(bit error rate): " + logAvgBER.ToString("F01") +
                   "          recovered voltage: " + avgPower.ToString("F01") + " V " +
                   "          missing frames: " + totalMissingFrames +
                   "          false frames: " + totalFalseFrames);

                avgBER = 0.0;
                avgPower = 0.0;
                totalMissingFrames = 0;
                totalFalseFrames = 0;
            }
            else
            {
                avgBER += BER;
                avgPower += avgVunreg;
                totalFalseFrames += falseFrameCount;
                totalMissingFrames += missingFrameCount;
                avgStatCounter++;
            }

        }

        private static System.Int64 t0 = 0;

        private void doConsoleDataOutput(int numPagesLeftInRAM)
        {
            if (t0 == 0) t0 = System.DateTime.Now.Ticks;

            Console.WriteLine("---> Console data out called at time: " + (long)((System.DateTime.Now.Ticks - t0) / 1e4) + "ms plotQueue.Count=" + plotQueue.Count + " numPagesLeftInRAM=" + numPagesLeftInRAM);
            foreach (USBData data in plotQueue)
            {
                UInt16[,] array = null;
                StringWriter writer = new StringWriter(System.Globalization.CultureInfo.InvariantCulture);

                array = data.neuralData16;
                for (int i = 0; i < Constant.TotalNeuralChannels; ++i)
                {
                    writer.Write("NEU_{0:D}{{", i);
                    for (int j = 0; j < Constant.NeuralSamplesPerFrame * Constant.FramesPerBlock; ++j)
                    {
                        if (j > 0) writer.Write(",");
                        writer.Write(array[i, j]);
                    }
                    writer.WriteLine("}");
                }
                array = data.EMGData16;
                for (int i = 0; i < Constant.TotalEMGChannels; ++i)
                {
                    writer.Write("EMG_{0:D}{{", i);
                    for (int j = 0; j < Constant.FramesPerBlock; ++j)
                    {
                        if (j > 0) writer.Write(",");
                        writer.Write(array[i, j]);
                    }
                    writer.WriteLine("}");
                }
                array = data.auxData16;
                for (int i = 0; i < Constant.TotalAuxChannels; ++i)
                {
                    writer.Write("AUX_{0:D}{{", i);
                    for (int j = 0; j < Constant.FramesPerBlock; ++j)
                    {
                        if (j > 0) writer.Write(",");
                        writer.Write(array[i, j]);
                    }
                    writer.WriteLine("}");
                }
                bool[,] ttls = null;
                data.CopyTTLDataToArray(ref ttls);
                for (int i = 0; i < Constant.TotalTTLChannels; ++i)
                {
                    writer.Write("TTL_{0:D}{{", i);
                    for (int j = 0; j < Constant.FramesPerBlock; ++j)
                    {
                        if (j > 0) writer.Write(",");
                        writer.Write((int)(ttls[i, j] ? 1 : 0));
                    }
                    writer.WriteLine("}");
                }

                UInt16[] shortarr = null;

                shortarr = data.chipID;
                writer.Write("CHIPID{");
                for (int i = 0; i < Constant.FramesPerBlock; ++i)
                {
                    if (i != 0) writer.Write(",");
                    writer.Write(shortarr[i]);
                }
                writer.WriteLine("}");

                int[] intarr = null;

                intarr = data.chipFrameCounter;
                writer.Write("CHIP_FC{");
                for (int i = 0; i < Constant.FramesPerBlock; ++i)
                {
                    if (i != 0) writer.Write(",");
                    writer.Write(intarr[i]);
                }
                writer.WriteLine("}");

                shortarr = data.frameMarkerCorrelation;
                writer.Write("FRAME_MARKER_COR{");
                for (int i = 0; i < Constant.FramesPerBlock; ++i)
                {
                    if (i != 0) writer.Write(",");
                    writer.Write(shortarr[i]);
                }
                writer.WriteLine("}");

                intarr = data.boardFrameCounter;
                writer.Write("BOARD_FC{");
                for (int i = 0; i < Constant.FramesPerBlock; ++i)
                {
                    if (i != 0) writer.Write(",");
                    writer.Write(intarr[i]);
                }
                writer.WriteLine("}");

                intarr = data.boardFrameTimer;
                writer.Write("BOARD_FRAME_TIMER{");
                for (int i = 0; i < Constant.FramesPerBlock; ++i)
                {
                    if (i != 0) writer.Write(",");
                    writer.Write(intarr[i]);
                }
                writer.WriteLine("}");

                writer.Write("BER{"); writer.Write(data.BER); writer.WriteLine("}");
                writer.Write("WER{"); writer.Write(data.WER); writer.WriteLine("}");
                writer.Write("MISSING_FC{"); writer.Write(data.missingFrameCount); writer.WriteLine("}");
                writer.Write("FALSE_FC{"); writer.Write(data.falseFrameCount); writer.WriteLine("}");

                writer.Flush();
                Console.Write(writer.ToString());
            }
        }

        // tmrDraw is a timer that 'ticks' once every 5 milliseconds.  Upon a 'tick', we check to see
        // if there is enough new data from the USB port to update our waveform plots.
        private void tmrDraw_Tick(object sender, EventArgs e)
        {
            int numPagesLeftInRAM;

            if (readMode && !rawMode)
            {
                numPagesLeftInRAM = myUSBSource.CheckForUSBData(plotQueue, saveQueue, dataRate, rawMode, myHammingDecoder);

                if (plotQueue.Count > 0)
                {
                    
                    doPlot(numPagesLeftInRAM); // NB: do the plot even if GUI hidden.. as it writes to the USB device in some cases!
                    if (Params.consoleData) doConsoleDataOutput(numPagesLeftInRAM);
                    plotQueue.Clear(); // must call this else data will leak..
                    noDataCounter = noDataCounter2 = 0;
                }
                else // plotQueue.count() == 0
                {
                    noDataCounter++;
                    noDataCounter2++;

                    // Turn off LED bar graph if we haven't seen any valid frames in a while
                    if (noDataCounter > 10)
                    {
                        myUSBSource.LEDControl(0);
                        lblFrameFound.Text = "No";
                        lblFrameFound.ForeColor = Color.Red;
                        lblChipID.Text = "n/a";
                        noDataCounter = 0;
                    }

                    if (noDataCounter2 > 200)
                    {
                        if (Params.consoleData) Console.WriteLine("WARNMSG: No frame data received in a while, continuing...");
                        noDataCounter2 = 0;
                    }
                }

                if (saveMode == true)
                {
                    USBData saveData;

                    while (saveQueue.Count > 0)
                    {
                        saveData = saveQueue.Dequeue();
                        saveData.CopyRawDataToArray(rawData, dataRate, rawMode);
                        for (int i = 0; i < Constant.FrameSize(dataRate, rawMode) * Constant.FramesPerBlock; i++)
                        {
                            binWriter.Write(rawData[i]);
                        }
                        fileSize += ((double)(Constant.FrameSize(dataRate, rawMode) * Constant.FramesPerBlock)) / 1000000.0;  // aux inputs

                        fileSaveTime += ((double)Constant.FramesPerBlock) * Constant.FramePeriod;

                        if (fileSaveTime >= (60.0 * (double)numMaxMinutes.Value))    // Every X minutes, close existing file and start a new one
                        {
                            binWriter.Flush();
                            binWriter.Close();
                            fs.Close();

                            DateTime dt = DateTime.Now;
                            saveFileName = String.Concat(Path.GetDirectoryName(saveFileDialog1.FileName), Path.DirectorySeparatorChar, Path.GetFileNameWithoutExtension(saveFileDialog1.FileName), dt.ToString("_yyMMdd_HHmmss"), ".int");
                            fs = new FileStream(saveFileName, FileMode.Create);
                            binWriter = new BinaryWriter(fs);
                            fileSize = 0.0;
                            fileSaveTime = 0.0;

                            this.WriteSaveFileHeader(binWriter);
                        }
                    }
                }
                else
                {
                    saveQueue.Clear();
                }

            }
            else if (readMode && rawMode)
            {
                numPagesLeftInRAM = myUSBSource.CheckForUSBData(plotQueue, saveQueue, dataRate, rawMode, myHammingDecoder);

                doConsoleDataOutput(numPagesLeftInRAM); // yep, we call it here too..

                plotQueue.Clear();

                if (saveMode)
                {
                    USBData saveData;

                    lblStatus.Text = String.Concat("Saving raw data to file ", saveFileName, ".  Estimated file size = ", fileSize.ToString("F01"), " MB.");

                    while (saveQueue.Count > 0)
                    {
                        saveData = saveQueue.Dequeue();
                        saveData.CopyRawDataToArray(rawData, dataRate, rawMode);
                        for (int i = 0; i < Constant.FrameSize(dataRate, rawMode) * Constant.FramesPerBlock; i++)
                        {
                            binWriter.Write(rawData[i]);
                        }
                        fileSize += ((double)(Constant.FrameSize(dataRate, rawMode) * Constant.FramesPerBlock)) / 1000000.0;

                        fileSaveTime += ((double)Constant.FramesPerBlock) * Constant.FramePeriod;
                    }
                }
                else
                {
                    saveQueue.Clear();
                }
            }
        }

        // tmrSynthData is a timer that 'ticks' once every 30 milliseconds to emulate the
        // data rate from the USB port when an Intan board is connected.
        private void tmrSynthData_Tick(object sender, EventArgs e)
        {
            myUSBSource.NewSynthDataReady = true;
        }

        // Start button
        private void btnStart_Click(object sender, EventArgs e)
        {
            btnStop.Enabled = true;
            btnStart.Enabled = false;
            cmbDataRate.Enabled = false;

            if (saveMode == false)
            {
                xSlowPos = 0;
                xSlowPosBER = 0;
                myUSBSource.Start();

                //btnZCheck.ForeColor = SystemColors.ControlDark;

                if (myUSBSource.SynthDataMode)
                    lblStatus.Text = "Viewing synthesized neural data.";
                else
                    lblStatus.Text = "Viewing live data.";

                readMode = true;
            }
        }

        // Stop button
        private void btnStop_Click(object sender, EventArgs e)
        {
            readMode = false;
            rawMode = false;

            btnStart.Enabled = true;
            btnStop.Enabled = false;
            // cmbDataRate.Enabled = true;

            myUSBSource.Stop();

            myUSBSource.EnableRawMode(false);

            if (saveMode == true)
            {
                btnRecord.Enabled = true;
                btnRaw.Enabled = true;

                binWriter.Flush();
                binWriter.Close();
                fs.Close();
                saveMode = false;

                chkNeural5.Enabled = true;
                chkNeural6.Enabled = true;
                chkEMG1.Enabled = true;
                chkEMG2.Enabled = true;
                chkEMG3.Enabled = true;
                chkEMG4.Enabled = true;

                if (dataRate == DataRate.Medium || dataRate == DataRate.High)
                {
                    chkNeural3.Enabled = true;
                    chkNeural4.Enabled = true;
                    chkNeural7.Enabled = true;
                }

                if (dataRate == DataRate.High)
                {
                    chkNeural1.Enabled = true;
                    chkNeural2.Enabled = true;
                    chkNeural8.Enabled = true;
                    chkNeural9.Enabled = true;
                    chkNeural10.Enabled = true;
                }
            }

            lblStatus.Text = "Ready to start.";

            lblFrameFound.Text = "n/a";
            lblFrameFound.ForeColor = SystemColors.ControlText;
            lblChipID.Text = "n/a";

            Rectangle barGraph = new Rectangle(765, 84, 80, 5);
            myBuffer.Graphics.FillRectangle(SystemBrushes.Control, barGraph);
        }

        // Neural voltage axis 'Zoom In' button
        private void btnYZoomIn_Click(object sender, EventArgs e)
        {
            if (yScaleIndex > 0)
            {
                yScaleIndex--;
                lblYScale.Text = YScaleText[yScaleIndex];
            }
            if (spikeWindowVisible)
            {
                frmMagnifyForm.UpdateYScale(yScaleIndex);
            }
        }

        // Neural voltage axis 'Zoom Out' button
        private void btnYZoomOut_Click(object sender, EventArgs e)
        {
            if (yScaleIndex < (NumYScales - 1))
            {
                yScaleIndex++;
                lblYScale.Text = YScaleText[yScaleIndex];
            }
            if (spikeWindowVisible)
            {
                frmMagnifyForm.UpdateYScale(yScaleIndex);
            }
        }

        // EMG voltage axis 'Zoom In' button
        private void btnYEMGZoomIn_Click(object sender, EventArgs e)
        {
            if (yScaleIndexEMG > 0)
            {
                yScaleIndexEMG--;
                lblEMGYScale.Text = YScaleTextEMG[yScaleIndexEMG];
            }
        }

        // EMG voltage axis 'Zoom Out' button
        private void btnYEMGZoomOut_Click(object sender, EventArgs e)
        {
            if (yScaleIndexEMG < (NumYScalesEMG - 1))
            {
                yScaleIndexEMG++;
                lblEMGYScale.Text = YScaleTextEMG[yScaleIndexEMG];
            }
        }

        // Time axis 'Zoom In' button
        private void btnXZoomIn_Click(object sender, EventArgs e)
        {
            if (xScaleIndex > 0)
            {
                xScaleIndex--;
                lblXScale.Text = XScaleText[xScaleIndex];
                xSlowPos = 0;
            }
        }

        // Time axis 'Zoom Out' button
        private void btnXZoomOut_Click(object sender, EventArgs e)
        {
            if (xScaleIndex < (NumXScales - 1))
            {
                xScaleIndex++;
                lblXScale.Text = XScaleText[xScaleIndex];
                xSlowPos = 0;
            }
        }

        // 'Hide All' button
        private void btnAllChannelsOff_Click(object sender, EventArgs e)
        {
            chkNeural1.Checked = false;
            chkNeural2.Checked = false;
            chkNeural3.Checked = false;
            chkNeural4.Checked = false;
            chkNeural5.Checked = false;
            chkNeural6.Checked = false;
            chkNeural7.Checked = false;
            chkNeural8.Checked = false;
            chkNeural9.Checked = false;
            chkNeural10.Checked = false;
            chkEMG1.Checked = false;
            chkEMG2.Checked = false;
            chkEMG3.Checked = false;
            chkEMG4.Checked = false;
        }

        // 'Show All' button
        private void btnAllChannelsOn_Click(object sender, EventArgs e)
        {
            chkNeural1.Checked = true;
            chkNeural2.Checked = true;
            chkNeural3.Checked = true;
            chkNeural4.Checked = true;
            chkNeural5.Checked = true;
            chkNeural6.Checked = true;
            chkNeural7.Checked = true;
            chkNeural8.Checked = true;
            chkNeural9.Checked = true;
            chkNeural10.Checked = true;
            chkEMG1.Checked = true;
            chkEMG2.Checked = true;
            chkEMG3.Checked = true;
            chkEMG4.Checked = true;
        }

        // Open or close 'Spike Scope' window
        private void btnSpikeWindow_Click(object sender, EventArgs e)
        {
            if (spikeWindowVisible)
            {
                this.CloseMagnifyForm();
            }
            else
            {
                frmMagnifyForm = new MagnifyForm();
                frmMagnifyForm.Location = new Point(this.Location.X + spikeWindowOffset.X, this.Location.Y + spikeWindowOffset.Y);
                frmMagnifyForm.Show();
                frmMagnifyForm.SetSpikeRecord(mySpikeRecord);
                frmMagnifyForm.SetUpGraphicsAndSound(myContext, this);
                frmMagnifyForm.UpdateYScale(yScaleIndex);
                spikeWindowVisible = true;
                btnSpikeWindow.Text = "Close Spike Scope";
            }
        }

        // Close 'Spike Scope' window
        public void CloseMagnifyForm()
        {
            spikeWindowOffset.X = frmMagnifyForm.Location.X - this.Location.X;
            spikeWindowOffset.Y = frmMagnifyForm.Location.Y - this.Location.Y;
            frmMagnifyForm.Close();
            frmMagnifyForm.Dispose();
            spikeWindowVisible = false;
            btnSpikeWindow.Text = "Open Spike Scope";
        }

        // Disable software notch filter
        private void btnNotchFilterDisable_CheckedChanged(object sender, EventArgs e)
        {
            myUSBSource.EnableNotch = false;
        }

        // Enable software 60-Hz notch filter
        private void btnNotchFilter60Hz_CheckedChanged(object sender, EventArgs e)
        {
            myUSBSource.EnableNotch = true;
        }

        // Enable/disable software high-pass filter
        private void chkEnableHPF_CheckedChanged(object sender, EventArgs e)
        {
            if (chkEnableHPF.Checked == true)
            {
                myUSBSource.EnableHPF = true;
            }
            else
            {
                myUSBSource.EnableHPF = false;
            }
        }

        // Software high-pass filter cutoff frequency text box
        private void txtHPF_TextChanged(object sender, EventArgs e)
        {
            double new_fHPF;

            new_fHPF = myUSBSource.FHPF;

            try
            {
                new_fHPF = Convert.ToDouble(txtHPF.Text);
            }
            catch
            {
                txtHPF.Text = myUSBSource.FHPF.ToString();
            }

            if (new_fHPF < 0.0)
            {
                myUSBSource.FHPF = 0.0;
                txtHPF.Text = "0";
            }
            else if (new_fHPF > 10000.0)
            {
                myUSBSource.FHPF = 10000.0;
                txtHPF.Text = "10000";
            }
            else
            {
                myUSBSource.FHPF = new_fHPF;
            }
        }

        // Frame marker correlation error tolerance number selector
        private void numFrameErrorTolerance_ValueChanged(object sender, EventArgs e)
        {
            myUSBSource.SetCorrelationThreshold(Constant.MaxFrameCorrelation - 2 * ((int)numFrameErrorTolerance.Value));
            lblChipID.Focus();  // remove focus from this control
        }

        // Data clock edge selector
        private void cmbClockEdge_SelectedIndexChanged(object sender, EventArgs e)
        {
            myUSBSource.SetClockEdge(cmbClockEdge.SelectedIndex);
            lblChipID.Focus();  // remove focus from this control
        }

        // Data rate selector
        private void cmbDataRate_SelectedIndexChanged(object sender, EventArgs e)
        {
            if (cmbDataRate.SelectedIndex == 0)
            {
                dataRate = DataRate.Low;

                chkNeural1.Enabled = false;
                chkNeural2.Enabled = false;
                chkNeural3.Enabled = false;
                chkNeural4.Enabled = false;
                chkNeural5.Enabled = true;
                chkNeural6.Enabled = true;
                chkNeural7.Enabled = false;
                chkNeural8.Enabled = false;
                chkNeural9.Enabled = false;
                chkNeural10.Enabled = false;

                chkNeural1.Checked = false;
                chkNeural2.Checked = false;
                chkNeural3.Checked = false;
                chkNeural4.Checked = false;
                chkNeural5.Checked = true;
                chkNeural6.Checked = true;
                chkNeural7.Checked = false;
                chkNeural8.Checked = false;
                chkNeural9.Checked = false;
                chkNeural10.Checked = false;

            }
            else if (cmbDataRate.SelectedIndex == 1)
            {
                dataRate = DataRate.Medium;

                chkNeural1.Enabled = false;
                chkNeural2.Enabled = false;
                chkNeural3.Enabled = true;
                chkNeural4.Enabled = true;
                chkNeural5.Enabled = true;
                chkNeural6.Enabled = true;
                chkNeural7.Enabled = true;
                chkNeural8.Enabled = false;
                chkNeural9.Enabled = false;
                chkNeural10.Enabled = false;

                chkNeural1.Checked = false;
                chkNeural2.Checked = false;
                chkNeural3.Checked = true;
                chkNeural4.Checked = true;
                chkNeural5.Checked = true;
                chkNeural6.Checked = true;
                chkNeural7.Checked = true;
                chkNeural8.Checked = false;
                chkNeural9.Checked = false;
                chkNeural10.Checked = false;
            }
            else
            {
                dataRate = DataRate.High;

                chkNeural1.Enabled = true;
                chkNeural2.Enabled = true;
                chkNeural3.Enabled = true;
                chkNeural4.Enabled = true;
                chkNeural5.Enabled = true;
                chkNeural6.Enabled = true;
                chkNeural7.Enabled = true;
                chkNeural8.Enabled = true;
                chkNeural9.Enabled = true;
                chkNeural10.Enabled = true;

                chkNeural1.Checked = true;
                chkNeural2.Checked = true;
                chkNeural3.Checked = true;
                chkNeural4.Checked = true;
                chkNeural5.Checked = true;
                chkNeural6.Checked = true;
                chkNeural7.Checked = true;
                chkNeural8.Checked = true;
                chkNeural9.Checked = true;
                chkNeural10.Checked = true;
            }
            myUSBSource.SetDataRate(dataRate);
            lblChipID.Focus();  // remove focus from this control
        }

        // Exit, from menu
        private void exitToolStripMenuItem_Click(object sender, EventArgs e)
        {
            this.Close();
        }

        // About, from menu
        private void aboutToolStripMenuItem_Click(object sender, EventArgs e)
        {
            AboutForm frmAbout = new AboutForm();
            frmAbout.ShowDialog();
        }

        // Start New File Every X Minutes selector
        private void numMaxMinutes_ValueChanged(object sender, EventArgs e)
        {
            lblChipID.Focus();  // remove focus from this control
        }

        // Select Base Filename button
        private void btnSelectFilename_Click(object sender, EventArgs e)
        {
            saveFileDialog1.Title = "Specify Base Filename for Saved Data";
            saveFileDialog1.Filter = "Intan Data Files (*.int)|*.int";
            saveFileDialog1.FilterIndex = 1;

            saveFileDialog1.OverwritePrompt = true;

            if (saveFileDialog1.ShowDialog() != DialogResult.Cancel)
            {
                txtSaveFilename.Text = Path.GetFileNameWithoutExtension(saveFileDialog1.FileName);
            }

            bool enabl = txtSaveFilename.Text.Length > 0;

            btnRecord.Enabled = enabl;
            btnRaw.Enabled = enabl;
        }

        // Record button
        private void btnRecord_Click(object sender, EventArgs e)
        {
            if (saveFileDialog1.FileName != null)
            {
                btnStop.Enabled = true;
                btnStart.Enabled = false;
                btnRecord.Enabled = false;
                btnRaw.Enabled = false;

                DateTime dt = DateTime.Now;
                saveFileName = String.Concat(Path.GetDirectoryName(saveFileDialog1.FileName), Path.DirectorySeparatorChar, Path.GetFileNameWithoutExtension(saveFileDialog1.FileName), dt.ToString("_yyMMdd_HHmmss"), ".int");
                fs = new FileStream(saveFileName, FileMode.Create);
                binWriter = new BinaryWriter(fs);
                fileSize = 0.0;
                fileSaveTime = 0.0;

                cmbDataRate.Enabled = false;

                saveMode = true;
                xSlowPos = 0;
                xSlowPosBER = 0;
                myUSBSource.Start();

                readMode = true;

                chkNeural1.Enabled = false;
                chkNeural2.Enabled = false;
                chkNeural3.Enabled = false;
                chkNeural4.Enabled = false;
                chkNeural5.Enabled = false;
                chkNeural6.Enabled = false;
                chkNeural7.Enabled = false;
                chkNeural8.Enabled = false;
                chkNeural9.Enabled = false;
                chkNeural10.Enabled = false;
                chkEMG1.Enabled = false;
                chkEMG2.Enabled = false;
                chkEMG3.Enabled = false;
                chkEMG4.Enabled = false;

                displayNeuralChannel[0] = chkNeural1.Checked;
                displayNeuralChannel[1] = chkNeural2.Checked;
                displayNeuralChannel[2] = chkNeural3.Checked;
                displayNeuralChannel[3] = chkNeural4.Checked;
                displayNeuralChannel[4] = chkNeural5.Checked;
                displayNeuralChannel[5] = chkNeural6.Checked;
                displayNeuralChannel[6] = chkNeural7.Checked;
                displayNeuralChannel[7] = chkNeural8.Checked;
                displayNeuralChannel[8] = chkNeural9.Checked;
                displayNeuralChannel[9] = chkNeural10.Checked;

                displayEMGChannel[0] = chkEMG1.Checked;
                displayEMGChannel[1] = chkEMG2.Checked;
                displayEMGChannel[2] = chkEMG3.Checked;
                displayEMGChannel[3] = chkEMG4.Checked;

                this.WriteSaveFileHeader(binWriter);
            }
        }

        // Write header information to save file
        private void WriteSaveFileHeader(BinaryWriter binWriter)
        {
            binWriter.Write((byte)100);
            binWriter.Write((byte)1);
            binWriter.Write((byte)dataRate);

            for (int channel = 0; channel < Constant.TotalNeuralChannels; channel++)
            {
                if (displayNeuralChannel[channel] == true)
                    binWriter.Write((byte)1);
                else
                    binWriter.Write((byte)0);
            }
            for (int channel = 0; channel < Constant.TotalEMGChannels; channel++)
            {
                if (displayEMGChannel[channel] == true)
                    binWriter.Write((byte)1);
                else
                    binWriter.Write((byte)0);
            }
            for (int i = 0; i < 48; i++)        // Future expansion
                binWriter.Write((byte)0);
        }

        // Record Raw Data button
        private void btnRaw_Click(object sender, EventArgs e)
        {
            if (saveFileDialog1.FileName != null)
            {
                btnStop.Enabled = true;
                btnStart.Enabled = false;
                btnRecord.Enabled = false;
                btnRaw.Enabled = false;

                myUSBSource.EnableRawMode(true);

                DateTime dt = DateTime.Now;
                saveFileName = String.Concat(Path.GetDirectoryName(saveFileDialog1.FileName), Path.DirectorySeparatorChar, Path.GetFileNameWithoutExtension(saveFileDialog1.FileName), dt.ToString("_yyMMdd_HHmmss"), ".raw");
                fs = new FileStream(saveFileName, FileMode.Create);
                binWriter = new BinaryWriter(fs);
                fileSize = 0.0;
                fileSaveTime = 0.0;

                cmbDataRate.Enabled = false;

                saveMode = true;
                rawMode = true;
                xSlowPos = 0;
                xSlowPosBER = 0;
                myUSBSource.Start();

                readMode = true;

                chkNeural1.Enabled = false;
                chkNeural2.Enabled = false;
                chkNeural3.Enabled = false;
                chkNeural4.Enabled = false;
                chkNeural5.Enabled = false;
                chkNeural6.Enabled = false;
                chkNeural7.Enabled = false;
                chkNeural8.Enabled = false;
                chkNeural9.Enabled = false;
                chkNeural10.Enabled = false;
                chkEMG1.Enabled = false;
                chkEMG2.Enabled = false;
                chkEMG3.Enabled = false;
                chkEMG4.Enabled = false;

                displayNeuralChannel[0] = chkNeural1.Checked;
                displayNeuralChannel[1] = chkNeural2.Checked;
                displayNeuralChannel[2] = chkNeural3.Checked;
                displayNeuralChannel[3] = chkNeural4.Checked;
                displayNeuralChannel[4] = chkNeural5.Checked;
                displayNeuralChannel[5] = chkNeural6.Checked;
                displayNeuralChannel[6] = chkNeural7.Checked;
                displayNeuralChannel[7] = chkNeural8.Checked;
                displayNeuralChannel[8] = chkNeural9.Checked;
                displayNeuralChannel[9] = chkNeural10.Checked;

                displayEMGChannel[0] = chkEMG1.Checked;
                displayEMGChannel[1] = chkEMG2.Checked;
                displayEMGChannel[2] = chkEMG3.Checked;
                displayEMGChannel[3] = chkEMG4.Checked;
            }
        }

    }
}
