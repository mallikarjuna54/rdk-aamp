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
#include "_base64.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>


char *base16_Encode(const unsigned char *src, size_t len)
{
	static const char *mBase16IndexToChar = "0123456789abcdef";
	size_t outLen = len*2;
	char *outData = (char *)malloc(1 + outLen);
	if( outData )
	{
		char *dst = outData;
		const unsigned char *finish = src + len;
		while (src < finish)
		{
			unsigned char c = *src++;
			*dst++ = mBase16IndexToChar[c>>4];
			*dst++ = mBase16IndexToChar[c&0xf];
		}
		*dst = 0x00;
	}
	return outData;
}

unsigned char *base16_Decode( const char *srcPtr, size_t srcLen, size_t *len )
{
	static const signed char mBase16CharToIndex[256] = {
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		 0,  1,  2,  3,  4,  5,  6,  7,  8,  9, -1, -1,	-1, -1, -1, -1,
		-1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, 10, 11, 12,	13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1,	-1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,	-1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1,	-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1,	-1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,	-1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
	};
	size_t numChars = (srcLen/2);
	unsigned char *outData = (unsigned char *)malloc(numChars);
	if (outData)
	{
		unsigned char *dst = outData;
		const char *finish = srcPtr + srcLen;
		while (srcPtr < finish)
		{
			*dst = mBase16CharToIndex[(unsigned char)*srcPtr++] << 4;
			*dst++ |= mBase16CharToIndex[(unsigned char)*srcPtr++];
		}
		*len = numChars;
	}
	else
	{ // insufficient memory
		*len = 0;
	}
	return outData;
}
