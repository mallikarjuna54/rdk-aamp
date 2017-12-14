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
/* Comcast DRM Session management for Aamp
 *
 */

#ifndef AampDRMSessionManager_h
#define AampDRMSessionManager_h

//#define MAX_DRM_SESSIONS 3
#include "AampDrmSession.h"
#include "main_aamp.h"
#include <string>
#include <curl/curl.h>

#ifdef USE_SECCLIENT
#include "sec_client.h"
#endif

#define MAX_DRM_SESSIONS 2
#define VIDEO_SESSION 0
#define AUDIO_SESSION 1
#define KEYID_TAG_START "<KID>"
#define KEYID_TAG_END "</KID>"

typedef struct _drmSessionContext
{
	size_t dataLength;
	unsigned char* data;
	AampDrmSession * drmSession;
}DrmSessionContext;

typedef struct _keyId
{
	size_t len;
	unsigned char* data;
	long long creationTime;
}KeyID;

class AampDRMSessionManager
{

private:

	static DrmSessionContext drmSessionContexts[MAX_DRM_SESSIONS];
	static KeyID cachedKeyIDs[MAX_DRM_SESSIONS];
	static KeyID failedKeyId;
	static long failedErrorCode;
	static size_t write_callback(char *ptr, size_t size, size_t nmemb,
			void *userdata);
	static char* accessToken;
	static int accessTokenLen;
public:

	void initializeDrmSessions();
	AampDRMSessionManager();
	~AampDRMSessionManager();
	AampDrmSession * createDrmSession(const char* systemId,
			const unsigned char * initDataPtr, uint16_t dataLength, MediaType streamType, PrivateInstanceAAMP* aamp, AAMPTuneFailure *error_code = NULL);
	AampDrmSession * createDrmSession(const char* systemId,
			const unsigned char * initDataPtr, uint16_t dataLength, MediaType streamType,
			const unsigned char *contentMetadata, PrivateInstanceAAMP* aamp, AAMPTuneFailure *error_code = NULL);
	DrmData * getLicense(DrmData * keyChallenge, string destinationURL, long *httpError, bool isComcastStream = false);
	static void clearSessionData();
	static void clearAccessToken();
	static const char* getAccessToken(int *tokenLength);
};

unsigned char * _extractDataFromPssh(const char* psshData, int dataLength,
											const char* startStr, const char* endStr, int *len);
unsigned char * _extractKeyIdFromPssh(const char* psshData, int dataLength, int *len, bool isWidevine);
unsigned char * _extractWVContentMetadataFromPssh(const char* psshData, int dataLength, int *len);

#endif
