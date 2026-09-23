#include "get_sn_v2.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <ixml.h>
#include <linux/sockios.h>
#include <net/if.h>
#include <net/route.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/time.h>

#include "anj_mw_comm.h"
#include "anj_mw_crypt.h"
#include "anj_mw_file.h"
#include "anj_mw_net.h"
#include "anj_mw_thread.h"
#include "anj_sysmng.h"

#include "driver_interface.h"
#include "aes.h"
#include "platform_sn.h"
#include "encrypt/encrypt.h"
#include "flash_rw.h"
#include "sn_utils.h"
#include "sn_uuid.h"
#include "sn_tools.h"
#include "get_sn.h"


// Global variables
static anj_thread_s s_stSoftSnDataThreadV2 = {0};
static SN_GET_OK_CALLBACK g_pFunc_v2 = NULL;
static SoftSnV2Status gRequestStatus_v2 = SOFTSN_V2_STATUS_INIT;

// Fixed key for RC4Base64 encryption
static const char *rc4base64_key = "Anjvision.Aiot99.Softsn.2025.key";
static int g_SoftSnVersion = -1;
// AES key generation function prototypes
static void generate_aes_key_uuid(unsigned char *key);
static void generate_aes_key_v2(const int keyIndex, unsigned char *key);

// Auxiliary function declarations
static int udp_discovery_server(const char *myuuid, EdgeServerInfo *servers, int *server_count);
static int select_best_server(EdgeServerInfo *servers, int server_count, EdgeServerInfo *selected_server);
static int tcp_connect_server(EdgeServerInfo *server, int *sockfd);
static int send_v2_message(int sockfd, const char *data, int data_len);
static int recv_v2_message(int sockfd, int *msg_code, char *data, int *data_len);
static int encrypt_message(const char *plaintext, int plaintext_len, char *ciphertext, int *ciphertext_len, int algo);
static int decrypt_message(const char *ciphertext, int ciphertext_len, char *plaintext, int *plaintext_len, int *algo_out);
static int handle_cmd_uuid_response_v2(int cmd, const int sockfd, char* recv_buf, char* send_buf, const char *my_uuid);
static int handle_cmd_sn_response_v2(int cmd, const int sockfd, char* recv_buf, char* send_buf, const char *my_uuid);
int write_encript_data_to_soft_v2(unsigned char *buf, int len, const char *uuid);
static void copy_prevention_encrypt_v2(const char *uuid, void *buffer, int buflen);
int CheckSoftSNData_v2(const char* buf, int inbuflen, char *output_decrypt, int outbuflen, int bShowInfo);

// Generate AES key 1 (32 bytes) - for first encryption algorithm
static void generate_aes_key_uuid(unsigned char *key)
{
    // Base values for key generation
    const unsigned char base_values[] = {
        0x1a, 0x2b, 0x3c, 0x4d, 0x5e, 0x6f, 0x70, 0x81,
        0x92, 0xa3, 0xb4, 0xc5, 0xd6, 0xe7, 0xf8, 0x09,
        0xba, 0xcb, 0xdc, 0xed, 0xfe, 0x0f, 0x1e, 0x2d,
        0x3c, 0x4b, 0x5a, 0x69, 0x78, 0x87, 0x96, 0xa5
    };
    
    // Fixed values for entropy (instead of function addresses)
    const unsigned long long fixed_values1[] = {
        0x123456789ABCDEF0, 0x0FEDCBA987654321,
        0x55AA55AA55AA55AA, 0xAA55AA55AA55AA55
    };

    int i;
    // Generate key using a combination of base values and algorithmic transformations
    for (i = 0; i < 32; i++) {
        // Mix base values with fixed values and simple arithmetic
        unsigned char val = base_values[i];
        
        // XOR with different bytes from fixed values
        val ^= (fixed_values1[i % 4] >> (i % 8) * 8) & 0xff;
        val ^= (fixed_values1[(i + 2) % 4] >> ((i + 4) % 8) * 8) & 0xff;
        
        // Apply simple transformations
        val = (val << 3) | (val >> 5);  // Rotate left by 3 bits
        val ^= 0x55;                    // XOR with fixed value
        val = (val * 0x1f) & 0xff;       // Multiply by prime number
        val ^= i;                       // XOR with index
        
        // Store final key byte
        key[i] = val;
    }
    
    // Final mixing round for additional security
    for (i = 0; i < 32; i++) {
        key[i] ^= key[(i + 17) % 32];  // XOR with another byte in the key
        key[i] = (key[i] << 1) | (key[i] >> 7);  // Rotate left by 1 bit
    }
}

// Generate AES key 2 (32 bytes) - for second encryption algorithm
static void generate_aes_key_v2(const int keyIndex, unsigned char *key)
{
    // Different base values for second key
    const unsigned char base_values[] = {
        0xf0, 0xe1, 0xd2, 0xc3, 0xb4, 0xa5, 0x96, 0x87,
        0x78, 0x69, 0x5a, 0x4b, 0x3c, 0x2d, 0x1e, 0x0f,
        0x01, 0x12, 0x23, 0x34, 0x45, 0x56, 0x67, 0x78,
        0x89, 0x9a, 0xab, 0xbc, 0xcd, 0xde, 0xef, 0xf0};

    // Different fixed values for entropy (instead of function addresses)
    const unsigned long long fixed_values2[] = {
        0xDEADBEEFDEADBEEF, 0xBEEFDEADBEEFDEAD,
        0xCAFEBABECAFEBABE, 0xBABECAFEBABECAFE};
    int i;
    // Generate key using a different algorithmic approach
    for (i = 0; i < 32; i++)
    {
        // Mix base values with fixed values and different arithmetic
        unsigned char val = base_values[i] + keyIndex;

        // XOR with different bytes from fixed values
        val ^= (fixed_values2[i % 4] >> (i % 8) * 8) & 0xff;
        val ^= (fixed_values2[(i + 3) % 4] >> ((i + 6) % 8) * 8) & 0xff;

        // Apply different transformations
        val = (val >> 2) | (val << 6); // Rotate right by 2 bits
        val ^= 0xaa;                   // XOR with different fixed value
        val = (val * 0x23) & 0xff;     // Multiply by different prime number
        val ^= (31 - i);               // XOR with reversed index

        // Store final key byte
        key[i] = val;
    }

    // Final mixing round with different parameters
    for (i = 0; i < 32; i++)
    {
        key[i] ^= key[(i + 23) % 32];           // XOR with different offset
        key[i] = (key[i] >> 1) | (key[i] << 7); // Rotate right by 1 bit
    }

#if 0
    printf("key %d:", keyIndex);
    for (i = 0; i < 32; i++) {
        if( i % 8 == 0 )
            printf("\n");
        printf("0x%02x ", key[i]);
    }
    printf("\n");
#endif
}

// Check V2 version software authorization data
int CheckSoftSNData_v2(const char *buf, int inbuflen, char *output_decrypt, int outbuflen, int bShowInfo)
{
    int i;

    // V2 SN data format: 192 bytes, 96 bytes binary data converted to 192 bytes hex string
    // 96 bytes binary data = 6 * 16 bytes AES encrypted blocks
    // Encrypted data format:
    //   - 8 bytes software serial
    //   - 8 bytes fixed string "AJSOSN" + 0xffff (version)
    //   - 8 bytes authorization timestamp
    //   - 72 bytes &-separated data: UUID&username&model

    // Check input length: must be exactly 192 bytes
    if (inbuflen != 192)
    {
        __ERR("V2 SN data length invalid: %d (must be 192 bytes)\n", inbuflen);
        return -1;
    }

    int num_blocks = 6;                       // 96 bytes / 16 bytes per block = 6 blocks
    unsigned char combined_decrypt[96] = {0}; // Exactly 96 bytes of decrypted data

    // Process each 32-byte hex string block (representing 16 bytes binary data)
    for (i = 0; i < num_blocks; i++)
    {
        unsigned char block_hex[32] = {0};
        unsigned char buffer[16] = {0};
        unsigned char decrypt_block[16] = {0};
        unsigned char decrypt_result[16] = {0};

        // Extract 32-byte hex string block
        memcpy((char *)block_hex, buf + i * 32, 32);

        // Convert hex string to binary data
        hexStrToUInt((char *)block_hex, 32, buffer);

#if SN_DEBUG
        {
            __INFO("V2 SN block %d INPUT:", i);
            debug_show_data_hex_ex(buffer, 16, 0);
        }
#endif

        // Use V2 AES key 1 for decryption
        unsigned char aes_key[32] = {0};
        generate_aes_key_v2(i + 1, aes_key);

        struct crypto_aes_ctx ctx;
        crypto_aes_expand_key(&ctx, aes_key, 32);

        // Decrypt block using AES
        memcpy(decrypt_block, buffer, 16);
        aes_decrypt_(&ctx, decrypt_result, decrypt_block);

#if SN_DEBUG
        {
            __INFO("V2 SN block %d decrypt:", i);
            debug_show_data_hex_ex(decrypt_result, 16, 0);
        }
#endif

        // Combine decrypted blocks into 96 bytes buffer
        memcpy(combined_decrypt + i * 16, decrypt_result, 16);
    }

    // Check if any data was decrypted (should not be all zero)
    int has_data = 0;
    for (i = 0; i < 96; i++)
    {
        if (combined_decrypt[i] != 0)
        {
            has_data = 1;
            break;
        }
    }

    if (!has_data)
    {
        __ERR("V2 SN all blocks zero\n");
        return -1;
    }

    // Check for fixed string "AJSOSN" at the correct position (offset 8)
    if (memcmp(combined_decrypt + 8, "AJSOSN", 6) != 0)
    {
        __ERR("V2 SN missing signature\n");
        return -1;
    }

    unsigned short nVersion = 0;
    memcpy(&nVersion, combined_decrypt + 14, 2);

    // Extract authorization timestamp (offset 16, 8 bytes)
    unsigned long long timestamp = 0;
    memcpy(&timestamp, combined_decrypt + 16, 8);

    // Extract combined data from offset 24 (72 bytes)
    char combined_data[73] = {0};
    memcpy(combined_data, combined_decrypt + 24, 72);
    combined_data[72] = '\0'; // Ensure null termination

    // Parse &-separated data: UUID&username&model
    char uuid_from_sn[33] = {0};     // 32 bytes UUID + 1 null terminator
    char username_from_sn[17] = {0}; // 16 bytes username + 1 null terminator
    char model_from_sn[9] = {0};     // 8 bytes model + 1 null terminator

    char *token = strtok(combined_data, "&");
    if (token != NULL)
    {
        strncpy(uuid_from_sn, token, 32);
        uuid_from_sn[32] = '\0';

        token = strtok(NULL, "&");
        if (token != NULL)
        {
            strncpy(username_from_sn, token, 16);
            username_from_sn[16] = '\0';

            token = strtok(NULL, "&");
            if (token != NULL)
            {
                strncpy(model_from_sn, token, 8);
                model_from_sn[8] = '\0';
            }
        }
    }

#if SN_DEBUG
    {
        __ERR("V2 SN decrypted data:");
        debug_show_data_hex_ex(combined_decrypt, 96, 0);
    }
#endif

    if (bShowInfo)
    {
        // Print detailed breakdown of decrypted data
        char szTimeStamp[64] = {0};
        format_utc_timestamp(timestamp, szTimeStamp, sizeof(szTimeStamp));

        __INFO("SN Version: V2 %#X \n", nVersion);
        __INFO("SN Serial: %02X%02X%02X%02X%02X%02X%02X%02X \n",
               combined_decrypt[0], combined_decrypt[1], combined_decrypt[2], combined_decrypt[3],
               combined_decrypt[4], combined_decrypt[5], combined_decrypt[6], combined_decrypt[7]);
        __INFO("SN Timestamp: %llu [%s] \n", (unsigned long long)timestamp, szTimeStamp);
        __INFO("SN UUID: %s \n", uuid_from_sn);
        __INFO("SN Username: %s \n", username_from_sn);
        __INFO("SN Model: %s \n", model_from_sn);
    }

    // Get current device UUID for verification
    const char *current_uuid = get_uuid();
    if (current_uuid != NULL && strlen(current_uuid) >= 5)
    {
        // Verify UUID matches current device UUID
        if (strcasecmp(current_uuid, uuid_from_sn) != 0)
        {
            __ERR("V2 SN UUID mismatch: %s != %s\n", current_uuid, uuid_from_sn);
            return -1;
        }
    }
    // Copy combined decrypted data to output
    if (output_decrypt != NULL && outbuflen > 0)
    {
        int copy_len = 96;
        if (copy_len > outbuflen - 1)
        {
            copy_len = outbuflen - 1;
        }
        memcpy(output_decrypt, combined_decrypt, copy_len);
        output_decrypt[copy_len] = '\0';
    }

    __INFO("V2 SN check passed\n");
    return 0;
}

// Set callback function
void set_softsn_v2_cb(SN_GET_OK_CALLBACK cb)
{
    g_pFunc_v2 = cb;
}

void set_softsn_cb(SN_GET_OK_CALLBACK cb)
{
    set_softsn_v1_cb(cb);
    set_softsn_v2_cb(cb);
}

// Copy prevention value encryption - first algorithm: AES encryption for 32-byte UUID
static void copy_prevention_encrypt_v2(const char *uuid, void *buffer, int buflen)
{
    if( buflen < 32 )
        return;
    
    // Generate AES key for UUID encryption
    unsigned char aes_key[32] = {0};
    generate_aes_key_uuid(aes_key);
    
    struct crypto_aes_ctx ctx;
    crypto_aes_expand_key(&ctx, aes_key, 32);
    
    // Process UUID in 16-byte blocks (32 bytes total)
    unsigned char uuid_blocks[64] = {0};
    unsigned char encrypted_blocks[32] = {0};
    
    // Copy UUID to blocks
    strncpy((char*)uuid_blocks, uuid, 32);
    
    // Encrypt first 16-byte block
    aes_encrypt_(&ctx, encrypted_blocks, uuid_blocks);
    
    // Encrypt second 16-byte block
    aes_encrypt_(&ctx, encrypted_blocks + 16, uuid_blocks + 16);
    
    // Copy encrypted UUID to buffer
    memcpy(buffer, encrypted_blocks, 32);
}

// Copy prevention value decryption - AES decryption for 32-byte UUID
// Used to decrypt uuid_check information back to original UUID
void copy_prevention_decrypt_v2(const void *encrypted_uuid, char *uuid, int uuid_len)
{
    if( uuid_len < 33 ) // Need at least 32 bytes for UUID + 1 for null terminator
        return;
    
    // Generate the same AES key used for encryption
    unsigned char aes_key[32] = {0};
    generate_aes_key_uuid(aes_key);
    
    struct crypto_aes_ctx ctx;
    crypto_aes_expand_key(&ctx, aes_key, 32);
    
    // Process encrypted UUID in 16-byte blocks (32 bytes total)
    const unsigned char *encrypted_blocks = (const unsigned char*)encrypted_uuid;
    unsigned char decrypted_blocks[32] = {0};
    
    // Decrypt first 16-byte block
    aes_decrypt_(&ctx, decrypted_blocks, encrypted_blocks);
    
    // Decrypt second 16-byte block
    aes_decrypt_(&ctx, decrypted_blocks + 16, encrypted_blocks + 16);
    
    // Copy decrypted UUID to output buffer and add null terminator
    memcpy(uuid, decrypted_blocks, 32);
    uuid[32] = '\0';
}

// UDP discovery server
static int udp_discovery_server(const char *myuuid, EdgeServerInfo *servers, int *server_count)
{
    int sockfd = -1;
    int ret = 0;
    char szDeviceType[32];
    anj_sysmng_dev_str_get(szDeviceType);
    
    struct sockaddr_in addr;
    struct sockaddr_in remote_addr;
    socklen_t addr_len = sizeof(remote_addr);
    char recv_buf[1024] = {0};
    char plaintext_buf[1024] = {0};
    
    // Create UDP socket
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        __ERR("socket create failed: %s\n", strerror(errno));
        return -1;
    }
    
    // Set broadcast option
    int opt = 1;
    ret = setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &opt, sizeof(opt));
    if (ret < 0) {
        __ERR("setsockopt SO_BROADCAST failed: %s\n", strerror(errno));
        close(sockfd);
        return -1;
    }
    
    int nReuseAddress = 1;
    int nRet = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char *)&nReuseAddress, sizeof(nReuseAddress));
    if (nRet != 0)
    {
        __ERR("set socket reuse address option failed, errno=%d\n", errno);
        close(sockfd);

        return -1;
        ;
    }

    // Bind port
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(BROADCASTING_PORT_SN);
    
    ret = bind(sockfd, (struct sockaddr *)&addr, sizeof(addr));
    if (ret < 0)
    {
        int real_errno = errno; // 极其重要：第一时间保护现场！
        __ERR("bind failed (real errno=%d): %s\n", real_errno, strerror(real_errno));
        close(sockfd);
        return -1;
    }
    
    // Set a small socket timeout for recvfrom to allow checking overall timeout
    struct timeval socket_timeout;
    socket_timeout.tv_sec = 0;
    socket_timeout.tv_usec = 100000; // 100ms per recvfrom call
    ret = setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &socket_timeout, sizeof(socket_timeout));
    if (ret < 0)
    {
        int real_errno = errno; // 极其重要：第一时间保护现场！
        __ERR("setsockopt SO_RCVTIMEO failed (real errno=%d): %s\n", real_errno, strerror(real_errno));
        close(sockfd);
        return -1;
    }
    
    // Send broadcast request
    memset(&remote_addr, 0, sizeof(remote_addr));
    remote_addr.sin_family = AF_INET;
    remote_addr.sin_addr.s_addr = htonl(INADDR_BROADCAST);
    remote_addr.sin_port = htons(BROADCASTING_PORT_SN);
    
    // Build XML message body
    sprintf(plaintext_buf,
        "<ENCRYPT>\n"
        "<MESSAGE_HEADER Msg_type=\"SYSTEM_ENCRYPTSN_MESSAGE\" Msg_code=\"%d\" Msg_flag=\"0\" />\n"
        "<MESSAGE_BODY>\n"
        "<DEVICE DeviceType=\"%s\" Identity=\"%s\" RequestType=\"DISCOVERY\"/>\n"
        "</MESSAGE_BODY>\n"
        "</ENCRYPT>\n",
        IPC_MESSAGE_V2_DESCOVERY,
        szDeviceType, myuuid);

    __INFO("len %d:%s", strlen(plaintext_buf), plaintext_buf);

    // Encrypt message with header
    char full_msg[sizeof(SnV2MsgHeader) + 1024] = {0};
    int full_msg_len = 0;
    
    ret = encrypt_message(plaintext_buf, strlen(plaintext_buf), full_msg, &full_msg_len, SN_V2_ENCRYPT_ALGO_RC4BASE64);
    if (ret < 0)
    {
        __ERR("encrypt_message failed\n");
        close(sockfd);
        return -1;
    }
    
    // Send the full message including header
    ret = sendto(sockfd, full_msg, full_msg_len, 0, (struct sockaddr *)&remote_addr, sizeof(remote_addr));
    if (ret < 0)
    {
        int real_errno = errno;
        __ERR("broardcast_send_request failed (real errno=%d): %s\n", real_errno, strerror(real_errno));
        close(sockfd);

        __INFO("try to add route for address 255.255.255.255\n");

        sn_broadcast_route_add();

        SLEEP_SECOND(1);

        return -1;
    }
    __INFO("send udp broardcast len %d, return %d", full_msg_len, ret);
    
    // Receive server responses
    *server_count = 0;
    
    // Calculate total timeout end time
    unsigned long long start_time, current_time;
    start_time = sn_get_runtime();

    while (1)
    {
        // Check if we've exceeded the total discovery timeout
        current_time = sn_get_runtime();
        long long elapsed_ms = (current_time - start_time);

        if (elapsed_ms >= SN_V2_TIMEOUT_DISCOVERY)
        {
            // Total discovery timeout reached, end receiving
            __INFO("Discovery timeout after %lld ms, received %d servers\n",
                     elapsed_ms, *server_count);
            break;
        }

        ret = recvfrom(sockfd, recv_buf, sizeof(recv_buf) - 1, 0, (struct sockaddr *)&remote_addr, &addr_len);
        if (ret < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                // Socket timeout, continue to check overall timeout
                continue;
            }
            __ERR("recvfrom failed: %s\n", strerror(errno));
            // Continue loop instead of exiting on recvfrom error
            continue;
        }

        // Decrypt message with header
        int plaintext_len = 0;
        int decrypt_ret = decrypt_message(recv_buf, ret, plaintext_buf, &plaintext_len, NULL);
        if (decrypt_ret < 0)
        {
            __ERR("decrypt_message failed\n");
            continue;
        }
        plaintext_buf[plaintext_len] = '\0';
        //        __INFO("recved: %s", plaintext_buf);

        // Parse server information
        IXML_Document *pDocNode = ixmlParseBuffer(plaintext_buf);
        if (pDocNode == NULL)
        {
            __ERR("xml error\r\n");
            continue;
        }

        // Check message type
        int msg_code = 0;
        IXML_NodeList *header_nodelist = ixmlDocument_getElementsByTagName(pDocNode, "MESSAGE_HEADER");
        if (header_nodelist != NULL)
        {
            IXML_Node *tmp = header_nodelist->nodeItem->firstAttr;
            while (tmp != NULL)
            {
                if (strcmp(tmp->nodeName, "Msg_code") == 0 && tmp->nodeValue != NULL)
                {
                    msg_code = atoi(tmp->nodeValue);
                    break;
                }
                tmp = tmp->nextSibling;
            }
            ixmlNodeList_free(header_nodelist);
        }

        // Only process discovery response messages
        if (msg_code != IPC_MESSAGE_V2_DESCOVERY_RESPONSE)
        {
            //            __ERR("Invalid message type: %d, expected: %d\n", msg_code, IPC_MESSAGE_V2_DESCOVERY_RESPONSE);
            ixmlDocument_free(pDocNode);
            continue;
        }

        // Parse DEVICE node to get Identity
        char identity[128] = {0};
        IXML_NodeList *device_nodelist = ixmlDocument_getElementsByTagName(pDocNode, "DEVICE");
        if (device_nodelist != NULL)
        {
            IXML_Node *tmp = device_nodelist->nodeItem->firstAttr;
            while (tmp != NULL)
            {
                if (strcmp(tmp->nodeName, "Identity") == 0 && tmp->nodeValue != NULL)
                {
                    strncpy(identity, tmp->nodeValue, sizeof(identity) - 1);
                    break;
                }
                tmp = tmp->nextSibling;
            }
            ixmlNodeList_free(device_nodelist);
        }

        // Verify Identity matches myuuid
        if (identity[0] == 0 || strcasecmp(identity, myuuid) != 0)
        {
            //            __ERR("Identity mismatch: %s != %s\n", identity, myuuid);
            ixmlDocument_free(pDocNode);
            continue;
        }

        // Parse ServerInfo node to get server details
        IXML_NodeList *server_nodelist = ixmlDocument_getElementsByTagName(pDocNode, "ServerInfo");
        if (server_nodelist != NULL)
        {
            IXML_Node *server_node = server_nodelist->nodeItem;
            IXML_Node *attr = server_node->firstAttr;
            EdgeServerInfo server;
            memset(&server, 0, sizeof(server));

            // Initialize with socket address as fallback
            strcpy(server.ip, inet_ntoa(remote_addr.sin_addr));
            server.addr = remote_addr;
            server.port = 0;
            server.load = 0;
            // Initialize allocated_ip to empty string
            server.allocated_ip[0] = '\0';

            // Parse ServerInfo attributes
            while (attr != NULL)
            {
                if (strcmp(attr->nodeName, "IP") == 0 && attr->nodeValue != NULL)
                {
                    strncpy(server.ip, attr->nodeValue, sizeof(server.ip) - 1);
                }
                else if (strcmp(attr->nodeName, "Port") == 0 && attr->nodeValue != NULL)
                {
                    server.port = atoi(attr->nodeValue);
                }
                else if (strcmp(attr->nodeName, "Load") == 0 && attr->nodeValue != NULL)
                {
                    server.load = atoi(attr->nodeValue);
                }
                else if (strcmp(attr->nodeName, "AllocatedIP") == 0 && attr->nodeValue != NULL)
                {
                    // Add support for AllocatedIP attribute
                    strncpy(server.allocated_ip, attr->nodeValue, sizeof(server.allocated_ip) - 1);
                }
                attr = attr->nextSibling;
            }

            // Check if we got valid server information
            if (server.port > 0)
            {
                // Add to server list
                if (*server_count < 10)
                { // Save up to 10 servers
                    servers[*server_count] = server;
                    (*server_count)++;
                    // Print server information including AllocatedIP if available
                    if (server.allocated_ip[0] != '\0')
                    {
                        __INFO("Found server: IP=%s, Port=%d, Load=%d, AllocatedIP=%s\n",
                                 server.ip, server.port, server.load, server.allocated_ip);
                    }
                    else
                    {
                        // __INFO("Found server: IP=%s, Port=%d, Load=%d\n",  server.ip, server.port, server.load);
                    }

                    // Got a valid server, no need to look for more
                    __INFO("Found server %s:%u, exiting discovery loop\n", server.ip, server.port);
                    ixmlNodeList_free(server_nodelist);
                    ixmlDocument_free(pDocNode);
                    goto EXIT_LOOP;
                }
            }
            else
            {
                __ERR("Invalid server information: missing Port\n");
            }

            ixmlNodeList_free(server_nodelist);
        }
        else
        {
            __ERR("Missing ServerInfo node\n");
        }

        ixmlDocument_free(pDocNode);
    }

EXIT_LOOP:
    close(sockfd);
    return 0;
}

// Select best server
static int select_best_server(EdgeServerInfo *servers, int server_count, EdgeServerInfo *selected_server)
{
    int i;
    if (server_count <= 0)
    {
        return -1;
    }

    // Simply select the server with the lowest load
    int best_idx = 0;
    int min_load = servers[0].load;

    for (i = 1; i < server_count; i++)
    {
        if (servers[i].load < min_load)
        {
            min_load = servers[i].load;
            best_idx = i;
        }
    }

    *selected_server = servers[best_idx];
    __INFO("Selected server: %s:%d, load: %d\n", selected_server->ip, selected_server->port, selected_server->load);

    return 0;
}

// Get local IP address from network interface
static int get_local_ip(char *ip, int ip_len)
{
    struct ifaddrs *ifaddr, *ifa;
    int ret = -1;

    // Get all network interfaces
    if (getifaddrs(&ifaddr) == -1)
    {
        __ERR("getifaddrs failed: %s\n", strerror(errno));
        return -1;
    }

    // Iterate through all interfaces
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next)
    {
        // Skip non-IPv4 interfaces
        if (ifa->ifa_addr == NULL || ifa->ifa_addr->sa_family != AF_INET)
        {
            continue;
        }

        // Skip loopback interface
        if (strcmp(ifa->ifa_name, "lo") == 0)
        {
            continue;
        }

        // Get IPv4 address
        struct sockaddr_in *addr = (struct sockaddr_in *)ifa->ifa_addr;
        strncpy(ip, inet_ntoa(addr->sin_addr), ip_len - 1);
        ip[ip_len - 1] = '\0';

        __INFO("Found local IP on %s: %s\n", ifa->ifa_name, ip);
        ret = 0;
        break;
    }

    freeifaddrs(ifaddr);
    return ret;
}

// Set IP address to network interface
static int set_interface_ip(const char *ifname, const char *ip)
{
    int sockfd;
    struct ifreq ifr;
    struct sockaddr_in *addr;

    // Create socket
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        __ERR("socket create failed for setting IP: %s\n", strerror(errno));
        return -1;
    }

    // Set interface name
    snprintf(ifr.ifr_name, IFNAMSIZ, "%s", ifname);

    // Set IP address
    addr = (struct sockaddr_in *)&ifr.ifr_addr;
    addr->sin_family = AF_INET;
    if (inet_pton(AF_INET, ip, &addr->sin_addr) <= 0)
    {
        __ERR("Invalid IP address: %s\n", ip);
        close(sockfd);
        return -1;
    }

    // Set IP address to interface
    if (ioctl(sockfd, SIOCSIFADDR, &ifr) < 0)
    {
        __ERR("ioctl SIOCSIFADDR failed: %s\n", strerror(errno));
        close(sockfd);
        return -1;
    }

    // Set subnet mask (using 255.255.255.0 as default)
    addr = (struct sockaddr_in *)&ifr.ifr_netmask;
    addr->sin_family = AF_INET;
    if (inet_pton(AF_INET, "255.255.255.0", &addr->sin_addr) <= 0)
    {
        __ERR("Invalid subnet mask\n");
        close(sockfd);
        return -1;
    }
    if (ioctl(sockfd, SIOCSIFNETMASK, &ifr) < 0)
    {
        __ERR("ioctl SIOCSIFNETMASK failed: %s\n", strerror(errno));
        // Continue even if subnet mask setting fails
    }

    // Set broadcast address
    addr = (struct sockaddr_in *)&ifr.ifr_broadaddr;
    addr->sin_family = AF_INET;
    // Calculate broadcast address (assuming /24 subnet)
    struct in_addr ip_addr, bcast_addr;
    if (inet_pton(AF_INET, ip, &ip_addr) <= 0)
    {
        __ERR("inet_pton %s failed: %s\n", ip, strerror(errno));
        close(sockfd);
        return -1;
    }

    bcast_addr.s_addr = ip_addr.s_addr | (~htonl(0xFFFFFF));
    addr->sin_addr = bcast_addr;
    if (ioctl(sockfd, SIOCSIFBRDADDR, &ifr) < 0)
    {
        __ERR("ioctl SIOCSIFBRDADDR failed: %s\n", strerror(errno));
        // Continue even if broadcast address setting fails
    }

    // Bring down the interface first
    if (ioctl(sockfd, SIOCGIFFLAGS, &ifr) < 0)
    {
        __ERR("ioctl SIOCGIFFLAGS failed: %s\n", strerror(errno));
        close(sockfd);
        return -1;
    }

    ifr.ifr_flags &= ~(IFF_UP | IFF_RUNNING);
    if (ioctl(sockfd, SIOCSIFFLAGS, &ifr) < 0)
    {
        __ERR("ioctl SIOCSIFFLAGS (down) failed: %s\n", strerror(errno));
        close(sockfd);
        return -1;
    }

    // Bring up the interface
    ifr.ifr_flags |= IFF_UP | IFF_RUNNING;
    if (ioctl(sockfd, SIOCSIFFLAGS, &ifr) < 0)
    {
        __ERR("ioctl SIOCSIFFLAGS (up) failed: %s\n", strerror(errno));
        close(sockfd);
        return -1;
    }

    close(sockfd);

    // Set default route using rtentry struct
    struct in_addr gw_addr;
    // Generate gateway IP by setting last octet to 1
    // Convert IP to host byte order, clear last octet, set to 1
    uint32_t ip_uint = ntohl(ip_addr.s_addr);
    uint32_t gw_uint = (ip_uint & 0xFFFFFF00) | 0x00000001;
    gw_addr.s_addr = htonl(gw_uint);

    // Create socket for route manipulation
    int route_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (route_sockfd >= 0)
    {
        struct rtentry route;
        memset(&route, 0, sizeof(route));

        // Set destination to default (0.0.0.0)
        struct sockaddr_in *dst = (struct sockaddr_in *)&route.rt_dst;
        dst->sin_family = AF_INET;
        dst->sin_addr.s_addr = INADDR_ANY;

        // Set gateway directly from struct in_addr
        struct sockaddr_in *gw = (struct sockaddr_in *)&route.rt_gateway;
        gw->sin_family = AF_INET;
        gw->sin_addr = gw_addr;

        // Set interface
        struct sockaddr_in *genmask = (struct sockaddr_in *)&route.rt_genmask;
        genmask->sin_family = AF_INET;
        genmask->sin_addr.s_addr = INADDR_ANY;

        // Set route flags
        route.rt_flags = RTF_UP | RTF_GATEWAY;
        route.rt_metric = 0;
        route.rt_dev = (char *)ifname;

        // Add route
        if (ioctl(route_sockfd, SIOCADDRT, &route) < 0)
        {
            __ERR("ioctl SIOCADDRT failed: %s\n", strerror(errno));
        }
        else
        {
            // Convert gateway to string for logging
            char gw_str[16] = {0};
            if (inet_ntop(AF_INET, &gw_addr, gw_str, sizeof(gw_str)) != NULL)
            {
                __INFO("Successfully added default route through %s\n", gw_str);
            }
            else
            {
                __INFO("Successfully added default route\n");
            }
        }

        close(route_sockfd);
    }
    else
    {
        __ERR("socket create failed for route setting: %s\n", strerror(errno));
    }

    __INFO("Set IP %s to interface %s, with subnet mask 255.255.255.0\n", ip, ifname);
    return 0;
}

// Get main network interface name
static int get_main_interface(char *ifname, int ifname_len)
{
    struct ifaddrs *ifaddr, *ifa;
    int ret = -1;

    // Get all network interfaces
    if (getifaddrs(&ifaddr) == -1)
    {
        __ERR("getifaddrs failed: %s\n", strerror(errno));
        return -1;
    }

    // Iterate through all interfaces
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next)
    {
        // Skip non-IPv4 interfaces
        if (ifa->ifa_addr == NULL || ifa->ifa_addr->sa_family != AF_INET)
        {
            continue;
        }

        // Skip loopback interface
        if (strcmp(ifa->ifa_name, "lo") == 0)
        {
            continue;
        }

        // Use this interface
        strncpy(ifname, ifa->ifa_name, ifname_len - 1);
        ifname[ifname_len - 1] = '\0';

        __INFO("Found main interface: %s\n", ifname);
        ret = 0;
        break;
    }

    freeifaddrs(ifaddr);
    return ret;
}

// TCP connect to server
static int tcp_connect_server(EdgeServerInfo *server, int *sockfd)
{
    int ret = 0;
    struct sockaddr_in addr;

    // Check if server has allocated IP
    if (server->allocated_ip[0] != '\0')
    {
        char local_ip[16] = {0};
        char ifname[IFNAMSIZ] = {0};

        // Get main network interface
        if (get_main_interface(ifname, sizeof(ifname)) != 0)
        {
            strcpy(ifname, WIRE_INTERFACE_NAME);
            __ERR("Failed to get main network interface. set to %s\n", ifname);
            mysystem_with_param("ifconfig %s up", ifname);
        }

        int bSetLocalIp = 0;
        // Get local IP
        if (get_local_ip(local_ip, sizeof(local_ip)) == 0)
        {
            __INFO("Local IP: %s, Allocated IP: %s\n", local_ip, server->allocated_ip);

            // Check if local IP matches allocated IP
            if (strcmp(local_ip, server->allocated_ip) != 0)
            {
                bSetLocalIp = 1;
                __INFO("Local IP doesn't match allocated IP, setting interface IP to %s\n", server->allocated_ip);
            }
        }
        else
        {
            bSetLocalIp = 1;
            __ERR("Failed to get local IP\n");
        }

        if( bSetLocalIp)
        {
            // Set interface IP to allocated IP
            if (set_interface_ip(ifname, server->allocated_ip) < 0)
            {
                __ERR("Failed to set interface IP to %s\n", server->allocated_ip);
            }
            else
            {
                __INFO("Successfully set interface %s IP to %s\n", ifname, server->allocated_ip);
            }
        }
    }

    // Create TCP socket
    *sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (*sockfd < 0)
    {
        __ERR("socket create failed: %s\n", strerror(errno));
        return -1;
    }

    // Set non-blocking
    int flags = fcntl(*sockfd, F_GETFL, 0);
    fcntl(*sockfd, F_SETFL, flags | O_NONBLOCK);

    // Set connection address
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(server->ip);
    addr.sin_port = htons(server->port);

    // Try to connect
    ret = connect(*sockfd, (struct sockaddr *)&addr, sizeof(addr));
    if (ret < 0)
    {
        if (errno != EINPROGRESS)
        {
            __ERR("connect %s:%d failed: %s\n", server->ip, server->port, strerror(errno));
            close(*sockfd);
            return -1;
        }

        // Use select to wait for connection completion
        fd_set write_fds;
        struct timeval timeout;

        FD_ZERO(&write_fds);
        FD_SET(*sockfd, &write_fds);

        timeout.tv_sec = SN_V2_TIMEOUT_TCP_CONNECT / 1000;
        timeout.tv_usec = (SN_V2_TIMEOUT_TCP_CONNECT % 1000) * 1000;

        ret = select(*sockfd + 1, NULL, &write_fds, NULL, &timeout);
        if (ret <= 0)
        {
            // Timeout or error
            __ERR("connect %s:%d timeout or error\n", server->ip, server->port);
            close(*sockfd);
            return -1;
        }

        // Check if connection was successful
        int error = 0;
        socklen_t len = sizeof(error);
        if (getsockopt(*sockfd, SOL_SOCKET, SO_ERROR, &error, &len) < 0 || error != 0)
        {
            __ERR("connect %s:%d failed: %s\n", server->ip, server->port, strerror(error));
            close(*sockfd);
            return -1;
        }
    }

    // Restore to blocking mode
    fcntl(*sockfd, F_SETFL, flags);

    __INFO("TCP connected to %s:%d\n", server->ip, server->port);
    return 0;
}

// Send message
static int send_v2_message(int sockfd, const char *data, int data_len)
{
    int ret = 0;

    // Encrypt message with header
    char full_msg[sizeof(SnV2MsgHeader) + 1024] = {0};
    int full_msg_len = 0;

    ret = encrypt_message(data, data_len, full_msg, &full_msg_len, SN_V2_ENCRYPT_ALGO_RC4BASE64);
    if (ret < 0)
    {
        __ERR("encrypt_message failed\n");
        return -1;
    }

    // Send the full message including header
    ret = send(sockfd, full_msg, full_msg_len, 0);
    if (ret < 0)
    {
        __ERR("send failed: %s\n", strerror(errno));
        return -1;
    }

    return 0;
}

// Receive message
static int recv_v2_message(int sockfd, int *msg_code, char *data, int *data_len)
{
    int ret = 0;
    char recv_buf[1024] = {0};
    int recv_len = 0;

    // Receive the full message including header
    recv_len = recv(sockfd, recv_buf, sizeof(recv_buf) - 1, 0);
    if (recv_len < 0)
    {
        __ERR("recv failed: %s\n", strerror(errno));
        return -1;
    }
    else if (recv_len == 0)
    {
        __ERR("connection closed\n");
        return -1;
    }

    // Decrypt message with header
    ret = decrypt_message(recv_buf, recv_len, data, data_len, NULL);
    if (ret < 0)
    {
        __ERR("decrypt_message failed\n");
        return -1;
    }

    // Parse message code
    IXML_Document *pDocNode = ixmlParseBuffer(data);
    if (pDocNode == NULL)
    {
        __ERR("xml error:%s", data);
        return -1;
    }

    IXML_NodeList *pNodelist = ixmlDocument_getElementsByTagName(pDocNode, "MESSAGE_HEADER");
    if (pNodelist != NULL)
    {
        IXML_Node *tmp = pNodelist->nodeItem->firstAttr;
        while (tmp != NULL)
        {
            if (strcmp(tmp->nodeName, "Msg_code") == 0 && tmp->nodeValue != NULL)
            {
                *msg_code = atoi(tmp->nodeValue);
                break;
            }
            tmp = tmp->nextSibling;
        }
        ixmlNodeList_free(pNodelist);
    }

    ixmlDocument_free(pDocNode);
    return 0;
}

// Encrypt message with header
static int encrypt_message(const char *plaintext, int plaintext_len, char *ciphertext, int *ciphertext_len, int algo)
{
    int ret = 0;
    char encrypted_data[1024] = {0};
    int encrypted_len = 0;

    // Encrypt the plaintext first
    switch (algo)
    {
    case SN_V2_ENCRYPT_ALGO_NONE:
        // No encryption, direct copy
        memcpy(encrypted_data, plaintext, plaintext_len);
        encrypted_len = plaintext_len;
        break;

    case SN_V2_ENCRYPT_ALGO_RC4BASE64:
    {
        // Use RC4Base64 encryption for software authorization with fixed key
        rc4base64_encode(plaintext, encrypted_data, rc4base64_key);
        encrypted_len = strlen(encrypted_data);
        break;
    }

    default:
        __ERR("unsupported encryption algorithm: %d\n", algo);
        return -1;
    }

    // Build message header
    SnV2MsgHeader header;
    header.magic = htons(SN_V2_MSG_MAGIC);
    header.version = SN_V2_MSG_VERSION;
    header.encrypt_algo = algo;
    header.data_len = htonl(encrypted_len);

    // Combine header and encrypted data
    memcpy(ciphertext, &header, sizeof(header));
    memcpy(ciphertext + sizeof(header), encrypted_data, encrypted_len);
    *ciphertext_len = sizeof(header) + encrypted_len;

    return ret;
}

// Decrypt message with header
static int decrypt_message(const char *ciphertext, int ciphertext_len, char *plaintext, int *plaintext_len, int *algo_out)
{
    int ret = 0;

    // Check if we have at least a header
    if (ciphertext_len < sizeof(SnV2MsgHeader))
    {
        __ERR("decrypt_message: insufficient data for header\n");
        return -1;
    }

    // Extract and parse the header
    SnV2MsgHeader header;
    memcpy(&header, ciphertext, sizeof(header));
    unsigned short magic = ntohs(header.magic);
    unsigned int data_len = ntohl(header.data_len);

    // Verify magic number
    if (magic != SN_V2_MSG_MAGIC)
    {
        __ERR("decrypt_message: invalid magic number\n");
        return -1;
    }

    // Check if we have the full encrypted data
    if (ciphertext_len < sizeof(SnV2MsgHeader) + data_len)
    {
        __ERR("decrypt_message: insufficient data for encrypted content\n");
        return -1;
    }

    // Extract the encrypted data
    const char *encrypted_data = ciphertext + sizeof(SnV2MsgHeader);
    int algo = header.encrypt_algo;

    // Return the algorithm if requested
    if (algo_out != NULL)
    {
        *algo_out = algo;
    }

    // Decrypt based on the algorithm
    switch (algo)
    {
    case SN_V2_ENCRYPT_ALGO_NONE:
        // No encryption, direct copy
        memcpy(plaintext, encrypted_data, data_len);
        *plaintext_len = data_len;
        break;

    case SN_V2_ENCRYPT_ALGO_RC4BASE64:
    {
        // Use RC4Base64 decryption for software authorization with fixed key
        base64rc4_decode(encrypted_data, plaintext, rc4base64_key);
        *plaintext_len = strlen(plaintext);
        break;
    }

    default:
        __ERR("decrypt_message: unsupported encryption algorithm: %d\n", algo);
        return -1;
    }

    return ret;
}

// Handle UUID response
static int handle_cmd_uuid_response_v2(int cmd, const int sockfd, char *recv_buf, char *send_buf, const char *my_uuid)
{
    // For TCP connections, we don't need remote address information
    __INFO("got %d from TCP connection\n", cmd);
    __INFO("%s\n", recv_buf);

    char identity[128];
    char uuiddata[128];
    char checksum[64];
    if( 0 != soft_enc_xml_uuid_data_get(recv_buf, identity, 128, uuiddata, 128, checksum, 64) )
    {
        return -1;
    }

    if (strcasecmp(my_uuid, identity) != 0)
    {
        __INFO("%s != %s \n", my_uuid, identity);
        return 0;
    }

    if (write_uuid((unsigned char *)uuiddata, strlen(uuiddata)) < 0)
        return -1;

    return 1;
}

// Handle SN response
static int handle_cmd_sn_response_v2(int cmd, const int sockfd, char *recv_buf, char *send_buf, const char *my_uuid)
{
    // For TCP connections, we don't need remote address information
    __INFO("got %d from TCP connection\n", cmd);
    __INFO("%s\n", recv_buf);

    char cameraid[128];
    char sndata[256];
    char checksum[64];
    if( 0 != soft_enc_xml_sn_data_parse(recv_buf, cameraid, 128, sndata, 256, checksum, 64, NULL) )
    {
        return -1;
    }

    if (strcasecmp(my_uuid, cameraid) != 0)
    {
        __INFO("%s != %s \n", my_uuid, cameraid);
        return 0;
    }

    if (cmd == IPC_MESSAGE_V2_REQUEST_SN_RESPONSE)
    {
        // Check V2 software authorization data
        char decrypted_sn[256] = {0};
        if (CheckSoftSNData_v2(sndata, strlen(sndata), decrypted_sn, sizeof(decrypted_sn), 0) < 0)
        {
            __ERR("V2 SN check failed\n");
            return -1;
        }

        if (write_encript_data_to_soft_v2((unsigned char *)sndata, strlen(sndata), my_uuid) < 0)
            return -1;
    }
    else if (cmd == IPC_MESSAGE_V1_REQUEST_SN_RESPONSE)
    {
        // Check V1 software authorization data
        char decrypted_sn[16] = {0};
        if (CheckSoftSNData_v1(sndata, strlen(sndata), decrypted_sn, sizeof(decrypted_sn), 0) < 0)
        {
            __ERR("V1 SN check failed\n");
            return -1;
        }

        if (write_encript_data_to_soft_v1((unsigned char *)sndata, strlen(sndata), my_uuid) < 0)
            return -1;
    }

    return 1;
}

// Write encrypted data to flash - V2 save mechanism, no additional encryption
int write_encript_data_to_soft_v2(unsigned char *buf, int len, const char *uuid)
{
    if (len < 32 || len > 192)
    {
        __INFO("len %d error\n", len);
        return -1;
    }

    int ret = 0;
    unsigned int size_sect = 4096;
    unsigned char *pSectBuffer = (unsigned char *)malloc(size_sect);
    if (NULL == pSectBuffer)
    {
        __INFO("malloc %u error\n", size_sect);
        return -1;
    }

    // Read flash data
    if (FlashIsSpiNandFlash() > 0)
    {
        ret = read_sndata_by_file((char *)pSectBuffer, size_sect);
        if (0 != ret)
        {
            free(pSectBuffer);
            return -1;
        }
    }
    else
    {
        const char *mtd_block = GET_SN_MTD_BLOCK();
        unsigned int sect_number = platform_sn_sect_no_get();
        ret = flash_sect_rw(0, mtd_block, size_sect, sect_number, (unsigned char *)pSectBuffer);
        if (0 != ret)
        {
            free(pSectBuffer);
            __INFO("sect %d: flash_read error\n", sect_number);
            return -1;
        }
    }

    // Call copy_prevention_encrypt_v2 function to get encrypted UUID for copy prevention
    char uuid_encrypt[32];
    copy_prevention_encrypt_v2(uuid, uuid_encrypt, 32);

    int iIndex = 0;
    for (iIndex = 0; iIndex < 2; iIndex++)
    {
        unsigned char *p = pSectBuffer + iIndex * 256;
        AjSnHeaderV2 *pHeader = (AjSnHeaderV2 *)p;

        if (pHeader->magic1 == MAGIC1_V2 &&
            pHeader->magic2 == MAGIC2_V2 &&
            pHeader->magic3 == MAGIC3_V2 &&
            pHeader->magic4 == MAGIC4_V2)
        {
            if (pHeader->len == len &&
                memcmp(pHeader->data, buf, len) == 0)
            {
                FREE_SECT_BUFFER();
                __INFO("%d: %s: data same, not changed.\n", iIndex, buf);
                return 0;
            }
        }

        pHeader->magic1 = MAGIC1_V2;
        pHeader->magic2 = MAGIC2_V2;
        pHeader->magic3 = MAGIC3_V2;
        pHeader->magic4 = MAGIC4_V2;
        pHeader->len = len;

        // Copy software authorization and UUID directly to flash, no encryption
        memcpy(pHeader->data, buf, pHeader->len);

        // Generate crc
        pHeader->crc = GetCrcValue((char *)pHeader->data, pHeader->len);

        // Store copy prevention value
        memcpy(pHeader->uuid_check, uuid_encrypt, 32);
    }

    if (FlashIsSpiNandFlash() > 0)
    {
        ret = write_sndata_by_file((char *)pSectBuffer, size_sect);
        if (0 != ret)
        {
            FREE_SECT_BUFFER();
            return -1;
        }
    }
    else
    {
        const char *mtd_block = GET_SN_MTD_BLOCK();
        unsigned int sect_number = platform_sn_sect_no_get();
        ret = flash_sect_rw(1, mtd_block, size_sect, sect_number, (unsigned char *)pSectBuffer);
        if (0 != ret)
        {
            FREE_SECT_BUFFER();
            __INFO("sect %d: flash_read error.\n", sect_number);
            return -1;
        }
    }

    FREE_SECT_BUFFER();
    return 0;
}

int WriteEncriptDataToSoft(unsigned char *buf, int len, int version)
{
    const char *uuid = get_uuid();
    if (version == 1)
        return write_encript_data_to_soft_v1(buf, len, uuid);
    else if (version == 2)
        return write_encript_data_to_soft_v2(buf, len, uuid);
    else
    {
        __ERR("version %d error\n", version);
        return -1;
    }
}

// Read encrypted data from flash - compatible with V1 and V2 save mechanisms
static int ReadEncriptDataFromSoft_inner(unsigned char *output_decrypt, int buflen, int bShowInfo)
{
    int iIndex;

    int ret = 0;
    unsigned int size_sect = 4096;
    unsigned char *pSectBuffer = (unsigned char *)malloc(size_sect);
    if (NULL == pSectBuffer)
    {
        __INFO("malloc %u error\n", size_sect);
        return -1;
    }

    // Read flash data
    if (FlashIsSpiNandFlash() > 0)
    {
        __INFO("spi nand flash");
        ret = read_sndata_by_file((char *)pSectBuffer, size_sect);
        if (0 != ret)
        {
            FREE_SECT_BUFFER();
            return -1;
        }
    }
    else
    {
        const char *mtd_block = GET_SN_MTD_BLOCK();
        unsigned int sect_number = platform_sn_sect_no_get();
        __INFO("spi nor flash on %s:%u \n", mtd_block, sect_number);
        ret = flash_sect_rw(0, mtd_block, size_sect, sect_number, (unsigned char *)pSectBuffer);
        if (0 != ret)
        {
            FREE_SECT_BUFFER();
            __INFO("sect %d: flash_read error\n", sect_number);
            return -1;
        }
    }

    do
    {
        // Try to read V1 save mechanism
        const char *uuid = get_uuid();
        char uuid_encrypt[128];
        copy_prevention_encrypt_v1(uuid, uuid_encrypt, sizeof(uuid_encrypt));

        for (iIndex = 0; iIndex < 4; iIndex++)
        {
            unsigned char *p = pSectBuffer + iIndex * 128;
            AjSnHeaderV1 *pHeader = (AjSnHeaderV1 *)p;

            if (MAGIC1_V1 != pHeader->magic1 ||
                MAGIC2_V1 != pHeader->magic2 ||
                MAGIC3_V1 != pHeader->magic3 ||
                MAGIC4_V1 != pHeader->magic4)
            {
                continue;
            }

            __INFO("%d: found V1 softsn", iIndex);

            if (pHeader->len < 32 || pHeader->len > 64)
            {
                __INFO("V1 len %d error\n", pHeader->len);
                continue;
            }

            unsigned int crc = GetCrcValue((char *)pHeader->data, pHeader->len);
            if (crc != pHeader->crc)
            {
                __INFO("V1 crc %#x != %#x\n", crc, pHeader->crc);
                continue;
            }

            char decrypted_sn[256] = {0};
            if (CheckSoftSNData_v1((const char *)pHeader->data, 32, (char *)decrypted_sn, sizeof(decrypted_sn), bShowInfo) < 0)
            {
                __ERR("%u: V1 SN check failed", iIndex);
                continue;
            }

            if (memcmp(uuid_encrypt, pHeader->uuid_check, 32) != 0)
            {
                char sn_str[32] = {0};
                memset(sn_str, 0, sizeof(sn_str));

                int i;
                for (i = 0; i < 8; i++)
                {
                    sprintf(sn_str + strlen(sn_str), "%02X", (unsigned char)decrypted_sn[i]);
                }
                __ERR("V1 uuid check failed, it's not for %s:%s\n", get_uuid(), sn_str);
                continue;
            }

            __INFO("%d: check V1 softsn OK", iIndex);
            FREE_SECT_BUFFER();

            int copylen = sizeof(decrypted_sn);
            if (buflen < copylen)
                copylen = buflen;
            memcpy(output_decrypt, decrypted_sn, copylen);

            g_SoftSnVersion = 1;
            return 0;
        }
    } while (0);

    // Try to read V2 save mechanism
    do
    {
        const char *uuid = get_uuid();
        char uuid_encrypt[128];
        copy_prevention_encrypt_v2(uuid, uuid_encrypt, sizeof(uuid_encrypt));

        for (iIndex = 0; iIndex < 2; iIndex++)
        {
            unsigned char *p = pSectBuffer + iIndex * 256;
            AjSnHeaderV2 *pHeader = (AjSnHeaderV2 *)p;

            if (MAGIC1_V2 != pHeader->magic1 ||
                MAGIC2_V2 != pHeader->magic2 ||
                MAGIC3_V2 != pHeader->magic3 ||
                MAGIC4_V2 != pHeader->magic4)
            {
                continue;
            }

            __INFO("%d: found V2 softsn \n", iIndex);

            if (pHeader->len < 32 || pHeader->len > 192)
            {
                __INFO("V2 len %d error\n", pHeader->len);
                continue;
            }

            unsigned int crc = GetCrcValue((char *)pHeader->data, pHeader->len);
            if (crc != pHeader->crc)
            {
                __INFO("V2 crc %#x != %#x\n", crc, pHeader->crc);
                continue;
            }

            char decrypted_sn[256] = {0};
            if (CheckSoftSNData_v2((char *)pHeader->data, pHeader->len, decrypted_sn, sizeof(decrypted_sn), bShowInfo) < 0)
            {
                __ERR("%u: V2 SN check failed", iIndex);
                continue;
            }
            if (memcmp(uuid_encrypt, pHeader->uuid_check, 32) != 0)
            {
                char sn_str[32] = {0};
                memset(sn_str, 0, sizeof(sn_str));

                int i;
                for (i = 0; i < 8; i++)
                {
                    sprintf(sn_str + strlen(sn_str), "%02X", (unsigned char)decrypted_sn[i]);
                }

                __ERR("V2 uuid check failed, it's not for %s:%s\n", get_uuid(), sn_str);
                continue;
            }

            __INFO("%d: check V2 softsn OK \n", iIndex);
            FREE_SECT_BUFFER();

            int copylen = sizeof(decrypted_sn);
            if (buflen < copylen)
                copylen = buflen;
            memcpy(output_decrypt, decrypted_sn, copylen);
            g_SoftSnVersion = 2;
            return 0;
        }
    } while (0);

    FREE_SECT_BUFFER();
    g_SoftSnVersion = -1;
    return -1;
}

int ReadEncriptDataFromSoft(unsigned char *output_decrypt, int buflen)
{
    return ReadEncriptDataFromSoft_inner(output_decrypt, buflen, 0);
}

int ReadEncriptDataFromSoft_ex(unsigned char *output_decrypt, int buflen)
{
    return ReadEncriptDataFromSoft_inner(output_decrypt, buflen, 1);
}

void ipc_softsn_set_status(unsigned int line, SoftSnV2Status data)
{
    const char *pStatusName[] = {
        "SOFTSN_V2_STATUS_INIT",          // Initial state
        "SOFTSN_V2_STATUS_DISCOVERY",     // UDP broadcast discovery
        "SOFTSN_V2_STATUS_SELECT_SERVER", // Select server
        "SOFTSN_V2_STATUS_TCP_CONNECT",   // TCP connection
        "SOFTSN_V2_STATUS_REQUEST_UUID",  // Request UUID
        "SOFTSN_V2_STATUS_REQUEST_SN",    // Request serial number
        "SOFTSN_V2_STATUS_OK"             // Authorization completed
    };

    __INFO("Set softsn status = %d [%s] (line %u)", data, pStatusName[data], line);
    gRequestStatus_v2 = data;
}

// Thread function
static int softsn_thread_v2(void *ctx, int *bStart)
{
    int tcp_sockfd = -1;
    int nRet;

    char szDeviceType[32];
    anj_sysmng_dev_str_get(szDeviceType);
    const char *my_uuid = get_uuid();

    char szRandomId[128] = {0};
    if( NULL == my_uuid || strlen(my_uuid) < 5)
    {
        int iIndex;
        // Generate random ID
        struct timeval tv;
        gettimeofday(&tv, NULL);
        int svalud = tv.tv_sec + tv.tv_usec;
        srand(svalud);
        for( iIndex = 0; iIndex < 32; iIndex++)
        {
            unsigned int data = rand()%0xff;
            sprintf(szRandomId+strlen(szRandomId), "%02X", data);
        }
    }
    else
    {
        strncpy(szRandomId, my_uuid, sizeof(szRandomId) - 1);
        szRandomId[sizeof(szRandomId) - 1] = '\0';
    }
    
    ipc_softsn_set_status(__LINE__, SOFTSN_V2_STATUS_DISCOVERY);
    
    char recv_buf[1024];
    char send_buf[1024];

    unsigned int tStartTime = GetCurrentTimeStamp();
    
    while(bStart && *bStart)
    {
        // Check total timeout
        if( GetCurrentTimeStamp() - tStartTime > SN_V2_TIMEOUT_TOTAL )
        {
            __INFO("total timeout, restart\n");
            ipc_softsn_set_status(__LINE__, SOFTSN_V2_STATUS_INIT);
            tStartTime = GetCurrentTimeStamp();
        }
        
        switch (gRequestStatus_v2)
        {
        case SOFTSN_V2_STATUS_INIT:
        {
            // Initial state
            if (NULL == my_uuid || strlen(my_uuid) < 5)
            {
                int iIndex;
                ipc_softsn_set_status(__LINE__, SOFTSN_V2_STATUS_DISCOVERY);
                // Generate random ID
                struct timeval tv;
                gettimeofday(&tv, NULL);
                int svalud = tv.tv_sec + tv.tv_usec;
                srand(svalud);
                for (iIndex = 0; iIndex < 32; iIndex++)
                {
                    unsigned int data = rand() % 0xff;
                    sprintf(szRandomId + strlen(szRandomId), "%02X", data);
                }
            }
            else
            {
                ipc_softsn_set_status(__LINE__, SOFTSN_V2_STATUS_DISCOVERY);
            }
            break;
        }

        case SOFTSN_V2_STATUS_DISCOVERY:
        {
            // UDP broadcast discovery
            EdgeServerInfo servers[10];
            int server_count = 0;

            __INFO("start UDP discovery\n");
            nRet = udp_discovery_server(szRandomId, servers, &server_count);
            if (nRet < 0 || server_count <= 0)
            {
                __INFO("UDP discovery failed, retry\n");
                SLEEP_SECOND(1);
                continue;
            }

            // Select server
            EdgeServerInfo selected_server;
            nRet = select_best_server(servers, server_count, &selected_server);
            if (nRet < 0)
            {
                __INFO("select server failed, retry\n");
                SLEEP_SECOND(1);
                continue;
            }

            // Enter TCP connect state
            ipc_softsn_set_status(__LINE__, SOFTSN_V2_STATUS_TCP_CONNECT);

            // Try to establish TCP connection
            __INFO("start TCP connect to %s:%d\n", selected_server.ip, selected_server.port);
            nRet = tcp_connect_server(&selected_server, &tcp_sockfd);
            if (nRet < 0)
            {
                __INFO("TCP connect failed, retry discovery\n");
                ipc_softsn_set_status(__LINE__, SOFTSN_V2_STATUS_DISCOVERY);
                SLEEP_SECOND(1);
                continue;
            }

            // TCP connect success, enter request state
            if (NULL == my_uuid || strlen(my_uuid) < 5)
            {
                ipc_softsn_set_status(__LINE__, SOFTSN_V2_STATUS_REQUEST_UUID);
            }
            else
            {
                ipc_softsn_set_status(__LINE__, SOFTSN_V2_STATUS_REQUEST_SN);
            }

            break;
        }

        case SOFTSN_V2_STATUS_REQUEST_UUID:
        {
            // Request UUID
            __INFO("request UUID\n");

            sprintf(send_buf,
                    "<ENCRYPT>\n"
                    "<MESSAGE_HEADER Msg_type=\"SYSTEM_ENCRYPTSN_MESSAGE\" Msg_code=\"%d\" Msg_flag=\"0\" />\n"
                    "<MESSAGE_BODY>\n"
                    "<DEVICE DeviceType=\"%s\" Identity=\"%s\"/>\n"
                    "</MESSAGE_BODY>\n"
                    "</ENCRYPT>\n",
                    IPC_MESSAGE_V2_REQUEST_UUID,
                    szDeviceType,
                    szRandomId);

            // Send message
            nRet = send_v2_message(tcp_sockfd, send_buf, strlen(send_buf));
            if (nRet < 0)
            {
                __INFO("send UUID request failed, retry\n");
                close(tcp_sockfd);
                tcp_sockfd = -1;
                ipc_softsn_set_status(__LINE__, SOFTSN_V2_STATUS_DISCOVERY);
                continue;
            }

            // Receive response
            int cmd = 0;
            int data_len = 0;
            nRet = recv_v2_message(tcp_sockfd, &cmd, recv_buf, &data_len);
            if (nRet < 0)
            {
                __INFO("recv UUID response failed, retry\n");
                close(tcp_sockfd);
                tcp_sockfd = -1;
                ipc_softsn_set_status(__LINE__, SOFTSN_V2_STATUS_DISCOVERY);
                continue;
            }

            // Handle response
            if (0 >= handle_cmd_uuid_response_v2(cmd, tcp_sockfd, recv_buf, send_buf, szRandomId))
            {
                __INFO("handle UUID response failed, retry\n");
                close(tcp_sockfd);
                tcp_sockfd = -1;
                ipc_softsn_set_status(__LINE__, SOFTSN_V2_STATUS_DISCOVERY);
                continue;
            }
            else
            {
                my_uuid = get_uuid();
                __INFO("get_uuid=%s\n", my_uuid);
                ipc_softsn_set_status(__LINE__, SOFTSN_V2_STATUS_REQUEST_SN);
            }
            break;
        }

        case SOFTSN_V2_STATUS_REQUEST_SN:
        {
            // Request software authorization
            __INFO("request SN\n");

            sprintf(send_buf,
                    "<ENCRYPT>\n"
                    "<MESSAGE_HEADER Msg_type=\"SYSTEM_ENCRYPTSN_MESSAGE\" Msg_code=\"%d\" Msg_flag=\"0\" />\n"
                    "<MESSAGE_BODY>\n"
                    "<DEVICE DeviceType=\"%s\" CameraID=\"%s\"/>\n"
                    "</MESSAGE_BODY>\n"
                    "</ENCRYPT>\n",
                    IPC_MESSAGE_V2_REQUEST_SN,
                    szDeviceType,
                    my_uuid);

            // Send message
            nRet = send_v2_message(tcp_sockfd, send_buf, strlen(send_buf));
            if (nRet < 0)
            {
                __INFO("send SN request failed, retry\n");
                close(tcp_sockfd);
                tcp_sockfd = -1;
                ipc_softsn_set_status(__LINE__, SOFTSN_V2_STATUS_DISCOVERY);
                continue;
            }

            // Receive response
            int cmd = 0;
            int data_len = 0;
            nRet = recv_v2_message(tcp_sockfd, &cmd, recv_buf, &data_len);
            if (nRet < 0)
            {
                __INFO("recv SN response failed, retry\n");
                close(tcp_sockfd);
                tcp_sockfd = -1;
                ipc_softsn_set_status(__LINE__, SOFTSN_V2_STATUS_DISCOVERY);
                continue;
            }

            // Handle response
            if (0 >= handle_cmd_sn_response_v2(cmd, tcp_sockfd, recv_buf, send_buf, my_uuid))
            {
                __INFO("handle SN response failed, retry\n");
                close(tcp_sockfd);
                tcp_sockfd = -1;
                ipc_softsn_set_status(__LINE__, SOFTSN_V2_STATUS_DISCOVERY);
                continue;
            }
            else
            {
                // Authorization success
                __INFO("SN request success\n");

                // Close TCP connection
                if (tcp_sockfd >= 0)
                {
                    close(tcp_sockfd);
                    tcp_sockfd = -1;
                }

                // Call callback function
                if (g_pFunc_v2 != NULL)
                {
                    g_pFunc_v2();
                }

                // Enter completed state
                ipc_softsn_set_status(__LINE__, SOFTSN_V2_STATUS_OK);

                // Exit thread
                goto THREAD_EXIT;
            }
            break;
        }

        case SOFTSN_V2_STATUS_OK:
        {
            // Authorization completed, exit thread
            goto THREAD_EXIT;
            break;
        }

        default:
        {
            // Invalid state, restart process
            ipc_softsn_set_status(__LINE__, SOFTSN_V2_STATUS_INIT);
            break;
        }
        }

        usleep(100000); // 100ms
    }

THREAD_EXIT:
    if( tcp_sockfd >= 0 ) {
        close(tcp_sockfd);
        tcp_sockfd = -1;
    }

    __INFO("softsn v2 exit thread\n");
    return 0;
}

int stop_softsn_thread_v2(void)
{
    __INFO("softsn v2 thread stop\n");
    anj_thread_task_destroy(&s_stSoftSnDataThreadV2, -1);
    return 0;
}

int start_softsn_thread_v2(void)
{
    int iRet = 0;
    if (s_stSoftSnDataThreadV2.start != 0)
    {
        __INFO("softsn v2 thread stop!\n");
        anj_thread_task_destroy(&s_stSoftSnDataThreadV2, -1);
        usleep(500 * 1000);
    }

    if (s_stSoftSnDataThreadV2.start == 0)
    {
        memset(&s_stSoftSnDataThreadV2, 0, sizeof(anj_thread_s));
        s_stSoftSnDataThreadV2.bAutoDestroy = 1;
        strncpy(s_stSoftSnDataThreadV2.iThreadName, "anj_softsn_thread_v2",
                sizeof(s_stSoftSnDataThreadV2.iThreadName) - 1);
        s_stSoftSnDataThreadV2.iThreadjob.ctx = &s_stSoftSnDataThreadV2;
        s_stSoftSnDataThreadV2.iThreadjob.func = softsn_thread_v2;
        iRet = anj_thread_task_create(&s_stSoftSnDataThreadV2);
    }

    return iRet;
}

int wait_softsn_thread_v2(void)
{
    while(s_stSoftSnDataThreadV2.end != 0)
    {
        usleep(1000);
    }

    return 0;
}

#define AUTH_METHOD_OLD 0

int stop_softsn_thread(void)
{
#if AUTH_METHOD_OLD
    return stop_softsn_thread_v1();
#else
    return stop_softsn_thread_v2();
#endif
}

// Start thread
int start_softsn_thread(void)
{
#if AUTH_METHOD_OLD
    return start_softsn_thread_v1();
#else
    return start_softsn_thread_v2();
#endif
}

// Wait for thread to end
int wait_softsn_thread(void)
{
#if AUTH_METHOD_OLD
    return wait_softsn_thread_v1();
#else
    return wait_softsn_thread_v2();
#endif
}

int ssn_get_version()
{
    return g_SoftSnVersion;
}
