
// MFCApplication2Dlg.h : header file
//

#pragma once
#include "afxwin.h"

#include "SapClassBasic.h"
#include "SapClassGui.h"
#include "afxcmn.h"
#include <vector>
#include <string>

#include "SpikeGLHandlerThread.h"
class MEAControlDlgAutoProxy;


	typedef struct				//	image data structure	 
	{	int			width;		//	image width  (# cols) 
		int			height;		//	image height (# rows) 
		uchar		*image;		//	pointer to image data 
	} imageS, *imageP;	

	typedef struct
	{	int			width;
		int			height;
		uchar		*image;
	} imageRGB4S, *imageRGB4P;

// MEAControlDlg dialog
class MEAControlDlg : public CDialogEx
{
	DECLARE_DYNAMIC(MEAControlDlg);
	friend class MEAControlDlgAutoProxy;

// Construction
public:
	MEAControlDlg(CWnd* pParent = NULL);	// standard constructor
	virtual ~MEAControlDlg();
	void	FPGA_Protocol_Construction(int, int, INT32);
	//---------------------------------------------------------------------------------------------------------------
	// CameraLink Frame Grab Parameters
	//---------------------------------------------------------------------------------------------------------------
	
	bool						Coreco_Board_Setup(const char*);
	void						FreeUp_Resource();
	std::string					Coreco_Camera_File_Name;												// Coreco Camera File Name
	SapAcquisition				*m_Acq;
	SapBuffer					*m_Buffers;
	SapTransfer					*m_Xfer;
	SapView						*m_View;
	CImageWnd					*m_ImageWnd;
	int							xTop, xBot;
	int							yTop, yBot;
	#define DisplayOffeset		650																		//	805 (for Camera Link 1kx1k)		// display coodinate offset between image 1 and imagee 2display
	#define MaxDisplaySize		800
	SapLocation					*Coreco_pLoc;
	#define BSIZE				4																		//	ring buffer size
	imageP						m_DecodedByte[BSIZE];													//	Gray Image	
	imageRGB4P					m_DecodedRGB4[BSIZE];													//	RGB image
	static void					Coreco_Image1_XferCallback(SapXferCallbackInfo *pInfo);
	static void					Coreco_Image1_StatusCallback(SapAcqCallbackInfo *pInfo);
	void						Coreco_Display_Source1_Image(imageRGB4P rgb4, CRect &dRect, bool flag);
	int							m_RingBufferCounter;													//	Ring Buffer counter for Image Source 1 and Image Source 2
    std::vector<BYTE>           m_spikeGLFrameBuf;
    SpikeGLOutThread        *m_spikeGL;
    SpikeGLInputThread *m_spikeGLIn;
    BOOL m_visible;

    static MEAControlDlg *instance;
    static UINT_PTR timerId;

    int SetupUart();
    int ReadUart(int);
    int	configure(bool printMsgs = false);

// Dialog Data
	enum { IDD = IDD_MFCAPPLICATION2_DIALOG };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support

private:		
	void handleSpikeGLEnvParms();
    void doSpikeGLAutoStart();
    void handleSpikeGLCommand(XtCmd *);
    static void sapStatusCallback(SapManCallbackInfo *info);

// Implementation
protected:
	MEAControlDlgAutoProxy* m_pAutoProxy;
	HICON m_hIcon;

	BOOL CanExit();

	// Generated message map functions
	virtual BOOL OnInitDialog();
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg void OnClose();
	virtual void OnOK();
	virtual void OnCancel();
	DECLARE_MESSAGE_MAP()
public:
	afx_msg void Control_On();
	afx_msg void Control_Off();
	afx_msg void OnBnClickedCancel();
	afx_msg void OnCbnSelchangeSel();
	afx_msg void Register_Reset();
	CComboBox m_GetPort1Num;
	CEdit m_Port1Format;
	afx_msg void OnCbnSelchangeBuad();
	CComboBox m_GetPort1BuadRate;
	CComboBox m_GetPort1DataBit;
	CComboBox m_GetPort1Parity;
	CComboBox m_GetPort1StopBit;
	afx_msg void OnCbnSelchangeData();
	afx_msg void OnCbnSelchangeParity();
	afx_msg void OnCbnSelchangeStop();
	afx_msg void OnBnClickedOpen();
	CListBox m_Port1Message;
	CButton m_Port1Enable;
	CButton m_LED8;
	CButton m_LED7;
	CButton m_LED6;
	CButton m_LED5;
	CButton m_LED4;
	CButton m_LED3;
	CButton m_LED2;
	CButton m_LED1;
	afx_msg void OnBnClicked1();
	CStatic m_LED_Ctl_Code;
	CButton m_LED_Reset;
	afx_msg void OnBnClickedReset();
	afx_msg void OnBnClicked2();
	afx_msg void OnBnClicked3();
	afx_msg void OnBnClicked4();
	afx_msg void OnBnClicked5();
	afx_msg void OnBnClicked6();
	afx_msg void OnBnClicked7();
	afx_msg void OnBnClicked8();
	afx_msg void OnBnClickedDoutReset();
	afx_msg void Digital_Output_Update();
	CButton m_DigitalOut_1;
	CButton m_DoutRst;
	CButton m_DigitalOut2;
	CButton m_DigitalOut3;
	CButton m_DigitalOut4;
	CButton m_DigitalOut5;
	CButton m_DigitalOut6;
	CButton m_DigitalOut7;
	CButton m_DigitalOut8;
	CButton m_DigitalOut9;
	CButton m_DigitalOut10;
	CButton m_DigitalOut11;
	CButton m_DigitalOut12;
	CButton m_DigitalOut13;
	CButton m_DigitalOut14;
	CButton m_DigitalOut15;
	CButton m_DigitalOut16;
	afx_msg void OnBnClickedOut1();
	CStatic m_DigitalOut_Code;
	afx_msg void OnBnClickedButton1();
	afx_msg void OnBnClickedOut2();
	afx_msg void OnBnClickedOut3();
	afx_msg void OnBnClickedOut4();
	afx_msg void OnBnClickedOut5();
	afx_msg void OnBnClickedOut6();
	afx_msg void OnBnClickedOut7();
	afx_msg void OnBnClickedOut8();
	afx_msg void OnBnClickedOut9();
	afx_msg void OnBnClickedOut10();
	afx_msg void OnBnClickedOut11();
	afx_msg void OnBnClickedOut12();
	afx_msg void OnBnClickedOut13();
	afx_msg void OnBnClickedOut14();
	afx_msg void OnBnClickedOut15();
	afx_msg void OnBnClickedOut16();
	
	afx_msg void OnWindowPosChanging(WINDOWPOS* lpwndpos);

    // added by Calin, idle handler to read spikegl commands in main thread...
    afx_msg LRESULT SpikeGLIdleHandler(WPARAM, LPARAM);

	CButton m_Intan1A;
	CButton m_Intan2A;
	CButton m_Intan3A;
	CButton m_Intan4A;
	CButton m_Intan5A;
	CButton m_Intan6A;
	CButton m_Intan7A;
	CButton m_Intan8A;
	CButton m_Intan9A;
	CButton m_Intan10A;
	CButton m_Intan11A; 
	CButton m_Intan12A;
	CButton m_Intan13A;
	CButton m_Intan14A;
	CButton m_Intan15A;
	CButton m_Intan16A;
	CButton m_Intan17A;
	CButton m_Intan18A;
	CButton m_Intan19A;
	CButton m_Intan20A;
	CButton m_Intan21A;
	CButton m_Intan22A;
	CButton m_Intan23A;
	CButton m_Intan24A;
	CButton m_Intan25A;
	CButton m_Intan26A;
	CButton m_Intan27A;
	CButton m_Intan28A;
	CButton m_Intan29A;
	CButton m_Intan30A;
	CButton m_Intan31A;
	CButton m_Intan32A;
	CButton m_Intan33A;
	CButton m_Intan34A;
	CButton m_Intan35A;
	CButton m_Intan36A;

	afx_msg void OnBnClickedIntan64Test();
	afx_msg void SPI_Test_Display();
	CComboBox m_IntanChipNum;
	CComboBox m_IntanOperationMode;
	CButton m_ClearPortMonitor;
	afx_msg void OnBnClickedClearportmonitor();
	CButton BufferBiasSetUp;
	CEdit m_BufferBias;
	CEdit m_MuxBias;
	CEdit m_MuxLoad;
	CEdit m_OutputOffset;
	CEdit m_ImpedanceControl;
	CEdit m_ImpedanceAMP;
	CEdit m_ImpedanceDAC;
	CEdit m_AMPBandwidth1;
	CEdit m_AMPBandwidth2;
	CEdit m_AMPBandwidth3;
	CEdit m_AMPBandwidth4;
	CEdit m_AMPBandwidth5;
	CEdit m_AMPBandwidth6;
	CEdit m_AMPPower1;
	CEdit m_AMPPOWER2;
	CEdit m_AMPPower3;
	CEdit m_AMPPower8;
	CEdit m_AMPPower7;
	CEdit m_AMPPower6;
	CEdit m_AMPPower4;
	CEdit m_AMPPower5;
	CEdit m_Data_A;
	CEdit m_Data_B;
	CEdit m_ADCConfig;
	afx_msg void OnBnClickedTestclear();
	afx_msg void OnBnClickedBufferbiass();
	int m_BuffBias_Value;
	afx_msg void MEAControlDlg::delay(long);
	afx_msg void OnBnClickedAdcconfigs();
	afx_msg void OnBnClickedMuxbiass();
	afx_msg void OnBnClickedMuxloads();
	afx_msg void OnBnClickedOutputoffsets();
	afx_msg void OnBnClickedImpedancecontrols();
	afx_msg void OnBnClickedImpedancedacs();
	afx_msg void OnBnClickedImpedanceamps();
	afx_msg void OnBnClickedAmpbandwidths1();
	afx_msg void OnBnClickedAmpbandwidths2();
	afx_msg void OnBnClickedAmpbandwidths3();
	afx_msg void OnBnClickedAmpbandwidths4();
	afx_msg void OnBnClickedAmpbandwidths5();
	afx_msg void OnBnClickedAmpbandwidths6();
	afx_msg void OnBnClickedAmppowers1();
	afx_msg void OnBnClickedAmppowers2();
	afx_msg void OnBnClickedAmppowers3();
	afx_msg void OnBnClickedAmppowers4();
	afx_msg void OnBnClickedAmppowers5();
	afx_msg void OnBnClickedAmppowers6();
	afx_msg void OnBnClickedAmppowers7();
	afx_msg void OnBnClickedAmppowers8();
	afx_msg void OnBnClickedContinuousadc();
	CStatic m_ViewWnd;
	CStatic m_ViewWnd2;
	CEdit m_CameraConfigFileDir;
	CEdit m_FrameSize;
	CEdit m_TapSize;
	CEdit m_ColorMono;
	CEdit m_BitSize;
	CEdit m_ServerName;
	CEdit m_DeviceName;
	CEdit m_Version;
	CButton m_ClockSignal1;
	CButton m_ClockSignal2;
	CButton m_Clocksignal3;
	CButton m_FrameSignal;
	CButton m_LineSignal;
	CEdit m_PixelBytes;
	CEdit m_TotalPixel;
	CEdit m_Pitch;
	CEdit m_TotalLine;
	CEdit m_PixelDepth;
	CEdit m_Format;
	CButton m_FrameGrabberEnable;
	afx_msg void OnBnClickedFramegrabberenable1();	
	CEdit m_DataGridRaw1;

    bool gotFirstXferCallback;

};
