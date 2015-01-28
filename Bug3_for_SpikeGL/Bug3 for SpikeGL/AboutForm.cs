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

namespace Bug3
{
    /// <summary>
    /// Display information about application.
    /// </summary>
    public partial class AboutForm : Form
    {
        public AboutForm()
        {
            InitializeComponent();
        }

        private void AboutForm_Load(object sender, EventArgs e)
        {

        }

        private void btnOK_Click(object sender, EventArgs e)
        {
            this.Close();
        }

        private void picIntanLogo_Click(object sender, EventArgs e)
        {
            //Call the Process.Start method to open the default browser with a URL:
            System.Diagnostics.Process.Start("http://www.intantech.com");
        }

        private void lnkIntan_LinkClicked(object sender, LinkLabelLinkClickedEventArgs e)
        {
            //Call the Process.Start method to open the default browser with a URL:
            System.Diagnostics.Process.Start("http://www.intantech.com");
        }
    }
}
