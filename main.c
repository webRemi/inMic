#include <Windows.h>
#include <stdio.h>
#include <initguid.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>
#include <endpointvolume.h>
#include <Audioclient.h>
#include <ks.h>
#include <ksmedia.h>
#include "info.h"

DEFINE_GUID(KSDATAFORMAT_SUBTYPE_PCM,
	0x00000001, 0x0000, 0x0010,
	0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71);

BYTE* pBuffer = NULL;
DWORD flags;

void WriteToFile(BYTE* pBuffer, UINT32 numFramesAvailable, FILE* audioFile, WAVEFORMATEX* pDeviceFormat) {
	if (numFramesAvailable > 0) {
		size_t bytesToWrite = numFramesAvailable * pDeviceFormat->nBlockAlign;
		fwrite(pBuffer, 1, bytesToWrite, audioFile);
		fflush(audioFile);
	}
}

void LowPassFilter(BYTE* pBuffer, UINT32 numFramesAvailable, WAVEFORMATEX* pDeviceFormat) {
	static short lastSample = 0;
	short* samples = (short*)pBuffer;
	int numSamples = numFramesAvailable * pDeviceFormat->nChannels;

	for (int i = 0; i < numSamples; i++) {
		samples[i] = (samples[i] + lastSample) / 2;
		lastSample = samples[i];
	}
}

void CopyData(BYTE* pData, UINT32 numFramesAvailable, BOOL* bDone, FILE* audioFile, WAVEFORMATEX* pDeviceFormat) {
	if (numFramesAvailable == 0) return;

	if (pData == NULL || (flags & AUDCLNT_BUFFERFLAGS_SILENT)) {
		memset(pBuffer, 0, numFramesAvailable * pDeviceFormat->nBlockAlign);
	}
	else {
		memcpy(pBuffer, pData, numFramesAvailable * pDeviceFormat->nBlockAlign);
	}

	LowPassFilter(pBuffer, numFramesAvailable, pDeviceFormat);

	WriteToFile(pBuffer, numFramesAvailable, audioFile, pDeviceFormat);

	static int frameCount = 0;
	frameCount += numFramesAvailable;
	if (frameCount >= pDeviceFormat->nSamplesPerSec * 5) {
		*bDone = TRUE;
		LOG("Recording finished after 5 seconds.");
	}
}

int main() {
	LOG("Starting program...");
	FILE* audioFile = fopen("output.raw", "wb");
	if (!audioFile) {
		NOT("Failed to open file for writing.");
		return 1;
	}

	HRESULT hr;
	const CLSID CLSID_MMDeviceEnumerator = { 0xBCDE0395, 0xE52F, 0x467C, { 0x8e, 0x3d, 0xC4, 0x57, 0x92, 0x91, 0x69, 0x2E } };
	const IID IID_IMMDeviceEnumerator = { 0xa95664d2, 0x9614, 0x4f35, { 0xa7, 0x46, 0xde, 0x8d, 0xb6, 0x36, 0x17, 0xe6 } };
	const IID IID_IAudioEndpointVolume = { 0x5cdf2c82, 0x841e, 0x4546, { 0x97, 0x22, 0x0c, 0xf7, 0x40, 0x78, 0x22, 0x9a } };
	const IID IID_IAudioClient = { 0X1CB9AD4C, 0XDBFA, 0X4C32, { 0XB1, 0X78, 0XC2, 0XF5, 0X68, 0XA7, 0X03, 0XB2 } };
	const IID IID_IAudioCaptureClient = { 0xc8adbd64, 0xe71e, 0x48a0, { 0xa4, 0xde, 0x18, 0x5c, 0x39, 0x5c, 0xd3, 0x17 } };

	IMMDeviceEnumerator* pDeviceEnumerator = NULL;
	IMMDevice* pAudioDevice = NULL;
	IPropertyStore* pPropertyStore = NULL;
	PROPVARIANT pAudioDeviceName;
	PropVariantInit(&pAudioDeviceName);
	BYTE* pData = NULL;
	UINT32 numFramesAvailable = 0;
	UINT32 packetLength = 0;
	BOOL bDone = FALSE;
	BOOL bIsMute;
	IAudioEndpointVolume* pAudioEndpointVolume = NULL;

	/* <<<<<<<<<<<<<<<<<<<<[ START - SEARCHING AUDIO DEVICE NAME ]>>>>>>>>>>>>>>>>>>>> */
	hr = CoInitialize(NULL);
	if (hr != S_OK) {
		NOT("CoInitialize failed with error: 0x%lx", hr);
		return 1;
	}
	YES("Successfully initialized COM object");

	hr = CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, &IID_IMMDeviceEnumerator, (void**)&pDeviceEnumerator);
	if (hr != S_OK) {
		NOT("CoCreateInstance failed with error: 0x%lx", hr);
		return 1;
	}
	YES("Successfully created instance of COM object");

	hr = pDeviceEnumerator->lpVtbl->GetDefaultAudioEndpoint(pDeviceEnumerator, eCapture, eConsole, &pAudioDevice);
	if (hr != S_OK) {
		NOT("GetDefaultAudioEndpoint failed with error: 0x%lx", hr);
		return 1;
	}
	YES("Successfully got default audio device");

	hr = pAudioDevice->lpVtbl->OpenPropertyStore(pAudioDevice, STGM_READ, &pPropertyStore);
	if (hr != S_OK) {
		NOT("OpenPropertyStore failed with error: 0x%lx", hr);
		return 1;
	}
	YES("Successfully opened device store");

	hr = pPropertyStore->lpVtbl->GetValue(pPropertyStore, &PKEY_Device_FriendlyName, &pAudioDeviceName);
	if (hr != S_OK) {
		NOT("GetValue failed with error: 0x%lx", hr);
		return 1;
	}
	YES("Successfully retrieved name of the audio device");

	wprintf(L"[+] Audio device name: %s\n", pAudioDeviceName.pwszVal);
	/* <<<<<<<<<<<<<<<<<<<<[ END - SEARCHING AUDIO DEVICE NAME ]>>>>>>>>>>>>>>>>>>>> */

	/* <<<<<<<<<<<<<<<<<<<<[ START - ACTIVATE AUDIO 100% VOLUME UNMUTE ]>>>>>>>>>>>>>>>>>>>> */
	LOG("Searching for volume state...");

	hr = pAudioDevice->lpVtbl->Activate(pAudioDevice, &IID_IAudioEndpointVolume, CLSCTX_ALL, NULL, (void**)&pAudioEndpointVolume);
	if (hr != S_OK) {
		NOT("Activate failed with error: 0x%lx", hr);
		return 1;
	}
	wprintf(L"[+] Successfully activate audio device %s\n", pAudioDeviceName.pwszVal);
	
	hr = pAudioEndpointVolume->lpVtbl->GetMute(pAudioEndpointVolume, &bIsMute);
	if (hr != S_OK) {
		NOT("GetMute failed with error: 0x%lx", hr);
		return 1;
	}
	if (bIsMute) {
		LOG("Audio device is mute");
	}
	else {
		LOG("Audio device is ready");
	}

	if (bIsMute) {
		BOOL bMute = FALSE;
		hr = pAudioEndpointVolume->lpVtbl->SetMute(pAudioEndpointVolume, bMute, NULL);
		if (hr != S_OK) {
			NOT("SetMute failed with error: 0x%lx", hr);
			return 1;
		}
		YES("Successfully unmute device");
	}

	FLOAT fLevelDB = 1.0;
	hr = pAudioEndpointVolume->lpVtbl->SetMasterVolumeLevelScalar(pAudioEndpointVolume, fLevelDB, NULL);
	if (hr != S_OK) {
		NOT("SetMasterVolumeLevel failed with error: 0x%lx", hr);
		return 1;
	}
	YES("Successfully set device volume at 100%%");
	/* <<<<<<<<<<<<<<<<<<<<[ END - ACTIVATE AUDIO 100% VOLUME UNMUTE ]>>>>>>>>>>>>>>>>>>>> */

	IAudioClient* pAudioClient = NULL;
	hr = pAudioDevice->lpVtbl->Activate(pAudioDevice, &IID_IAudioClient, CLSCTX_ALL, NULL, (void**)&pAudioClient);
	if (hr != S_OK) {
		NOT("Activate failed with error: 0x%lx", hr);
		return 1;
	}
	YES("AudioClient activated");

	WAVEFORMATEX* pDeviceFormat = NULL;
	hr = pAudioClient->lpVtbl->GetMixFormat(pAudioClient, &pDeviceFormat);
	if (hr != S_OK) {
		NOT("GetMixFormat failed with error: 0x%lx", hr);
		return 1;
	}
	YES("Got mixed format");

	REFERENCE_TIME hnsRequestedDuration = (REFERENCE_TIME)((double)100 * 10 * 1000 * 1000);
	REFERENCE_TIME hnsActualDuration = 0;
	hr = pAudioClient->lpVtbl->Initialize(pAudioClient, AUDCLNT_SHAREMODE_SHARED, 0, hnsRequestedDuration, hnsActualDuration, pDeviceFormat, NULL);
	if (hr != S_OK) {
		NOT("Initialize failed with error: 0x%lx", hr);
		return 1;
	}
	YES("Initialized audio stream");

	IAudioCaptureClient* pCaptureClient = NULL;
	hr = pAudioClient->lpVtbl->GetService(pAudioClient, &IID_IAudioCaptureClient, (void**)&pCaptureClient);
	if (hr != S_OK) {
		NOT("GetService failed with error: 0x%lx", hr);
		return 1;
	}
	YES("Got microphone service");

	/* <<<<<<<<<<<<<<<<<<<<[ START - SETUP AUDIO FORMAT ]>>>>>>>>>>>>>>>>>>>> */
	WAVEFORMATEXTENSIBLE waveFormatEx;
	ZeroMemory(&waveFormatEx, sizeof(WAVEFORMATEXTENSIBLE));

	waveFormatEx.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;  
	waveFormatEx.Format.nChannels = 2;  
	waveFormatEx.Format.nSamplesPerSec = 44100; 
	waveFormatEx.Format.wBitsPerSample = 16;  
	waveFormatEx.Format.nBlockAlign = (waveFormatEx.Format.nChannels * waveFormatEx.Format.wBitsPerSample) / 8;
	waveFormatEx.Format.nAvgBytesPerSec = waveFormatEx.Format.nSamplesPerSec * waveFormatEx.Format.nBlockAlign;
	waveFormatEx.Format.cbSize = 22; 

	waveFormatEx.Samples.wValidBitsPerSample = 16;
	waveFormatEx.dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
	waveFormatEx.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
	/* <<<<<<<<<<<<<<<<<<<<[ END - SETUP AUDIO FORMAT ]>>>>>>>>>>>>>>>>>>>> */

	UINT32 bufferFrameCount = 0;
	UINT32 framesAvailable = 0;
	hr = pAudioClient->lpVtbl->GetBufferSize(pAudioClient, &bufferFrameCount);
	if (hr != S_OK) {
		NOT("GetBufferSize failed with error: 0x%lx", hr);
		return 1;
	}
	YES("Get buffer size");
	hnsActualDuration = (double)10 * bufferFrameCount / pDeviceFormat->nSamplesPerSec;

	hr = pAudioClient->lpVtbl->Start(pAudioClient);
	if (hr != S_OK) {
		NOT("Start failed with error: 0x%lx", hr);
		return 1;
	}
	YES("Starting playing...");

	
	pBuffer = (BYTE*)malloc(bufferFrameCount * pDeviceFormat->nBlockAlign);
	if (!pBuffer) {
		NOT("Memory allocation for buffer failed");
		return 1;
	}

	while (bDone == FALSE) {
		Sleep(hnsActualDuration / 10 / 2);

		hr = pCaptureClient->lpVtbl->GetNextPacketSize(pCaptureClient, &packetLength);
		if (hr != S_OK) {
			NOT("GetNextPacketSize failed with error: 0x%lx", hr);
			return 1;
		}
		YES("Got next packet size");
		while (packetLength != 0) {

			hr = pCaptureClient->lpVtbl->GetBuffer(pCaptureClient, &pData, &numFramesAvailable, &flags, NULL, NULL);
			if (hr != S_OK) {
				NOT("GetBuffer failed with error: 0x%lx", hr);
				return 1;
			}
			YES("Get buffer...");

			if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
				pData = NULL;
			}

			CopyData(pData, numFramesAvailable, &bDone, audioFile, pDeviceFormat);

			hr = pCaptureClient->lpVtbl->ReleaseBuffer(pCaptureClient, numFramesAvailable);
			if (hr != S_OK) {
				NOT("ReleaseBuffer failed with error: 0x%lx", hr);
				return 1;
			}
			YES("Releasing buffer...");


			hr = pCaptureClient->lpVtbl->GetNextPacketSize(pCaptureClient, &packetLength);
			if (hr != S_OK) {
				NOT("GetNextPacketSize failed with error: 0x%lx", hr);
				return 1;
			}
			YES("Get next packet size");
		}
	}

	hr = pAudioClient->lpVtbl->Stop(pAudioClient);
	if (hr != S_OK) {
		NOT("Stop failed with error: 0x%lx", hr);
		return 1;
	}
	YES("Stop recording...");

	/* <<<<<<<<<<<<<<<<<<<<[ START - CLEANING ]>>>>>>>>>>>>>>>>>>>> */
	free(pBuffer);
	PropVariantClear(&pAudioDeviceName);
	pPropertyStore->lpVtbl->Release(pPropertyStore);
	pAudioDevice->lpVtbl->Release(pAudioDevice);
	pDeviceEnumerator->lpVtbl->Release(pDeviceEnumerator);
	CoUninitialize();
	/* <<<<<<<<<<<<<<<<<<<<[ END - CLEANING ]>>>>>>>>>>>>>>>>>>>> */

	return 0;
}