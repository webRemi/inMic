#include <Windows.h>
#include <stdio.h>
#include <initguid.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>
#include <endpointvolume.h>
#include "info.h"

int main() {
	LOG("Starting program...");

	HRESULT hr;
	const CLSID CLSID_MMDeviceEnumerator	=	{ 0xBCDE0395, 0xE52F, 0x467C, { 0x8e, 0x3d, 0xC4, 0x57, 0x92, 0x91, 0x69, 0x2E } };
	const IID IID_IMMDeviceEnumerator		=	{ 0xa95664d2, 0x9614, 0x4f35, { 0xa7, 0x46, 0xde, 0x8d, 0xb6, 0x36, 0x17, 0xe6 } };
	const IID IID_IAudioEndpointVolume =		{ 0x5cdf2c82, 0x841e, 0x4546, { 0x97, 0x22, 0x0c, 0xf7, 0x40, 0x78, 0x22, 0x9a } };

	IMMDeviceEnumerator* pDeviceEnumerator = NULL;
	IMMDevice* pAudioDevice = NULL;
	IPropertyStore* pPropertyStore = NULL;
	PROPVARIANT pAudioDeviceName;
	PropVariantInit(&pAudioDeviceName);

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

	hr = pDeviceEnumerator->lpVtbl->GetDefaultAudioEndpoint(pDeviceEnumerator, eRender, eConsole, &pAudioDevice);
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

	/* <<<<<<<<<<<<<<<<<<<<[ START - SEARCHING AUDIO DEVICE VOLUME STATE ]>>>>>>>>>>>>>>>>>>>> */
	LOG("Searching for volume state...");

	IAudioEndpointVolume* pAudioEndpointVolume = NULL;
	hr = pAudioDevice->lpVtbl->Activate(pAudioDevice, &IID_IAudioEndpointVolume, CLSCTX_ALL, NULL, (void**)&pAudioEndpointVolume);
	if (hr != S_OK) {
		NOT("Activate failed with error: 0x%lx", hr);
		return 1;
	}
	wprintf(L"[+] Successfully activate audio device %s\n", pAudioDeviceName.pwszVal);

	BOOL bIsMute;
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





	/* <<<<<<<<<<<<<<<<<<<<[ END - SEARCHING AUDIO DEVICE VOLUME STATE ]>>>>>>>>>>>>>>>>>>>> */

	/* <<<<<<<<<<<<<<<<<<<<[ START - CLEANING ]>>>>>>>>>>>>>>>>>>>> */
	PropVariantClear(&pAudioDeviceName);
	pPropertyStore->lpVtbl->Release(pPropertyStore);
	pAudioDevice->lpVtbl->Release(pAudioDevice);
	pDeviceEnumerator->lpVtbl->Release(pDeviceEnumerator);
	CoUninitialize();
	/* <<<<<<<<<<<<<<<<<<<<[ END - CLEANING ]>>>>>>>>>>>>>>>>>>>> */

	return 0;
}