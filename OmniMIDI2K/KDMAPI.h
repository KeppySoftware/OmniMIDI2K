// KDMAPI calls

BOOL KDMAPI ReturnKDMAPIVer(LPDWORD Major, LPDWORD Minor, LPDWORD Build, LPDWORD Revision) {
	*Major = 1; *Minor = 30; *Build = 0; *Revision = 51;
	MessageBoxW(NULL, L"You're using OmniMIDI2K!!!", L"OmniMIDI -  WARNING", MB_OK | MB_ICONWARNING | MB_SYSTEMMODAL);
	return TRUE;
}

BOOL KDMAPI IsKDMAPIAvailable() {
	// Dummy
	return TRUE;
}

VOID KDMAPI InitializeKDMAPIStream() {
	InitializeBASS();
}

VOID KDMAPI TerminateKDMAPIStream() {
	TerminateBASS();
}

VOID KDMAPI ResetKDMAPIStream() {
	BASS_MIDI_StreamEvent(OMStream, 0, MIDI_EVENT_SYSTEM, MIDI_SYSTEM_DEFAULT);
	BASS_MIDI_StreamEvent(OMStream, 0, MIDI_EVENT_SYSTEMEX, MIDI_SYSTEM_DEFAULT);
}

MMRESULT KDMAPI SendDirectData(DWORD dwMsg) {
	return ParseData(MODM_DATA, dwMsg, 0);
}

MMRESULT KDMAPI SendDirectDataNoBuf(DWORD dwMsg) {
	// Dummy
	return SendDirectData(dwMsg);
}

MMRESULT KDMAPI SendDirectLongData(MIDIHDR* IIMidiHdr) {
	return ParseLongData(MODM_LONGDATA, (DWORD_PTR)(IIMidiHdr->lpData), (DWORD_PTR)sizeof(IIMidiHdr->lpData));
}

MMRESULT KDMAPI SendDirectLongDataNoBuf(MIDIHDR* IIMidiHdr) {
	return SendDirectLongData(IIMidiHdr);
}

MMRESULT KDMAPI PrepareLongData(MIDIHDR* IIMidiHdr) {
	if (!IIMidiHdr || sizeof(IIMidiHdr->lpData) > LONGMSG_MAXSIZE) return MMSYSERR_INVALPARAM;			// The buffer doesn't exist or is too big, invalid parameter

	void* Mem = IIMidiHdr->lpData;
	unsigned long Size = sizeof(IIMidiHdr->lpData);

	// Lock the MIDIHDR buffer, to prevent the MIDI app from accidentally writing to it
	if (!NtLockVirtualMemory(GetCurrentProcess(), &Mem, &Size, LOCK_VM_IN_WORKING_SET | LOCK_VM_IN_RAM))
		return MMSYSERR_NOMEM;

	// Mark the buffer as prepared, and say that everything is oki-doki
	IIMidiHdr->dwFlags |= MHDR_PREPARED;
	return MMSYSERR_NOERROR;
}

MMRESULT KDMAPI UnprepareLongData(MIDIHDR* IIMidiHdr) {
	// Check if the MIDIHDR buffer is valid
	if (!IIMidiHdr) return MMSYSERR_INVALPARAM;								// The buffer doesn't exist, invalid parameter
	if (!(IIMidiHdr->dwFlags & MHDR_PREPARED)) return MMSYSERR_NOERROR;		// Already unprepared, everything is fine
	if (IIMidiHdr->dwFlags & MHDR_INQUEUE) return MIDIERR_STILLPLAYING;		// The buffer is currently being played from the driver, cannot unprepare

	IIMidiHdr->dwFlags &= ~MHDR_PREPARED;									// Mark the buffer as unprepared

	void* Mem = IIMidiHdr->lpData;
	unsigned long Size = sizeof(IIMidiHdr->lpData);

	// Unlock the buffer, and say that everything is oki-doki
	NtUnlockVirtualMemory(GetCurrentProcess(), &Mem, &Size, LOCK_VM_IN_WORKING_SET | LOCK_VM_IN_RAM);
	RtlSecureZeroMemory(IIMidiHdr->lpData, sizeof(IIMidiHdr->lpData));
	return MMSYSERR_NOERROR;
}

VOID KDMAPI ChangeDriverSettings(const Settings* Struct, DWORD StructSize) {
	MessageBox(NULL, "Not available in OmniMIDI2K.", "ERROR", MB_OK | MB_ICONWARNING | MB_SYSTEMMODAL);
}

VOID KDMAPI LoadCustomSoundFontsList(const wchar_t* Directory) {
	MessageBox(NULL, "Not available in OmniMIDI2K.", "ERROR", MB_OK | MB_ICONWARNING | MB_SYSTEMMODAL);
}

DebugInfo* KDMAPI GetDriverDebugInfo() {
	MessageBox(NULL, "Not available in OmniMIDI2K.", "ERROR", MB_OK | MB_ICONWARNING | MB_SYSTEMMODAL);
	return NULL;
}