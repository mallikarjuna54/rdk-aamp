/*
 * If not stated otherwise in this file or this component's license file the
 * following copyright and licenses apply:
 *
 * Copyright 2018 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/
#ifndef AampDrmSession_h
#define AampDrmSession_h
#include "AampDRMutils.h"
#include <string>
#include <stdint.h>

using namespace std;

#define PLAYREADY_PROTECTION_SYSTEM_ID "9a04f079-9840-4286-ab92-e65be0885f95"
#define WIDEVINE_PROTECTION_SYSTEM_ID "edef8ba9-79d6-4ace-a3c8-27dcd51d21ed"

#define PLAYREADY_KEY_SYSTEM_STRING "com.microsoft.playready"
#define WIDEVINE_KEY_SYSTEM_STRING "com.widevine.alpha"

typedef	enum
{
	// Has been initialized.
	KEY_INIT = 0,
	// Has a key message pending to be processed.
	KEY_PENDING = 1,
	// Has a usable key.
	KEY_READY = 2,
	// Has an error.
	KEY_ERROR = 3,
	// Has been closed.
	KEY_CLOSED = 4
} KeyState;

class AampDrmSession
{
public:
	virtual void generateAampDRMSession(const uint8_t *f_pbInitData,uint32_t f_cbInitData) = 0;
	virtual DrmData* aampGenerateKeyRequest(string& destinationURL) = 0;
	virtual int aampDRMProcessKey(DrmData* key) = 0;
	virtual int decrypt(const uint8_t *f_pbIV, uint32_t f_cbIV,const uint8_t *payloadData, uint32_t payloadDataSize, uint8_t **ppOpaqueData) = 0;
	virtual KeyState getState() = 0;
	virtual void clearDecryptContext() = 0;
};
#endif
