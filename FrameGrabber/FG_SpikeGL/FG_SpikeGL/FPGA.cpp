#include "stdafx.h"
#include "FPGA.h"
#include "Thread.h"

struct FPGA::Handler : public Thread {
    HANDLE h;
    volatile bool pleaseStop;
    Mutex mut;
    std::list<std::string> q;

    Handler(HANDLE h) : h(h) {}
    ~Handler(); 
};

FPGA::Handler::~Handler() {
    if (isRunning()) { 
        pleaseStop = true; 
        if (!wait(250)) kill();
    }
}

struct FPGA::Writer : public Handler {
    Writer(HANDLE hh) : Handler(hh) {}
    void threadFunc(); ///< virtual from Thread
};

struct FPGA::Reader : public Handler {
    Reader(HANDLE hh) : Handler(hh) {}
    void threadFunc();
};

FPGA::FPGA(const int parms[6])
    : hPort1(INVALID_HANDLE_VALUE), is_ok(false), writer(0), reader(0)
{
    if (is_ok = configure(parms)) {
        writer = new Writer(hPort1);
        reader = new Reader(hPort1);
        writer->start();
        reader->start();

        // auto-reset dout and leds..
        protocol_Write(1, 0, 0);
        protocol_Write(2, 0, 0);
    }
}


FPGA::~FPGA()
{
    if (hPort1 != INVALID_HANDLE_VALUE) CloseHandle(hPort1);
    hPort1 = INVALID_HANDLE_VALUE;
    delete reader; delete writer;
    reader = 0; writer = 0;
}

bool FPGA::configure(const int parms[6])
{
    std::string str1, str2;

    memset(&Port1DCB, 0, sizeof(Port1DCB));

    // Change the DCB structure settings
    Port1DCB.fBinary = TRUE;				// Binary mode; no EOF check
    Port1DCB.fParity = TRUE;				// Enable parity checking 
    Port1DCB.fDsrSensitivity = FALSE;		// DSR sensitivity 
    Port1DCB.fErrorChar = FALSE;			// Disable error replacement 
    Port1DCB.fOutxDsrFlow = FALSE;			// No DSR output flow control 
    Port1DCB.fAbortOnError = FALSE;			// Do not abort reads/writes on error
    Port1DCB.fNull = FALSE;					// Disable null stripping 
    Port1DCB.fTXContinueOnXoff = TRUE;		// XOFF continues Tx 

    switch (parms[0]) // Port Num
    {
    case 0:		PortNum = "COM1";				break;
    case 1:		PortNum = "COM2";				break;
    case 2:		PortNum = "COM3";				break;
    case 3:		PortNum = "COM4";				break;
    default:	PortNum = "COM1";				break;
    }

    switch (parms[1]) // BAUD Rate
    {
    case 0:		Port1DCB.BaudRate = 230400;		break;
    case 1:		Port1DCB.BaudRate = 115200;		break;
    case 2:		Port1DCB.BaudRate = 57600;		break;
    case 3:		Port1DCB.BaudRate = 38400;		break;
    case 4:		Port1DCB.BaudRate = 28800;		break;
    case 5:		Port1DCB.BaudRate = 19200;      break;
    case 6:		Port1DCB.BaudRate = 9600;		break;
    case 7:		Port1DCB.BaudRate = 4800;		break;
    case 8:		Port1DCB.BaudRate = 2400;		break;
    default:	Port1DCB.BaudRate = 0;			break;
    }

    switch (parms[2]) // Number of bits/byte, 5-8 
    {
    case 0:		Port1DCB.ByteSize = 8;			break;
    case 1:		Port1DCB.ByteSize = 7;			break;
    default:	Port1DCB.ByteSize = 0;			break;
    }

    switch (parms[3]) // 0-4=no,odd,even,mark,space 
    {
    case 0:		Port1DCB.Parity = NOPARITY;
        str1 = "N";						break;
    case 1:		Port1DCB.Parity = EVENPARITY;
        str1 = "E";						break;
    case 2:		Port1DCB.Parity = ODDPARITY;
        str1 = "O";						break;
    default:	Port1DCB.Parity = 0;
        str1 = "X";						break;
    }

    switch (parms[4])
    {
    case 0:		Port1DCB.StopBits = ONESTOPBIT;
        str2 = "1";						break;
    case 1:		Port1DCB.StopBits = TWOSTOPBITS;
        str2 = "2";						break;
    default:	Port1DCB.StopBits = 0;
        str2 = "0";						break;
    }

    switch (parms[5])
    {
    case 0:		
        Port1DCB.fOutxCtsFlow = TRUE;					// CTS output flow control 
        Port1DCB.fDtrControl = DTR_CONTROL_ENABLE;		// DTR flow control type 
        Port1DCB.fOutX = FALSE;							// No XON/XOFF out flow control 
        Port1DCB.fInX = FALSE;							// No XON/XOFF in flow control 
        Port1DCB.fRtsControl = RTS_CONTROL_ENABLE;		// RTS flow control   
        break;
    case 1:		
        Port1DCB.fOutxCtsFlow = FALSE;					// No CTS output flow control 
        Port1DCB.fDtrControl = DTR_CONTROL_ENABLE;		// DTR flow control type 
        Port1DCB.fOutX = FALSE;							// No XON/XOFF out flow control 
        Port1DCB.fInX = FALSE;							// No XON/XOFF in flow control 
        Port1DCB.fRtsControl = RTS_CONTROL_ENABLE;		// RTS flow control 
        break;
    case 2:		
        Port1DCB.fOutxCtsFlow = FALSE;					// No CTS output flow control 
        Port1DCB.fDtrControl = DTR_CONTROL_ENABLE;		// DTR flow control type 
        Port1DCB.fOutX = TRUE;							// Enable XON/XOFF out flow control 
        Port1DCB.fInX = TRUE;							// Enable XON/XOFF in flow control 
        Port1DCB.fRtsControl = RTS_CONTROL_ENABLE;		// RTS flow control    
        break;
    default:	break;
    }

    char buf[512];
    _snprintf_c(buf, sizeof(buf), "%s %d, %d, %s, %s", PortNum.c_str(), Port1DCB.BaudRate, Port1DCB.ByteSize, str1.c_str(), str2.c_str());
    PortConfig = buf;
    spikeGL->pushConsoleDebug(PortConfig);

    return setupCOM();
}

bool FPGA::setupCOM()
{
    if (hPort1 != INVALID_HANDLE_VALUE) CloseHandle(hPort1);

    hPort1 = CreateFile(PortNum.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    if (hPort1 == INVALID_HANDLE_VALUE)
    {
        spikeGL->pushConsoleWarning("Port Open Failed");
        return false;
    }

    DCB tmpDcb;
    memset(&tmpDcb, 0, sizeof(tmpDcb));
    tmpDcb.DCBlength = sizeof(tmpDcb);
    Port1DCB.DCBlength = sizeof(DCB);			//Initialize the DCBlength member. 

    if (!GetCommState(hPort1, &tmpDcb))	// Get the default port setting information.
    {
        spikeGL->pushConsoleWarning("GetCommState Failed");
        CloseHandle(hPort1); hPort1 = INVALID_HANDLE_VALUE;
        return false;
    }

    tmpDcb.BaudRate = Port1DCB.BaudRate;
    tmpDcb.ByteSize = Port1DCB.ByteSize;
    tmpDcb.Parity = Port1DCB.Parity;
    tmpDcb.StopBits = Port1DCB.StopBits;
    tmpDcb.fOutxCtsFlow = Port1DCB.fOutxCtsFlow;
    tmpDcb.fDtrControl = Port1DCB.fDtrControl;
    tmpDcb.fOutX = Port1DCB.fOutX;
    tmpDcb.fInX = Port1DCB.fInX;
    tmpDcb.fRtsControl = Port1DCB.fRtsControl;

    tmpDcb.fBinary = Port1DCB.fBinary;
    tmpDcb.fParity = Port1DCB.fParity;
    tmpDcb.fDsrSensitivity = Port1DCB.fDsrSensitivity;
    tmpDcb.fErrorChar = Port1DCB.fErrorChar;
    tmpDcb.fOutxDsrFlow = Port1DCB.fOutxDsrFlow;
    tmpDcb.fAbortOnError = Port1DCB.fAbortOnError;
    tmpDcb.fNull = Port1DCB.fNull;
    tmpDcb.fTXContinueOnXoff = Port1DCB.fTXContinueOnXoff;

    //Re-configure the port with the new DCB structure. 
    if (!SetCommState(hPort1, &tmpDcb))
    {
        spikeGL->pushConsoleWarning("SetCommState Failed");
        CloseHandle(hPort1); hPort1 = INVALID_HANDLE_VALUE;
        return false;
    }

    if (!GetCommState(hPort1, &Port1DCB))	// Get and save the new port setting information.
    {
        spikeGL->pushConsoleWarning("GetCommState Failed");
        CloseHandle(hPort1); hPort1 = INVALID_HANDLE_VALUE;
        return false;
    }

    memset(&CommTimeouts, 0, sizeof(CommTimeouts));
    GetCommTimeouts(hPort1, &CommTimeouts);
    CommTimeouts.ReadIntervalTimeout = 50;
    CommTimeouts.ReadTotalTimeoutConstant = 50;
    CommTimeouts.ReadTotalTimeoutMultiplier = 10;
    CommTimeouts.WriteTotalTimeoutMultiplier = 10;
    CommTimeouts.WriteTotalTimeoutConstant = 50;

    // Set the time-out parameters for all read and write operations on the port. 
    if (!SetCommTimeouts(hPort1, &CommTimeouts))
    {
        spikeGL->pushConsoleWarning("SetCommTimeouts Failed");
        CloseHandle(hPort1); hPort1 = INVALID_HANDLE_VALUE;
        return false;
    }

    // Clear the port of any existing data. 
    if (PurgeComm(hPort1, PURGE_TXCLEAR | PURGE_RXCLEAR) == 0)
    {
        spikeGL->pushConsoleWarning("Clearing The Port Failed");
        CloseHandle(hPort1); hPort1 = INVALID_HANDLE_VALUE;
        return false;
    }
    return true;
}

void FPGA::Writer::threadFunc() {
    pleaseStop = false;
    while (!pleaseStop) {
        std::list<std::string> my;
        mut.lock();
        my.swap(q);
        mut.unlock();
        int ct = 0;
        for (std::list<std::string>::iterator it = my.begin(); it != my.end(); ++it) {
            DWORD bytesWritten = 0;
            spikeGL->pushConsoleDebug(std::string("Attempt to write: ") + *it);
            if (!WriteFile(h, &((*it)[0]), DWORD((*it).length()), &bytesWritten, NULL)) {
                spikeGL->pushConsoleDebug("FPGA::Writer::threadFunc() -- WriteFile() returned FALSE!");
            } else if (bytesWritten != DWORD((*it).length())) {
                char buf[512];
                _snprintf_c(buf, sizeof(buf), "FPGA::Writer::threadFunc() -- WriteFile() returned %d, expected %d!", (int)bytesWritten, (int)(*it).length());
                spikeGL->pushConsoleDebug(buf);
            } else {
                spikeGL->pushConsoleDebug(std::string("Wrote to COM --> ") + *it);
            }
            ++ct;
        }
        if (!ct && !pleaseStop) Sleep(100);
    }
}

void FPGA::Reader::threadFunc() {
    pleaseStop = false;
    while (!pleaseStop) {
        char buf[1024];
        buf[1023] = 0;
        DWORD nread = 0;
        bool readok = false;
        if (ReadFile(h, buf, sizeof(buf) - 1, &nread, NULL)) {
            if (nread) {
                if (nread > 1023) nread = 1023;
                buf[nread] = 0;
                std::string s(buf, nread);
                spikeGL->pushConsoleDebug(std::string("Read COM --> ") + s);
                mut.lock();
                q.push_back(s);
                mut.unlock();
            } else {
                spikeGL->pushConsoleDebug("FPGA::Reader::threadFunc() -- read 0 bytes...");
            }
            readok = true;
        } else {
            spikeGL->pushConsoleDebug("FPGA::Reader::threadFunc() -- ReadFile() returned FALSE!");
        }
        if (!pleaseStop && !readok) Sleep(100);
    }
}

void FPGA::write(const std::string &s) { ///< queued write.. returns immediately, writes in another thread
    if (!writer) return;
    writer->mut.lock();
    writer->q.push_back(s);
    writer->mut.unlock();
}

std::list<std::string> FPGA::readAll() { ///< reads all data available, returns immediately, may return an empty list if no data is available
    std::list<std::string> ret;
    if (reader) {
        reader->mut.lock();
        ret.swap(reader->q);
        reader->mut.unlock();
    }
    return ret;
}

void FPGA::protocol_Write(int CMD_Code, int Value_1, INT32 Value_2)
{
    char    str[512];

    _snprintf_c(str, sizeof(str), "%c%02d%05d%06d\n\r", '~', CMD_Code, Value_1, Value_2);
    write(str);
}
