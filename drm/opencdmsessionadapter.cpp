#include "config.h"
#include "opencdmsessionadapter.h"

#include <gst/gst.h>
#include <assert.h>
#include <iostream>
#include <sstream>
#include <string>
#include <string.h>
#include <vector>
#include <sys/utsname.h>
#include <unistd.h>
#include <sys/syscall.h>
#include "priv_aamp.h"

#include <sys/time.h>
#include <gst/gstbuffer.h>

#define USEC_PER_SEC   1000000
static inline uint64_t GetCurrentTimeStampInUSec()
{
	   struct timeval  timeStamp;
	   uint64_t        retVal = 0;

	   gettimeofday(&timeStamp, NULL);

	   // Convert timestamp to Micro Seconds
	   retVal = (uint64_t)(((uint64_t)timeStamp.tv_sec * USEC_PER_SEC) + timeStamp.tv_usec);

	   return retVal;
}
static inline uint64_t GetCurrentTimeStampInMSec()
{
	   return GetCurrentTimeStampInUSec() / 1000;
}

#define LOG_DECRYPT_STATS 1
#define DECRYPT_AVG_TIME_THRESHOLD 5.0 //5 milliseconds
#ifdef LOG_DECRYPT_STATS
#define MAX_THREADS 10
#define INTERVAL 120

/**
 * @struct DecryptStats
 * @brief Holds decryption profile stats
 */
struct DecryptStats
{
    uint64_t    nBytesInterval;
    uint64_t    nTimeInterval;
    uint64_t    nBytesTotal;
    uint64_t    nTimeTotal;
    uint64_t    nCallsTotal;
    pthread_t   threadID;

};
#endif // LOG_DECRYPT_STATS
#define SEC_SIZE size_t
void LogPerformanceExt(const char* strFunc, uint64_t msStart, uint64_t msEnd, SEC_SIZE nDataSize)
{
    bool        bThreshold  = false;
    uint64_t    delta       = msEnd - msStart;
    uint32_t    nRateMin    = 1000;     // Bytes/ms
    uint32_t    nRestart    = 5;
    uint32_t    nDataMin    = 1000;
    uint32_t    nTimeMin    = 5; // Can not be < 1 to protect against divide by 0 error

#ifdef LOG_DECRYPT_STATS
    {
        static DecryptStats stats[MAX_THREADS] = { 0 };
        int idx = 0;
        while(idx < MAX_THREADS) {
            if(stats[idx].threadID == pthread_self()) {
                break;
            }
            idx++;
        }
        if(idx == MAX_THREADS) {
            // new thread
            idx = 0;
            while(idx < MAX_THREADS) {
                if(stats[idx].threadID == 0) {
                    // empty slot
                    stats[idx].threadID = pthread_self();
                    break;
                }
                idx++;
            }
        }
        if(idx == MAX_THREADS) {
            printf("%s >>>>>>>> All slots allocated!!!, idx = %d, clearing the array.\n", __FUNCTION__, idx);
            memset(stats, 0, sizeof(DecryptStats) * MAX_THREADS);
            return;
        }

        if(nDataSize > 0 ) {
            stats[idx].nBytesInterval     += (uint64_t)nDataSize;
            stats[idx].nTimeInterval      += delta;
            stats[idx].nCallsTotal++;

            if(stats[idx].nCallsTotal % INTERVAL == 0) {
               stats[idx].nBytesTotal += stats[idx].nBytesInterval;
               stats[idx].nTimeTotal += stats[idx].nTimeInterval;
               double avgTime = (double)stats[idx].nTimeTotal/(double)stats[idx].nCallsTotal;
               if(avgTime >= DECRYPT_AVG_TIME_THRESHOLD) {
                  logprintf("%s >>>>>>>> Thread ID %X (%d) Avg Time %0.2llf ms, Avg Bytes %llu  calls (%llu) Interval avg time %0.2llf, Interval avg bytes %llu",
                     strFunc, stats[idx].threadID, idx, avgTime, stats[idx].nBytesTotal/stats[idx].nCallsTotal,
                     stats[idx].nCallsTotal, (double)stats[idx].nTimeInterval/(double)INTERVAL,
                     stats[idx].nBytesInterval/INTERVAL);
               }
               stats[idx].nBytesInterval = 0;
               stats[idx].nTimeInterval = 0;

            }
        }
    }
#endif //LOG_DECRYPT_STATS
}

AAMPOCDMSession::AAMPOCDMSession(string& keySystem) :
		AampDrmSession(keySystem),
		m_eKeyState(KEY_INIT),
		m_pOpenCDMSystem(NULL),
		m_pOpenCDMSession(NULL),
		m_pOutputProtection(NULL),
		decryptMutex(),
		m_sessionID(),
		m_challenge(),
		m_challengeReady(),
		m_challengeSize(0),
		m_keyStatus(InternalError),
		m_keyStatusReady(),
		m_OCDMSessionCallbacks(),
		m_destUrl(),
		m_keyLength(0),
		m_keyId(NULL)
{
	logprintf("AAMPOCDMSession :: enter ");
	pthread_mutex_init(&decryptMutex, NULL);

	initAampDRMSystem();

	// Get output protection pointer
	m_pOutputProtection = AampOutputProtection::GetAampOutputProcectionInstance();
	logprintf("AAMPOCDMSession :: exit ");
}

void AAMPOCDMSession::initAampDRMSystem()
{
	logprintf("initAampDRMSystem :: enter ");
	pthread_mutex_lock(&decryptMutex);
	if (m_pOpenCDMSystem == nullptr) {
#ifdef USE_THUNDER_OCDM_API_0_2
		m_pOpenCDMSystem = opencdm_create_system(m_keySystem.c_str());
#else
		m_pOpenCDMSystem = opencdm_create_system();
#endif
	}
	pthread_mutex_unlock(&decryptMutex);
	logprintf("initAampDRMSystem :: exit ");
}

AAMPOCDMSession::~AAMPOCDMSession()
{
	logprintf("[HHH]OCDMSession destructor called! keySystem %s", m_keySystem.c_str());
	clearDecryptContext();

	pthread_mutex_destroy(&decryptMutex);

	if (m_pOpenCDMSystem) {
		opencdm_destruct_system(m_pOpenCDMSystem);
		m_pOpenCDMSystem = NULL;
	}

	if(m_pOutputProtection) {
			m_pOutputProtection->Release();
	}

	if (!m_keyId) {
		free(m_keyId);
		m_keyId = nullptr;
	}

}

void AAMPOCDMSession::generateAampDRMSession(const uint8_t *f_pbInitData,
		uint32_t f_cbInitData)
{
	logprintf("generateAampDRMSession :: enter ");

	pthread_mutex_lock(&decryptMutex);

	memset(&m_OCDMSessionCallbacks, 0, sizeof(m_OCDMSessionCallbacks));
	m_OCDMSessionCallbacks.process_challenge_callback = [](OpenCDMSession* session, void* userData, const char destUrl[], const uint8_t challenge[], const uint16_t challengeSize) {

		AAMPOCDMSession* userSession = reinterpret_cast<AAMPOCDMSession*>(userData);
		userSession->processOCDMChallenge(destUrl, challenge, challengeSize);
	};

	m_OCDMSessionCallbacks.key_update_callback = [](OpenCDMSession* session, void* userData, const uint8_t key[], const uint8_t keySize) {
		AAMPOCDMSession* userSession = reinterpret_cast<AAMPOCDMSession*>(userData);
		userSession->keyUpdateOCDM(key, keySize);
	};

	m_OCDMSessionCallbacks.error_message_callback = [](OpenCDMSession* session, void* userData, const char message[]) {
	};

	m_OCDMSessionCallbacks.keys_updated_callback = [](const OpenCDMSession* session, void* userData) {
		AAMPOCDMSession* userSession = reinterpret_cast<AAMPOCDMSession*>(userData);
		userSession->keysUpdatedOCDM();
	};

#ifdef USE_THUNDER_OCDM_API_0_2
	opencdm_construct_session(m_pOpenCDMSystem, LicenseType::Temporary, "video/mp4",
#else
	opencdm_construct_session(m_pOpenCDMSystem, m_keySystem.c_str(), LicenseType::Temporary, "video/mp4",
#endif

				  const_cast<unsigned char*>(f_pbInitData), f_cbInitData,
				  nullptr, 0, //No Custom Data
				  &m_OCDMSessionCallbacks,
				  static_cast<void*>(this),
				  &m_pOpenCDMSession);
	if (!m_pOpenCDMSession) {
		logprintf("Could not create session");
	}

	pthread_mutex_unlock(&decryptMutex);
}

void AAMPOCDMSession::processOCDMChallenge(const char destUrl[], const uint8_t challenge[], const uint16_t challengeSize) {

	m_challenge.assign(reinterpret_cast<const char *>(challenge), challengeSize);
	logprintf("processOCDMChallenge challenge = %s", m_challenge.c_str());

	m_destUrl.assign(destUrl);
	logprintf("processOCDMChallenge destUrl = %s", m_destUrl.c_str());

	m_challengeReady.signal();
}

void AAMPOCDMSession::keyUpdateOCDM(const uint8_t key[], const uint8_t keySize) {
	if (m_pOpenCDMSession) {
		m_keyStatus = opencdm_session_status(m_pOpenCDMSession, key, keySize);
	}
  
}

void AAMPOCDMSession::keysUpdatedOCDM() {
	m_keyStatusReady.signal();
}

DrmData * AAMPOCDMSession::aampGenerateKeyRequest(string& destinationURL)
{
	DrmData * result = NULL;

	m_eKeyState = KEY_ERROR;
	if (m_challengeReady.wait(2000) == true) {
		if (m_challenge.empty() != true) {
			std::string delimiter (":Type:");
			std::string requestType (m_challenge.substr(0, m_challenge.find(delimiter)));
			if ( (requestType.size() != 0) && (requestType.size() !=  m_challenge.size()) ) {
				m_challenge.erase(0, m_challenge.find(delimiter) + delimiter.length());
			}

			result = new DrmData(reinterpret_cast<unsigned char*>(const_cast<char*>(m_challenge.c_str())), m_challenge.length());
			destinationURL.assign((m_destUrl.c_str()));
			logprintf("destination url is %s", destinationURL.c_str());
			m_eKeyState = KEY_PENDING;
		}
	}
	return result;
}


int AAMPOCDMSession::aampDRMProcessKey(DrmData* key)
{
	int retValue = -1;

        OpenCDMError status = opencdm_session_update(m_pOpenCDMSession, key->getData(), key->getDataLength());
	if (status == OpenCDMError::ERROR_NONE) {
		if (m_keyStatusReady.wait(2000) == true) {
			logprintf("Key Status updated");
		}
#ifdef USE_THUNDER_OCDM_API_0_2
		if (m_keyStatus == Usable) {
#else
		if (m_keyStatus == KeyStatus::Usable) {
#endif
			logprintf("processKey: Key Usable!");
			m_eKeyState = KEY_READY;
			retValue = 0;
		}
#ifdef USE_THUNDER_OCDM_API_0_2
		else if(m_keyStatus == HWError)
#else
		else if(m_keyStatus == KeyStatus::HWError)
#endif
		{
			// BCOM-3537 - SAGE Hang .. Need to restart the wpecdmi process and then self kill player to recover
			AAMPLOG_WARN("processKey: Update() returned HWError.Restarting process...");
			int systemResult = -1;
			// In Release another process handles opencdm which needs to be restarts .In Sprint this process is not available.
			// So check if process exists before killing it .
			systemResult = system("pgrep WPEcdmi");
			if(systemResult == 0)
			{
				systemResult = system("pkill -9 WPEcdmi");
				if(systemResult != 0)
				{
					AAMPLOG_WARN("Unable to shutdown WPEcdmi process.%d", systemResult);
				}
			}
			else
			{
				// check for WPEFramework process
				systemResult = system("pgrep WPEFramework");
				if(systemResult == 0)
				{
					systemResult = system("pkill -9 WPEFramework");
					if(systemResult != 0)
					{
						AAMPLOG_WARN("Unable to shutdown WPEFramework process.%d", systemResult);
					}
				}
			}

			// wait for 5sec for all the logs to be flushed
			sleep(5);
			// Now kill self
			pid_t pid = getpid();
			syscall(__NR_tgkill, pid, pid, SIGKILL);
		}
		else {
#ifdef USE_THUNDER_OCDM_API_0_2
			if(m_keyStatus == OutputRestricted)
#else
			if(m_keyStatus == KeyStatus::OutputRestricted)
#endif
			{
				AAMPLOG_WARN("processKey: Update() Output restricted keystatus: %d", (int) m_keyStatus);
				retValue = HDCP_OUTPUT_PROTECTION_FAILURE;
			}
#ifdef USE_THUNDER_OCDM_API_0_2
			else if(m_keyStatus == OutputRestrictedHDCP22)
#else
			else if(m_keyStatus == KeyStatus::OutputRestrictedHDCP22)
#endif
			{
				AAMPLOG_WARN("processKey: Update() Output Compliance error keystatus: %d\n", (int) m_keyStatus);
				retValue = HDCP_COMPLIANCE_CHECK_FAILURE;
			}
			else
			{
				AAMPLOG_WARN("processKey: Update() returned keystatus: %d\n", (int) m_keyStatus);
				retValue = (int) m_keyStatus;
			}
			m_eKeyState = KEY_ERROR;
		}
	} else {
	}
	return retValue;
}

int AAMPOCDMSession::decrypt(GstBuffer* keyIDBuffer, GstBuffer* ivBuffer, GstBuffer* buffer, unsigned subSampleCount, GstBuffer* subSamplesBuffer)
{
	int retValue = -1;

	if (m_pOpenCDMSession) {
		uint64_t start_decrypt_time;
		uint64_t end_decrypt_time;

		// Verify output protection parameters
        	// -----------------------------------
        	// Widevine output protection is currently supported without any external configuration.
        	// But the Playready output protection will be enabled based on 'enablePROutputProtection' flag which can be configured through RFC/aamp.cfg..
        	if((m_keySystem == PLAYREADY_KEY_SYSTEM_STRING && gpGlobalConfig->enablePROutputProtection) && m_pOutputProtection->IsSourceUHD()) {
                	// Source material is UHD
                	if(!m_pOutputProtection->isHDCPConnection2_2()) {
                        	// UHD and not HDCP 2.2
                        	AAMPLOG_WARN("%s : UHD source but not HDCP 2.2. FAILING decrypt\n", __FUNCTION__);
                        	return HDCP_COMPLIANCE_CHECK_FAILURE;
                	}
        	}

		pthread_mutex_lock(&decryptMutex);
		start_decrypt_time = GetCurrentTimeStampInMSec();
		retValue = opencdm_gstreamer_session_decrypt(m_pOpenCDMSession, buffer, subSamplesBuffer, subSampleCount, ivBuffer, keyIDBuffer, 0);
		end_decrypt_time = GetCurrentTimeStampInMSec();
		if(retValue != 0)
		{
			GstMapInfo keyIDMap;
			if (gst_buffer_map(keyIDBuffer, &keyIDMap, (GstMapFlags) GST_MAP_READ) == true) 
			{
        			uint8_t *mappedKeyID = reinterpret_cast<uint8_t* >(keyIDMap.data);
        			uint32_t mappedKeyIDSize = static_cast<uint32_t >(keyIDMap.size);
#ifdef USE_THUNDER_OCDM_API_0_2
				KeyStatus keyStatus = opencdm_session_status(m_pOpenCDMSession, mappedKeyID,mappedKeyIDSize );
#else
				media::OpenCdm::KeyStatus keyStatus = opencdm_session_status(m_pOpenCDMSession, mappedKeyID,mappedKeyIDSize );
#endif
				AAMPLOG_INFO("AAMPOCDMSession:%s : decrypt returned : %d key status is : %d", __FUNCTION__, retValue,keyStatus);
#ifdef USE_THUNDER_OCDM_API_0_2
				if(keyStatus == OutputRestricted){
#else
				if(keyStatus == media::OpenCdm::KeyStatus::OutputRestricted){
#endif
					retValue =  HDCP_OUTPUT_PROTECTION_FAILURE;
				}
#ifdef USE_THUNDER_OCDM_API_0_2
				else if(keyStatus == OutputRestrictedHDCP22){
#else
				else if(keyStatus == media::OpenCdm::KeyStatus::OutputRestrictedHDCP22){
#endif
					retValue =  HDCP_COMPLIANCE_CHECK_FAILURE;
				}
				gst_buffer_unmap(keyIDBuffer, &keyIDMap);
			}
		}
		
		GstMapInfo mapInfo;
        if (gst_buffer_map(buffer, &mapInfo, GST_MAP_READ)) {
			if (mapInfo.size > 0) {
				LogPerformanceExt(__FUNCTION__, start_decrypt_time, end_decrypt_time, mapInfo.size);
			}
			gst_buffer_unmap(buffer, &mapInfo);
		}

		pthread_mutex_unlock(&decryptMutex);
	}
	return retValue;
}

KeyState AAMPOCDMSession::getState()
{
	return m_eKeyState;
}

void AAMPOCDMSession:: clearDecryptContext()
{
	logprintf("[HHH] clearDecryptContext.");

	pthread_mutex_lock(&decryptMutex);

	if (m_pOpenCDMSession) {
		opencdm_session_close(m_pOpenCDMSession);
		opencdm_destruct_session(m_pOpenCDMSession);
		m_pOpenCDMSession = NULL;
	}

	pthread_mutex_unlock(&decryptMutex);
	m_eKeyState = KEY_INIT;
}

void AAMPOCDMSession::setKeyId(const char* keyId, int32_t keyLength)
{
	if (!m_keyId) {
		free(m_keyId);
		m_keyId = (uint8_t *)malloc(keyLength);
	}
	memcpy(m_keyId, keyId, keyLength);
	m_keyLength = keyLength;
}
