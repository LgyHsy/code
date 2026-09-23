#ifndef __GET_SN_V2_H__
#define __GET_SN_V2_H__

#include <sys/socket.h>   // socket definitions
#include <netinet/in.h>   // struct sockaddr_in definitions

// #include "platform.h"
#include "sn_header.h"
// #include "check_util.h"
#include "anj_mw_crypt.h"


// Message header format
#define SN_V2_MSG_MAGIC 0x5AA5                  // 2-byte magic
#define SN_V2_MSG_VERSION 2                     // Version

// Encryption algorithm options
#define SN_V2_ENCRYPT_ALGO_NONE 0               // No encryption
#define SN_V2_ENCRYPT_ALGO_RC4BASE64 1                // RC4BASE64 encryption

// Timeout constants
#define SN_V2_TIMEOUT_DISCOVERY 1000            // UDP discovery timeout (ms)
#define SN_V2_TIMEOUT_TCP_CONNECT 3000          // TCP connection timeout (ms)
#define SN_V2_TIMEOUT_MSG_RESPONSE 3000         // Message response timeout (ms)
#define SN_V2_TIMEOUT_TOTAL 60000               // Total timeout (ms)

// Data structures

// Message header structure (8 bytes) - V2
typedef struct {
    unsigned short magic;          // 2-byte magic: 0x5AA5
    unsigned char version;         // 1-byte version
    unsigned char encrypt_algo;    // 1-byte encryption algorithm
    unsigned int data_len;         // 4-byte data length
} SnV2MsgHeader;


// Edge server information structure
typedef struct {
    char ip[16];                   // Edge server IP address
    char allocated_ip[16];         // Allocated IP address for the device
    int port;                      // Edge server port
    int load;                      // Edge server load
    struct sockaddr_in addr;       // Edge server socket address
} EdgeServerInfo;

// State machine states
typedef enum {
    SOFTSN_V2_STATUS_INIT = 0,                // Initial state
    SOFTSN_V2_STATUS_DISCOVERY,               // UDP broadcast discovery
    SOFTSN_V2_STATUS_SELECT_SERVER,           // Select server
    SOFTSN_V2_STATUS_TCP_CONNECT,             // TCP connection
    SOFTSN_V2_STATUS_REQUEST_UUID,            // Request UUID
    SOFTSN_V2_STATUS_REQUEST_SN,              // Request serial number
    SOFTSN_V2_STATUS_OK                       // Authorization completed
} SoftSnV2Status;

// Function declarations
void set_softsn_v2_cb(SN_GET_OK_CALLBACK cb);
void set_softsn_cb(SN_GET_OK_CALLBACK cb);

int start_softsn_thread_v2(void);
int stop_softsn_thread_v2(void);
int wait_softsn_thread_v2(void);

// Helper function declarations for copy prevention
// Decrypt uuid_check information back to original UUID
void copy_prevention_decrypt_v2(const void *encrypted_uuid, char *uuid, int uuid_len);

int ssn_get_version();


#endif /* __GET_SN_V2_H__ */
