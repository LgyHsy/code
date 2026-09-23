/***************************************************************************************
 *
 *  IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING.
 *
 *  By downloading, copying, installing or using the software you agree to this license.
 *  If you do not agree to this license, do not download, install, 
 *  copy or use the software.
 *
 *  Copyright (C) 2014-2024, Happytimesoft Corporation, all rights reserved.
 *
 *  Redistribution and use in binary forms, with or without modification, are permitted.
 *
 *  Unless required by applicable law or agreed to in writing, software distributed 
 *  under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 *  CONDITIONS OF ANY KIND, either express or implied. See the License for the specific
 *  language governing permissions and limitations under the License.
 *
****************************************************************************************/

#include "sys_inc.h"
#include "rfc_md5.h"
#include "sha256.h"
#include "http_auth.h"


/***************************************************************************************/

/* calculate H(A1) as per spec */
void MD5_DigestCalcHA1(
    const char * pszAlg,
    const char * pszUserName,
    const char * pszRealm,
    const char * pszPassword,
    const char * pszNonce,
    const char * pszCNonce,
    char SessionKey[33]
)
{
    uint8 HA1[16];
	md5_context ctx;

	md5_starts(&ctx);
	md5_update(&ctx, (uint8 *)pszUserName, (uint32)strlen(pszUserName));
	md5_update(&ctx, (uint8 *)(":"), 1);
	md5_update(&ctx, (uint8 *)pszRealm, (uint32)strlen(pszRealm));
	md5_update(&ctx, (uint8 *)(":"), 1);
	md5_update(&ctx, (uint8 *)pszPassword, (uint32)strlen(pszPassword));
	md5_finish(&ctx, HA1);

	if (strcasecmp(pszAlg, "md5-sess") == 0) 
	{
		md5_starts(&ctx);
		md5_update(&ctx, HA1, 16);
		md5_update(&ctx, (uint8 *)(":"), 1);
		md5_update(&ctx, (uint8 *)pszNonce, (uint32)strlen(pszNonce));
		md5_update(&ctx, (uint8 *)(":"), 1);
		md5_update(&ctx, (uint8 *)pszCNonce, (uint32)strlen(pszCNonce));
		md5_finish(&ctx, HA1);
	};

	bin_to_hex_str(HA1, 16, SessionKey, 33);
};

/* calculate request-digest/response-digest as per HTTP Digest spec */
void MD5_DigestCalcResponseHash(
    char HA1[33], /* H(A1) */
    const char * pszNonce, /* nonce from server */
    const char * pszNonceCount, /* 8 hex digits */
    const char * pszCNonce, /* client nonce */
    const char * pszQop, /* qop-value: "", "auth", "auth-int" */
    const char * pszMethod, /* method from the request */
    const char * pszDigestUri, /* requested URL */
    char HEntity[33], /* H(entity body) if qop="auth-int" */
    uint8 RespHash[16] /* request-digest or response-digest */
)
{
	uint8 HA2[16];
	char HA2Hex[33];
	md5_context ctx;

	// calculate H(A2)
	md5_starts(&ctx);
	md5_update(&ctx, (uint8 *)pszMethod, (uint32)strlen(pszMethod));
	md5_update(&ctx, (uint8 *)(":"), 1);
	md5_update(&ctx, (uint8 *)pszDigestUri, (uint32)strlen(pszDigestUri));

	if (strcmp(pszQop, "auth-int") == 0) 
	{
		md5_update(&ctx, (uint8 *)(":"), 1);
		md5_update(&ctx, (uint8 *)(&HEntity[0]), 33);
	};
	
	md5_finish(&ctx, HA2);
	
	bin_to_hex_str(HA2, 16, HA2Hex, 33);

	// calculate response
	md5_starts(&ctx);
	md5_update(&ctx, (uint8 *)(&HA1[0]), 32);
	md5_update(&ctx, (uint8 *)(":"), 1);
	md5_update(&ctx, (uint8 *)pszNonce, (uint32)strlen(pszNonce));
	md5_update(&ctx, (uint8 *)(":"), 1);

	if (*pszQop) 
	{
		md5_update(&ctx, (uint8 *)pszNonceCount, (uint32)strlen(pszNonceCount));
		md5_update(&ctx, (uint8 *)(":"), 1);
		md5_update(&ctx, (uint8 *)pszCNonce, (uint32)strlen(pszCNonce));
		md5_update(&ctx, (uint8 *)(":"), 1);
		md5_update(&ctx, (uint8 *)pszQop, (uint32)strlen(pszQop));
		md5_update(&ctx, (uint8 *)(":"), 1);
	};
	
	md5_update(&ctx, (uint8 *)(&HA2Hex[0]), 32);
	md5_finish(&ctx, RespHash);
}

/* calculate request-digest/response-digest as per HTTP Digest spec */
void MD5_DigestCalcResponse(
    char HA1[33], /* H(A1) */
    const char * pszNonce, /* nonce from server */
    const char * pszNonceCount, /* 8 hex digits */
    const char * pszCNonce, /* client nonce */
    const char * pszQop, /* qop-value: "", "auth", "auth-int" */
    const char * pszMethod, /* method from the request */
    const char * pszDigestUri, /* requested URL */
    char HEntity[33], /* H(entity body) if qop="auth-int" */
    char Response[33] /* request-digest or response-digest */
)
{
	uint8 RespHash[16];
	
    MD5_DigestCalcResponseHash(HA1, pszNonce, pszNonceCount, pszCNonce, 
        pszQop, pszMethod, pszDigestUri, HEntity, RespHash);

	bin_to_hex_str(RespHash, 16, Response, 33);
}

/* calculate H(A1) as per spec */
void SHA256_DigestCalcHA1(
    const char * pszAlg,
    const char * pszUserName,
    const char * pszRealm,
    const char * pszPassword,
    const char * pszNonce,
    const char * pszCNonce,
    char SessionKey[65]
)
{
    uint8 HA1[32];
    sha256_context ctx;
    
    sha256_starts(&ctx);
    sha256_update(&ctx, (uint8 *)pszUserName, (uint32)strlen(pszUserName));
    sha256_update(&ctx, (uint8 *)(":"), 1);
	sha256_update(&ctx, (uint8 *)pszRealm, (uint32)strlen(pszRealm));
	sha256_update(&ctx, (uint8 *)(":"), 1);
	sha256_update(&ctx, (uint8 *)pszPassword, (uint32)strlen(pszPassword));
	sha256_finish(&ctx, HA1);

	if (strcasecmp(pszAlg, "sha-256-sess") == 0) 
    {
        sha256_starts(&ctx);
        sha256_update(&ctx, HA1, 32);
        sha256_update(&ctx, (uint8 *)(":"), 1);
        sha256_update(&ctx, (uint8 *)pszNonce, (uint32)strlen(pszNonce));
        sha256_update(&ctx, (uint8 *)(":"), 1);
        sha256_update(&ctx, (uint8 *)pszCNonce, (uint32)strlen(pszCNonce));
        sha256_finish(&ctx, HA1);
    };

	bin_to_hex_str(HA1, 32, SessionKey, 65);
}

/* calculate request-digest/response-digest as per HTTP Digest spec */
void SHA256_DigestCalcResponseHash(
	char HA1[65], /* H(A1) */
	const char * pszNonce, /* nonce from server */
	const char * pszNonceCount, /* 8 hex digits */
	const char * pszCNonce, /* client nonce */
	const char * pszQop, /* qop-value: "", "auth", "auth-int" */
	const char * pszMethod, /* method from the request */
	const char * pszDigestUri, /* requested URL */
	char HEntity[65], /* H(entity body) if qop="auth-int" */
	uint8 RespHash[32] /* request-digest or response-digest */
)
{
	uint8 HA2[32];
	char HA2Hex[65];
	sha256_context ctx;
	
	// calculate H(A2)
	sha256_starts(&ctx);
	sha256_update(&ctx, (uint8 *)pszMethod, (uint32)strlen(pszMethod));
	sha256_update(&ctx, (uint8 *)(":"), 1);
	sha256_update(&ctx, (uint8 *)pszDigestUri, (uint32)strlen(pszDigestUri));

	if (strcmp(pszQop, "auth-int") == 0)
	{
		sha256_update(&ctx, (uint8 *)(":"), 1);
		sha256_update(&ctx, (uint8 *)(&HEntity[0]), 64);
	};

	sha256_finish(&ctx, HA2);
	
	bin_to_hex_str(HA2, 32, HA2Hex, 65);

	// calculate response
	sha256_starts(&ctx);
	sha256_update(&ctx, (uint8 *)(&HA1[0]), 64);
	sha256_update(&ctx, (uint8 *)(":"), 1);
	sha256_update(&ctx, (uint8 *)pszNonce, (uint32)strlen(pszNonce));
	sha256_update(&ctx, (uint8 *)(":"), 1);

	if (*pszQop)
	{
		sha256_update(&ctx, (uint8 *)pszNonceCount, (uint32)strlen(pszNonceCount));
		sha256_update(&ctx, (uint8 *)(":"), 1);
		sha256_update(&ctx, (uint8 *)pszCNonce, (uint32)strlen(pszCNonce));
		sha256_update(&ctx, (uint8 *)(":"), 1);
		sha256_update(&ctx, (uint8 *)pszQop, (uint32)strlen(pszQop));
		sha256_update(&ctx, (uint8 *)(":"), 1);
	};

	sha256_update(&ctx, (uint8 *)(&HA2Hex[0]), 64);
	sha256_finish(&ctx, RespHash);
}

/* calculate request-digest/response-digest as per HTTP Digest spec */
void SHA256_DigestCalcResponse(
	char HA1[65], /* H(A1) */
	const char * pszNonce, /* nonce from server */
	const char * pszNonceCount, /* 8 hex digits */
	const char * pszCNonce, /* client nonce */
	const char * pszQop, /* qop-value: "", "auth", "auth-int" */
	const char * pszMethod, /* method from the request */
	const char * pszDigestUri, /* requested URL */
	char HEntity[65], /* H(entity body) if qop="auth-int" */
	char Response[65] /* request-digest or response-digest */
)
{
	uint8 RespHash[32];

	SHA256_DigestCalcResponseHash(HA1, pszNonce, pszNonceCount, pszCNonce,
		pszQop, pszMethod, pszDigestUri, HEntity, RespHash);
	
	bin_to_hex_str(RespHash, 32, Response, 65);
};

BOOL DigestAuthProcess(
    HD_AUTH_INFO * p_auth, 
    HD_AUTH_INFO * p_lauth, 
    const char * method, 
    const char * password
)
{
    if (p_auth->auth_algorithm[0] == '\0' || 
        strncasecmp(p_auth->auth_algorithm, "MD5", 3) == 0)
    {
        char HA1[33] = {'\0'};
    	char HA2[33] = {'\0'};
    	char response[33] = {'\0'};
    	
    	MD5_DigestCalcHA1(p_auth->auth_algorithm, p_auth->auth_name, 
    	    p_lauth->auth_realm, password, p_auth->auth_nonce, 
    	    p_auth->auth_cnonce, HA1);
        
    	MD5_DigestCalcResponse(HA1, p_lauth->auth_nonce, 
    	    p_auth->auth_ncstr, p_auth->auth_cnonce, 
    	    p_auth->auth_qop, method, p_auth->auth_uri, 
    	    HA2, response);
    		
    	if (strcasecmp(response, p_auth->auth_response) == 0)
    	{
    		return TRUE;
        }
    }
    else if (strncasecmp(p_auth->auth_algorithm, "SHA-256", 7) == 0)
    {
        char HA1[65] = {'\0'};
    	char HA2[65] = {'\0'};
    	char response[65] = {'\0'};
    	
    	SHA256_DigestCalcHA1(p_auth->auth_algorithm, p_auth->auth_name, 
    	    p_lauth->auth_realm, password, p_auth->auth_nonce, 
    	    p_auth->auth_cnonce, HA1);
        
    	SHA256_DigestCalcResponse(HA1, p_lauth->auth_nonce, 
    	    p_auth->auth_ncstr, p_auth->auth_cnonce, 
    	    p_auth->auth_qop, method, p_auth->auth_uri, 
    	    HA2, response);
    		
    	if (strcasecmp(response, p_auth->auth_response) == 0)
    	{
    		return TRUE;
        }
    }
    
	return FALSE;
}





