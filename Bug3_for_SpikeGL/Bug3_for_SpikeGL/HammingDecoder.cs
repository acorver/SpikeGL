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
using System.Linq;
using System.Text;
using System.Threading;
using System.Diagnostics;
using System.IO;


namespace Bug3
{
    /// <summary>
    /// (16,11) extended Hamming decoder class
    /// </summary>
    public class HammingDecoder
    {
        static readonly UInt16[] hammingDecodedData = new UInt16[65536];
        static readonly UInt16[] hammingNumErrors = new UInt16[65536];

        /// <summary>
        /// Generate decoding table for extended (16,11) Hamming code with SECDED
        /// Single Error Correction; Double Error Detection).
        /// </summary>
        static HammingDecoder()
        {
            int i;
            UInt16 inputValue, encodedValue, encodedValueOneBitError;
            UInt16[] dataBit = new UInt16[11];
            UInt16[] errorBit = new UInt16[5];

            // Set default values for two-bit-error cases
            for (i = 0; i < 65536; i++)
            {
                hammingDecodedData[i] = 1023;   // default value for two bit errors
                hammingNumErrors[i] = 2;        // two bit errors
            }

            for (inputValue = 0; inputValue < 2048; inputValue++)
            {
                // Create valid code for zero-bit-error case
                dataBit[0] = ((inputValue & 0x001) > 0) ? (UInt16)1 : (UInt16)0;
                dataBit[1] = ((inputValue & 0x002) > 0) ? (UInt16)1 : (UInt16)0;
                dataBit[2] = ((inputValue & 0x004) > 0) ? (UInt16)1 : (UInt16)0;
                dataBit[3] = ((inputValue & 0x008) > 0) ? (UInt16)1 : (UInt16)0;
                dataBit[4] = ((inputValue & 0x010) > 0) ? (UInt16)1 : (UInt16)0;
                dataBit[5] = ((inputValue & 0x020) > 0) ? (UInt16)1 : (UInt16)0;
                dataBit[6] = ((inputValue & 0x040) > 0) ? (UInt16)1 : (UInt16)0;
                dataBit[7] = ((inputValue & 0x080) > 0) ? (UInt16)1 : (UInt16)0;
                dataBit[8] = ((inputValue & 0x100) > 0) ? (UInt16)1 : (UInt16)0;
                dataBit[9] = ((inputValue & 0x200) > 0) ? (UInt16)1 : (UInt16)0;
                dataBit[10] = ((inputValue & 0x400) > 0) ? (UInt16)1 : (UInt16)0;
                errorBit[0] = (UInt16)(dataBit[6] ^ dataBit[5] ^ dataBit[4] ^ dataBit[3] ^ dataBit[2] ^ dataBit[1] ^ dataBit[0]);
                errorBit[1] = (UInt16)(dataBit[10] ^ dataBit[9] ^ dataBit[8] ^ dataBit[6] ^ dataBit[3] ^ dataBit[1] ^ dataBit[0]);
                errorBit[2] = (UInt16)(dataBit[10] ^ dataBit[9] ^ dataBit[7] ^ dataBit[6] ^ dataBit[4] ^ dataBit[2] ^ dataBit[0]);
                errorBit[3] = (UInt16)(dataBit[10] ^ dataBit[8] ^ dataBit[7] ^ dataBit[6] ^ dataBit[5] ^ dataBit[2] ^ dataBit[1]);
                errorBit[4] = (UInt16)(dataBit[9] ^ dataBit[8] ^ dataBit[7] ^ dataBit[6] ^ dataBit[5] ^ dataBit[4] ^ dataBit[3]);

                encodedValue = (UInt16)((inputValue << 5) + 16 * errorBit[4] + 8 * errorBit[3] + 4 * errorBit[2] + 2 * errorBit[1] + errorBit[0]);
                hammingDecodedData[encodedValue] = inputValue;
                hammingNumErrors[encodedValue] = 0;                             // zero bit errors

                // Now create all one-bit-error cases from this valid code
                for (i = 0; i < 16; i++)
                {
                    encodedValueOneBitError = (UInt16)(encodedValue ^ (1 << i));          // flip one bit
                    hammingDecodedData[encodedValueOneBitError] = inputValue;
                    hammingNumErrors[encodedValueOneBitError] = 1;              // one bit error
                }
            }
        }


        public UInt16 DecodeData(UInt16 encodedWord)
        {
            return hammingDecodedData[encodedWord];
        }


        public UInt16 NumErrors(UInt16 encodedWord)
        {
            return hammingNumErrors[encodedWord];
        }

        public UInt16 DecodeDataCountErrors(UInt16 encodedWord, out int NumErrors, ref int BitErrorCount, ref int WordErrorCount)
        {
            NumErrors = (int)hammingNumErrors[encodedWord];
            BitErrorCount += NumErrors;
            if (NumErrors == 2) WordErrorCount++;

            return hammingDecodedData[encodedWord];
        }

        public UInt16 EncodeData(int input, Random rand, double BER)
        {
            UInt16 inputValue, encodedValue;
            UInt16[] dataBit = new UInt16[11];
            UInt16[] errorBit = new UInt16[5];

            if (input > 2047)
                input = 2047;
            else if (input < 0)
                input = 0;

            inputValue = (UInt16)input;

            dataBit[0] = ((inputValue & 0x001) > 0) ? (UInt16)1 : (UInt16)0;
            dataBit[1] = ((inputValue & 0x002) > 0) ? (UInt16)1 : (UInt16)0;
            dataBit[2] = ((inputValue & 0x004) > 0) ? (UInt16)1 : (UInt16)0;
            dataBit[3] = ((inputValue & 0x008) > 0) ? (UInt16)1 : (UInt16)0;
            dataBit[4] = ((inputValue & 0x010) > 0) ? (UInt16)1 : (UInt16)0;
            dataBit[5] = ((inputValue & 0x020) > 0) ? (UInt16)1 : (UInt16)0;
            dataBit[6] = ((inputValue & 0x040) > 0) ? (UInt16)1 : (UInt16)0;
            dataBit[7] = ((inputValue & 0x080) > 0) ? (UInt16)1 : (UInt16)0;
            dataBit[8] = ((inputValue & 0x100) > 0) ? (UInt16)1 : (UInt16)0;
            dataBit[9] = ((inputValue & 0x200) > 0) ? (UInt16)1 : (UInt16)0;
            dataBit[10] = ((inputValue & 0x400) > 0) ? (UInt16)1 : (UInt16)0;
            errorBit[0] = (UInt16)(dataBit[6] ^ dataBit[5] ^ dataBit[4] ^ dataBit[3] ^ dataBit[2] ^ dataBit[1] ^ dataBit[0]);
            errorBit[1] = (UInt16)(dataBit[10] ^ dataBit[9] ^ dataBit[8] ^ dataBit[6] ^ dataBit[3] ^ dataBit[1] ^ dataBit[0]);
            errorBit[2] = (UInt16)(dataBit[10] ^ dataBit[9] ^ dataBit[7] ^ dataBit[6] ^ dataBit[4] ^ dataBit[2] ^ dataBit[0]);
            errorBit[3] = (UInt16)(dataBit[10] ^ dataBit[8] ^ dataBit[7] ^ dataBit[6] ^ dataBit[5] ^ dataBit[2] ^ dataBit[1]);
            errorBit[4] = (UInt16)(dataBit[9] ^ dataBit[8] ^ dataBit[7] ^ dataBit[6] ^ dataBit[5] ^ dataBit[4] ^ dataBit[3]);

            encodedValue = (UInt16)((inputValue << 5) + 16 * errorBit[4] + 8 * errorBit[3] + 4 * errorBit[2] + 2 * errorBit[1] + errorBit[0]);

            if (BER > 0.0)
            {
                if (rand.NextDouble() < BER) encodedValue = (UInt16)(encodedValue ^ (UInt16)0x8000);
                if (rand.NextDouble() < BER) encodedValue = (UInt16)(encodedValue ^ (UInt16)0x4000);
                if (rand.NextDouble() < BER) encodedValue = (UInt16)(encodedValue ^ (UInt16)0x2000);
                if (rand.NextDouble() < BER) encodedValue = (UInt16)(encodedValue ^ (UInt16)0x1000);
                if (rand.NextDouble() < BER) encodedValue = (UInt16)(encodedValue ^ (UInt16)0x0800);
                if (rand.NextDouble() < BER) encodedValue = (UInt16)(encodedValue ^ (UInt16)0x0400);
                if (rand.NextDouble() < BER) encodedValue = (UInt16)(encodedValue ^ (UInt16)0x0200);
                if (rand.NextDouble() < BER) encodedValue = (UInt16)(encodedValue ^ (UInt16)0x0100);
                if (rand.NextDouble() < BER) encodedValue = (UInt16)(encodedValue ^ (UInt16)0x0080);
                if (rand.NextDouble() < BER) encodedValue = (UInt16)(encodedValue ^ (UInt16)0x0040);
                if (rand.NextDouble() < BER) encodedValue = (UInt16)(encodedValue ^ (UInt16)0x0020);
                if (rand.NextDouble() < BER) encodedValue = (UInt16)(encodedValue ^ (UInt16)0x0010);
                if (rand.NextDouble() < BER) encodedValue = (UInt16)(encodedValue ^ (UInt16)0x0008);
                if (rand.NextDouble() < BER) encodedValue = (UInt16)(encodedValue ^ (UInt16)0x0004);
                if (rand.NextDouble() < BER) encodedValue = (UInt16)(encodedValue ^ (UInt16)0x0002);
                if (rand.NextDouble() < BER) encodedValue = (UInt16)(encodedValue ^ (UInt16)0x0001);
            }

            return encodedValue;
        }

    }
}
