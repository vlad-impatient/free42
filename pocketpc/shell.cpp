// Free42.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "resource.h"
#include <commctrl.h>
#include <aygshell.h>
#include <sipapi.h>
#include <stdio.h>

#include "free42.h"
#include "shell.h"
#include "shell_skin.h"
#include "shell_spool.h"
#include "core_display.h"
#include "core_main.h"

#define MAX_LOADSTRING 100
#define FILENAMELEN 256

// Global Variables:
static HINSTANCE hInst;				// The current instance
static HWND hMainWnd = NULL;
static HWND hPrintOutWnd = NULL;
static HWND hwndCB;					// The command bar handle
static TCHAR szMainTitle[MAX_LOADSTRING];
static TCHAR szPrintOutTitle[MAX_LOADSTRING];
static TCHAR szMainWindowClass[MAX_LOADSTRING];
static TCHAR szPrintOutWindowClass[MAX_LOADSTRING];

static SHACTIVATEINFO s_sai;

static UINT timer = 0;
static UINT timer3 = 0;
static int running = 0;
static int enqueued = 0;

static int ckey = 0;
static int skey;
static bool mouse_key;


#define SHELL_VERSION 1

typedef struct state {
	int printerToTxtFile;
	int printerToGifFile;
	TCHAR printerTxtFileName[FILENAMELEN];
	TCHAR printerGifFileName[FILENAMELEN];
	int printerGifMaxLength;
	TCHAR skinName[FILENAMELEN];
} state_type;

static state_type state;

static TCHAR free42dirname[FILENAMELEN];
static TCHAR statefilename[FILENAMELEN];
static FILE *statefile = NULL;
static TCHAR printfilename[FILENAMELEN];

static TCHAR export_file_name[FILENAMELEN];
static FILE *export_file = NULL;
static FILE *import_file = NULL;

static int ann_updown = 0;
static int ann_shift = 0;
static int ann_print = 0;
static int ann_run = 0;
static int ann_battery = 0;
static int ann_g = 0;
static int ann_rad = 0;


// Forward declarations of functions included in this code module:
static void MyRegisterClass(HINSTANCE);
static BOOL InitInstance(HINSTANCE, int);
static LRESULT CALLBACK	MainWndProc(HWND, UINT, WPARAM, LPARAM);
static HWND CreateRpCommandBar(HWND);
static LRESULT CALLBACK	About(HWND, UINT, WPARAM, LPARAM);
static LRESULT CALLBACK	Preferences(HWND, UINT, WPARAM, LPARAM);
static int browse_file(HWND owner, TCHAR *title, int save, TCHAR *filter, TCHAR *defExt, TCHAR *buf, int buflen);
static void Quit();

static VOID CALLBACK repeater(HWND hwnd, UINT uMsg, UINT idEvent, DWORD dwTime);
static VOID CALLBACK timeout1(HWND hwnd, UINT uMsg, UINT idEvent, DWORD dwTime);
static VOID CALLBACK timeout2(HWND hwnd, UINT uMsg, UINT idEvent, DWORD dwTime);
static VOID CALLBACK timeout3(HWND hwnd, UINT uMsg, UINT idEvent, DWORD dwTime);

static void init_shell_state(int4 version);
static bool read_shell_state(int4 *version);
static bool write_shell_state();


int WINAPI WinMain(	HINSTANCE hInstance,
					HINSTANCE hPrevInstance,
					LPTSTR    lpCmdLine,
					int       nCmdShow)
{
	MSG msg;
	HACCEL hAccelTable;

	// Initialize global strings
#ifdef BCD_MATH
	LoadString(hInstance, IDS_APP_TITLE_DEC, szMainTitle, MAX_LOADSTRING);
#else
	LoadString(hInstance, IDS_APP_TITLE_BIN, szMainTitle, MAX_LOADSTRING);
#endif
	LoadString(hInstance, IDS_PRINTOUT_TITLE, szPrintOutTitle, MAX_LOADSTRING);
	LoadString(hInstance, IDC_FREE42, szMainWindowClass, MAX_LOADSTRING);
	LoadString(hInstance, IDC_FREE42_PRINTOUT, szPrintOutWindowClass, MAX_LOADSTRING);
	MyRegisterClass(hInstance);

	MyRegisterClass(hInstance);
	
	// Perform application initialization:
	if (!InitInstance (hInstance, nCmdShow)) 
	{
		return FALSE;
	}

	hAccelTable = LoadAccelerators(hInstance, (LPCTSTR)IDC_FREE42);

	// Main message loop:
	while (GetMessage(&msg, NULL, 0, 0)) 
	{
		if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg)) 
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	Quit();
	return msg.wParam;
}

//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
//  COMMENTS:
//
//    It is important to call this function so that the application 
//    will get 'well formed' small icons associated with it.
//
static void MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASS	wc;

    wc.style			= CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc		= (WNDPROC) MainWndProc;
    wc.cbClsExtra		= 0;
    wc.cbWndExtra		= 0;
    wc.hInstance		= hInstance;
    wc.hIcon			= LoadIcon(hInstance, MAKEINTRESOURCE(IDI_FREE42));
    wc.hCursor			= 0;
    wc.hbrBackground	= (HBRUSH) GetStockObject(WHITE_BRUSH);
    wc.lpszMenuName		= 0;
    wc.lpszClassName	= szMainWindowClass;

	RegisterClass(&wc);
}

//
//  FUNCTION: InitInstance(HANDLE, int)
//
//  PURPOSE: Saves instance handle and creates main window
//
//  COMMENTS:
//
//    In this function, we save the instance handle in a global variable and
//    create and display the main program window.
//
static BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
	hInst = hInstance;		// Store instance handle in our global variable

	srand(GetTickCount());

	GetModuleFileName(0, free42dirname, FILENAMELEN - 1);
	TCHAR *lastbackslash = _tcsrchr(free42dirname, _T('\\'));
	if (lastbackslash != NULL)
		*lastbackslash = 0;
	else
		free42dirname[0] = 0;
	_tcscpy(statefilename, free42dirname);
	_tcscat(statefilename, _T("\\state.bin"));
	_tcscpy(printfilename, free42dirname);
	_tcscat(printfilename, _T("\\print.bin"));

	int init_mode;
	int4 version;

	statefile = _tfopen(statefilename, _T("rb"));
	if (statefile != NULL) {
		if (read_shell_state(&version))
			init_mode = 1;
		else {
			init_shell_state(-1);
			init_mode = 2;
		}
	} else {
		init_shell_state(-1);
		init_mode = 0;
	}

	//If it is already running, then focus on the window
	HWND hWnd = FindWindow(szMainWindowClass, szMainTitle);	
	if (hWnd) 
	{
		// set focus to foremost child window
		// The "| 0x01" is used to bring any owned windows to the foreground and
		// activate them.
		SetForegroundWindow((HWND)((ULONG) hWnd | 0x00000001));
		return 0;
	} 

	hMainWnd = CreateWindow(szMainWindowClass, szMainTitle, WS_VISIBLE,
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, NULL, NULL, hInstance, NULL);
	if (!hMainWnd)
	{	
		return FALSE;
	}
	//When the main window is created using CW_USEDEFAULT the height of the menubar (if one
	// is created) is not taken into account. So we resize the window after creating it
	// if a menubar is present
	if (hwndCB)
    {
		RECT rc;
        RECT rcMenuBar;

		GetWindowRect(hMainWnd, &rc);
        GetWindowRect(hwndCB, &rcMenuBar);
		rc.bottom -= (rcMenuBar.bottom - rcMenuBar.top);
		
		MoveWindow(hMainWnd, rc.left, rc.top, rc.right-rc.left, rc.bottom-rc.top, FALSE);
	}

	long dummy_w, dummy_h;
	skin_load(state.skinName, free42dirname, &dummy_w, &dummy_h);

	core_init(init_mode, version);
	if (statefile != NULL) {
		fclose(statefile);
		statefile = NULL;
	}

	ShowWindow(hMainWnd, nCmdShow);
	UpdateWindow(hMainWnd);

	return TRUE;
}

static void shell_keydown() {
	if (ckey != 0) {
		HDC hdc = GetDC(hMainWnd);
		HDC memdc = CreateCompatibleDC(hdc);
		if (skey == -1)
			skey = skin_find_skey(ckey);
		skin_repaint_key(hdc, memdc, skey, 1);
		DeleteObject(memdc);
		ReleaseDC(hMainWnd, hdc);
	}
	if (timer != 0) {
		KillTimer(NULL, timer);
		timer = 0;
	}
	if (timer3 != 0 && ckey != 28 /* SHIFT */) {
		KillTimer(NULL, timer3);
		timer3 = 0; 
		core_timeout3(0);
	}
	int repeat;
	if (ckey >= 38 && ckey <= 255) {
		// Macro
		unsigned char *macro = skin_find_macro(ckey);
		if (macro == NULL || *macro == 0) {
			squeak();
			return;
		}
		while (*macro != 0) {
			running = core_keydown(*macro++, &enqueued, &repeat);
			if (*macro != 0 && !enqueued)
				core_keyup();
		}
		repeat = 0;
	} else
		running = core_keydown(ckey, &enqueued, &repeat);
	if (!running) {
		if (repeat)
			timer = SetTimer(NULL, 0, 1000, repeater);
		else if (!enqueued)
			timer = SetTimer(NULL, 0, 250, timeout1);
	}
}

static void shell_keyup() {
	HDC hdc = GetDC(hMainWnd);
	HDC memdc = CreateCompatibleDC(hdc);
	skin_repaint_key(hdc, memdc, skey, 0);
	DeleteObject(memdc);
	ReleaseDC(hMainWnd, hdc);
	ckey = 0;
	skey = -1;
	if (timer != 0) {
		KillTimer(NULL, timer);
		timer = 0;
	}
	if (!enqueued)
		running = core_keyup();
}

//
//  FUNCTION: MainWndProc(HWND, unsigned, WORD, LONG)
//
//  PURPOSE:  Processes messages for the main window.
//
//  WM_COMMAND	- process the application menu
//  WM_PAINT	- Paint the main window
//  WM_DESTROY	- post a quit message and return
//
//
static LRESULT CALLBACK MainWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	int wmId, wmEvent;

	switch (message) 
	{
		case WM_COMMAND:
			wmId    = LOWORD(wParam); 
			wmEvent = HIWORD(wParam); 
			// Parse the menu selections:
			switch (wmId)
			{	
				case IDM_PREFERENCES:
					DialogBox(hInst, (LPCTSTR)IDD_PREFERENCES, hWnd, (DLGPROC)Preferences);
					break;
				case IDM_EXIT:
					DestroyWindow(hWnd);
					break;
				case IDM_ABOUT:
					DialogBox(hInst, (LPCTSTR)IDD_ABOUTBOX, hWnd, (DLGPROC)About);
				    break;
				case IDOK:
					DestroyWindow(hWnd);
					break;
				default:
				   return DefWindowProc(hWnd, message, wParam, lParam);
			}
			break;
		case WM_CREATE:
			hwndCB = CreateRpCommandBar(hWnd);
            // Initialize the shell activate info structure
            memset (&s_sai, 0, sizeof (s_sai));
            s_sai.cbSize = sizeof (s_sai);
			break;
		case WM_PAINT: {
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(hWnd, &ps);
			HDC memdc = CreateCompatibleDC(hdc);
			skin_repaint(hdc, memdc);
			skin_repaint_display(hdc, memdc);
			skin_repaint_annunciator(hdc, memdc, 1, ann_updown);
			skin_repaint_annunciator(hdc, memdc, 2, ann_shift);
			skin_repaint_annunciator(hdc, memdc, 3, ann_print);
			skin_repaint_annunciator(hdc, memdc, 4, ann_run);
			skin_repaint_annunciator(hdc, memdc, 5, ann_battery);
			skin_repaint_annunciator(hdc, memdc, 6, ann_g);
			skin_repaint_annunciator(hdc, memdc, 7, ann_rad);
			if (ckey != 0)
				skin_repaint_key(hdc, memdc, skey, 1);
			DeleteDC(memdc);
			EndPaint(hWnd, &ps);
			break;
		}
		case WM_LBUTTONDOWN: {
			if (ckey == 0) {
				int x = LOWORD(lParam);  // horizontal position of cursor
				int y = HIWORD(lParam);  // vertical position of cursor
				skin_find_key(x, y, &skey, &ckey);
				if (ckey != 0) {
					shell_keydown();
					mouse_key = true;
				}
			}
			break;
		}
		case WM_LBUTTONUP:
			if (ckey != 0 && mouse_key)
				shell_keyup();
			break;
		case WM_DESTROY:
			CommandBar_Destroy(hwndCB);
			PostQuitMessage(0);
			break;
		case WM_ACTIVATE:
            // Notify shell of our activate message
			SHHandleWMActivate(hWnd, wParam, lParam, &s_sai, FALSE);
     		break;
		case WM_SETTINGCHANGE:
			SHHandleWMSettingChange(hWnd, wParam, lParam, &s_sai);
     		break;
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
   }
   return 0;
}

static HWND CreateRpCommandBar(HWND hwnd)
{
	SHMENUBARINFO mbi;

	memset(&mbi, 0, sizeof(SHMENUBARINFO));
	mbi.cbSize     = sizeof(SHMENUBARINFO);
	mbi.hwndParent = hwnd;
	mbi.nToolBarId = IDM_MENU;
	mbi.hInstRes   = hInst;
	mbi.nBmpId     = 0;
	mbi.cBmpImages = 0;

	if (!SHCreateMenuBar(&mbi)) 
		return NULL;

	return mbi.hwndMB;
}

// Mesage handler for the About box.
static LRESULT CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	//SHINITDLGINFO shidi;

	switch (message)
	{
		case WM_INITDIALOG:
			// Create a Done button and size it.  
			//shidi.dwMask = SHIDIM_FLAGS;
			//shidi.dwFlags = SHIDIF_DONEBUTTON | SHIDIF_SIPDOWN | SHIDIF_SIZEDLGFULLSCREEN;
			//shidi.hDlg = hDlg;
			//SHInitDialog(&shidi);
			return TRUE; 

		case WM_COMMAND:
			if (LOWORD(wParam) == IDOK)
			{
				EndDialog(hDlg, LOWORD(wParam));
				return TRUE;
			}
			break;
	}
    return FALSE;
}

// Mesage handler for preferences dialog.
static LRESULT CALLBACK Preferences(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	// TODO: track focus changes so that we can force the IDC_PRINTER_GIF_HEIGHT
	// text field to contain a legal value whenever it loses focus.
	// Question: HOW do you track focus changes? I don't know what message
	// to look for.

	switch (message)
	{
		case WM_INITDIALOG: {
			// Initialize the dialog from the prefs structures
			HWND ctl;
			if (core_settings.matrix_singularmatrix) {
				ctl = GetDlgItem(hDlg, IDC_MATRIX_SINGULARMATRIX);
				SendMessage(ctl, BM_SETCHECK, 1, 0);
			}
			if (core_settings.matrix_outofrange) {
				ctl = GetDlgItem(hDlg, IDC_MATRIX_OUTOFRANGE);
				SendMessage(ctl, BM_SETCHECK, 1, 0);
			}
			if (state.printerToTxtFile) {
				ctl = GetDlgItem(hDlg, IDC_PRINTER_TXT);
				SendMessage(ctl, BM_SETCHECK, 1, 0);
			}
			SetDlgItemText(hDlg, IDC_PRINTER_TXT_NAME, state.printerTxtFileName);
			if (core_settings.raw_text) {
				ctl = GetDlgItem(hDlg, IDC_RAW_TEXT);
				SendMessage(ctl, BM_SETCHECK, 1, 0);
			}
			if (state.printerToGifFile) {
				ctl = GetDlgItem(hDlg, IDC_PRINTER_GIF);
				SendMessage(ctl, BM_SETCHECK, 1, 0);
			}
			SetDlgItemText(hDlg, IDC_PRINTER_GIF_NAME, state.printerGifFileName);
			SetDlgItemInt(hDlg, IDC_PRINTER_GIF_HEIGHT, state.printerGifMaxLength, TRUE);
			return TRUE;
		}

		case WM_COMMAND: {
			int cmd = LOWORD(wParam);
			switch (cmd) {
				case IDOK: {
					// Update the prefs structures from the dialog
					HWND ctl = GetDlgItem(hDlg, IDC_MATRIX_SINGULARMATRIX);
					core_settings.matrix_singularmatrix = SendMessage(ctl, BM_GETCHECK, 0, 0) != 0;
					ctl = GetDlgItem(hDlg, IDC_MATRIX_OUTOFRANGE);
					core_settings.matrix_outofrange = SendMessage(ctl, BM_GETCHECK, 0, 0) != 0;

					ctl = GetDlgItem(hDlg, IDC_PRINTER_TXT);
					state.printerToTxtFile = SendMessage(ctl, BM_GETCHECK, 0, 0);
					TCHAR buf[FILENAMELEN];
					GetDlgItemText(hDlg, IDC_PRINTER_TXT_NAME, buf, FILENAMELEN - 1);
					int len = _tcslen(buf);
					if (len > 0 && (len < 4 || _tcsicmp(buf + len - 4, _T(".txt")) != 0))
						_tcscat(buf, _T(".txt"));
//					if (print_txt != NULL && (!state.printerToTxtFile
//							|| _tcsicmp(state.printerTxtFileName, buf) != 0)) {
//						fclose(print_txt);
//						print_txt = NULL;
//					}
					_tcscpy(state.printerTxtFileName, buf);
					ctl = GetDlgItem(hDlg, IDC_RAW_TEXT);
					core_settings.raw_text = SendMessage(ctl, BM_GETCHECK, 0, 0) != 0;
					ctl = GetDlgItem(hDlg, IDC_PRINTER_GIF);
					state.printerToGifFile = SendMessage(ctl, BM_GETCHECK, 0, 0);
					BOOL success;
					int maxlen = (int) GetDlgItemInt(hDlg, IDC_PRINTER_GIF_HEIGHT, &success, TRUE);
					state.printerGifMaxLength = !success ? 256 : maxlen < 32 ? 32 : maxlen > 32767 ? 32767 : maxlen;
					GetDlgItemText(hDlg, IDC_PRINTER_GIF_NAME, buf, FILENAMELEN - 1);
					len = _tcslen(buf);
					if (len > 0 && (len < 4 || _tcsicmp(buf + len - 4, _T(".gif")) != 0))
						_tcscat(buf, _T(".gif"));
//				    if (print_gif != NULL && (!state.printerToGifFile
//							|| _tcsicmp(state.printerGifFileName, buf) != 0)) {
//						shell_finish_gif(gif_seeker, gif_writer);
//						fclose(print_gif);
//						print_gif = NULL;
//					}
					_tcscpy(state.printerGifFileName, buf);
					// fall through
				}
				case IDCANCEL:
					EndDialog(hDlg, LOWORD(wParam));
					return TRUE;
				case IDC_PRINTER_TXT_BROWSE: {
					TCHAR buf[FILENAMELEN];
					GetDlgItemText(hDlg, IDC_PRINTER_TXT_NAME, buf, FILENAMELEN - 1);
					if (browse_file(hDlg,
									_T("Select Text File Name"),
									1,
									_T("Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0\0"),
									_T("txt"),
									buf,
									FILENAMELEN))
						SetDlgItemText(hDlg, IDC_PRINTER_TXT_NAME, buf);
					return TRUE;
				}
				case IDC_PRINTER_GIF_BROWSE: {
					TCHAR buf[FILENAMELEN];
					GetDlgItemText(hDlg, IDC_PRINTER_GIF_NAME, buf, FILENAMELEN - 1);
					if (browse_file(hDlg,
									_T("Select GIF File Name"),
									1,
									_T("GIF Files (*.gif)\0*.gif\0All Files (*.*)\0*.*\0\0"),
									_T("gif"),
									buf,
									FILENAMELEN))
						SetDlgItemText(hDlg, IDC_PRINTER_GIF_NAME, buf);
					return TRUE;
				}
			}
			break;
		}
	}
    return FALSE;
}

static int browse_file(HWND owner, TCHAR *title, int save, TCHAR *filter, TCHAR *defExt, TCHAR *buf, int buflen) {
	OPENFILENAME ofn;
	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = owner;
	ofn.lpstrFilter = filter;
	ofn.lpstrCustomFilter = NULL;
	ofn.lpstrFile = buf;
	ofn.nMaxFile = buflen;
	ofn.lpstrFileTitle = NULL;
	ofn.lpstrInitialDir = NULL;
	ofn.lpstrTitle = title;
	ofn.Flags = OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;
	ofn.lpstrDefExt = defExt;
	return save ? GetSaveFileName(&ofn) : GetOpenFileName(&ofn);
}

static void Quit() {
	statefile = _tfopen(statefilename, _T("wb"));
	if (statefile != NULL)
		write_shell_state();
	//core_quit();
	if (statefile != NULL)
		fclose(statefile);
}

static VOID CALLBACK repeater(HWND hwnd, UINT uMsg, UINT idEvent, DWORD dwTime) {
	KillTimer(NULL, timer);
	core_repeat();
	timer = SetTimer(NULL, 0, 200, repeater);
}

static VOID CALLBACK timeout1(HWND hwnd, UINT uMsg, UINT idEvent, DWORD dwTime) {
	KillTimer(NULL, timer);
	if (ckey != 0) {
		core_keytimeout1();
		timer = SetTimer(NULL, 0, 1750, timeout2);
	} else
		timer = 0;
}

static VOID CALLBACK timeout2(HWND hwnd, UINT uMsg, UINT idEvent, DWORD dwTime) {
	KillTimer(NULL, timer);
	if (ckey != 0)
		core_keytimeout2();
	timer = 0;
}

static VOID CALLBACK timeout3(HWND hwnd, UINT uMsg, UINT idEvent, DWORD dwTime) {
	KillTimer(NULL, timer3);
	core_timeout3(1);
	timer3 = 0;
}

void shell_blitter(const char *bits, int bytesperline, int x, int y,
				   int width, int height) {
	HDC hdc = GetDC(hMainWnd);
	skin_display_blitter(hdc, bits, bytesperline, x, y, width, height);
	if (skey >= -7 && skey <= -2) {
		HDC memdc = CreateCompatibleDC(hdc);
		skin_repaint_key(hdc, memdc, skey, 1);
		DeleteObject(memdc);
	}
	ReleaseDC(hMainWnd, hdc);
}

void shell_beeper(int frequency, int duration) {
	// TODO
}

/* shell_annunciators()
 * Callback invoked by the emulator core to change the state of the display
 * annunciators (up/down, shift, print, run, battery, (g)rad).
 * Every parameter can have values 0 (turn off), 1 (turn on), or -1 (leave
 * unchanged).
 * The battery annunciator is missing from the list; this is the only one of
 * the lot that the emulator core does not actually have any control over, and
 * so the shell is expected to handle that one by itself.
 */
void shell_annunciators(int updn, int shf, int prt, int run, int g, int rad) {
	HDC hdc = GetDC(hMainWnd);
	HDC memdc = CreateCompatibleDC(hdc);

    if (updn != -1 && ann_updown != updn) {
		ann_updown = updn;
		skin_repaint_annunciator(hdc, memdc, 1, ann_updown);
    }
    if (shf != -1 && ann_shift != shf) {
		ann_shift = shf;
		skin_repaint_annunciator(hdc, memdc, 2, ann_shift);
    }
    if (prt != -1 && ann_print != prt) {
		ann_print = prt;
		skin_repaint_annunciator(hdc, memdc, 3, ann_print);
    }
    if (run != -1 && ann_run != run) {
		ann_run = run;
		skin_repaint_annunciator(hdc, memdc, 4, ann_run);
    }
    if (g != -1 && ann_g != g) {
		ann_g = g;
		skin_repaint_annunciator(hdc, memdc, 6, ann_g);
    }
    if (rad != -1 && ann_rad != rad) {
		ann_rad = rad;
		skin_repaint_annunciator(hdc, memdc, 7, ann_rad);
    }

	DeleteDC(memdc);
	ReleaseDC(hMainWnd, hdc);
}

int shell_wants_cpu() {
	MSG msg;
	return PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE) != 0;
}

void shell_delay(int duration) {
	Sleep(duration);
}

/* Callback to ask the shell to call core_timeout3() after the given number of
 * milliseconds. If there are keystroke events during that time, the timeout is
 * cancelled. (Pressing 'shift' does not cancel the timeout.)
 * This function supports the delay after SHOW, MEM, and shift-VARMENU.
 */
void shell_request_timeout3(int delay) {
	if (timer3 != 0)
		KillTimer(NULL, timer3);
	timer3 = SetTimer(NULL, 0, delay, timeout3);
}

int4 shell_read_saved_state(void *buf, int4 bufsize) {
    if (statefile == NULL)
        return -1;
    else {
        int4 n = fread(buf, 1, bufsize, statefile);
        if (n != bufsize && ferror(statefile)) {
            fclose(statefile);
            statefile = NULL;
            return -1;
        } else
            return n;
    }
}

bool shell_write_saved_state(const void *buf, int4 nbytes) {
    if (statefile == NULL)
        return false;
    else {
        int4 n = fwrite(buf, 1, nbytes, statefile);
        if (n != nbytes) {
            fclose(statefile);
			DeleteFile(statefilename);
            statefile = NULL;
            return false;
        } else
            return true;
    }
}

int4 shell_get_mem() {
	MEMORYSTATUS memstat;
	GlobalMemoryStatus(&memstat);
	return memstat.dwAvailPhys;
}

int shell_low_battery() {
	// TODO
	int lowbat = 0;
	if (ann_battery != lowbat) {
		ann_battery = lowbat;
		HDC hdc = GetDC(hMainWnd);
		HDC memdc = CreateCompatibleDC(hdc);
		skin_repaint_annunciator(hdc, memdc, 5, ann_battery);
		DeleteDC(memdc);
		ReleaseDC(hMainWnd, hdc);
	}
	return lowbat;
}

void shell_powerdown() {
	PostQuitMessage(0);
}

double shell_random_seed() {
	return ((double) rand()) / (RAND_MAX + 1.0);
}

uint4 shell_milliseconds() {
	return GetTickCount();
}

void shell_print(const char *text, int length,
		 const char *bits, int bytesperline,
		 int x, int y, int width, int height) {
	// TODO
}

int shell_write(const char *buf, int4 buflen) {
    int4 written;
    if (export_file == NULL)
		return 0;
    written = fwrite(buf, 1, buflen, export_file);
    if (written != buflen) {
		TCHAR buf[1000];
		fclose(export_file);
		export_file = NULL;
		_stprintf(buf, _T("Writing \"%s\" failed."), export_file_name);
		MessageBox(hMainWnd, buf, _T("Message"), MB_ICONWARNING);
		return 0;
    } else
		return 1;
}

int4 shell_read(char *buf, int4 buflen) {
    int4 nread;
    if (import_file == NULL)
		return -1;
    nread = fread(buf, 1, buflen, import_file);
    if (nread != buflen && ferror(import_file)) {
		fclose(import_file);
		import_file = NULL;
		MessageBox(hMainWnd, _T("An error occurred; import was terminated prematurely."), _T("Message"), MB_ICONWARNING);
		return -1;
    } else
		return nread;
}

shell_bcd_table_struct *shell_get_bcd_table() {
	return NULL;
}

shell_bcd_table_struct *shell_put_bcd_table(shell_bcd_table_struct* bcdtab,
					    uint4 size) {
	return bcdtab;
}

void shell_release_bcd_table(shell_bcd_table_struct *bcdtab) {
	free(bcdtab);
}

static void init_shell_state(int4 version) {
	switch (version) {
		case -1:
			state.printerToTxtFile = 0;
			state.printerToGifFile = 0;
			state.printerTxtFileName[0] = 0;
			state.printerGifFileName[0] = 0;
			state.printerGifMaxLength = 256;
			state.skinName[0] = 0;
			// fall through
		case 0:
			// current version (SHELL_VERSION = 0),
			// so nothing to do here since everything
			// was initialized from the state file.
			;
	}
}

static bool read_shell_state(int4 *ver) {
    int4 magic;
    int4 version;
    int4 state_size;
    int4 state_version;

    if (shell_read_saved_state(&magic, sizeof(int4)) != sizeof(int4))
        return false;
    if (magic != FREE42_MAGIC)
        return false;

    if (shell_read_saved_state(&version, sizeof(int4)) != sizeof(int4))
        return false;
    if (version < 0 || version > FREE42_VERSION)
        /* Unknown state file version */
        return false;

	if (version > 0) {
		if (shell_read_saved_state(&state_size, sizeof(int4)) != sizeof(int4))
			return false;
		if (shell_read_saved_state(&state_version, sizeof(int4)) != sizeof(int4))
			return false;
		if (state_version < 0 || state_version > SHELL_VERSION)
			/* Unknown shell state version */
			return false;
		if (shell_read_saved_state(&state, state_size) != state_size)
			return false;
		// Initialize the parts of the shell state
		// that were NOT read from the state file
		init_shell_state(state_version);
	} else
		init_shell_state(-1);

	*ver = version;
    return true;
}

static bool write_shell_state() {
    int4 magic = FREE42_MAGIC;
    int4 version = FREE42_VERSION;
    int4 state_size = sizeof(state_type);
    int4 state_version = SHELL_VERSION;

    if (!shell_write_saved_state(&magic, sizeof(int4)))
        return false;
    if (!shell_write_saved_state(&version, sizeof(int4)))
        return false;
    if (!shell_write_saved_state(&state_size, sizeof(int4)))
        return false;
    if (!shell_write_saved_state(&state_version, sizeof(int4)))
        return false;
    if (!shell_write_saved_state(&state, sizeof(state_type)))
        return false;

    return true;
}