/*
OmniMIDI, a fork of BASSMIDI Driver
Thank you Kode54 for allowing me to fork your awesome driver.
*/

#pragma once

typedef unsigned __int64 QWORD;

#define STRICT
#define __STDC_LIMIT_MACROS
#define _CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES 1

#define BASSDEF(f) (WINAPI *f)
#define BASSENCDEF(f) (WINAPI *f)	
#define BASSMIDIDEF(f) (WINAPI *f)	
#define Between(value, a, b) (value <= b && value >= a)

#define MIDI_IO_PACKED 0x00000000L			// Legacy mode, used by all MIDI apps
#define MIDI_IO_COOKED 0x00000002L			// Stream mode, used by some apps (Such as Pinball 3D), NOT SUPPORTED

#define ERRORCODE		0
#define CAUSE			1
#define LONGMSG_MAXSIZE	65535

#define LOCK_VM_IN_WORKING_SET 1
#define LOCK_VM_IN_RAM 2

#include <Windows.h>
#include <process.h>
#include <mmddk.h>
#include "OmniMIDI.h"

// BASS headers
#include <bass.h>
#include <bassmidi.h>

#define LOADBASSFUNCTION(f) *((void**)&f)=GetProcAddress(bass,#f)
#define LOADBASSMIDIFUNCTION(f) *((void**)&f)=GetProcAddress(bassmidi,#f)

static HMODULE bass = 0;			// bass handle
static HMODULE bassmidi = 0;		// bassmidi handle

// BASS stuff
static HSTREAM OMStream = NULL;

// Device stuff
static CRITICAL_SECTION mim_section;
static WCHAR installpath[MAX_PATH];
static HSOUNDFONT DefaultSF;
static WCHAR SynthNameW[32];
static BOOL Device_Initialized = FALSE;
static DWORD_PTR OMCallback = NULL;
static DWORD_PTR OMInstance = NULL;
static DWORD OMFlags = NULL;
static HDRVR OMDevice = NULL;

// Values
HINSTANCE hinst = NULL;

// EVBuffer
struct evbuf_t {
	UINT			uMsg;
	DWORD_PTR		dwParam1;
	DWORD_PTR		dwParam2;
};	// The buffer's structure

static evbuf_t * evbuf;						// The buffer
static volatile ULONGLONG writehead = 0;	// Current write position in the buffer
static volatile ULONGLONG readhead = 0;		// Current read position in the buffer
static volatile LONGLONG eventcount = 0;	// Total events present in the buffer
static ULONGLONG EvBufferSize = 4096;

// Built-in blacklist
static LPCWSTR BuiltInBlacklist[30] =
{
	L"Battle.net Launcher.exe",
	L"LogonUI.exe",
	L"LSASS.EXE",
	L"NVDisplay.Container.exe",
	L"NVIDIA Share.exe",
	L"NVIDIA Web Helper.exe",
	L"RustClient.exe",
	L"SearchUI.exe",
	L"SecurityHealthService.exe",
	L"SecurityHealthSystray.exe",
	L"ShellExperienceHost.exe",
	L"SndVol.exe",
	L"WUDFHost.exe",
	L"conhost.exe",
	L"consent.exe",
	L"csrss.exe",
	L"ctfmon.exe",
	L"dwm.exe",
	L"explorer.exe",
	L"lsass.exe",
	L"mstsc.exe",
	L"nvcontainer.exe",
	L"nvsphelper64.exe",
	L"services.exe",
	L"smss.exe",
	L"spoolsv.exe",
	L"vcpkgsrv.exe",
	L"winlogon.exe",
	L"winmgmt.exe",
	L"vmware-hostd.exe"
};

// Functions that aren't present in the Windows Server 2003 SDK
typedef UINT (NTAPI * NLVM)(IN HANDLE process, IN OUT void** baseAddress, IN OUT ULONG* size, IN ULONG flags);
typedef UINT (NTAPI * NULVM)(IN HANDLE process, IN OUT void** baseAddress, IN OUT ULONG* size, IN ULONG flags);
typedef BOOL (NTAPI * PRFSW)(LPWSTR pszPath);
typedef BOOL (NTAPI * PSPW)(LPWSTR pszPath);

static HINSTANCE NTDLLInstance;
static HINSTANCE SHLWAPIInstance;

static FARPROC NLVMProc;
static FARPROC NULVMProc;
static FARPROC PRFSWProc;
static FARPROC PSPWProc;

static NLVM NtLockVirtualMemory;
static NULVM NtUnlockVirtualMemory;
static PRFSW PathRemoveFileSpecW;
static PSPW PathStripPathW;

extern "C" BOOL APIENTRY DllMain(HANDLE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	switch (fdwReason) {
	case DLL_PROCESS_ATTACH:
		hinst = (HINSTANCE)hinstDLL;
		DisableThreadLibraryCalls((HMODULE)hinstDLL);

		// Load funcs
		NTDLLInstance = GetModuleHandle("ntdll");
		NLVMProc = GetProcAddress(HMODULE(NTDLLInstance), "NtLockVirtualMemory");
		NULVMProc = GetProcAddress(HMODULE(NTDLLInstance), "NtUnlockVirtualMemory");

		SHLWAPIInstance = LoadLibrary("shlwapi.dll");
		PRFSWProc = GetProcAddress(HMODULE(SHLWAPIInstance), "PathRemoveFileSpecW");
		PSPWProc = GetProcAddress(HMODULE(SHLWAPIInstance), "PathStripPathW");
		
		if (!NLVMProc | !NULVMProc | !PRFSWProc | !PSPWProc) exit(0);

		NtLockVirtualMemory = NLVM(NLVMProc);
		NtUnlockVirtualMemory = NULVM(NULVMProc);
		PathRemoveFileSpecW = PRFSW(PRFSWProc);
		PathStripPathW = PSPW(PSPWProc);
		break;
	case DLL_PROCESS_DETACH: break;
	case DLL_THREAD_ATTACH: break;
	case DLL_THREAD_DETACH: break;
	}

	return TRUE;
}

STDAPI_(LONG_PTR) DriverProc(DWORD_PTR dwDriverId, HDRVR hdrvr, UINT uMsg, LPARAM lParam1, LPARAM lParam2)
{
	switch (uMsg) {
	case DRV_QUERYCONFIGURE:
		return DRV_OK;
	case DRV_CONFIGURE:
		return DRV_OK;
	case DRV_LOAD:
		InitializeCriticalSection(&mim_section);
		return DRV_OK;
	case DRV_ENABLE:
		return DRV_OK;
	case DRV_REMOVE:
		DeleteCriticalSection(&mim_section);
		return DRV_OK;
	case DRV_FREE:
		return DRV_OK;
	case DRV_OPEN:
		OMDevice = hdrvr;
		return DRV_OK;
	case DRV_CLOSE:
		return DRV_OK;
	default:
		return DRV_OK;
	}
}

DWORD modGetCaps(PVOID capsPtr, DWORD capsSize) {
	static MIDIOUTCAPS2W MIDICaps = { 0 };
	const GUID OMCLSID = { 0x210CE0E8, 0x6837, 0x448E, { 0xB1, 0x3F, 0x09, 0xFE, 0x71, 0xE7, 0x44, 0xEC } };

	ZeroMemory(SynthNameW, 32);
	wcscpy_s(SynthNameW, 32, L"OmniMIDI for NT 5.0\0");

	// Prepare the caps item
	if (!MIDICaps.wMid) {
		memcpy(MIDICaps.szPname, SynthNameW, sizeof(SynthNameW));
		MIDICaps.ManufacturerGuid = OMCLSID;
		MIDICaps.NameGuid = OMCLSID;
		MIDICaps.ProductGuid = OMCLSID;
		MIDICaps.dwSupport = MIDICAPS_VOLUME;
		MIDICaps.wChannelMask = 0xFFFF;
		MIDICaps.wMid = 0xFFFF;
		MIDICaps.wNotes = 65535;
		MIDICaps.wPid = 0x000A;
		MIDICaps.wTechnology = MOD_MIDIPORT;
		MIDICaps.wVoices = 65535;
		MIDICaps.vDriverVersion = 0x0501;
	}

	// Copy the item to the app's caps
	memcpy(capsPtr, &MIDICaps, min(sizeof(MIDICaps), capsSize));
	return MMSYSERR_NOERROR;
}

BOOL LoadBASSFunctions() {
	try {
		WCHAR bassmidipath[MAX_PATH];
		WCHAR basspath[MAX_PATH];

		memset(installpath, 0, MAX_PATH); 
		GetModuleFileNameW(hinst, installpath, MAX_PATH);
		PathRemoveFileSpecW(installpath);
		MessageBoxW(NULL, L"Keep in mind that this is just a proof of concept!\nPlease don't complain about bugs and other stuff like that, kthx.", L"OmniMIDI -  WARNING", MB_OK | MB_ICONWARNING | MB_SYSTEMMODAL);

		memset(basspath, 0, MAX_PATH); 
		lstrcatW(basspath, installpath);
		lstrcatW(basspath, L"\\bass.dll");
		if (!(bass = LoadLibraryW(basspath))) {
			MessageBoxW(NULL, basspath, L"OmniMIDI - Failed to load library", MB_OK | MB_ICONERROR | MB_SYSTEMMODAL);
			exit(0);
		}

		memset(bassmidipath, 0, MAX_PATH); 
		lstrcatW(bassmidipath, installpath);
		lstrcatW(bassmidipath, L"\\bassmidi.dll");
		if (!(bassmidi = LoadLibraryW(bassmidipath))) {
			MessageBoxW(NULL, basspath, L"OmniMIDI - Failed to load library", MB_OK | MB_ICONERROR | MB_SYSTEMMODAL);
			exit(0);
		}

		LOADBASSFUNCTION(BASS_ChannelFlags);
		LOADBASSFUNCTION(BASS_ChannelGetAttribute);
		LOADBASSFUNCTION(BASS_ChannelGetData);
		LOADBASSFUNCTION(BASS_ChannelGetLevelEx);
		LOADBASSFUNCTION(BASS_ChannelIsActive);
		LOADBASSFUNCTION(BASS_ChannelPlay);
		LOADBASSFUNCTION(BASS_ChannelRemoveFX);
		LOADBASSFUNCTION(BASS_ChannelSeconds2Bytes);
		LOADBASSFUNCTION(BASS_ChannelSetAttribute);
		LOADBASSFUNCTION(BASS_ChannelSetDevice);
		LOADBASSFUNCTION(BASS_ChannelSetFX);
		LOADBASSFUNCTION(BASS_ChannelSetSync);
		LOADBASSFUNCTION(BASS_ChannelStop);
		LOADBASSFUNCTION(BASS_ChannelUpdate);
		LOADBASSFUNCTION(BASS_Update);
		LOADBASSFUNCTION(BASS_ErrorGetCode);
		LOADBASSFUNCTION(BASS_Free);
		LOADBASSFUNCTION(BASS_Stop);
		LOADBASSFUNCTION(BASS_GetDevice);
		LOADBASSFUNCTION(BASS_GetDeviceInfo);
		LOADBASSFUNCTION(BASS_GetInfo);
		LOADBASSFUNCTION(BASS_Init);
		LOADBASSFUNCTION(BASS_PluginLoad);
		LOADBASSFUNCTION(BASS_GetConfig);
		LOADBASSFUNCTION(BASS_SetConfig);
		LOADBASSFUNCTION(BASS_SetDevice);
		LOADBASSFUNCTION(BASS_SetVolume);
		LOADBASSFUNCTION(BASS_StreamFree);
		LOADBASSFUNCTION(BASS_FXSetParameters);
		LOADBASSMIDIFUNCTION(BASS_MIDI_FontFree);
		LOADBASSMIDIFUNCTION(BASS_MIDI_FontInit);
		LOADBASSMIDIFUNCTION(BASS_MIDI_FontLoad);
		LOADBASSMIDIFUNCTION(BASS_MIDI_StreamCreate);
		LOADBASSMIDIFUNCTION(BASS_MIDI_StreamEvent);
		LOADBASSMIDIFUNCTION(BASS_MIDI_StreamEvents);
		LOADBASSMIDIFUNCTION(BASS_MIDI_StreamGetEvent);
		LOADBASSMIDIFUNCTION(BASS_MIDI_StreamLoadSamples);
		LOADBASSMIDIFUNCTION(BASS_MIDI_StreamSetFonts);

		return TRUE;
	}
	catch (...) {
		exit(0);
		return FALSE;
	}
}

MMRESULT ParseLongData(UINT uMsg, DWORD_PTR dwParam1, DWORD dwParam2) {
	MIDIHDR* IIMidiHdr = (MIDIHDR*)dwParam1;
	if (!(IIMidiHdr->dwFlags & MHDR_PREPARED)) return MIDIERR_UNPREPARED;

	// Mark the buffer as in queue
	IIMidiHdr->dwFlags &= ~MHDR_DONE;
	IIMidiHdr->dwFlags |= MHDR_INQUEUE;

	// Do the stuff with it, if it's not to be ignored
	BASS_MIDI_StreamEvents(OMStream, BASS_MIDI_EVENTS_RAW, IIMidiHdr->lpData, IIMidiHdr->dwBytesRecorded);

	// Mark the buffer as done
	IIMidiHdr->dwFlags &= ~MHDR_INQUEUE;
	IIMidiHdr->dwFlags |= MHDR_DONE;

	DriverCallback(OMCallback, OMFlags, OMDevice, MOM_DONE, OMInstance, dwParam1, 0);
	return MMSYSERR_NOERROR;
}

MMRESULT ParseData(UINT uMsg, DWORD_PTR dwParam1, DWORD dwParam2) {
	EnterCriticalSection(&mim_section);

	if (++writehead >= EvBufferSize) writehead = 0;

	evbuf[writehead].uMsg = uMsg;
	evbuf[writehead].dwParam1 = dwParam1;
	evbuf[writehead].dwParam2 = dwParam2;

	LeaveCriticalSection(&mim_section);

	// Haha everything is fine
	return MMSYSERR_NOERROR;
}

int PlayData() {
	ULONGLONG whe = writehead;

	if (!((readhead != whe) ? ~0 : 0)) {
		return ~0;
	}
	do {
		EnterCriticalSection(&mim_section);
		if (++readhead >= EvBufferSize) readhead = 0;
		evbuf_t TempBuffer = *(evbuf + readhead);
		LeaveCriticalSection(&mim_section);

		if (!(TempBuffer.dwParam1 - 0x80 & 0xC0))
		{
			BASS_MIDI_StreamEvents(OMStream, BASS_MIDI_EVENTS_RAW, &TempBuffer.dwParam1, 3);
			break;
		}

		DWORD len = 3;

		if (!((TempBuffer.dwParam1 - 0xC0) & 0xE0)) len = 2;
		else if ((TempBuffer.dwParam1 & 0xF0) == 0xF0)
		{
			switch (TempBuffer.dwParam1 & 0xF)
			{
			case 3:
				len = 2;
				break;
			default:
				len = 1;
				break;
			}
		}
		
		BASS_MIDI_StreamEvents(OMStream, BASS_MIDI_EVENTS_RAW, &TempBuffer.dwParam1, len);
	} while ((readhead != whe) ? ~0 : 0);

	return 0;
}

void UpdateStream(void * nothing) {
	while (Device_Initialized) {
		PlayData();
		BASS_ChannelUpdate(OMStream, 0);
		Sleep(1);
	}
}

void InitializeBASS() {
	char sfpath[MAX_PATH];
	OPENFILENAME ofn;

	if (!Device_Initialized) {
		// Allocate buffer
		evbuf = (evbuf_t *)calloc(EvBufferSize, sizeof(evbuf_t));
		if (!evbuf) {
			MessageBoxW(NULL, L"An error has occured while allocating the events buffer!", L"OmniMIDI - Error allocating memory", MB_OK | MB_ICONEXCLAMATION | MB_SYSTEMMODAL);
			exit(0x8);
		}

		LoadBASSFunctions();
		BOOL init = BASS_Init(-1, 44100, BASS_DEVICE_STEREO | BASS_DEVICE_DSOUND, 0, NULL);

		DWORD len=BASS_GetConfig(BASS_CONFIG_UPDATEPERIOD);
		BASS_INFO info;
		BASS_GetInfo(&info);
		len+=info.minbuf+1;
		BASS_SetConfig(BASS_CONFIG_UPDATETHREADS, 0);
		BASS_SetConfig(BASS_CONFIG_UPDATEPERIOD, 0);
		BASS_SetConfig(BASS_CONFIG_BUFFER, len);
		OMStream = BASS_MIDI_StreamCreate(16, 0, 44100);
		BASS_ChannelSetAttribute(OMStream, BASS_ATTRIB_MIDI_CHANS, 16);
		BASS_MIDI_StreamEvent(OMStream, 0, MIDI_EVENT_SYSTEM, MIDI_SYSTEM_DEFAULT);
		BASS_MIDI_StreamEvent(OMStream, 9, MIDI_EVENT_DRUMS, 1);
		BASS_ChannelSetAttribute(OMStream, BASS_ATTRIB_MIDI_VOICES, 500);
		BASS_ChannelSetAttribute(OMStream, BASS_ATTRIB_MIDI_CPU, 75);
		BASS_ChannelPlay(OMStream, FALSE);

		memset(&ofn, 0, sizeof(ofn));
		ofn.lStructSize = sizeof(ofn);
		ofn.hwndOwner = NULL;
		ofn.nMaxFile = MAX_PATH;
		ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST| OFN_HIDEREADONLY | OFN_EXPLORER;

		memset(sfpath, 0, MAX_PATH);
		ofn.lpstrFilter="Soundfonts (sf2/sf2pack)\0*.sf2;*.sf2pack\0All files\0*.*\0\0";
		ofn.lpstrFile=sfpath;
		if (GetOpenFileName(&ofn)) { 
			HSOUNDFONT NewSF = BASS_MIDI_FontInit(sfpath, 0);

			if (NewSF) {
				BASS_MIDI_FontLoad(NewSF, -1, -1);
				BASS_MIDI_FONT sf;
				sf.font = NewSF;
				sf.preset= -1;
				sf.bank = 0;
				BASS_MIDI_StreamSetFonts(0, &sf, 1);
				BASS_MIDI_StreamSetFonts(OMStream, &sf, 1);
				BASS_MIDI_FontFree(DefaultSF);
				DefaultSF = NewSF;
			}
			else {
				MessageBoxW(NULL, L"Failed to load SoundFont.\nPress OK to quit.", L"ERROR", MB_OK | MB_ICONERROR | MB_SYSTEMMODAL);
				exit(0);
			}
		}
		else exit(0);
		Device_Initialized = TRUE;
		_beginthread(UpdateStream, 0, NULL); 
	}
}

void TerminateBASS() {
	if (Device_Initialized) {
		// Free BASS
		BASS_ChannelStop(OMStream);
		BASS_StreamFree(OMStream);
		BASS_Stop();
		BASS_Free();

		// Free buffer
		memset(evbuf, 0, sizeof(evbuf));
		free(evbuf);
		evbuf = NULL;

		// Send callback
		DriverCallback(OMCallback, OMFlags, OMDevice, MOM_CLOSE, OMInstance, 0, 0);

		Device_Initialized = FALSE;
	}
}

DWORD BannedSystemProcess() {
	// These processes are PERMANENTLY banned because of some internal bugs inside them.
	wchar_t modulename[MAX_PATH];
	memset(modulename, 0, MAX_PATH);
	GetModuleFileNameW(NULL, modulename, MAX_PATH);
	PathStripPathW(modulename);

	// Check if the current process is banned
	for (int i = 0; i < sizeof(BuiltInBlacklist)/sizeof(BuiltInBlacklist[0]); i++) {
		// It's a match, the process is banned
		if (!_wcsicmp(modulename, BuiltInBlacklist[i])) return 0x0;
	}

	// All good, go on
	return 0x1;
}

// At last.
#include "KDMAPI.h"

STDAPI_(DWORD) modMessage(UINT uDeviceID, UINT uMsg, DWORD_PTR dwUser, DWORD_PTR dwParam1, DWORD_PTR dwParam2){
	switch (uMsg) {
	case MODM_DATA:
		return ParseData(uMsg, dwParam1, dwParam2);
	case MODM_LONGDATA:
		return ParseLongData(uMsg, dwParam1, dwParam2);
	case MODM_OPEN:
		// The driver doesn't support stream mode
		if ((DWORD)dwParam2 & MIDI_IO_COOKED) return MMSYSERR_NOTENABLED;
	
		InitializeBASS();

		// Parse callback and instance
		OMCallback = ((MIDIOPENDESC*)dwParam1)->dwCallback;
		OMInstance = ((MIDIOPENDESC*)dwParam1)->dwInstance;
		OMFlags = HIWORD((DWORD)dwParam2);
		DriverCallback(OMCallback, OMFlags, OMDevice, MOM_OPEN, OMInstance, 0, 0);
		return MMSYSERR_NOERROR;
	case MODM_CLOSE:
		return MMSYSERR_NOERROR;
	case MODM_PREPARE:
		// Pass it to a KDMAPI function
		return PrepareLongData((MIDIHDR*)dwParam1);
	case MODM_UNPREPARE:
		// Pass it to a KDMAPI function
		return UnprepareLongData((MIDIHDR*)dwParam1);
	case MODM_RESET:
		BASS_MIDI_StreamEvent(OMStream, 0, MIDI_EVENT_SYSTEM, MIDI_SYSTEM_DEFAULT);
		BASS_MIDI_StreamEvent(OMStream, 0, MIDI_EVENT_SYSTEMEX, MIDI_SYSTEM_DEFAULT);
		return MMSYSERR_NOERROR;
	case MODM_STOP:
		DriverCallback(OMCallback, OMFlags, OMDevice, MOM_DONE, OMInstance, 0, 0);
		return MMSYSERR_NOERROR;
	case MODM_GETNUMDEVS:
		return 0x1;
	case MODM_GETDEVCAPS:
		return modGetCaps((PVOID)dwParam1, (DWORD)dwParam2);
	case DRV_QUERYDEVICEINTERFACESIZE:
		// Maximum longmsg size, 64kB
		*(LONG*)dwParam1 = 65535;
		return MMSYSERR_NOERROR;
	case DRV_QUERYDEVICEINTERFACE:
		// The app is asking for the driver's name, let's give it to them
		memcpy((VOID*)dwParam1, SynthNameW, sizeof(SynthNameW));
		*(LONG*)dwParam2 = 65535;
		return MMSYSERR_NOERROR;
	default:
		return MMSYSERR_NOERROR;
	}
}