
// MFCApplication2Dlg.cpp : implementation file
//

#include "stdafx.h"
#include "MFCApplication2.h"
#include "MFCApplication2Dlg.h"
#include "DlgProxy.h"
#include "afxdialogex.h"

#include "SapClassBasic.h"
#include "SapClassGui.h"

#include "XtCmd.h"
#include "Thread.h"
#include "SpikeGLHandlerThread.h"


#include <vector>
#include <list>

#include <sys/stat.h>
#include <stdio.h>
#include <fcntl.h>
#include <io.h>
#include <stdlib.h>
#include <string.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// Data Grid Related Parameters


// CmaeraLink Related Parameters
BOOL VSyncSignal;
BOOL HSyncSignal;
BOOL PixelCLKSignal1, PixelCLKSignal2, PixelCLKSignal3;
BOOL SignalAvailiable;
int	 Frame_Grabber_Enabled = 0;
int	 BMPScale = 7;
int  BMPX = 25;
int  BMPY = 600;
BYTE DataGridRaw[40][500];
int  DataGrid[40][100];

CEvent g_dataReady;			// set flag when both image sources are grabbing. 
BOOL SpikeGL_Mode = 1;

imageP AllocImageMemory(int w, int h, int s)
{	uchar	*p;
	imageP	 I;

	// allocate memory
	I = (imageP)malloc(sizeof(imageS));
	p = (uchar *)malloc(w * h * s);

	if (p == NULL)
	{	fprintf(stderr, "MemoryallocImage: Insufficient memory\n");
		return ((imageP)NULL);
	}

	// init structure
	I->width	= w;
	I->height	= h;
	I->image	= p;

	return(I);
}

imageRGB4P	 AllocImageRGB4Memory(int w, int h)
{
	uchar	*p;
	imageRGB4P	 I;

	// allocate memory 
	I = (imageRGB4P)malloc(sizeof(imageRGB4S));
	p = (uchar *)malloc(w * h * 4);

	if (p == NULL)
	{
		fprintf(stderr, "MemoryallocImage: Insufficient memory\n");
		return ((imageRGB4P)NULL);
	}

	// init structure
	I->width = w;
	I->height = h;
	I->image = p;

	return(I);
}

void FreeImageMemory(imageP I)
{
	free((char *)I->image);
	free((char *)I);
}

void FreeImageRGB4Memory(imageRGB4P I)
{
	free((char *)I->image);
	free((char *)I);
}

void MEAControlDlg::FreeUp_Resource()
{
	if (m_Xfer		&&	*m_Xfer)		m_Xfer->Destroy();
	if (m_Buffers	&&	*m_Buffers)		m_Buffers->Destroy();
	if (m_Acq		&&	*m_Acq)			m_Acq->Destroy();
	if (m_View		&&	*m_View)		m_View->Destroy();

	if (m_Xfer)			delete m_Xfer;
	if (m_Buffers)		delete m_Buffers;
	if (m_Acq)			delete m_Acq;
	if (m_View)			delete m_View;
	if (m_ImageWnd)		delete m_ImageWnd;

	m_Xfer		= 0;
	m_Buffers	= 0;
	m_Acq		= 0;
	m_View		= 0;
	m_ImageWnd	= 0;
}


void MEAControlDlg::Coreco_Display_Source1_Image(imageRGB4P rgb4, CRect &dRect, bool flag)
{
	CClientDC tmp(&m_ViewWnd);
	HDC	 hDC;
	INT nMode;
	BITMAPINFO bmi;
	MSG message;

	hDC = tmp.GetSafeHdc();

	nMode = ::SetStretchBltMode(hDC, COLORONCOLOR);

	memset(&bmi, 0, sizeof(bmi));
	bmi.bmiHeader.biSize			= sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth			= rgb4->width;
	bmi.bmiHeader.biHeight			= -rgb4->height; // top-down image
	bmi.bmiHeader.biBitCount		= 32;
	bmi.bmiHeader.biSizeImage		= rgb4->width * rgb4->height * 4;
	bmi.bmiHeader.biPlanes			= 1;
	bmi.bmiHeader.biXPelsPerMeter	= 0;
	bmi.bmiHeader.biYPelsPerMeter	= 0;
	bmi.bmiHeader.biClrImportant	= 0;
	bmi.bmiHeader.biClrUsed			= 0;
	bmi.bmiHeader.biCompression		= BI_RGB;

	StretchDIBits(hDC, dRect.left, dRect.top, (dRect.Width())*BMPScale, (dRect.Height()) *BMPScale, 0, 0, rgb4->width, rgb4->height, rgb4->image, &bmi, DIB_RGB_COLORS, SRCCOPY);
	::SetStretchBltMode(hDC, nMode);

	ReleaseDC(&tmp);

	if (::PeekMessage(&message, NULL, 0, 0, PM_REMOVE))
	{	::TranslateMessage(&message);
		::DispatchMessage(&message);
	}
}

void MEAControlDlg::Coreco_Image1_StatusCallback(SapAcqCallbackInfo *pInfo)
{

}

void MEAControlDlg::Coreco_Image1_XferCallback(SapXferCallbackInfo *pInfo)
{
	MEAControlDlg	*pDlg = (MEAControlDlg *)pInfo->GetContext();

	BYTE			*pData, *pByte, *pRGB4, *pXt = 0;
    XtCmdImg *xt = 0;
//	WORD			*word;
	BYTE			*pLine;
	int				count;
	CString			temp;

	pDlg->m_Buffers->GetAddress((void **)(&pData));			// Get image buffer start memory address.

	int bpp		= pDlg->m_Buffers->GetBytesPerPixel();		// bpp:		get number of bytes required to store a single image
	int pitch	= pDlg->m_Buffers->GetPitch();				// pitch:	get number of bytes between two consecutive lines of all the buffer resource
	int width	= pDlg->m_Buffers->GetWidth();				// width:	get the width (in pixel) of the image
	int height	= pDlg->m_Buffers->GetHeight();				// Height:	get the height of the image

    if (SpikeGL_Mode) {
        size_t len = (sizeof(XtCmdImg)-1) + width*height;
        if (size_t(pDlg->m_spikeGLFrameBuf.size()) < len) pDlg->m_spikeGLFrameBuf.resize(len);
        if (size_t(pDlg->m_spikeGLFrameBuf.size()) >= len ) {
            xt = (XtCmdImg *)&(pDlg->m_spikeGLFrameBuf[0]);
            xt->init(width, height);
            pXt = xt->img;
        }
    }

	// ring buffer counter, keep track which buffer to read. 
	pDlg->m_RingBufferCounter++;							// start camera #1 ring buffer counter, total ringbuffer for camera #1 is BSIZE=4.	
	if (pDlg->m_RingBufferCounter >= BSIZE)	pDlg->m_RingBufferCounter = 0;
	count = pDlg->m_RingBufferCounter;

	pByte = pDlg->m_DecodedByte[count]->image;			// Assign the Gray Image Pixel address to local pByte
	pRGB4 = pDlg->m_DecodedRGB4[count]->image;			// Assign the RGB Image Pixel address to local pRGB4

	g_dataReady.ResetEvent();

	for (int i = 0; i<height; i++)						// width
	{
		pLine = pData;									// copy the image buffer address
		for (int j = 0; j<width; j++)					// line
		{	pByte[0] = pLine[0];						// gray image				

			pRGB4[0] = pLine[0];						// R
			pRGB4++;									// increase address by 8 bit
			pRGB4[0] = pLine[0];						// G
			pRGB4++;									// increase address by 8 bit
			pRGB4[0] = pLine[0];						// B
			pRGB4++;									// increase address by 8 bit
			pRGB4++;									// in case RGB, pixel data is two word (32-bit)
            if (pXt) *pXt++ = *pLine; // for SpikeGL
            pLine++;									// increase address by 8 bit becasue 1 byte image
            pByte++;									// increase gray image address by 8 bit
			DataGridRaw[i][j] = pLine[0];
			temp.Format(_T("%x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x"), DataGridRaw[0][0], DataGridRaw[0][1], DataGridRaw[0][2], DataGridRaw[0][3], DataGridRaw[0][4], DataGridRaw[0][5], DataGridRaw[0][6], DataGridRaw[0][7], DataGridRaw[0][8], DataGridRaw[0][9], DataGridRaw[0][10], DataGridRaw[0][11], DataGridRaw[0][12], DataGridRaw[0][13], DataGridRaw[0][14], DataGridRaw[0][15], DataGridRaw[0][16], DataGridRaw[0][17], DataGridRaw[0][18], DataGridRaw[0][19], DataGridRaw[0][20], DataGridRaw[0][21], DataGridRaw[0][22], DataGridRaw[0][23]);
			pDlg->m_DataGridRaw1.SetWindowTextW(temp);
		}
		pData += pitch;
	}

	g_dataReady.SetEvent();

    // for SpikeGL
    if (xt) {
        if (!pDlg->m_spikeGL->pushCmd(xt)) { /* todo:.. handle error here!*/ }
    }

	// display image #1 
	CRect rect(0, 0, width, height);
	pDlg->Coreco_Display_Source1_Image(pDlg->m_DecodedRGB4[count], rect, 1); // show original image
}

bool MEAControlDlg::Coreco_Board_Setup(const char *Coreco_FileName)
{	
	struct stat stFileInfo;
	char C_filename[200];
	CString temp;
	SapAcqDevice device;
	char CameraSetParameter[100];
	char ServerName[100];

	// check whether or not the camera files exists	
	strncpy_s(C_filename, Coreco_FileName, 200);
    C_filename[199] = 0;
	if (stat(C_filename, &stFileInfo) != 0)
	{	MessageBox(CString("Cannot Find Camera Description File, Application Abort"), CString("Error Message"));
		exit(1);
		return false;						// couldn't find the camera ccf file.
	}
	else
		m_CameraConfigFileDir.SetWindowTextW(CString(C_filename));

	// Assign the camera configuration parameters
	SapManager::GetServerName(1, ServerName, sizeof(ServerName));
	m_ServerName.SetWindowTextW(CString(ServerName));	
	SapManager::GetResourceName(ServerName, SapManager::ResourceAcq, 0, CameraSetParameter, sizeof(CameraSetParameter));
	m_DeviceName.SetWindowTextW(CString(CameraSetParameter));
	m_Version.SetWindowTextW(CString("3.01"));
	m_TapSize.SetWindowTextW(CString("8"));
	m_BitSize.SetWindowTextW(CString("8"));
	m_FrameSize.SetWindowTextW(CString("32x36"));

	// Assign the camera configuration file the Coreco board sellected.
	USES_CONVERSION;
	Coreco_Camera_File_Name = T2A(CString(C_filename));
	Coreco_pLoc = new SapLocation(ServerName, 0);

	// Reset and free up board buffers for newly sellected board 
	FreeUp_Resource();

	// Prepare the board for image acquisition.
	m_Acq		= new SapAcquisition(*Coreco_pLoc, (char *)Coreco_Camera_File_Name);
	m_Buffers	= new SapBufferWithTrash(2, m_Acq);
	
	m_View		= new SapView(m_Buffers,	m_ViewWnd.GetSafeHwnd());
	m_ImageWnd	= new CImageWnd(m_View,		&m_ViewWnd, NULL, NULL, this);
	m_Xfer		= new SapAcqToBuf(m_Acq,	m_Buffers,	Coreco_Image1_XferCallback, this);

	// check video frame grabber for error 
	if (m_Acq		&& !*m_Acq		&&	!m_Acq->Create())			OnCancel();
	if (m_Buffers	&& !*m_Buffers	&&	!m_Buffers->Create())		OnCancel();

	if (m_Xfer		&& !*m_Xfer		&&	!m_Xfer->Create())			OnCancel();
	if (m_View		&& !*m_View		&&	!m_View->Create())			OnCancel();

	//-----------------------------------------------------------------------------------------
	// Check Initial Clock, Frame and Line Signals 
	//-----------------------------------------------------------------------------------------
	m_Acq->GetSignalStatus(SapAcquisition::SignalPixelClk1Present, &PixelCLKSignal1);
	if (PixelCLKSignal1)
		m_ClockSignal1.SetCheck(TRUE);
	else
		m_ClockSignal1.SetCheck(FALSE);

	m_Acq->GetSignalStatus(SapAcquisition::SignalPixelClk2Present, &PixelCLKSignal2);
	if (PixelCLKSignal2)
		m_ClockSignal2.SetCheck(TRUE);
	else
		m_ClockSignal2.SetCheck(FALSE);

	m_Acq->GetSignalStatus(SapAcquisition::SignalPixelClk3Present, &PixelCLKSignal3);
	if (PixelCLKSignal3)
		m_Clocksignal3.SetCheck(TRUE);
	else
		m_Clocksignal3.SetCheck(FALSE);

	m_Acq->GetSignalStatus(SapAcquisition::SignalHSyncPresent, &HSyncSignal);
	if (HSyncSignal)
		m_LineSignal.SetCheck(TRUE);
	else
		m_LineSignal.SetCheck(FALSE);

	m_Acq->GetSignalStatus(SapAcquisition::SignalVSyncPresent, &VSyncSignal);
	if (VSyncSignal)
		m_FrameSignal.SetCheck(TRUE);
	else
		m_FrameSignal.SetCheck(FALSE);

	//-----------------------------------------------------------------------------------------
	// Check Initial Buffer Contains 
	//-----------------------------------------------------------------------------------------
	int bpp		=	m_Buffers->GetBytesPerPixel();		// bpp:		get number of bytes required to store a single image
	int pitch	=	m_Buffers->GetPitch();				// pitch:	get number of bytes between two consecutive lines of all the buffer resource
	int width	=	m_Buffers->GetWidth();				// width:	get the width (in pixel) of the image
	int height	=	m_Buffers->GetHeight();				// Height:	get the height of the image
	int depth	=	m_Buffers->GetPixelDepth();			// PixelDepth: get pixel depth 
	int format	=	m_Buffers->GetFormat();				// PixelDepth: get pixel depth 

	temp.Format(_T("%d"), bpp);
	m_PixelBytes.SetWindowTextW(temp);
	temp.Format(_T("%d"), pitch);
	m_Pitch.SetWindowTextW(temp);
	temp.Format(_T("%d"), width);
	m_TotalPixel.SetWindowTextW(temp);
	temp.Format(_T("%d"), height);
	m_TotalLine.SetWindowTextW(temp);
	temp.Format(_T("%d"), depth);
	m_PixelDepth.SetWindowTextW(temp);
	temp.Format(_T("%d"), format);
	m_Format.SetWindowTextW(temp);

	//-----------------------------------------------------------------------------------------
	// get max. image size, width and height 
	//-----------------------------------------------------------------------------------------
	int CorecoImageWidth = m_Buffers->GetWidth();
	if (CorecoImageWidth >= MaxDisplaySize) CorecoImageWidth = MaxDisplaySize;

	int CorecoImageHeight = m_Buffers->GetHeight();
	if (CorecoImageHeight >= MaxDisplaySize) CorecoImageHeight = MaxDisplaySize;

	//-----------------------------------------------------------------------------------------
	// set window display size, where is image display on window  
	//-----------------------------------------------------------------------------------------
	xTop = BMPX;
	yTop = BMPY;
	xBot = CorecoImageWidth;
	yBot = CorecoImageHeight;
	m_ViewWnd.SetWindowPos(NULL, xTop, yTop, CorecoImageWidth + 5, CorecoImageHeight + 5, NULL);

	// Allocate Frame to image processing ring buffer, 4 rings are used
	for (int i = 0; i<BSIZE; i++) // Four Frame Ring Buffer
	{	// allocate memory for gray image input, note: 8-bit data depth is used here
		if (m_DecodedByte[i])		FreeImageMemory(m_DecodedByte[i]);
		m_DecodedByte[i] = AllocImageMemory(m_Buffers->GetWidth(), m_Buffers->GetHeight(), 4);
		if (!m_DecodedByte[i])
		{	MessageBox(CString("Cannot Allocate Coreca Gray Scale Buffer, Application Abort"), CString("Coreco Error Message"));
			exit(1);
			return false;
		}

		// allocate memory for RGB image input, note: 8-bit for each color data depth is used here
		if (m_DecodedRGB4[i])		FreeImageRGB4Memory(m_DecodedRGB4[i]);
		m_DecodedRGB4[i] = AllocImageRGB4Memory(m_Buffers->GetWidth(), m_Buffers->GetHeight());
		if (!m_DecodedRGB4[i])
		{	MessageBox(CString("Cannot Allocate Coreca RGB Buffer, Application Abort"), CString("Coreco Error Message"));
			exit(1);
			return false;
		}
	}

	// Grad the input video frame.
	m_Xfer->Grab();
	return true;
}

// **************************************************************************************************
// RS232 Serial Port #1
// Serial Communication with FPGA via CameraLink Mapped RS232 port
// **************************************************************************************************
int				index0 = -1, index1 = -1, index2 = -1, index3 = -1, index4 = -1, index5 = -1;
CString			PortConfig, PortNum;
DCB				Port1DCB;
COMMTIMEOUTS	CommTimeouts;
HANDLE			hPort1;
int				CloseUart(void);
int				WriteUart(unsigned char *, int);
int				Serial_OK = -1;
char			buf3[100], lastError[1024];
byte			buf2[100];
UINT			Background_Update(LPVOID);
int				Return_Text = 24;  // this number must match with the FPGA uC code
int				R_Data1, R_Data2, R_Data3;

// Serial Send Commands, ~: sync, xx:Command Code, xxx:Value 1, xxxxxx:Value2 (max. 18-bit)
int				CMD;
int				CMD_Code[10] = {	0,		// null
									1,		// LED code
									2,		// Digital Output
									3,		// Test Intan-64 One by One
									4,		// write to Intan64 register
									5,		// validate Intan64 register
									6		// Continuous ADC
								};

int				Register[25] = {	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21};

const			int CMD_LED = 1;
int				CMD_Digital_Out = 2;
int				Val1;
long			Val2;
char			sync = '~';

// LED Display 
int				LED_CtlCode;

// Digital Output Control Code
int				DigitalOut_Code;


IMPLEMENT_DYNAMIC(MEAControlDlg, CDialogEx);

MEAControlDlg::MEAControlDlg(CWnd* pParent /*=NULL*/)
	: CDialogEx(MEAControlDlg::IDD, pParent)
	, m_BuffBias_Value(0)
{
	m_visible = !SpikeGL_Mode || !::getenv("SPIKEGL_PARMS");
    m_spikeGL = 0; m_spikeGLIn = 0;
	EnableActiveAccessibility();
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
	m_pAutoProxy = NULL;
	
	if (SpikeGL_Mode) {
        m_spikeGLIn = new SpikeGLInputThread;
        m_spikeGL = new SpikeGLOutThread;
        m_spikeGLIn->start();
        m_spikeGL->start();
    }	
}

void MEAControlDlg::handleSpikeGLEnvParms()
{
	char *envstr = (char *)getenv("SPIKEGL_PARMS");
	
	if (envstr) {
		m_visible = FALSE;
		char *e = _strdup(envstr);
		int i = 0;
		for (char *pe, *p = e; *p && i<5; p=pe+1, ++i) {
			pe=strchr(p,',');
			if (!pe) pe=p+strlen(p);
			*pe = 0;
			int num = 0;
			if (sscanf(p,"%d",&num)==1) {
				switch (i) {
					case 0: // COM
						index0 = num >= 0 && num <= 3 ? num : index0;
						break;
					case 1: // speed
                        index1 = num >= 0 && num <= 8 ? num : index1;
						break;
					case 2: // bits
                        index2 = num >= 0 && num <= 1 ? num : index2;
						break;
					case 3: // parity
						index3 = num >= 0 && num <= 2 ? num : index3;
						break;
					case 4: // stop
						index4 = num >= 0 && num <= 1 ? num : index4;
						break;
					case 5: // flow control
						index5 = num >= 0 && num <= 2 ? num : index5;
						break;
				}
			}
		}
		free(e);
		configure();
	}	
}

LRESULT MEAControlDlg::SpikeGLIdleHandler(WPARAM p1, LPARAM p2)
{
    (void)p1, (void)p2;
    if (!m_spikeGLIn) return FALSE;
    int fail = 0;
    while (m_spikeGLIn->cmdQSize() && fail < 10) {
        std::vector<BYTE> buf;
        XtCmd *xt;
        if ((xt = m_spikeGLIn->popCmd(buf, 10))) {
            // todo.. handle commands here...
            handleSpikeGLCommand(xt);
        }
        else ++fail;

    }
    if (!m_spikeGLIn->cmdQSize()) Sleep(5); // idle throttle...
    return TRUE; // returning true makes this function be called repeatedly on idle..
}


void MEAControlDlg::handleSpikeGLCommand(XtCmd *xt)
{
    (void)xt;
    switch (xt->cmd) {
    case XtCmd_Exit:
        m_spikeGL->pushConsoleDebug("Got exit command.. exiting gracefully...");
        exit(1);
        break;
    case XtCmd_Test:
        m_spikeGL->pushConsoleDebug("Got 'TEST' command, replying with this debug message!");
        break;
    case XtCmd_GrabFrames:
        m_spikeGL->pushConsoleDebug("Got 'GrabFrames' command");
        if (Serial_OK) {
            m_FrameGrabberEnable.SetCheck(1);
            OnBnClickedFramegrabberenable1();
            m_spikeGL->pushConsoleMsg("GrabFrames command executed");
        } else {
            m_spikeGL->pushConsoleError("GrabFrames command failed, serial port not OK");
        }
        break;
    case XtCmd_FPGAProto: {
        XtCmdFPGAProto *x = (XtCmdFPGAProto *)xt;
        if (x->len >= 16) {
            m_spikeGL->pushConsoleDebug("Got 'FPGAProto' command");
            FPGA_Protocol_Construction(x->cmd_code, x->value1, x->value2);
        }
    }
        break;
    default: // ignore....?
        break;
    }
}

void MEAControlDlg::doSpikeGLAutoStart()
{
    OnBnClickedOpen(); // may throw error
    Sleep(100);
    m_spikeGL->pushConsoleMsg("Ready.");
}

MEAControlDlg::~MEAControlDlg()
{

	// If there is an automation proxy for this dialog, set
	//  its back pointer to this dialog to NULL, so it knows
	//  the dialog has been deleted.
	if (m_pAutoProxy != NULL)
		m_pAutoProxy->m_pDialog = NULL;
	
	for (int i = 0; i<BSIZE; i++)
	{	if (m_DecodedByte[i])			FreeImageMemory(m_DecodedByte[i]);
		m_DecodedByte[i] = 0;

		if (m_DecodedRGB4[i])			FreeImageRGB4Memory(m_DecodedRGB4[i]);
		m_DecodedRGB4[i] = 0;
	}

    if (m_spikeGL) delete m_spikeGL; m_spikeGL = 0;
    // if (m_spikeGLIn) delete m_spikeGLIn; m_spikeGLIn = 0; // may hang due to crappy impl...
}

void MEAControlDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, Port1_Sel, m_GetPort1Num);
	DDX_Control(pDX, IDC_EDIT1, m_Port1Format);
	DDX_Control(pDX, Port1_Buad, m_GetPort1BuadRate);
	DDX_Control(pDX, Port1_Data, m_GetPort1DataBit);
	DDX_Control(pDX, Port1_Parity, m_GetPort1Parity);
	DDX_Control(pDX, Port1_Stop, m_GetPort1StopBit);
	DDX_Control(pDX, IDC_LIST1, m_Port1Message);
	DDX_Control(pDX, Port1_Open, m_Port1Enable);
	DDX_Control(pDX, LED_8, m_LED8);
	DDX_Control(pDX, LED_7, m_LED7);
	DDX_Control(pDX, LED_6, m_LED6);
	DDX_Control(pDX, LED_5, m_LED5);
	DDX_Control(pDX, LED_4, m_LED4);
	DDX_Control(pDX, LED_3, m_LED3);
	DDX_Control(pDX, LED_2, m_LED2);
	DDX_Control(pDX, LED_1, m_LED1);
	DDX_Control(pDX, LED_Ctl_Code, m_LED_Ctl_Code);
	DDX_Control(pDX, LED_Reset, m_LED_Reset);
	DDX_Control(pDX, Digital_Out_1, m_DigitalOut_1);
	DDX_Control(pDX, Dout_Reset, m_DoutRst);
	DDX_Control(pDX, Digital_Out_2, m_DigitalOut2);
	DDX_Control(pDX, Digital_Out_3, m_DigitalOut3);
	DDX_Control(pDX, Digital_Out_4, m_DigitalOut4);
	DDX_Control(pDX, Digital_Out_5, m_DigitalOut5);
	DDX_Control(pDX, Digital_Out_6, m_DigitalOut6);
	DDX_Control(pDX, Digital_Out_7, m_DigitalOut7);
	DDX_Control(pDX, Digital_Out_8, m_DigitalOut8);
	DDX_Control(pDX, Digital_Out_9, m_DigitalOut9);
	DDX_Control(pDX, Digital_Out_10, m_DigitalOut10);
	DDX_Control(pDX, Digital_Out_11, m_DigitalOut11);
	DDX_Control(pDX, Digital_Out_12, m_DigitalOut12);
	DDX_Control(pDX, Digital_Out_13, m_DigitalOut13);
	DDX_Control(pDX, Digital_Out_14, m_DigitalOut14);
	DDX_Control(pDX, Digital_Out_15, m_DigitalOut15);
	DDX_Control(pDX, Digital_Out_16, m_DigitalOut16);
	DDX_Control(pDX, Dout_CTL_Code, m_DigitalOut_Code);

	DDX_Control(pDX, Intan1A, m_Intan1A);
	DDX_Control(pDX, Intan2A, m_Intan2A);
	DDX_Control(pDX, Intan3A, m_Intan3A);
	DDX_Control(pDX, Intan4A, m_Intan4A);
	DDX_Control(pDX, Intan5A, m_Intan5A);
	DDX_Control(pDX, Intan6A, m_Intan6A);
	DDX_Control(pDX, Intan7A, m_Intan7A);
	DDX_Control(pDX, Intan8A, m_Intan8A);
	DDX_Control(pDX, Intan9A, m_Intan9A);
	DDX_Control(pDX, Intan10A, m_Intan10A);
	DDX_Control(pDX, Intan11A, m_Intan11A);
	DDX_Control(pDX, Intan12A, m_Intan12A);
	DDX_Control(pDX, Intan13A, m_Intan13A);
	DDX_Control(pDX, Intan14A, m_Intan14A);
	DDX_Control(pDX, Intan15A, m_Intan15A);
	DDX_Control(pDX, Intan16A, m_Intan16A);
	DDX_Control(pDX, Intan17A, m_Intan17A);
	DDX_Control(pDX, Intan18A, m_Intan18A);
	DDX_Control(pDX, Intan19A, m_Intan19A);
	DDX_Control(pDX, Intan20A, m_Intan20A);
	DDX_Control(pDX, Intan21A, m_Intan21A);
	DDX_Control(pDX, Intan22A, m_Intan22A);
	DDX_Control(pDX, Intan23A, m_Intan23A);
	DDX_Control(pDX, Intan24A, m_Intan24A);
	DDX_Control(pDX, Intan25A, m_Intan25A);
	DDX_Control(pDX, Intan26A, m_Intan26A);
	DDX_Control(pDX, Intan27A, m_Intan27A);
	DDX_Control(pDX, Intan28A, m_Intan28A);
	DDX_Control(pDX, Intan29A, m_Intan29A);
	DDX_Control(pDX, Intan30A, m_Intan30A);
	DDX_Control(pDX, Intan31A, m_Intan31A);
	DDX_Control(pDX, Intan32A, m_Intan32A);
	DDX_Control(pDX, Intan33A, m_Intan33A);
	DDX_Control(pDX, Intan34A, m_Intan34A);
	DDX_Control(pDX, Intan35A, m_Intan35A);
	DDX_Control(pDX, Intan36A, m_Intan36A);

	DDX_Control(pDX, IntanChipNum, m_IntanChipNum);
	DDX_Control(pDX, Intan64OperationMode, m_IntanOperationMode);
	DDX_Control(pDX, ClearPortMonitor, m_ClearPortMonitor);
	DDX_Control(pDX, BufferBiasE, m_BufferBias);
	DDX_Control(pDX, MuxBiasE, m_MuxBias);
	DDX_Control(pDX, MuxLoadE, m_MuxLoad);
	DDX_Control(pDX, OutputOffsetE, m_OutputOffset);
	DDX_Control(pDX, ImpedanceControlE, m_ImpedanceControl);
	DDX_Control(pDX, ImpedanceDACE, m_ImpedanceDAC);
	DDX_Control(pDX, ImpedanceAMPE, m_ImpedanceAMP);
	DDX_Control(pDX, AMPBandwidth1E, m_AMPBandwidth1);
	DDX_Control(pDX, AMPBandwidth2E, m_AMPBandwidth2);
	DDX_Control(pDX, AMPBandwidth3E, m_AMPBandwidth3);
	DDX_Control(pDX, AMPBandwidth4E, m_AMPBandwidth4);
	DDX_Control(pDX, AMPBandwidth5E, m_AMPBandwidth5);
	DDX_Control(pDX, AMPBandwidth6E, m_AMPBandwidth6);
	DDX_Control(pDX, AMPPower1E, m_AMPPower1);
	DDX_Control(pDX, AMPPower2E, m_AMPPOWER2);
	DDX_Control(pDX, AMPPower3E, m_AMPPower3);
	DDX_Control(pDX, AMPPower4E, m_AMPPower4);
	DDX_Control(pDX, AMPPower5E, m_AMPPower5);
	DDX_Control(pDX, AMPPower8E, m_AMPPower8);
	DDX_Control(pDX, AMPPower7E, m_AMPPower7);
	DDX_Control(pDX, AMPPower6E, m_AMPPower6);
	DDX_Control(pDX, RefDataA, m_Data_A);
	DDX_Control(pDX, RefDataB, m_Data_B);
	DDX_Control(pDX, ADCConfigE, m_ADCConfig);
	DDX_Text(pDX, BufferBiasE, m_BuffBias_Value);
	DDX_Control(pDX, IDC_VIEW_WND, m_ViewWnd);
	DDX_Control(pDX, IDC_VIEW_WND2, m_ViewWnd2);
	DDX_Control(pDX, StatusCameraConfgFileLocation, m_CameraConfigFileDir);
	DDX_Control(pDX, StatusSize, m_FrameSize);
	DDX_Control(pDX, StatusTab, m_TapSize);
	DDX_Control(pDX, Status_Bit_Size, m_BitSize);
	DDX_Control(pDX, StatusServerName, m_ServerName);
	DDX_Control(pDX, StatusDeviceName, m_DeviceName);
	DDX_Control(pDX, StatusVersion, m_Version);
	DDX_Control(pDX, CLKSignal1, m_ClockSignal1);
	DDX_Control(pDX, CLKSignal2, m_ClockSignal2);
	DDX_Control(pDX, CLKSignal3, m_Clocksignal3);
	DDX_Control(pDX, FrameSignal, m_FrameSignal);
	DDX_Control(pDX, LineSignal, m_LineSignal);
	DDX_Control(pDX, StatusPixelBytes, m_PixelBytes);
	DDX_Control(pDX, StatusTotalPixel, m_TotalPixel);
	DDX_Control(pDX, StatusPitch, m_Pitch);
	DDX_Control(pDX, StatusTotalLine, m_TotalLine);
	DDX_Control(pDX, StatusPixelDepth, m_PixelDepth);
	DDX_Control(pDX, StatusFormat, m_Format);
	DDX_Control(pDX, FrameGrabberEnable1, m_FrameGrabberEnable);
	DDX_Control(pDX, DataGridRaw1, m_DataGridRaw1);
}

BEGIN_MESSAGE_MAP(MEAControlDlg, CDialogEx)
	ON_WM_CLOSE()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_WM_CTLCOLOR()
    ON_WM_WINDOWPOSCHANGING()
	ON_BN_CLICKED(IDCANCEL,			&MEAControlDlg::OnBnClickedCancel)
	ON_CBN_SELCHANGE(Port1_Sel,		&MEAControlDlg::OnCbnSelchangeSel)
	ON_CBN_SELCHANGE(Port1_Buad,	&MEAControlDlg::OnCbnSelchangeBuad)
	ON_CBN_SELCHANGE(Port1_Data,	&MEAControlDlg::OnCbnSelchangeData)
	ON_CBN_SELCHANGE(Port1_Parity,	&MEAControlDlg::OnCbnSelchangeParity)
	ON_CBN_SELCHANGE(Port1_Stop,	&MEAControlDlg::OnCbnSelchangeStop)
	ON_BN_CLICKED(Port1_Open,		&MEAControlDlg::OnBnClickedOpen)
	ON_BN_CLICKED(LED_1,			&MEAControlDlg::OnBnClicked1)
	ON_BN_CLICKED(LED_Reset,		&MEAControlDlg::OnBnClickedReset)
	ON_BN_CLICKED(LED_2,			&MEAControlDlg::OnBnClicked2)
	ON_BN_CLICKED(LED_3,			&MEAControlDlg::OnBnClicked3)
	ON_BN_CLICKED(LED_4,			&MEAControlDlg::OnBnClicked4)
	ON_BN_CLICKED(LED_5,			&MEAControlDlg::OnBnClicked5)
	ON_BN_CLICKED(LED_6,			&MEAControlDlg::OnBnClicked6)
	ON_BN_CLICKED(LED_7,			&MEAControlDlg::OnBnClicked7)
	ON_BN_CLICKED(LED_8,			&MEAControlDlg::OnBnClicked8)
	ON_BN_CLICKED(Digital_Out_1,	&MEAControlDlg::OnBnClickedOut1)
	ON_BN_CLICKED(Dout_Reset,		&MEAControlDlg::OnBnClickedDoutReset)

	ON_BN_CLICKED(Digital_Out_2,	&MEAControlDlg::OnBnClickedOut2)
	ON_BN_CLICKED(Digital_Out_3,	&MEAControlDlg::OnBnClickedOut3)
	ON_BN_CLICKED(Digital_Out_4,	&MEAControlDlg::OnBnClickedOut4)
	ON_BN_CLICKED(Digital_Out_5,	&MEAControlDlg::OnBnClickedOut5)
	ON_BN_CLICKED(Digital_Out_6,	&MEAControlDlg::OnBnClickedOut6)
	ON_BN_CLICKED(Digital_Out_7,	&MEAControlDlg::OnBnClickedOut7)
	ON_BN_CLICKED(Digital_Out_8,	&MEAControlDlg::OnBnClickedOut8)
	ON_BN_CLICKED(Digital_Out_9,	&MEAControlDlg::OnBnClickedOut9)
	ON_BN_CLICKED(Digital_Out_10,	&MEAControlDlg::OnBnClickedOut10)
	ON_BN_CLICKED(Digital_Out_11,	&MEAControlDlg::OnBnClickedOut11)
	ON_BN_CLICKED(Digital_Out_12,	&MEAControlDlg::OnBnClickedOut12)
	ON_BN_CLICKED(Digital_Out_13,	&MEAControlDlg::OnBnClickedOut13)
	ON_BN_CLICKED(Digital_Out_14,	&MEAControlDlg::OnBnClickedOut14)
	ON_BN_CLICKED(Digital_Out_15,	&MEAControlDlg::OnBnClickedOut15)
	ON_BN_CLICKED(Digital_Out_16,	&MEAControlDlg::OnBnClickedOut16)
	ON_BN_CLICKED(Intan_Test,		&MEAControlDlg::OnBnClickedIntan64Test)
	ON_BN_CLICKED(ClearPortMonitor, &MEAControlDlg::OnBnClickedClearportmonitor)
	ON_BN_CLICKED(IDC_TestClear,	&MEAControlDlg::OnBnClickedTestclear)
	ON_BN_CLICKED(BufferBiasS,		&MEAControlDlg::OnBnClickedBufferbiass)
	ON_BN_CLICKED(ADCConfigS,		&MEAControlDlg::OnBnClickedAdcconfigs)
	ON_BN_CLICKED(MuxBiasS,			&MEAControlDlg::OnBnClickedMuxbiass)
	ON_BN_CLICKED(MuxLoadS,			&MEAControlDlg::OnBnClickedMuxloads)
	ON_BN_CLICKED(OutputOffsetS,	&MEAControlDlg::OnBnClickedOutputoffsets)
	ON_BN_CLICKED(ImpedanceControlS,&MEAControlDlg::OnBnClickedImpedancecontrols)
	ON_BN_CLICKED(ImpedanceDACS,	&MEAControlDlg::OnBnClickedImpedancedacs)
	ON_BN_CLICKED(ImpedanceAmpS,	&MEAControlDlg::OnBnClickedImpedanceamps)
	ON_BN_CLICKED(AMPBandwidthS1,	&MEAControlDlg::OnBnClickedAmpbandwidths1)
	ON_BN_CLICKED(AMPBandwidthS2,	&MEAControlDlg::OnBnClickedAmpbandwidths2)
	ON_BN_CLICKED(AMPBandwidthS3,	&MEAControlDlg::OnBnClickedAmpbandwidths3)
	ON_BN_CLICKED(AMPBandwidthS4,	&MEAControlDlg::OnBnClickedAmpbandwidths4)
	ON_BN_CLICKED(AMPBandwidthS5,	&MEAControlDlg::OnBnClickedAmpbandwidths5)
	ON_BN_CLICKED(AMPBandwidthS6,	&MEAControlDlg::OnBnClickedAmpbandwidths6)
	ON_BN_CLICKED(AmpPowerS1,		&MEAControlDlg::OnBnClickedAmppowers1)
	ON_BN_CLICKED(AmpPowerS2,		&MEAControlDlg::OnBnClickedAmppowers2)
	ON_BN_CLICKED(AmpPowerS3,		&MEAControlDlg::OnBnClickedAmppowers3)
	ON_BN_CLICKED(AmpPowerS4,		&MEAControlDlg::OnBnClickedAmppowers4)
	ON_BN_CLICKED(AmpPowerS5,		&MEAControlDlg::OnBnClickedAmppowers5)
	ON_BN_CLICKED(AmpPowerS6,		&MEAControlDlg::OnBnClickedAmppowers6)
	ON_BN_CLICKED(AmpPowerS7,		&MEAControlDlg::OnBnClickedAmppowers7)
	ON_BN_CLICKED(AmpPowerS8,		&MEAControlDlg::OnBnClickedAmppowers8)
	ON_BN_CLICKED(ContinuousADC,	&MEAControlDlg::OnBnClickedContinuousadc)
	ON_BN_CLICKED(FrameGrabberEnable1, &MEAControlDlg::OnBnClickedFramegrabberenable1)
    ON_MESSAGE(WM_KICKIDLE, SpikeGLIdleHandler)
END_MESSAGE_MAP()


void MEAControlDlg::OnWindowPosChanging(WINDOWPOS* lpwndpos)
{
	if (!m_visible) {
		lpwndpos->flags &= ~SWP_SHOWWINDOW;
	}
	
	CDialogEx::OnWindowPosChanging(lpwndpos);
}


// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void MEAControlDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

// The system calls this function to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR MEAControlDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

// Automation servers should not exit when a user closes the UI
//  if a controller still holds on to one of its objects.  These
//  message handlers make sure that if the proxy is still in use,
//  then the UI is hidden but the dialog remains around if it
//  is dismissed.

void MEAControlDlg::OnClose()
{
	if (CanExit())
		CDialogEx::OnClose();
}

void MEAControlDlg::OnOK()
{
	if (CanExit())
		CDialogEx::OnOK();
}

void MEAControlDlg::OnCancel()
{
	if (CanExit())
		CDialogEx::OnCancel();
}

BOOL MEAControlDlg::CanExit()
{
	// If the proxy object is still around, then the automation
	//  controller is still holding on to this application.  Leave
	//  the dialog around, but hide its UI.
	if (m_pAutoProxy != NULL)
	{
		ShowWindow(SW_HIDE);
		return FALSE;
	}

	return TRUE;
}

// MEAControlDlg message handlers
// **************************************************************************************************
//	11/2014
//	Parameters Initialization
// **************************************************************************************************
BOOL MEAControlDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon
	ShowWindow(SW_RESTORE);

	// Initialize RS232 Port 1 Default 
	index0 = 1;
	m_GetPort1Num.SetCurSel(1);
	index1 = 1;
	m_GetPort1BuadRate.SetCurSel(1);
	index2 = 0;
	m_GetPort1DataBit.SetCurSel(0);
	index3 = 0;
	m_GetPort1Parity.SetCurSel(0);
	index4 = 0;
	m_GetPort1StopBit.SetCurSel(0);
	index5 = 0;
    handleSpikeGLEnvParms();
    configure();

	// Background Data Update Thread
	CWinThread* BackGroundUpDate = AfxBeginThread(Background_Update, this, THREAD_PRIORITY_LOWEST);	// Create a New Thread -> FusionProc
		 
	OnBnClickedReset();				// LED Control	
	Control_Off();					// control display disable	
	OnBnClickedDoutReset();			// Digital Output Control
	m_IntanChipNum.SetCurSel(0);	// Intan Chip Number 1 Selected 
	m_IntanOperationMode.SetCurSel(0); 

	Register_Reset();

    m_Port1Format.SetWindowTextW(PortConfig);   //  SetWindowTextW(PortConfig);

    if (SpikeGL_Mode && !m_visible) doSpikeGLAutoStart();

	return TRUE;			// return TRUE  unless you set the focus to a control
}
// **************************************************************************************************
//	11/2014
//	Control GUI 
// **************************************************************************************************
void MEAControlDlg::Control_Off()
{
	m_LED1.EnableWindow(false);
	m_LED2.EnableWindow(false);
	m_LED3.EnableWindow(false);
	m_LED4.EnableWindow(false);
	m_LED5.EnableWindow(false);
	m_LED6.EnableWindow(false);
	m_LED7.EnableWindow(false);
	m_LED8.EnableWindow(false);
	m_LED_Reset.EnableWindow(false);

	m_DigitalOut_1.EnableWindow(false);
	m_DigitalOut2.EnableWindow(false);
	m_DigitalOut3.EnableWindow(false);
	m_DigitalOut4.EnableWindow(false);
	m_DigitalOut5.EnableWindow(false);
	m_DigitalOut6.EnableWindow(false);
	m_DigitalOut7.EnableWindow(false);
	m_DigitalOut8.EnableWindow(false);
	m_DigitalOut9.EnableWindow(false);
	m_DigitalOut10.EnableWindow(false);
	m_DigitalOut11.EnableWindow(false);
	m_DigitalOut12.EnableWindow(false);
	m_DigitalOut13.EnableWindow(false);
	m_DigitalOut14.EnableWindow(false);
	m_DigitalOut15.EnableWindow(false);
	m_DigitalOut16.EnableWindow(false);
	m_DoutRst.EnableWindow(false);
	m_FrameGrabberEnable.EnableWindow(false);
	Frame_Grabber_Enabled = 0;
	FreeUp_Resource();

}
void MEAControlDlg::Control_On()
{
	if (Serial_OK == 1)
	{	m_LED1.EnableWindow(true);
		m_LED2.EnableWindow(true);
		m_LED3.EnableWindow(true);
		m_LED4.EnableWindow(true);
		m_LED5.EnableWindow(true);
		m_LED6.EnableWindow(true);
		m_LED7.EnableWindow(true);
		m_LED8.EnableWindow(true);
		m_LED_Reset.EnableWindow(true);

		m_DigitalOut_1.EnableWindow(true);
		m_DigitalOut2.EnableWindow(true);
		m_DigitalOut3.EnableWindow(true);
		m_DigitalOut4.EnableWindow(true);
		m_DigitalOut5.EnableWindow(true);
		m_DigitalOut6.EnableWindow(true);
		m_DigitalOut7.EnableWindow(true);
		m_DigitalOut8.EnableWindow(true);
		m_DigitalOut9.EnableWindow(true);
		m_DigitalOut10.EnableWindow(true);
		m_DigitalOut11.EnableWindow(true);
		m_DigitalOut12.EnableWindow(true);
		m_DigitalOut13.EnableWindow(true);
		m_DigitalOut14.EnableWindow(true);
		m_DigitalOut15.EnableWindow(true);
		m_DigitalOut16.EnableWindow(true);
		m_DoutRst.EnableWindow(true);
		m_FrameGrabberEnable.EnableWindow(true);
		//Coreco_Board_Setup("D:\\Project-Vitax-7\\ProjectBuildingDocumentation\\ProjectBoardTestProgram\\J_2000+_Electrode_8tap_8bit.ccf\0");

	}
}

// **************************************************************************************************
//	11/2014
//	Close GUI 
// **************************************************************************************************
void MEAControlDlg::OnBnClickedCancel()
{	CloseUart();
	CDialogEx::OnCancel();
}

// **************************************************************************************************
//	11/2014
//	Background Data Updata Thread, Running at Back Ground
// **************************************************************************************************
CEvent g_terminate;

UINT Background_Update(LPVOID pParam)
{
	static int	current_tick;
	static int	last_tick = 0;
	CString	d;
	CString temp;

	MEAControlDlg *pDlg = (MEAControlDlg*)(pParam);
	// Thread related 
	DWORD_PTR oldmask = SetThreadAffinityMask(GetCurrentThread(), 2);

    const char *str = 0;
    if (oldmask == 0) {
        pDlg->m_Port1Message.AddString(CString(str = "No Background Update"));// exit(0);
        if (pDlg->m_spikeGL) pDlg->m_spikeGL->pushConsoleMsg(str);
    }

	while (::WaitForSingleObject(g_terminate, 0) != WAIT_OBJECT_0)		// terminate this thread when program exits
	{	current_tick = GetTickCount();
		// image display is always set to 30 frames per second
		if (current_tick - last_tick >= 100)
		{	last_tick = current_tick;
			if (Serial_OK == 1)
				if (pDlg->ReadUart(Return_Text) != 0)
				{	pDlg->m_Port1Message.AddString(CString(str=buf3));// exit(0);
                    if (pDlg->m_spikeGL) pDlg->m_spikeGL->pushConsoleMsg(str);
					pDlg->m_Port1Message.SetTopIndex(pDlg->m_Port1Message.GetCount() - 1);
					d.Format(_T("%d"), R_Data2);
					pDlg->m_Data_A.SetWindowTextW(d); // SetWindowTextW(d);
					d.Format(_T("%d"), R_Data3);
					pDlg->m_Data_B.SetWindowTextW(d); // SetWindowTextW(d);
					pDlg->SPI_Test_Display();
				}
			//-----------------------------------------------------------------------------------------
			// Update Clock, Frame and Line Signals 
			//-----------------------------------------------------------------------------------------
			if (Frame_Grabber_Enabled == 1)
			{
				pDlg->m_Acq->GetSignalStatus(SapAcquisition::SignalPixelClk1Present, &PixelCLKSignal1);
				if (PixelCLKSignal1)
					pDlg->m_ClockSignal1.SetCheck(TRUE);
				else
					pDlg->m_ClockSignal1.SetCheck(FALSE);

				pDlg->m_Acq->GetSignalStatus(SapAcquisition::SignalPixelClk2Present, &PixelCLKSignal2);
				if (PixelCLKSignal2)
					pDlg->m_ClockSignal2.SetCheck(TRUE);
				else
					pDlg->m_ClockSignal2.SetCheck(FALSE);

				pDlg->m_Acq->GetSignalStatus(SapAcquisition::SignalPixelClk3Present, &PixelCLKSignal3);
				if (PixelCLKSignal3)
					pDlg->m_Clocksignal3.SetCheck(TRUE);
				else
					pDlg->m_Clocksignal3.SetCheck(FALSE);

				pDlg->m_Acq->GetSignalStatus(SapAcquisition::SignalHSyncPresent, &HSyncSignal);
				if (HSyncSignal)
					pDlg->m_LineSignal.SetCheck(TRUE);
				else
					pDlg->m_LineSignal.SetCheck(FALSE);

				pDlg->m_Acq->GetSignalStatus(SapAcquisition::SignalVSyncPresent, &VSyncSignal);
				if (VSyncSignal)
					pDlg->m_FrameSignal.SetCheck(TRUE);
				else
					pDlg->m_FrameSignal.SetCheck(FALSE);
				//-----------------------------------------------------------------------------------------
				//temp.Format(_T("%d"), &DataGridRaw[0][0]);
				//pDlg->m_DataGridRaw1.SetWindowTextW(temp);
			}
		}
	}
	return 0;
}

//**************************************************************************************************
//	11/2014
//	Serial Communication Subroutine
//		Serial Port Configuration, Buad Rate, Data Length, Parity and Stop bit
//**************************************************************************************************
int MEAControlDlg::configure(void)
{	CString str, str1;

	// Change the DCB structure settings
	Port1DCB.fBinary = TRUE;				// Binary mode; no EOF check
	Port1DCB.fParity = TRUE;				// Enable parity checking 
	Port1DCB.fDsrSensitivity = FALSE;		// DSR sensitivity 
	Port1DCB.fErrorChar = FALSE;			// Disable error replacement 
	Port1DCB.fOutxDsrFlow = FALSE;			// No DSR output flow control 
	Port1DCB.fAbortOnError = FALSE;			// Do not abort reads/writes on error
	Port1DCB.fNull = FALSE;					// Disable null stripping 
	Port1DCB.fTXContinueOnXoff = TRUE;		// XOFF continues Tx 

	switch (index0) // Port Num
	{	case 0:		PortNum = "COM1";				break;
		case 1:		PortNum = "COM2";				break;
		case 2:		PortNum = "COM3";				break;
		case 3:		PortNum = "COM4";				break;
		default:	PortNum = "COM1";				break;
	}

	switch (index1) // BAUD Rate
	{	case 0:		Port1DCB.BaudRate = 230400;		break;
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

	switch (index2) // Number of bits/byte, 5-8 
	{	case 0:		Port1DCB.ByteSize = 8;			break;
		case 1:		Port1DCB.ByteSize = 7;			break;
		default:	Port1DCB.ByteSize = 0;			break;
	}

	switch (index3) // 0-4=no,odd,even,mark,space 
	{	case 0:		Port1DCB.Parity = NOPARITY;
					str = "N";						break;
		case 1:		Port1DCB.Parity = EVENPARITY;
					str = "E";						break;
		case 2:		Port1DCB.Parity = ODDPARITY;
					str = "O";						break;
		default:	Port1DCB.Parity = 0;
					str = "X";						break;
	}

	switch (index4)
	{	case 0:		Port1DCB.StopBits = ONESTOPBIT;
					str1 = "1";						break;
		case 1:		Port1DCB.StopBits = TWOSTOPBITS;
					str1 = "2";						break;
		default:	Port1DCB.StopBits = 0;
					str1 = "0";						break;
	}

	switch (index5)
	{	case 0:		Port1DCB.fOutxCtsFlow = TRUE;					// CTS output flow control 
					Port1DCB.fDtrControl = DTR_CONTROL_ENABLE;		// DTR flow control type 
					Port1DCB.fOutX = FALSE;							// No XON/XOFF out flow control 
					Port1DCB.fInX = FALSE;							// No XON/XOFF in flow control 
					Port1DCB.fRtsControl = RTS_CONTROL_ENABLE;		// RTS flow control   
					break;
		case 1:		Port1DCB.fOutxCtsFlow = FALSE;					// No CTS output flow control 
					Port1DCB.fDtrControl = DTR_CONTROL_ENABLE;		// DTR flow control type 
					Port1DCB.fOutX = FALSE;							// No XON/XOFF out flow control 
					Port1DCB.fInX = FALSE;							// No XON/XOFF in flow control 
					Port1DCB.fRtsControl = RTS_CONTROL_ENABLE;		// RTS flow control 
					break;
		case 2:		Port1DCB.fOutxCtsFlow = FALSE;					// No CTS output flow control 
					Port1DCB.fDtrControl = DTR_CONTROL_ENABLE;		// DTR flow control type 
					Port1DCB.fOutX = TRUE;							// Enable XON/XOFF out flow control 
					Port1DCB.fInX = TRUE;							// Enable XON/XOFF in flow control 
					Port1DCB.fRtsControl = RTS_CONTROL_ENABLE;		// RTS flow control    
					break;
		default:	break;
	}
	PortConfig.Format(_T("%s %d, %d, %s, %s"), PortNum, Port1DCB.BaudRate, Port1DCB.ByteSize, str, str1);
    if (m_spikeGL) {
        char converted[512];
        wcstombs(converted, PortConfig, 512);
        m_spikeGL->pushConsoleMsg(converted);
    }
	return 1;
}

void MEAControlDlg::OnCbnSelchangeSel()
{	index0 = m_GetPort1Num.GetCurSel();
	configure();
	m_Port1Format.SetWindowTextW(PortConfig); // SetWindowTextW(PortConfig);
}

void MEAControlDlg::OnCbnSelchangeBuad()
{	index1 = m_GetPort1BuadRate.GetCurSel();
	configure();
	m_Port1Format.SetWindowTextW(PortConfig); // SetWindowTextW(PortConfig);
}

void MEAControlDlg::OnCbnSelchangeData()
{	index2 = m_GetPort1DataBit.GetCurSel();
	configure();
	m_Port1Format.SetWindowTextW(PortConfig); // SetWindowTextW(PortConfig);
}

void MEAControlDlg::OnCbnSelchangeParity()
{	index3 = m_GetPort1Parity.GetCurSel();
	configure();
	m_Port1Format.SetWindowTextW(PortConfig); // SetWindowTextW(PortConfig);
}

void MEAControlDlg::OnCbnSelchangeStop()
{	index4 = m_GetPort1StopBit.GetCurSel();
	configure();
	m_Port1Format.SetWindowTextW(PortConfig); // SetWindowTextW(PortConfig);
}

// Configuration Timeout
int configuretimeout(void)
{	//memset(&CommTimeouts, 0x00, sizeof(CommTimeouts)); 
	CommTimeouts.ReadIntervalTimeout			= 50;
	CommTimeouts.ReadTotalTimeoutConstant		= 50;
	CommTimeouts.ReadTotalTimeoutMultiplier		= 10;
	CommTimeouts.WriteTotalTimeoutMultiplier	= 10;
	CommTimeouts.WriteTotalTimeoutConstant		= 50;
	return 1;
}

// Open RS232
int MEAControlDlg::SetupUart()
{	LPCTSTR str;

	str = PortNum;
	hPort1 = CreateFile(str, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	if (hPort1 == INVALID_HANDLE_VALUE)
	{	
        if (m_visible) ::MessageBox (NULL, L"Port Open Failed" ,L"Error", MB_OK);
		return 1;
	}
	
	Port1DCB.DCBlength = sizeof(DCB);			//Initialize the DCBlength member. 
	if (GetCommState(hPort1, &Port1DCB) == 0)	// Get the default port setting information.
    {
        if (m_visible) ::MessageBox(NULL, L"(GetCommState Failed)", L"Error", MB_OK);
		CloseHandle(hPort1);
		return 2;
	}
	configure();	
	GetCommTimeouts(hPort1, &CommTimeouts);
	configuretimeout();

	//Re-configure the port with the new DCB structure. 
	if (!SetCommState(hPort1, &Port1DCB))
    {
        if (m_visible) ::MessageBox(NULL, L"1.Could not create the read thread.(SetCommState Failed)", L"Error", MB_OK);
		CloseHandle(hPort1);
		return 2;
	}

	// Set the time-out parameters for all read and write operations on the port. 
	if (!SetCommTimeouts(hPort1, &CommTimeouts))
    {
        if (m_visible) ::MessageBox(NULL, L"Could not create the read thread.(SetCommTimeouts Failed)", L"Error", MB_OK);
		CloseHandle(hPort1);
		return 3;
	}

	// Clear the port of any existing data. 
	if (PurgeComm(hPort1, PURGE_TXCLEAR | PURGE_RXCLEAR) == 0)
    {
        if (m_visible) ::MessageBox(NULL, L"Clearing The Port Failed", L"Message", MB_OK);
		CloseHandle(hPort1);
		return 4;
	}
	//MessageBox (NULL, L"SERIAL SETUP OK." ,L"Message", MB_OK);
	return 0;
}

// Close RS232 Port
int CloseUart(void)
{
	if (Serial_OK == 1) CloseHandle(hPort1);
	return 1;
}

// Write to RS232
int WriteUart(unsigned char *buf1, int len)
{	DWORD dwNumBytesWritten;

	WriteFile(hPort1, buf1, len, &dwNumBytesWritten, NULL);
	if (dwNumBytesWritten > 0)
		return 1;		// transmission ok
	else
		return 0;		// transmission no good
}

void GetData(void)
{	int i;

	if (buf2[0] == '#')
	{	for (i = 0; i <= 22; i++)	// remove all non-number characters 
			if ((buf2[i] <= 48) | (buf2[i] >= 58))  buf2[i] = '0';

		R_Data1 = ((buf2[1] - 48) * 10) + (buf2[2] - 48);
		R_Data2 = ((buf2[6] - 48) * 100000)  + ((buf2[7] - 48) * 10000)  + ((buf2[8] - 48) * 1000)  + ((buf2[9] - 48) * 100)  + ((buf2[10] - 48) * 10) + (buf2[11] - 48);
		R_Data3 = ((buf2[16] - 48) * 100000) + ((buf2[17] - 48) * 10000) + ((buf2[18] - 48) * 1000) + ((buf2[19] - 48) * 100) + ((buf2[20] - 48) * 10) + (buf2[21] - 48);
		//sprintf_s(buf3, "%d %d %d\n\r", R_Data1, R_Data2, R_Data3);
	}
}

// Read from RS232
int MEAControlDlg::ReadUart(int len)
{	DWORD		dwRead = 0;
	BOOL		fWaitingOnRead = FALSE;
	OVERLAPPED	osReader = { 0 };
	unsigned long	retlen = 0;
	int			i;

	// Create the overlapped event. Must be closed before exiting to avoid a handle leak.
	osReader.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (osReader.hEvent == NULL) {
		if (m_visible || !m_spikeGL) ::MessageBox(NULL, L"Error in creating Overlapped event", L"Error", MB_OK); 
		else m_spikeGL->pushConsoleError("ReadUart: Error in creating Overlapped event");
		return 0;
	}
		

	if (!fWaitingOnRead)
	{	if (!ReadFile(hPort1, buf2, len, &dwRead, &osReader))
		{	FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPWSTR)lastError, 1024, NULL);
			if (m_visible || !m_spikeGL)	::MessageBox(NULL, (LPWSTR)lastError, L"MESSAGE", MB_OK);
			else {
				char myerr[512];
				::wcstombs(myerr, (LPWSTR)lastError, 512);
				m_spikeGL->pushConsoleError(myerr);
			}
		}
		else
		{	for (i = 0; i <= 50; i++)	buf3[i] = 0;
			sprintf_s(buf3, "%s\n\r", buf2);			
			GetData();
			for (i = 0; i <= 50; i++)	buf2[i] = 0;
		}
	}


	if (dwRead > 0)
		return (int)dwRead;	//MessageBox (NULL, L"Read DATA Success" ,L"Success", MB_OK);//If we have data//return the length
	else
		return 0;			//else no data has been read
}

// connect to RS232
void MEAControlDlg::OnBnClickedOpen()
{	int status;

	status = 0;
	status = SetupUart();
	const char * msgstr = 0; bool haveErr = false;
	if (status == 0)
	{	m_Port1Message.AddString(CString(msgstr="COM2 Ready"));
		GetDlgItem(Port1_Open)->EnableWindow(FALSE);
		Serial_OK = 1;
		// LED Control 
		OnBnClickedReset();
		Control_On();
	}
	else
	{	if (status == 1) m_Port1Message.AddString(CString(msgstr="Port1 Open Failed"));
		if (status == 2) m_Port1Message.AddString(CString(msgstr="SetCommState Failed"));
		if (status == 3) m_Port1Message.AddString(CString(msgstr="SetCommTimeouts Failed"));
		if (status == 4) m_Port1Message.AddString(CString(msgstr="Clearing The Port Failed"));
		haveErr = true;
		GetDlgItem(Port1_Open)->EnableWindow(TRUE);
		Serial_OK = 0;
	}
	
	if (msgstr && m_spikeGL) {
		if (haveErr) m_spikeGL->pushConsoleError(msgstr); 
		else m_spikeGL->pushConsoleMsg(msgstr);
	}
}

// Clear Port #1 Monitor Window
void MEAControlDlg::OnBnClickedClearportmonitor()
{	
	m_Port1Message.ResetContent();
}

//**************************************************************************************************
//	11/2014
//	Construct Command for Write to COM Port 
//**************************************************************************************************
void MEAControlDlg::FPGA_Protocol_Construction(int CMD_Code, int Value_1, INT32 Value_2)
{	CString	protocol;
	int		status;
	char    str[256];
	size_t		len;

	sprintf_s(str, "%c%02d%05d%06d\n\r", '~', CMD_Code, Value_1, Value_2);

	// display out going message 
	protocol.Format(_T(">%c%02d%05d%06d+13"), '~', CMD_Code, Value_1,Value_2);
	m_Port1Message.AddString(protocol);
	m_Port1Message.SetTopIndex(m_Port1Message.GetCount() - 1);

	// sent message to FPGA
	len = strlen(str);
    
	status = WriteUart((unsigned char *)str, (int)len);
    if (m_spikeGL) {
        if (status)
            m_spikeGL->pushConsoleDebug(std::string("UART Write: ") + str);
        else
            m_spikeGL->pushConsoleDebug(std::string("UART Write Error: ") + str);
    }

}

//**************************************************************************************************
//	11/2014
//	LED Display Test 
//**************************************************************************************************
void MEAControlDlg::OnBnClicked1()
{	CString code;

	LED_CtlCode = 0;
	if (m_LED1.GetCheck() == TRUE) LED_CtlCode = LED_CtlCode + 1;
	if (m_LED2.GetCheck() == TRUE) LED_CtlCode = LED_CtlCode + 2;
	if (m_LED3.GetCheck() == TRUE) LED_CtlCode = LED_CtlCode + 4;
	if (m_LED4.GetCheck() == TRUE) LED_CtlCode = LED_CtlCode + 8;
	if (m_LED5.GetCheck() == TRUE) LED_CtlCode = LED_CtlCode + 16;
	if (m_LED6.GetCheck() == TRUE) LED_CtlCode = LED_CtlCode + 32;
	if (m_LED7.GetCheck() == TRUE) LED_CtlCode = LED_CtlCode + 64;
	if (m_LED8.GetCheck() == TRUE) LED_CtlCode = LED_CtlCode + 128;

	code.Format(_T("%d"), LED_CtlCode);
	m_LED_Ctl_Code.SetWindowText(LPCTSTR(code));
	FPGA_Protocol_Construction(CMD_LED, LED_CtlCode, 0);
}

void MEAControlDlg::OnBnClicked2()
{	CString code;

	LED_CtlCode = 0;
	if (m_LED1.GetCheck() == TRUE) LED_CtlCode = LED_CtlCode + 1;
	if (m_LED2.GetCheck() == TRUE) LED_CtlCode = LED_CtlCode + 2;
	if (m_LED3.GetCheck() == TRUE) LED_CtlCode = LED_CtlCode + 4;
	if (m_LED4.GetCheck() == TRUE) LED_CtlCode = LED_CtlCode + 8;
	if (m_LED5.GetCheck() == TRUE) LED_CtlCode = LED_CtlCode + 16;
	if (m_LED6.GetCheck() == TRUE) LED_CtlCode = LED_CtlCode + 32;
	if (m_LED7.GetCheck() == TRUE) LED_CtlCode = LED_CtlCode + 64;
	if (m_LED8.GetCheck() == TRUE) LED_CtlCode = LED_CtlCode + 128;

	code.Format(_T("%d"), LED_CtlCode);
	m_LED_Ctl_Code.SetWindowText(LPCTSTR(code));
	FPGA_Protocol_Construction(CMD_LED, LED_CtlCode, 0);
}

void MEAControlDlg::OnBnClicked3()
{	CString code;

	LED_CtlCode = 0;
	if (m_LED1.GetCheck() == TRUE) LED_CtlCode = LED_CtlCode + 1;
	if (m_LED2.GetCheck() == TRUE) LED_CtlCode = LED_CtlCode + 2;
	if (m_LED3.GetCheck() == TRUE) LED_CtlCode = LED_CtlCode + 4;
	if (m_LED4.GetCheck() == TRUE) LED_CtlCode = LED_CtlCode + 8;
	if (m_LED5.GetCheck() == TRUE) LED_CtlCode = LED_CtlCode + 16;
	if (m_LED6.GetCheck() == TRUE) LED_CtlCode = LED_CtlCode + 32;
	if (m_LED7.GetCheck() == TRUE) LED_CtlCode = LED_CtlCode + 64;
	if (m_LED8.GetCheck() == TRUE) LED_CtlCode = LED_CtlCode + 128;

	code.Format(_T("%d"), LED_CtlCode);
	m_LED_Ctl_Code.SetWindowText(LPCTSTR(code));
	FPGA_Protocol_Construction(CMD_LED, LED_CtlCode, 0);
}

void MEAControlDlg::OnBnClicked4()
{	CString code;

	LED_CtlCode = 0;
	if (m_LED1.GetCheck() == TRUE) LED_CtlCode = LED_CtlCode + 1;
	if (m_LED2.GetCheck() == TRUE) LED_CtlCode = LED_CtlCode + 2;
	if (m_LED3.GetCheck() == TRUE) LED_CtlCode = LED_CtlCode + 4;
	if (m_LED4.GetCheck() == TRUE) LED_CtlCode = LED_CtlCode + 8;
	if (m_LED5.GetCheck() == TRUE) LED_CtlCode = LED_CtlCode + 16;
	if (m_LED6.GetCheck() == TRUE) LED_CtlCode = LED_CtlCode + 32;
	if (m_LED7.GetCheck() == TRUE) LED_CtlCode = LED_CtlCode + 64;
	if (m_LED8.GetCheck() == TRUE) LED_CtlCode = LED_CtlCode + 128;

	code.Format(_T("%d"), LED_CtlCode);
	m_LED_Ctl_Code.SetWindowText(LPCTSTR(code));
	FPGA_Protocol_Construction(CMD_LED, LED_CtlCode, 0);
}

void MEAControlDlg::OnBnClicked5()
{	CString code;

	LED_CtlCode = 0;
	if (m_LED1.GetCheck() == TRUE) LED_CtlCode = LED_CtlCode + 1;
	if (m_LED2.GetCheck() == TRUE) LED_CtlCode = LED_CtlCode + 2;
	if (m_LED3.GetCheck() == TRUE) LED_CtlCode = LED_CtlCode + 4;
	if (m_LED4.GetCheck() == TRUE) LED_CtlCode = LED_CtlCode + 8;
	if (m_LED5.GetCheck() == TRUE) LED_CtlCode = LED_CtlCode + 16;
	if (m_LED6.GetCheck() == TRUE) LED_CtlCode = LED_CtlCode + 32;
	if (m_LED7.GetCheck() == TRUE) LED_CtlCode = LED_CtlCode + 64;
	if (m_LED8.GetCheck() == TRUE) LED_CtlCode = LED_CtlCode + 128;

	code.Format(_T("%d"), LED_CtlCode);
	m_LED_Ctl_Code.SetWindowText(LPCTSTR(code));
	FPGA_Protocol_Construction(CMD_LED, LED_CtlCode, 0);
}

void MEAControlDlg::OnBnClicked6()
{	CString code;

	LED_CtlCode = 0;
	if (m_LED1.GetCheck() == TRUE) LED_CtlCode = LED_CtlCode + 1;
	if (m_LED2.GetCheck() == TRUE) LED_CtlCode = LED_CtlCode + 2;
	if (m_LED3.GetCheck() == TRUE) LED_CtlCode = LED_CtlCode + 4;
	if (m_LED4.GetCheck() == TRUE) LED_CtlCode = LED_CtlCode + 8;
	if (m_LED5.GetCheck() == TRUE) LED_CtlCode = LED_CtlCode + 16;
	if (m_LED6.GetCheck() == TRUE) LED_CtlCode = LED_CtlCode + 32;
	if (m_LED7.GetCheck() == TRUE) LED_CtlCode = LED_CtlCode + 64;
	if (m_LED8.GetCheck() == TRUE) LED_CtlCode = LED_CtlCode + 128;

	code.Format(_T("%d"), LED_CtlCode);
	m_LED_Ctl_Code.SetWindowText(LPCTSTR(code));
	FPGA_Protocol_Construction(CMD_LED, LED_CtlCode, 0);
}

void MEAControlDlg::OnBnClicked7()
{	CString code;

	LED_CtlCode = 0;
	if (m_LED1.GetCheck() == TRUE) LED_CtlCode = LED_CtlCode + 1;
	if (m_LED2.GetCheck() == TRUE) LED_CtlCode = LED_CtlCode + 2;
	if (m_LED3.GetCheck() == TRUE) LED_CtlCode = LED_CtlCode + 4;
	if (m_LED4.GetCheck() == TRUE) LED_CtlCode = LED_CtlCode + 8;
	if (m_LED5.GetCheck() == TRUE) LED_CtlCode = LED_CtlCode + 16;
	if (m_LED6.GetCheck() == TRUE) LED_CtlCode = LED_CtlCode + 32;
	if (m_LED7.GetCheck() == TRUE) LED_CtlCode = LED_CtlCode + 64;
	if (m_LED8.GetCheck() == TRUE) LED_CtlCode = LED_CtlCode + 128;

	code.Format(_T("%d"), LED_CtlCode);
	m_LED_Ctl_Code.SetWindowText(LPCTSTR(code));
	FPGA_Protocol_Construction(CMD_LED, LED_CtlCode, 0);
}

void MEAControlDlg::OnBnClicked8()
{	CString code;

	LED_CtlCode = 0;
	if (m_LED1.GetCheck() == TRUE) LED_CtlCode = LED_CtlCode + 1;
	if (m_LED2.GetCheck() == TRUE) LED_CtlCode = LED_CtlCode + 2;
	if (m_LED3.GetCheck() == TRUE) LED_CtlCode = LED_CtlCode + 4;
	if (m_LED4.GetCheck() == TRUE) LED_CtlCode = LED_CtlCode + 8;
	if (m_LED5.GetCheck() == TRUE) LED_CtlCode = LED_CtlCode + 16;
	if (m_LED6.GetCheck() == TRUE) LED_CtlCode = LED_CtlCode + 32;
	if (m_LED7.GetCheck() == TRUE) LED_CtlCode = LED_CtlCode + 64;
	if (m_LED8.GetCheck() == TRUE) LED_CtlCode = LED_CtlCode + 128;

	code.Format(_T("%d"), LED_CtlCode);
	m_LED_Ctl_Code.SetWindowText(LPCTSTR(code));
	FPGA_Protocol_Construction(CMD_LED, LED_CtlCode, 0);
}

void MEAControlDlg::OnBnClickedReset()
{	CString code; 

	LED_CtlCode = 0;
	m_LED1.SetCheck(FALSE);
	m_LED2.SetCheck(FALSE);
	m_LED3.SetCheck(FALSE);
	m_LED4.SetCheck(FALSE);
	m_LED5.SetCheck(FALSE);
	m_LED6.SetCheck(FALSE);
	m_LED7.SetCheck(FALSE);
	m_LED8.SetCheck(FALSE);
	code.Format(_T("%d"), LED_CtlCode);
	m_LED_Ctl_Code.SetWindowText(LPCTSTR(code));
	FPGA_Protocol_Construction(CMD_LED, LED_CtlCode, 0);
}

//**************************************************************************************************
//	11/2014
//	Digital Output Test 
//**************************************************************************************************
void MEAControlDlg::Digital_Output_Update()
{	CString code;

	DigitalOut_Code = 0;
	if (m_DigitalOut_1.GetCheck() == TRUE)	DigitalOut_Code = DigitalOut_Code + 1;
	if (m_DigitalOut2.GetCheck() == TRUE)	DigitalOut_Code = DigitalOut_Code + 2;
	if (m_DigitalOut3.GetCheck() == TRUE)	DigitalOut_Code = DigitalOut_Code + 4;
	if (m_DigitalOut4.GetCheck() == TRUE)	DigitalOut_Code = DigitalOut_Code + 8;
	if (m_DigitalOut5.GetCheck() == TRUE)	DigitalOut_Code = DigitalOut_Code + 16;
	if (m_DigitalOut6.GetCheck() == TRUE)	DigitalOut_Code = DigitalOut_Code + 32;
	if (m_DigitalOut7.GetCheck() == TRUE)	DigitalOut_Code = DigitalOut_Code + 64;
	if (m_DigitalOut8.GetCheck() == TRUE)	DigitalOut_Code = DigitalOut_Code + 128;
	if (m_DigitalOut9.GetCheck() == TRUE)	DigitalOut_Code = DigitalOut_Code + 256;
	if (m_DigitalOut10.GetCheck() == TRUE)	DigitalOut_Code = DigitalOut_Code + 512;
	if (m_DigitalOut11.GetCheck() == TRUE)	DigitalOut_Code = DigitalOut_Code + 1024;
	if (m_DigitalOut12.GetCheck() == TRUE)	DigitalOut_Code = DigitalOut_Code + 2048;
	if (m_DigitalOut13.GetCheck() == TRUE)	DigitalOut_Code = DigitalOut_Code + 4096;
	if (m_DigitalOut14.GetCheck() == TRUE)	DigitalOut_Code = DigitalOut_Code + 8192;
	if (m_DigitalOut15.GetCheck() == TRUE)	DigitalOut_Code = DigitalOut_Code + 16384;
	if (m_DigitalOut16.GetCheck() == TRUE)	DigitalOut_Code = DigitalOut_Code + 32768;

	code.Format(_T("%d"), DigitalOut_Code);
	m_DigitalOut_Code.SetWindowText(LPCTSTR(code));
	FPGA_Protocol_Construction(CMD_Code[2], DigitalOut_Code, 0);

}

void MEAControlDlg::OnBnClickedDoutReset()
{	CString code;

	DigitalOut_Code = 0;
	m_DigitalOut_1.SetCheck(FALSE);
	m_DigitalOut2.SetCheck(FALSE);
	m_DigitalOut3.SetCheck(FALSE);
	m_DigitalOut4.SetCheck(FALSE);
	m_DigitalOut5.SetCheck(FALSE);
	m_DigitalOut6.SetCheck(FALSE);
	m_DigitalOut7.SetCheck(FALSE);
	m_DigitalOut8.SetCheck(FALSE);
	m_DigitalOut9.SetCheck(FALSE);
	m_DigitalOut10.SetCheck(FALSE);
	m_DigitalOut11.SetCheck(FALSE);
	m_DigitalOut12.SetCheck(FALSE);
	m_DigitalOut13.SetCheck(FALSE);
	m_DigitalOut14.SetCheck(FALSE);
	m_DigitalOut15.SetCheck(FALSE);
	m_DigitalOut16.SetCheck(FALSE);
	code.Format(_T("%d"), DigitalOut_Code);
	m_DigitalOut_Code.SetWindowText(LPCTSTR(code));
	FPGA_Protocol_Construction(CMD_Code[2], DigitalOut_Code, 0);
}

void MEAControlDlg::OnBnClickedOut1()
{
	Digital_Output_Update();
}

void MEAControlDlg::OnBnClickedOut2()
{
	Digital_Output_Update();
}

void MEAControlDlg::OnBnClickedOut3()
{
	Digital_Output_Update();
}

void MEAControlDlg::OnBnClickedOut4()
{
	Digital_Output_Update();
}

void MEAControlDlg::OnBnClickedOut5()
{
	Digital_Output_Update();
}

void MEAControlDlg::OnBnClickedOut6()
{
	Digital_Output_Update();
}

void MEAControlDlg::OnBnClickedOut7()
{
	Digital_Output_Update();
}

void MEAControlDlg::OnBnClickedOut8()
{
	Digital_Output_Update();
}

void MEAControlDlg::OnBnClickedOut9()
{
	Digital_Output_Update();
}

void MEAControlDlg::OnBnClickedOut10()
{
	Digital_Output_Update();
}

void MEAControlDlg::OnBnClickedOut11()
{
	Digital_Output_Update();
}

void MEAControlDlg::OnBnClickedOut12()
{
	Digital_Output_Update();
}

void MEAControlDlg::OnBnClickedOut13()
{
	Digital_Output_Update();
}

void MEAControlDlg::OnBnClickedOut14()
{
	Digital_Output_Update();
}

void MEAControlDlg::OnBnClickedOut15()
{
	Digital_Output_Update();
}

void MEAControlDlg::OnBnClickedOut16()
{
	Digital_Output_Update();
}
//**************************************************************************************************
//	12/2014
//	InTan-64 SPI Test Result 
//**************************************************************************************************
void MEAControlDlg::SPI_Test_Display()
{
	switch (R_Data1) // 0-4=no,odd,even,mark,space 
	{
		case 1:		if ((R_Data2 == 53) && (R_Data3 == 58))	m_Intan1A.SetCheck(1);  else m_Intan1A.SetCheck(0);  break;			
		case 2:		if ((R_Data2 == 53) && (R_Data3 == 58))	m_Intan2A.SetCheck(1);  else m_Intan2A.SetCheck(0);  break;
		case 3:		if ((R_Data2 == 53) && (R_Data3 == 58))	m_Intan3A.SetCheck(1);  else m_Intan3A.SetCheck(0);  break;
		case 4:		if ((R_Data2 == 53) && (R_Data3 == 58))	m_Intan4A.SetCheck(1);  else m_Intan4A.SetCheck(0);  break;
		case 5:		if ((R_Data2 == 53) && (R_Data3 == 58))	m_Intan5A.SetCheck(1);  else m_Intan5A.SetCheck(0);  break;
		case 6:		if ((R_Data2 == 53) && (R_Data3 == 58))	m_Intan6A.SetCheck(1);  else m_Intan6A.SetCheck(0);  break;
		case 7:		if ((R_Data2 == 53) && (R_Data3 == 58))	m_Intan7A.SetCheck(1);  else m_Intan7A.SetCheck(0);  break;
		case 8:		if ((R_Data2 == 53) && (R_Data3 == 58))	m_Intan8A.SetCheck(1);  else m_Intan8A.SetCheck(0);  break;
		case 9:		if ((R_Data2 == 53) && (R_Data3 == 58))	m_Intan9A.SetCheck(1);  else m_Intan9A.SetCheck(0);  break;
		case 10:	if ((R_Data2 == 53) && (R_Data3 == 58))	m_Intan10A.SetCheck(1); else m_Intan10A.SetCheck(0); break;
		case 11:	if ((R_Data2 == 53) && (R_Data3 == 58))	m_Intan11A.SetCheck(1); else m_Intan11A.SetCheck(0); break;
		case 12:	if ((R_Data2 == 53) && (R_Data3 == 58))	m_Intan12A.SetCheck(1); else m_Intan12A.SetCheck(0); break;
		case 13:	if ((R_Data2 == 53) && (R_Data3 == 58))	m_Intan13A.SetCheck(1); else m_Intan13A.SetCheck(0); break;
		case 14:	if ((R_Data2 == 53) && (R_Data3 == 58))	m_Intan14A.SetCheck(1); else m_Intan14A.SetCheck(0); break;
		case 15:	if ((R_Data2 == 53) && (R_Data3 == 58))	m_Intan15A.SetCheck(1); else m_Intan15A.SetCheck(0); break;
		case 16:	if ((R_Data2 == 53) && (R_Data3 == 58))	m_Intan16A.SetCheck(1); else m_Intan16A.SetCheck(0); break;
		case 17:	if ((R_Data2 == 53) && (R_Data3 == 58))	m_Intan17A.SetCheck(1); else m_Intan17A.SetCheck(0); break;
		case 18:	if ((R_Data2 == 53) && (R_Data3 == 58))	m_Intan18A.SetCheck(1); else m_Intan18A.SetCheck(0); break;
		case 19:	if ((R_Data2 == 53) && (R_Data3 == 58))	m_Intan19A.SetCheck(1); else m_Intan19A.SetCheck(0); break;
		case 20:	if ((R_Data2 == 53) && (R_Data3 == 58))	m_Intan20A.SetCheck(1); else m_Intan20A.SetCheck(0); break;
		case 21:	if ((R_Data2 == 53) && (R_Data3 == 58))	m_Intan21A.SetCheck(1); else m_Intan21A.SetCheck(0); break;
		case 22:	if ((R_Data2 == 53) && (R_Data3 == 58))	m_Intan22A.SetCheck(1); else m_Intan22A.SetCheck(0); break;
		case 23:	if ((R_Data2 == 53) && (R_Data3 == 58))	m_Intan23A.SetCheck(1); else m_Intan23A.SetCheck(0); break;
		case 24:	if ((R_Data2 == 53) && (R_Data3 == 58))	m_Intan24A.SetCheck(1); else m_Intan24A.SetCheck(0); break;
		case 25:	if ((R_Data2 == 53) && (R_Data3 == 58))	m_Intan25A.SetCheck(1); else m_Intan25A.SetCheck(0); break;
		case 26:	if ((R_Data2 == 53) && (R_Data3 == 58))	m_Intan26A.SetCheck(1); else m_Intan26A.SetCheck(0); break;
		case 27:	if ((R_Data2 == 53) && (R_Data3 == 58))	m_Intan27A.SetCheck(1); else m_Intan27A.SetCheck(0); break;
		case 28:	if ((R_Data2 == 53) && (R_Data3 == 58))	m_Intan28A.SetCheck(1); else m_Intan28A.SetCheck(0); break;
		case 29:	if ((R_Data2 == 53) && (R_Data3 == 58))	m_Intan29A.SetCheck(1); else m_Intan29A.SetCheck(0); break;
		case 30:	if ((R_Data2 == 53) && (R_Data3 == 58))	m_Intan30A.SetCheck(1); else m_Intan30A.SetCheck(0); break;
		case 31:	if ((R_Data2 == 53) && (R_Data3 == 58))	m_Intan31A.SetCheck(1); else m_Intan31A.SetCheck(0); break;
		case 32:	if ((R_Data2 == 53) && (R_Data3 == 58))	m_Intan32A.SetCheck(1); else m_Intan32A.SetCheck(0); break;
		case 33:	if ((R_Data2 == 53) && (R_Data3 == 58))	m_Intan33A.SetCheck(1); else m_Intan33A.SetCheck(0); break;
		case 34:	if ((R_Data2 == 53) && (R_Data3 == 58))	m_Intan34A.SetCheck(1); else m_Intan34A.SetCheck(0); break;
		case 35:	if ((R_Data2 == 53) && (R_Data3 == 58))	m_Intan35A.SetCheck(1); else m_Intan35A.SetCheck(0); break;
		case 36:	if ((R_Data2 == 53) && (R_Data3 == 58))	m_Intan36A.SetCheck(1); else m_Intan36A.SetCheck(0); break;
	}
}

void MEAControlDlg::OnBnClickedTestclear()
{
	m_Intan1A.SetCheck(0);
	m_Intan2A.SetCheck(0);
	m_Intan3A.SetCheck(0);
	m_Intan4A.SetCheck(0);
	m_Intan5A.SetCheck(0);
	m_Intan6A.SetCheck(0);
	m_Intan7A.SetCheck(0);
	m_Intan8A.SetCheck(0);
	m_Intan9A.SetCheck(0);
	m_Intan10A.SetCheck(0);
	m_Intan11A.SetCheck(0);
	m_Intan12A.SetCheck(0);
	m_Intan13A.SetCheck(0);
	m_Intan14A.SetCheck(0);
	m_Intan15A.SetCheck(0);
	m_Intan16A.SetCheck(0);
	m_Intan17A.SetCheck(0);
	m_Intan18A.SetCheck(0);
	m_Intan19A.SetCheck(0);
	m_Intan20A.SetCheck(0);
	m_Intan21A.SetCheck(0);
	m_Intan22A.SetCheck(0);
	m_Intan23A.SetCheck(0);
	m_Intan24A.SetCheck(0);
	m_Intan25A.SetCheck(0);
	m_Intan26A.SetCheck(0);
	m_Intan27A.SetCheck(0);
	m_Intan28A.SetCheck(0);
	m_Intan29A.SetCheck(0);
	m_Intan30A.SetCheck(0);
	m_Intan31A.SetCheck(0);
	m_Intan32A.SetCheck(0);
	m_Intan33A.SetCheck(0);
	m_Intan34A.SetCheck(0);
	m_Intan35A.SetCheck(0);
	m_Intan36A.SetCheck(0);

}

//**************************************************************************************************
//	12/2014
//	InTan-64 Register Setup  
//**************************************************************************************************
void MEAControlDlg::Register_Reset()
{
	CString d;

	d.Format(_T("%d"), 206);
	m_ADCConfig.SetWindowTextW(d); // SetWindowTextW(d);

	d.Format(_T("%d"), 130);
	m_BufferBias.SetWindowTextW(d); // SetWindowTextW(d);

	d.Format(_T("%d"), 196);
	m_MuxBias.SetWindowTextW(d); // SetWindowTextW(d);

	d.Format(_T("%d"), 2);
	m_MuxLoad.SetWindowTextW(d); // SetWindowTextW(d);
	
	d.Format(_T("%d"), 1);
	m_OutputOffset.SetWindowTextW(d); // SetWindowTextW(d);

	d.Format(_T("%d"), 0);
	m_ImpedanceControl.SetWindowTextW(d); // SetWindowTextW(d);

	d.Format(_T("%d"), 0);
	m_ImpedanceAMP.SetWindowTextW(d); // SetWindowTextW(d);

	d.Format(_T("%d"), 0);
	m_ImpedanceDAC.SetWindowTextW(d); // SetWindowTextW(d);

	d.Format(_T("%d"), 8);
	m_AMPBandwidth1.SetWindowTextW(d); // SetWindowTextW(d);

	d.Format(_T("%d"), 0);
	m_AMPBandwidth2.SetWindowTextW(d); // SetWindowTextW(d);

	d.Format(_T("%d"), 4);
	m_AMPBandwidth3.SetWindowTextW(d); // SetWindowTextW(d);

	d.Format(_T("%d"), 0);
	m_AMPBandwidth4.SetWindowTextW(d); // SetWindowTextW(d);

	d.Format(_T("%d"), 16);
	m_AMPBandwidth5.SetWindowTextW(d); // SetWindowTextW(d);

	d.Format(_T("%d"), 124);
	m_AMPBandwidth6.SetWindowTextW(d); // SetWindowTextW(d);

	d.Format(_T("%d"), 255);
	m_AMPPower1.SetWindowTextW(d); // SetWindowTextW(d);

	d.Format(_T("%d"), 255);
	m_AMPPOWER2.SetWindowTextW(d); // SetWindowTextW(d);

	d.Format(_T("%d"), 255);
	m_AMPPower3.SetWindowTextW(d); // SetWindowTextW(d);

	d.Format(_T("%d"), 255);
	m_AMPPower8.SetWindowTextW(d); // SetWindowTextW(d);

	d.Format(_T("%d"), 255);
	m_AMPPower7.SetWindowTextW(d); // SetWindowTextW(d);

	d.Format(_T("%d"), 255);
	m_AMPPower6.SetWindowTextW(d); // SetWindowTextW(d);

	d.Format(_T("%d"), 255);
	m_AMPPower4.SetWindowTextW(d); // SetWindowTextW(d);

	d.Format(_T("%d"), 255);
	m_AMPPower5.SetWindowTextW(d); // SetWindowTextW(d);

	d.Format(_T("%d"), 0);
	m_Data_A.SetWindowTextW(d); // SetWindowTextW(d);

	d.Format(_T("%d"), 0);
	m_Data_B.SetWindowTextW(d); // SetWindowTextW(d);
}

//**************************************************************************************************
//	11/2014
//	InTan-64 Test 
//**************************************************************************************************
//void MEAControlDlg::OnBnClickedIntan1a()
//{
	// TODO: Add your control notification handler code here
//}

void MEAControlDlg::OnBnClickedIntan64Test() // Intan Communication Test
{	int num;
	int mode_code, mode;

	num = m_IntanChipNum.GetCurSel() + 1;
	mode_code = m_IntanOperationMode.GetCurSel();
	switch (mode_code) // Port Num
	{
		case 0:		mode = 1;						//setup register address one clock ahead, reg : 59, return test		
					break;  
		case 1:		mode = 2;						//setup register address one clock ahead, reg: 63, return 4
					break;	
		case 2:		mode = 3;						//setup register address one clock ahead, reg: 40, return '
					break;	
		default:	mode = 1;				break;
	}
	
	FPGA_Protocol_Construction(CMD_Code[3], num, mode);
}

//**************************************************************************************************
//	11/2014
//	InTan-64 Register Setup 
//	FPGA_Protocol_Construction(operation Code, register number, register data); when operation Code = 4, write
//	FPGA_Protocol_Construction(operation Code, chip number, register number);	when operation Code = 5, validate
//**************************************************************************************************
void MEAControlDlg::delay(long msec)
{	static int	current_tick;
	static int	last_tick;

	last_tick = GetTickCount();
	while ((current_tick - last_tick) <= msec)	current_tick = GetTickCount();
}

void MEAControlDlg::OnBnClickedAdcconfigs()
{	INT32 Data;

	Data = GetDlgItemInt(ADCConfigE);
	FPGA_Protocol_Construction(CMD_Code[4], 0, Data);
	FPGA_Protocol_Construction(CMD_Code[5], 1, Register[0]);
	FPGA_Protocol_Construction(CMD_Code[5], 1, Register[0]);
}

void MEAControlDlg::OnBnClickedBufferbiass()	// Set Register #1 Parameter
{	INT32 Data;

	Data = GetDlgItemInt(BufferBiasE);
	FPGA_Protocol_Construction(CMD_Code[4], 1, Data);
	FPGA_Protocol_Construction(CMD_Code[5], 1, Register[1]);
	FPGA_Protocol_Construction(CMD_Code[5], 1, Register[1]);
}

void MEAControlDlg::OnBnClickedMuxbiass()
{	INT32 Data;

	Data = GetDlgItemInt(MuxBiasE);
	FPGA_Protocol_Construction(CMD_Code[4], 2, Data);
	FPGA_Protocol_Construction(CMD_Code[5], 1, Register[2]);
	FPGA_Protocol_Construction(CMD_Code[5], 1, Register[2]);	
}

void MEAControlDlg::OnBnClickedMuxloads()
{	INT32 Data;

	Data = GetDlgItemInt(MuxLoadE);
	FPGA_Protocol_Construction(CMD_Code[4], 3, Data);
	FPGA_Protocol_Construction(CMD_Code[5], 1, Register[3]);
	FPGA_Protocol_Construction(CMD_Code[5], 1, Register[3]);
}

void MEAControlDlg::OnBnClickedOutputoffsets()
{	INT32 Data;

	Data = GetDlgItemInt(OutputOffsetE);
	FPGA_Protocol_Construction(CMD_Code[4], 4, Data);
	FPGA_Protocol_Construction(CMD_Code[5], 1, Register[4]);
	FPGA_Protocol_Construction(CMD_Code[5], 1, Register[4]);
}

void MEAControlDlg::OnBnClickedImpedancecontrols()
{	INT32 Data;

	Data = GetDlgItemInt(ImpedanceControlE);
	FPGA_Protocol_Construction(CMD_Code[4], 5, Data);
	FPGA_Protocol_Construction(CMD_Code[5], 1, Register[5]);
	FPGA_Protocol_Construction(CMD_Code[5], 1, Register[5]);
}

void MEAControlDlg::OnBnClickedImpedancedacs()
{	INT32 Data;

	Data = GetDlgItemInt(ImpedanceDACE);
	FPGA_Protocol_Construction(CMD_Code[4], 6, Data);
	FPGA_Protocol_Construction(CMD_Code[5], 1, Register[6]);
	FPGA_Protocol_Construction(CMD_Code[5], 1, Register[6]);
}

void MEAControlDlg::OnBnClickedImpedanceamps()
{	INT32 Data;

	Data = GetDlgItemInt(ImpedanceAMPE);
	FPGA_Protocol_Construction(CMD_Code[4], 7, Data);
	FPGA_Protocol_Construction(CMD_Code[5], 1, Register[7]);
	FPGA_Protocol_Construction(CMD_Code[5], 1, Register[7]);
}

void MEAControlDlg::OnBnClickedAmpbandwidths1()
{	INT32 Data;

	Data = GetDlgItemInt(AMPBandwidth1E);
	FPGA_Protocol_Construction(CMD_Code[4], 8, Data);
	FPGA_Protocol_Construction(CMD_Code[5], 1, Register[8]);
	FPGA_Protocol_Construction(CMD_Code[5], 1, Register[8]);
}

void MEAControlDlg::OnBnClickedAmpbandwidths2()
{	INT32 Data;

	Data = GetDlgItemInt(AMPBandwidth2E);
	FPGA_Protocol_Construction(CMD_Code[4], 9, Data);
	FPGA_Protocol_Construction(CMD_Code[5], 1, Register[9]);
	FPGA_Protocol_Construction(CMD_Code[5], 1, Register[9]);
}

void MEAControlDlg::OnBnClickedAmpbandwidths3()
{	INT32 Data;

	Data = GetDlgItemInt(AMPBandwidth3E);
	FPGA_Protocol_Construction(CMD_Code[4], 10, Data);
	FPGA_Protocol_Construction(CMD_Code[5], 1, Register[10]);
	FPGA_Protocol_Construction(CMD_Code[5], 1, Register[10]);
}

void MEAControlDlg::OnBnClickedAmpbandwidths4()
{	INT32 Data;

	Data = GetDlgItemInt(AMPBandwidth4E);
	FPGA_Protocol_Construction(CMD_Code[4], 11, Data);
	FPGA_Protocol_Construction(CMD_Code[5], 1, Register[11]);
	FPGA_Protocol_Construction(CMD_Code[5], 1, Register[11]);
}

void MEAControlDlg::OnBnClickedAmpbandwidths5()
{	INT32 Data;

	Data = GetDlgItemInt(AMPBandwidth5E);
	FPGA_Protocol_Construction(CMD_Code[4], 12, Data);
	FPGA_Protocol_Construction(CMD_Code[5], 1, Register[12]);
	FPGA_Protocol_Construction(CMD_Code[5], 1, Register[12]);
}

void MEAControlDlg::OnBnClickedAmpbandwidths6()
{	INT32 Data;

	Data = GetDlgItemInt(AMPBandwidth6E);
	FPGA_Protocol_Construction(CMD_Code[4], 13, Data);
	FPGA_Protocol_Construction(CMD_Code[5], 1, Register[13]);
	FPGA_Protocol_Construction(CMD_Code[5], 1, Register[13]);
}

void MEAControlDlg::OnBnClickedAmppowers1()
{	INT32 Data;

	Data = GetDlgItemInt(AMPPower1E);
	FPGA_Protocol_Construction(CMD_Code[4], 14, Data);
	FPGA_Protocol_Construction(CMD_Code[5], 1, Register[14]);
	FPGA_Protocol_Construction(CMD_Code[5], 1, Register[14]);
}

void MEAControlDlg::OnBnClickedAmppowers2()
{	INT32 Data;

	Data = GetDlgItemInt(AMPPower2E);
	FPGA_Protocol_Construction(CMD_Code[4], 15, Data);
	FPGA_Protocol_Construction(CMD_Code[5], 1, Register[15]);
	FPGA_Protocol_Construction(CMD_Code[5], 1, Register[15]);
}

void MEAControlDlg::OnBnClickedAmppowers3()
{	INT32 Data;

	Data = GetDlgItemInt(AMPPower3E);
	FPGA_Protocol_Construction(CMD_Code[4], 16, Data);
	FPGA_Protocol_Construction(CMD_Code[5], 1, Register[16]);
	FPGA_Protocol_Construction(CMD_Code[5], 1, Register[16]);
}

void MEAControlDlg::OnBnClickedAmppowers4()
{	INT32 Data;

	Data = GetDlgItemInt(AMPPower4E);
	FPGA_Protocol_Construction(CMD_Code[4], 17, Data);
	FPGA_Protocol_Construction(CMD_Code[5], 1, Register[17]);
	FPGA_Protocol_Construction(CMD_Code[5], 1, Register[17]);
}

void MEAControlDlg::OnBnClickedAmppowers5()
{	INT32 Data;

	Data = GetDlgItemInt(AMPPower5E);
	FPGA_Protocol_Construction(CMD_Code[4], 18, Data);
	FPGA_Protocol_Construction(CMD_Code[5], 1, Register[18]);
	FPGA_Protocol_Construction(CMD_Code[5], 1, Register[18]);
}

void MEAControlDlg::OnBnClickedAmppowers6()
{	INT32 Data;

	Data = GetDlgItemInt(AMPPower6E);
	FPGA_Protocol_Construction(CMD_Code[4], 19, Data);
	FPGA_Protocol_Construction(CMD_Code[5], 1, Register[19]);
	FPGA_Protocol_Construction(CMD_Code[5], 1, Register[19]);
}

void MEAControlDlg::OnBnClickedAmppowers7()
{	INT32 Data;

	Data = GetDlgItemInt(AMPPower7E);
	FPGA_Protocol_Construction(CMD_Code[4], 20, Data);
	FPGA_Protocol_Construction(CMD_Code[5], 1, Register[20]);
	FPGA_Protocol_Construction(CMD_Code[5], 1, Register[20]);
}

void MEAControlDlg::OnBnClickedAmppowers8()
{	INT32 Data;

	Data = GetDlgItemInt(AMPPower8E);
	FPGA_Protocol_Construction(CMD_Code[4], 21, Data);
	FPGA_Protocol_Construction(CMD_Code[5], 1, Register[21]);
	FPGA_Protocol_Construction(CMD_Code[5], 1, Register[21]);
}

//**************************************************************************************************
//	11/2014
//	InTan-64 Continuous ADC Read
//	FPGA_Protocol_Construction(operation Code, x, x);
//**************************************************************************************************
void MEAControlDlg::OnBnClickedContinuousadc()
{
	FPGA_Protocol_Construction(CMD_Code[6], 0, 0);
}


//**************************************************************************************************
//	11/2014
//	InTan-64 Continuous ADC Read
//	FPGA_Protocol_Construction(operation Code, x, x);
//**************************************************************************************************
void MEAControlDlg::OnBnClickedFramegrabberenable1()
{
	if (m_FrameGrabberEnable.GetCheck())
	{	m_FrameGrabberEnable.EnableWindow(false);	
		//Coreco_Board_Setup("D:\\Project-Vitax-7\\ProjectBuildingDocumentation\\ProjectBoardTestProgram\\J_2000+_Electrode_8tap_8bit.ccf\0");
        //Coreco_Board_Setup("C:\\Users\\calin\\Desktop\\Src\\XTiumCL_Stuff\\J_2000+_Electrode_8tap_8bit.ccf\0");
        Coreco_Board_Setup("J_2000+_Electrode_8tap_8bit.ccf\0");
        Frame_Grabber_Enabled = 1;
	}
}

