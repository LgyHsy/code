
#include "mbedtls_config_for_email.h"

#include <stdio.h>
#include <stdlib.h>

#include "mbedtls/base64.h"
#include "mbedtls/error.h"
#include "mbedtls/net_sockets.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/certs.h"
#include "mbedtls/x509.h"

#include <stdlib.h>
#include <string.h>

#if !defined(_MSC_VER) || defined(EFIX64) || defined(EFI32)
#include <unistd.h>
#else
#include <io.h>
#endif

#if defined(_WIN32) || defined(_WIN32_WCE)
#include <winsock2.h>
#include <windows.h>

#if defined(_MSC_VER)
#if defined(_WIN32_WCE)
#pragma comment( lib, "ws2.lib" )
#else
#pragma comment( lib, "ws2_32.lib" )
#endif
#endif /* _MSC_VER */
#endif

//TODO:中文标题与内容支持：https://www.cnblogs.com/kanzume/p/4614518.html

#define MAX_ATTACHMENTS_NUM 16
#define MAX_BUF_BYTES 3990//该值需为MAX_BASE64_SRC_BYTES_EACH_LINE的倍数（使附件每行的base64值刚好编码，不出现==）
#define MAX_BUF_ENC_BYTES 8192
#define MAX_BASE64_SRC_BYTES_EACH_LINE 57//一行base64值所对应字节数量

/* ERR CODE*/
#define MAIL_SEND_SUCCESS	0
#define MAIL_SOCKET_ERR		-1//send data timeout, or server close socket
#define MAIL_AUTH_FAIL		-2//username or password not correct
#define MAIL_HANDSHAKE_FAIL	-3//mode not correct

/* ssl mode */
#define MODE_NO_SSL             -1//most stmp server at port 25.
#define MODE_SSL_TLS            0//port 465
#define MODE_STARTTLS           1//port 587


/* default valud of options*/
#define DFL_SERVER_NAME         "localhost"
#define DFL_SERVER_PORT         "465"
#define DFL_USER_NAME           "admin"
#define DFL_USER_PWD            "123456"
#define DFL_MAIL_FROM           ""
#define DFL_MAIL_TO             ""
#define DFL_MAIL_CC           	""
#define DFL_MAIL_BCC            ""
#define DFL_MAIL_SUBJECT        ""
#define DFL_MAIL_CONTENT        ""
#define DFL_CA_FILE             ""
#define DFL_CRT_FILE            ""
#define DFL_KEY_FILE            ""
#define DFL_FORCE_CIPHER        0
#define DFL_MODE                0
#define DFL_AUTHENTICATION      0

#define mbedtls_time            time
#define mbedtls_time_t          time_t
#define mbedtls_fprintf         fprintf
#define mbedtls_printf          printf

#if defined(MBEDTLS_BASE64_C)
#define USAGE_AUTH \
    "    authentication=%%d   default: 0 (disabled)\n"      \
    "    user_name=%%s        default: \"user\"\n"          \
    "    user_pwd=%%s         default: \"password\"\n"
#else
#error "MBEDTLS_BASE64_C must be defined."
#endif /* MBEDTLS_BASE64_C */

#if defined(MBEDTLS_FS_IO)
#define USAGE_IO \
    "    ca_file=%%s          default: \"\" (pre-loaded)\n" \
    "    crt_file=%%s         default: \"\" (pre-loaded)\n" \
    "    key_file=%%s         default: \"\" (pre-loaded)\n"
#else
#define USAGE_IO \
    "    No file operations available (MBEDTLS_FS_IO not defined)\n"
#endif /* MBEDTLS_FS_IO */

#define USAGE \
    "\n usage: mail param=<>...\n"               \
    "\n acceptable parameters:\n"                           \
    "    server_name=%%s      default: localhost\n"         \
    "    server_port=%%d      default: 4433\n"              \
    "    mode=%%d             default: -1(NO SSL) 0 (SSL/TLS) (1 for STARTTLS)\n"  \
    USAGE_AUTH                                              \
    "    mail_from=%%s        default: \"\"\n"              \
    "    mail_to=%%s          default: \"\"\n"              \
    "    mail_cc=%%s          default: \"\"(split with ','.example: sam@smtp.com,yoyo@smtp.com )\n"              \
    "    mail_BCC=%%s         default: \"\"(send to who secretly.)\n"              \
    "    mail_subject=%%s     default: \"\"\n"              \
    "    mail_content=%%s     default: \"\"\n"              \
	"    attachments=%%s      default: \"\"\n"              \
    USAGE_IO                                                \
    "    force_ciphersuite=<name>    default: all enabled\n"\
    " acceptable ciphersuite names:\n"
	
	
/*
 * global options
 */
struct options
{
    const char *server_name;    /* hostname of the smtp server      */
    const char *server_port;    /* port on which the smtp service runs      */
    int mode;                   /* no ssl (-1) , SSL/TLS (0) or STARTTLS (1)*/
    int authentication;         /* if authentication is required            */
    const char *user_name;      /* username to use for authentication       */
    const char *user_pwd;       /* password to use for authentication       */
    const char *mail_from;      /* E-Mail address to use as sender          */
    const char *mail_to;        /* E-Mail address to use as recipient       */
	const char *mail_cc;		/* E-Mail address copy to       			*/
	const char *mail_bcc;		/* E-Mail address send secretly      		*/
	const char *mail_subject;
	const char *mail_content;
	char *attachments[MAX_ATTACHMENTS_NUM];
	int attachments_cnt;
    const char *ca_file;        /* the file with the CA certificate(s)      */
    const char *crt_file;       /* the file with the client certificate     */
    const char *key_file;       /* the file with the client key             */
    int force_ciphersuite[2];   /* protocol/ciphersuite to use, or all      */
} opt;

static void my_debug( void *ctx, int level,
                      const char *file, int line,
                      const char *str )
{
    ((void) level);

    mbedtls_fprintf( (FILE *) ctx, "%s:%04d: %s", file, line, str );
    fflush(  (FILE *) ctx  );
}

static int do_handshake( mbedtls_ssl_context *ssl )
{
    int ret;
    uint32_t flags;
    unsigned char buf[1024];
    memset(buf, 0, 1024);

    /*
     * 4. Handshake
     */
    mbedtls_printf( "  . Performing the SSL/TLS handshake..." );
    fflush( stdout );

    while( ( ret = mbedtls_ssl_handshake( ssl ) ) != 0 )
    {
        if( ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE )
        {
#if defined(MBEDTLS_ERROR_C)
            mbedtls_strerror( ret, (char *) buf, 1024 );
#endif
            mbedtls_printf( " failed\n  ! mbedtls_ssl_handshake returned %d: %s\n\n", ret, buf );
            return( -1 );
        }
    }

    mbedtls_printf( " ok\n    [ Ciphersuite is %s ]\n",
            mbedtls_ssl_get_ciphersuite( ssl ) );

    /*
     * 5. Verify the server certificate
     */
    mbedtls_printf( "  . Verifying peer X.509 certificate..." );

    /* In real life, we probably want to bail out when ret != 0 */
    if( ( flags = mbedtls_ssl_get_verify_result( ssl ) ) != 0 )
    {
        char vrfy_buf[512];

        mbedtls_printf( " failed\n" );

        mbedtls_x509_crt_verify_info( vrfy_buf, sizeof( vrfy_buf ), "  ! ", flags );

        mbedtls_printf( "%s\n", vrfy_buf );
    }
    else
        mbedtls_printf( " ok\n" );

    mbedtls_printf( "  . Peer certificate information    ...\n" );
    mbedtls_x509_crt_info( (char *) buf, sizeof( buf ) - 1, "      ",
                   mbedtls_ssl_get_peer_cert( ssl ) );
    mbedtls_printf( "%s\n", buf );

    return( 0 );
}

static int write_ssl_data( mbedtls_ssl_context *ssl, unsigned char *buf, size_t len )
{
    int ret;

    //mbedtls_printf("\n%s", buf);
    while( len && ( ret = mbedtls_ssl_write( ssl, buf, len ) ) <= 0 )
    {
        if( ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE )
        {
            mbedtls_printf( " failed\n  ! mbedtls_ssl_write returned %d\n\n", ret );
            return -1;
        }
    }

    return( 0 );
}

static int ssl_wait_get_response( mbedtls_ssl_context *ssl, unsigned char *buf, size_t len )
{
    int ret;
	ret = mbedtls_ssl_read( ssl, buf, len );

	do
    {
		if( ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE )
			continue;
		if( ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY )
            return -1;
		break;
	}while( 1 );
    buf[len-1] = '\0';
	return 0;
}

static int write_ssl_and_get_response( mbedtls_ssl_context *ssl, unsigned char *buf, size_t len )
{
    int ret;
    unsigned char data[128];
    char code[4];
    size_t i, idx = 0;

    mbedtls_printf("\n%s", buf);
    while( len && ( ret = mbedtls_ssl_write( ssl, buf, len ) ) <= 0 )
    {
        if( ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE )
        {
            mbedtls_printf( " failed\n  ! mbedtls_ssl_write returned %d\n\n", ret );
            return -1;
        }
    }

    do
    {
        len = sizeof( data ) - 1;
        memset( data, 0, sizeof( data ) );
        ret = mbedtls_ssl_read( ssl, data, len );

        if( ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE )
            continue;

        if( ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY )
            return -1;

        if( ret <= 0 )
        {
            mbedtls_printf( "failed\n  ! mbedtls_ssl_read returned %d\n\n", ret );
            return -1;
        }

        mbedtls_printf("\n%s", data);
		strcpy((char *)buf, (char *)data);
        len = ret;
        for( i = 0; i < len; i++ )
        {
            if( data[i] != '\n' )
            {
                if( idx < 4 )
                    code[ idx++ ] = data[i];
                continue;
            }

            if( idx == 4 && code[0] >= '0' && code[0] <= '9' && code[3] == ' ' )
            {
                code[3] = '\0';
                return atoi( code );
            }

            idx = 0;
        }
    }
    while( 1 );
}

static int write_data( mbedtls_net_context *sock_fd, unsigned char *buf, size_t len )
{
    int ret;
	
    //mbedtls_printf("%s\n", buf);
    if( len && ( ret = mbedtls_net_send( sock_fd, buf, len ) ) <= 0 )
    {
        mbedtls_printf( " failed\n  ! mbedtls_net_send returned %d\n\n", ret );
            return -1;
    }
	return 0;
}

static int wait_get_response( mbedtls_net_context *sock_fd, unsigned char *buf, size_t len )
{
	int ret;
	mbedtls_printf( "recv buf len: %d\n", len );
	ret = mbedtls_net_recv( sock_fd, buf, len );
	if( ret <= 0 )
	{
		mbedtls_printf( "failed\n  ! read returned %d\n\n", ret );
		return -1;
	}
    buf[len-1] = '\0';
    mbedtls_printf("\n%s", buf);
	return 0;
}

static int write_and_get_response( mbedtls_net_context *sock_fd, unsigned char *buf, size_t len )
{
    int ret;
    unsigned char data[128];
    char code[4];
    size_t i, idx = 0;

    mbedtls_printf("\n%s", buf);
    if( len && ( ret = mbedtls_net_send( sock_fd, buf, len ) ) <= 0 )
    {
        mbedtls_printf( " failed\n  ! mbedtls_ssl_write returned %d\n\n", ret );
            return -1;
    }

    do
    {
        len = sizeof( data ) - 1;
        memset( data, 0, sizeof( data ) );
        ret = mbedtls_net_recv( sock_fd, data, len );

        if( ret <= 0 )
        {
            mbedtls_printf( "failed\n  ! read returned %d\n\n", ret );
            return -1;
        }

        data[len] = '\0';
        mbedtls_printf("\n%s", data);
		strcpy((char *)buf, (char *)data);
        len = ret;
        for( i = 0; i < len; i++ )
        {
            if( data[i] != '\n' )
            {
                if( idx < 4 )
                    code[ idx++ ] = data[i];
                continue;
            }

            if( idx == 4 && code[0] >= '0' && code[0] <= '9' && code[3] == ' ' )
            {
                code[3] = '\0';
                return atoi( code );
            }

            idx = 0;
        }
    }
    while( 1 );
}

#define WRITE_BLOCK \
	do{ \
		if(opt->mode == MODE_NO_SSL) \
		{ret = write_data( server_fd, buf, len );} \
		else \
		{ret = write_ssl_data( ssl, buf, len );} \
		if(ret != 0) \
			{return -1;} \
	}while(0);
	

int write_mail_DATA(options *opt, mbedtls_ssl_context *ssl, mbedtls_net_context *server_fd)
{
	int len, ret;
	int err_occur = 0;
	size_t n;
	unsigned char buf[MAX_BUF_BYTES];
	unsigned char buf_encoded[MAX_BUF_ENC_BYTES];
	unsigned char subject_b[512];
	
	ret = mbedtls_base64_encode( subject_b, sizeof( subject_b ), &n,
		(const unsigned char *) opt->mail_subject,
		 strlen(opt->mail_subject) );
	if( ret != 0 ) {
		mbedtls_printf( " failed\n  ! mbedtls_base64_encode subject returned %d\n\n", ret );
		return -1;
	}
	
	//mbedtls_printf( " ready write mail content.\n" );
	
	/* write mail header*/
	len = sprintf( (char *) buf, 
			"TO: %s\r\n"
			"From: %s\r\n"
			"Cc: %s\r\n"
			"BCC: %s\r\n"
			"Subject: =?utf-8?B?%s?=\r\n"
            //"X-Mailer: Anj Mailer V1.0 build\r\n"//don't add X-Mailer, 163邮箱无法发出。。
			//"X-Priority: 3\r\n"
			"MIME-Version: 1.0\r\n"
			"Content-type: multipart/mixed;\r\n    boundary=\"=====000_Dragon=====\"\r\n\r\n"
			"This is a multi-part message in MIME format.\r\n",
			opt->mail_to, opt->mail_from , opt->mail_cc, opt->mail_bcc, (const char *)subject_b);
	//mbedtls_printf( " send mail header: %s\n\n", buf );
	WRITE_BLOCK
	
	/* write mail plain content*/
	
	len = sprintf( (char *) buf,
			"--=====000_Dragon=====\r\n"
			"Content-Type: text/plain; charset=\"UTF-8\"\r\n"
			"Content-Transfer-Encoding: quoted-printable\r\n\r\n"
			"%s\r\n",
			opt->mail_content);
	WRITE_BLOCK
	
	/* write mail attachments*/
	
	for(int j = 0; j < opt->attachments_cnt; ++j)
	{
		char *file_path = opt->attachments[j];
		if(file_path == NULL)
		{
			break;
		}
		
		if(access(file_path, F_OK) != 0)
		{
			mbedtls_printf( "%s not exist\n", file_path );
			break;
		}
		mbedtls_printf( "begin send: %s mode:%d\n", file_path, opt->mode );
		
		char *file_name = NULL;
		file_name = strrchr(file_path, '/');
		if(file_name == NULL)
		{
			file_name = file_path;
		}
		else
		{
			file_name++;
		}
		
		len = sprintf( (char *) buf,
			"\r\n--=====000_Dragon=====\r\n"
			"Content-Type: application/octet-stream;\r\n    name=%s\r\n"
			"Content-Transfer-Encoding: base64\r\n"
			"Content-Disposition: attachment;\r\n    filename=\"%s\"\r\n"
			"\r\n",
			file_name, file_name);
		WRITE_BLOCK
		
		FILE *fp = NULL;
		fp = fopen(file_path, "r");
		if (NULL != fp)
		{
			fseek(fp , 0L , SEEK_END) ;
			int file_len = ftell(fp) ;
			fseek(fp , 0L , SEEK_SET) ;
			int block_cnt = file_len/MAX_BUF_BYTES + 1;
			for(int i = 0; i < block_cnt; ++i)
			{
				int read_bytes = MAX_BUF_BYTES;
				if( i == block_cnt - 1)
				{
					read_bytes = file_len - i * MAX_BUF_BYTES;
					if(read_bytes == 0)
						continue;
				}
				//mbedtls_printf("read_bytes:%d\n", read_bytes);
				if(fread(buf, read_bytes, 1, fp) == 1)
				{
					//split by MAX_BASE64_SRC_BYTES_EACH_LINE bytes
					int split_cnt = read_bytes/MAX_BASE64_SRC_BYTES_EACH_LINE + 1;
					for(int m = 0; m < split_cnt; ++m)
					{
						int to_enc = MAX_BASE64_SRC_BYTES_EACH_LINE;
						if( m == split_cnt - 1)
						{
							to_enc = read_bytes - m * MAX_BASE64_SRC_BYTES_EACH_LINE;
							if(to_enc == 0)
								continue;
						}
						//mbedtls_printf("to_enc %d\n", to_enc);
						unsigned char *pos = buf + m * MAX_BASE64_SRC_BYTES_EACH_LINE;
						ret = mbedtls_base64_encode( buf_encoded, sizeof( buf_encoded ), &n,
							(const unsigned char *) pos,
                             to_enc );
						if( ret != 0 ) {
							mbedtls_printf( " failed\n  ! mbedtls_base64_encode returned %d\n\n", ret );
							err_occur = 1;
							break;
						}
						
						strcpy((char *)&buf_encoded[n], "\r\n");
						if(opt->mode == MODE_NO_SSL)
							ret = write_data( server_fd, buf_encoded, n + 2 );
						else
							ret = write_ssl_data( ssl, buf_encoded, n + 2 );
						if(ret != 0)
						{
							err_occur = 1;
							break;
						}
						
					}
					
					if(err_occur)
						break;
				}
				else
				{
					mbedtls_printf( " failed read file not reach end?\n");
					break;
				}
			}
		}
		else
		{
			mbedtls_printf( "%s open fail\n", file_path );
		}
		fclose(fp);
		
		if(err_occur)
			break;
	}
	
	len = sprintf( (char *) buf,
	"\r\n--=====000_Dragon=====--\r\n\r\n");
	WRITE_BLOCK
				
	if(err_occur)
		return -1;
	return 0;	
}

int send_mail_nossl(options opt)
{
	int exit_code = MAIL_SOCKET_ERR;
	int ret, len;
	size_t  n;
    char hostname[64];
	
    unsigned char base[1024];
    unsigned char buf[sizeof( base ) + 2];
    memset( &buf, 0, sizeof( buf ) );
	
    mbedtls_net_context server_fd;
	mbedtls_net_init( &server_fd );
	
	mbedtls_printf( "  .No ssl Connecting to tcp/%s/%s...", opt.server_name,
                                                opt.server_port );
    fflush( stdout );
	if( ( ret = mbedtls_net_connect( &server_fd, opt.server_name,
                             opt.server_port, MBEDTLS_NET_PROTO_TCP ) ) != 0 )
    {
        mbedtls_printf( " failed\n  ! mbedtls_net_connect returned %d\n\n", ret );
        goto exit;
    }
	
	mbedtls_printf( "  > Write EHLO to server:" );
    fflush( stdout );
	sprintf(hostname, "localhost");
	len = sprintf( (char *) buf, "EHLO %s\r\n", hostname );
    ret = write_and_get_response( &server_fd, buf, len );
	if( ret < 200 || ret > 299 )
	{
		mbedtls_printf( " failed\n  ! server responded with %d\n\n", ret );
		goto exit;
	}
	
	if( opt.authentication )
    {
        mbedtls_printf( "  > Write AUTH LOGIN to server:" );
        fflush( stdout );

        len = sprintf( (char *) buf, "AUTH LOGIN\r\n" );
        ret = write_and_get_response( &server_fd, buf, len );
        while(1){
			if(strstr((char *)buf, "Nlcm5hbWU6") != NULL)//VXNlcm5hbWU6 is base64 of "Username:",部分服务器U大写
			{
				mbedtls_printf( "  > got login request" );
				break;
			}
			//TODO recv
			if(wait_get_response(&server_fd, buf, sizeof(buf)) < 0)
				goto exit;
			mbedtls_printf("buf:%s\n" , buf);
		}

        mbedtls_printf(" ok\n" );

        mbedtls_printf( "  > Write username to server: %s", opt.user_name );
        fflush( stdout );

        ret = mbedtls_base64_encode( base, sizeof( base ), &n, (const unsigned char *) opt.user_name,
                             strlen( opt.user_name ) );

        if( ret != 0 ) {
            mbedtls_printf( " failed\n  ! mbedtls_base64_encode returned %d\n\n", ret );
            goto exit;
        }
        len = sprintf( (char *) buf, "%s\r\n", base );
        ret = write_and_get_response( &server_fd, buf, len  );
        if( ret < 300 || ret > 399 )
        {
            mbedtls_printf( " failed\n  ! server responded with %d\n\n", ret );
			exit_code = MAIL_AUTH_FAIL;
            goto exit;
        }

        mbedtls_printf(" ok\n" );

        mbedtls_printf( "  > Write password to server: %s", opt.user_pwd );
        fflush( stdout );

        ret = mbedtls_base64_encode( base, sizeof( base ), &n, (const unsigned char *) opt.user_pwd,
                             strlen( opt.user_pwd ) );

        if( ret != 0 ) {
            mbedtls_printf( " failed\n  ! mbedtls_base64_encode returned %d\n\n", ret );
            goto exit;
        }
        len = sprintf( (char *) buf, "%s\r\n", base );
        ret = write_and_get_response( &server_fd, buf, len );
        if( ret < 200 || ret > 399 )
        {
            mbedtls_printf( " failed\n  ! server responded with %d\n\n", ret );
			exit_code = MAIL_AUTH_FAIL;
            goto exit;
        }

        mbedtls_printf(" ok\n" );
    }
	
	
	mbedtls_printf( "  > Write MAIL FROM to server:" );
    fflush( stdout );

    len = sprintf( (char *) buf, "MAIL FROM:<%s>\r\n", opt.mail_from );
    ret = write_and_get_response( &server_fd, buf, len );
    if( ret < 200 || ret > 299 )
    {
        mbedtls_printf( " failed\n  ! server responded with %d\n\n", ret );
        goto exit;
    }

    mbedtls_printf(" ok\n" );

    mbedtls_printf( "  > Write RCPT TO to server:" );
    fflush( stdout );

    len = sprintf( (char *) buf, "RCPT TO:<%s>\r\n", opt.mail_to );
    ret = write_and_get_response( &server_fd, buf, len );
    if( ret < 200 || ret > 299 )
    {
        mbedtls_printf( " failed\n  ! server responded with %d\n\n", ret );
        goto exit;
    }
	
	if(strlen(opt.mail_cc) > 0)
	{
		len = sprintf( (char *) buf, "RCPT TO:<%s>\r\n", opt.mail_cc );
		ret = write_and_get_response( &server_fd, buf, len );
		if( ret < 200 || ret > 299 )
		{
			mbedtls_printf( " failed\n mail cc can not rcpt to ! server responded with %d\n\n", ret );
			//goto exit;
		}
	}
	
	if(strlen(opt.mail_bcc) > 0)
	{
		len = sprintf( (char *) buf, "RCPT TO:<%s>\r\n", opt.mail_bcc );
		ret = write_and_get_response( &server_fd, buf, len );
		if( ret < 200 || ret > 299 )
		{
			mbedtls_printf( " failed\n mail bcc can not rcpt to! server responded with %d\n\n", ret );
			//goto exit;
		}
	}
	
    mbedtls_printf(" ok\n" );

    mbedtls_printf( "  > Write DATA to server:" );
    fflush( stdout );

    len = sprintf( (char *) buf, "DATA\r\n" );
    ret = write_and_get_response( &server_fd, buf, len );
    if( ret < 200 || ret > 399 )//normal tip: 354 End data with <CR><LF>.<CR><LF>
    {
        mbedtls_printf( " failed\n  ! server responded with %d\n\n", ret );
        goto exit;
    }
	
    mbedtls_printf( "  > Write content to server:\n" );
    fflush( stdout );

    ret = write_mail_DATA(&opt, NULL, &server_fd);
	if(ret != 0)
		mbedtls_printf( " failed\n  ! write_mail_DATA: %d\n\n", ret );

    len = sprintf( (char *) buf, "\r\n.\r\n");
    ret = write_and_get_response( &server_fd, buf, len );
    if( ret < 200 || ret > 299 )//250 Ok: queued as xxx
    {
        mbedtls_printf( " failed\n  ! server responded with %d\n\n", ret );
        goto exit;
    }
	
	exit_code = MAIL_SEND_SUCCESS;
exit:
	len = sprintf( (char *) buf, "QUIT\r\n");
    ret = write_data(&server_fd, buf, len);
	

    mbedtls_net_free( &server_fd );
	return( exit_code );
}

int send_mail_ssl(options opt)
{
    size_t n;
	int ret = 1, len;
    int exit_code = MAIL_SOCKET_ERR;
    mbedtls_net_context server_fd;

    unsigned char base[1024];
    unsigned char buf[sizeof( base ) + 2];
	
    char hostname[64];
    const char *pers = "ssl_mail_client";

    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    mbedtls_x509_crt cacert;
    mbedtls_x509_crt clicert;
    mbedtls_pk_context pkey;
    /*
     * Make sure memory references are valid in case we exit early.
     */
    mbedtls_net_init( &server_fd );
    mbedtls_ssl_init( &ssl );
    mbedtls_ssl_config_init( &conf );
    memset( &buf, 0, sizeof( buf ) );
    mbedtls_x509_crt_init( &cacert );
    mbedtls_x509_crt_init( &clicert );
    mbedtls_pk_init( &pkey );
    mbedtls_ctr_drbg_init( &ctr_drbg );

	/*
     * 0. Initialize the RNG and the session data
     */
    mbedtls_printf( "\n  . Seeding the random number generator..." );
    fflush( stdout );

    mbedtls_entropy_init( &entropy );
    if( ( ret = mbedtls_ctr_drbg_seed( &ctr_drbg, mbedtls_entropy_func, &entropy,
                               (const unsigned char *) pers,
                               strlen( pers ) ) ) != 0 )
    {
        mbedtls_printf( " failed\n  ! mbedtls_ctr_drbg_seed returned %d\n", ret );
        goto exit;
    }

    mbedtls_printf( " ok\n" );

    /*
     * 1.1. Load the trusted CA
     */
    mbedtls_printf( "  . Loading the CA root certificate ..." );
    fflush( stdout );

#if defined(MBEDTLS_FS_IO)
    if( strlen( opt.ca_file ) )
        ret = mbedtls_x509_crt_parse_file( &cacert, opt.ca_file );
    else
#endif
#if defined(MBEDTLS_CERTS_C) && defined(MBEDTLS_PEM_PARSE_C)
        ret = mbedtls_x509_crt_parse( &cacert, (const unsigned char *) mbedtls_test_cas_pem,
                              mbedtls_test_cas_pem_len );
#else
    {
        mbedtls_printf("MBEDTLS_CERTS_C and/or MBEDTLS_PEM_PARSE_C not defined.");
        goto exit;
    }
#endif
    if( ret < 0 )
    {
        mbedtls_printf( " failed\n  !  mbedtls_x509_crt_parse returned %d\n\n", ret );
        goto exit;
    }

    mbedtls_printf( " ok (%d skipped)\n", ret );

    /*
     * 1.2. Load own certificate and private key
     *
     * (can be skipped if client authentication is not required)
     */
    mbedtls_printf( "  . Loading the client cert. and key..." );
    fflush( stdout );

#if defined(MBEDTLS_FS_IO)
    if( strlen( opt.crt_file ) )
        ret = mbedtls_x509_crt_parse_file( &clicert, opt.crt_file );
    else
#endif
#if defined(MBEDTLS_CERTS_C)
        ret = mbedtls_x509_crt_parse( &clicert, (const unsigned char *) mbedtls_test_cli_crt,
                              mbedtls_test_cli_crt_len );
#else
    {
        mbedtls_printf("MBEDTLS_CERTS_C not defined.");
        goto exit;
    }
#endif
    if( ret != 0 )
    {
        mbedtls_printf( " failed\n  !  mbedtls_x509_crt_parse returned %d\n\n", ret );
        goto exit;
    }

#if defined(MBEDTLS_FS_IO)
    if( strlen( opt.key_file ) )
        ret = mbedtls_pk_parse_keyfile( &pkey, opt.key_file, "" );
    else
#endif
#if defined(MBEDTLS_CERTS_C) && defined(MBEDTLS_PEM_PARSE_C)
        ret = mbedtls_pk_parse_key( &pkey, (const unsigned char *) mbedtls_test_cli_key,
                mbedtls_test_cli_key_len, NULL, 0 );
#else
    {
        mbedtls_printf("MBEDTLS_CERTS_C or MBEDTLS_PEM_PARSE_C not defined.");
        goto exit;
    }
#endif
    if( ret != 0 )
    {
        mbedtls_printf( " failed\n  !  mbedtls_pk_parse_key returned %d\n\n", ret );
        goto exit;
    }

    mbedtls_printf( " ok\n" );

    /*
     * 2. Start the connection
     */
    mbedtls_printf( "  . Connecting to tcp/%s/%s...", opt.server_name,
                                                opt.server_port );
    fflush( stdout );

    if( ( ret = mbedtls_net_connect( &server_fd, opt.server_name,
                             opt.server_port, MBEDTLS_NET_PROTO_TCP ) ) != 0 )
    {
        mbedtls_printf( " failed\n  ! mbedtls_net_connect returned %d\n\n", ret );
        goto exit;
    }

    mbedtls_printf( " ok\n" );

    /*
     * 3. Setup stuff
     */
    mbedtls_printf( "  . Setting up the SSL/TLS structure..." );
    fflush( stdout );

    if( ( ret = mbedtls_ssl_config_defaults( &conf,
                    MBEDTLS_SSL_IS_CLIENT,
                    MBEDTLS_SSL_TRANSPORT_STREAM,
                    MBEDTLS_SSL_PRESET_DEFAULT ) ) != 0 )
    {
        mbedtls_printf( " failed\n  ! mbedtls_ssl_config_defaults returned %d\n\n", ret );
        goto exit;
    }

    /* OPTIONAL is not optimal for security,
     * but makes interop easier in this simplified example */
    mbedtls_ssl_conf_authmode( &conf, MBEDTLS_SSL_VERIFY_OPTIONAL );

    mbedtls_ssl_conf_rng( &conf, mbedtls_ctr_drbg_random, &ctr_drbg );
    mbedtls_ssl_conf_dbg( &conf, my_debug, stdout );

    if( opt.force_ciphersuite[0] != DFL_FORCE_CIPHER )
        mbedtls_ssl_conf_ciphersuites( &conf, opt.force_ciphersuite );

    mbedtls_ssl_conf_ca_chain( &conf, &cacert, NULL );
    if( ( ret = mbedtls_ssl_conf_own_cert( &conf, &clicert, &pkey ) ) != 0 )
    {
        mbedtls_printf( " failed\n  ! mbedtls_ssl_conf_own_cert returned %d\n\n", ret );
        goto exit;
    }

    if( ( ret = mbedtls_ssl_setup( &ssl, &conf ) ) != 0 )
    {
        mbedtls_printf( " failed\n  ! mbedtls_ssl_setup returned %d\n\n", ret );
        goto exit;
    }

    if( ( ret = mbedtls_ssl_set_hostname( &ssl, opt.server_name ) ) != 0 )
    {
        mbedtls_printf( " failed\n  ! mbedtls_ssl_set_hostname returned %d\n\n", ret );
        goto exit;
    }

    mbedtls_ssl_set_bio( &ssl, &server_fd, mbedtls_net_send, mbedtls_net_recv, NULL );

    mbedtls_printf( " ok\n" );

    if( opt.mode == MODE_SSL_TLS )
    {
        if( do_handshake( &ssl ) != 0 )
		{
			exit_code = MAIL_HANDSHAKE_FAIL;
			goto exit;
		}

		mbedtls_printf( "  > Get header from server:" );
		fflush( stdout );

		ret = write_ssl_and_get_response( &ssl, buf, 0 );
		if( ret < 200 || ret > 299 )
		{
			mbedtls_printf( " failed\n  ! server responded with %d\n\n", ret );
			goto exit;
		}

		mbedtls_printf(" ok\n" );
        
        mbedtls_printf( "  > Write EHLO to server:" );
        fflush( stdout );

        sprintf(hostname, "localhost");
        len = sprintf( (char *) buf, "EHLO %s\r\n", hostname );
        ret = write_ssl_and_get_response( &ssl, buf, len );
        if( ret < 200 || ret > 299 )
        {
            mbedtls_printf( " failed\n  ! server responded with %d\n\n", ret );
            goto exit;
        }
    }
    else
    {
		mbedtls_printf( "  > Get header from server:" );
		fflush( stdout );

		ret = write_and_get_response( &server_fd, buf, 0 );
		if( ret < 200 || ret > 299 )
		{
			mbedtls_printf( " failed\n  ! server responded with %d\n\n", ret );
			goto exit;
		}

		mbedtls_printf(" ok\n" );
        
        mbedtls_printf( "  > Write EHLO to server:" );
        fflush( stdout );

        sprintf(hostname, "localhost");
        len = sprintf( (char *) buf, "EHLO %s\r\n", hostname );
        ret = write_and_get_response( &server_fd, buf, len );
        if( ret < 200 || ret > 299 )
        {
            mbedtls_printf( " failed\n  ! server responded with %d\n\n", ret );
            goto exit;
        }

        mbedtls_printf(" ok\n" );

        mbedtls_printf( "  > Write STARTTLS to server:" );
        fflush( stdout );

        sprintf(hostname, "localhost");
        len = sprintf( (char *) buf, "STARTTLS\r\n" );
        ret = write_and_get_response( &server_fd, buf, len );
        if( ret < 200 || ret > 299 )
        {
            mbedtls_printf( " failed\n  ! server responded with %d\n\n", ret );
            goto exit;
        }

        mbedtls_printf(" ok\n" );

        if( do_handshake( &ssl ) != 0 )
		{
			exit_code = MAIL_HANDSHAKE_FAIL;
			goto exit;
		}
		
		mbedtls_printf( "  > Write EHLO to server again:" );
        fflush( stdout );

        sprintf(hostname, "localhost");
        len = sprintf( (char *) buf, "EHLO %s\r\n", hostname );
        ret = write_ssl_and_get_response( &ssl, buf, len );
        if( ret < 200 || ret > 299 )
        {
            mbedtls_printf( " failed\n  ! server responded with %d\n\n", ret );
            goto exit;
        }
    }

#if defined(MBEDTLS_BASE64_C)
    if( opt.authentication )
    {
        mbedtls_printf( "  > Write AUTH LOGIN to server:" );
        fflush( stdout );

        len = sprintf( (char *) buf, "AUTH LOGIN\r\n" );
        ret = write_ssl_and_get_response( &ssl, buf, len );
        while(1){
			mbedtls_printf("buf:%s\n" , buf);
			if(strstr((char *)buf, "Nlcm5hbWU6") != NULL)//VXNlcm5hbWU6 is base64 of "Username:",部分服务器U大写
			{
				mbedtls_printf( "  > got login request" );
				break;
			}
			//TODO recv
			if(ssl_wait_get_response(&ssl, buf, sizeof(buf)) < 0)
				goto exit;
		}

        mbedtls_printf(" ok\n" );

        mbedtls_printf( "  > Write username to server: %s", opt.user_name );
        fflush( stdout );

        ret = mbedtls_base64_encode( base, sizeof( base ), &n, (const unsigned char *) opt.user_name,
                             strlen( opt.user_name ) );

        if( ret != 0 ) {
            mbedtls_printf( " failed\n  ! mbedtls_base64_encode returned %d\n\n", ret );
            goto exit;
        }
        len = sprintf( (char *) buf, "%s\r\n", base );
        ret = write_ssl_and_get_response( &ssl, buf, len );
        if( ret < 300 || ret > 399 )
        {
            mbedtls_printf( " failed\n  ! server responded with %d\n\n", ret );
			exit_code = MAIL_AUTH_FAIL;
            goto exit;
        }

        mbedtls_printf(" ok\n" );

        mbedtls_printf( "  > Write password to server: %s", opt.user_pwd );
        fflush( stdout );

        ret = mbedtls_base64_encode( base, sizeof( base ), &n, (const unsigned char *) opt.user_pwd,
                             strlen( opt.user_pwd ) );

        if( ret != 0 ) {
            mbedtls_printf( " failed\n  ! mbedtls_base64_encode returned %d\n\n", ret );
            goto exit;
        }
        len = sprintf( (char *) buf, "%s\r\n", base );
        ret = write_ssl_and_get_response( &ssl, buf, len );
        if( ret < 200 || ret > 399 )
        {
            mbedtls_printf( " failed\n  ! server responded with %d\n\n", ret );
			exit_code = MAIL_AUTH_FAIL;
            goto exit;
        }

        mbedtls_printf(" ok\n" );
    }
#endif

    mbedtls_printf( "  > Write MAIL FROM to server:" );
    fflush( stdout );

    len = sprintf( (char *) buf, "MAIL FROM:<%s>\r\n", opt.mail_from );
    ret = write_ssl_and_get_response( &ssl, buf, len );
    if( ret < 200 || ret > 299 )
    {
        mbedtls_printf( " failed\n  ! server responded with %d\n\n", ret );
        goto exit;
    }

    mbedtls_printf(" ok\n" );

    mbedtls_printf( "  > Write RCPT TO to server:" );
    fflush( stdout );

    len = sprintf( (char *) buf, "RCPT TO:<%s>\r\n", opt.mail_to );
    ret = write_ssl_and_get_response( &ssl, buf, len );
    if( ret < 200 || ret > 299 )
    {
        mbedtls_printf( " failed\n  ! server responded with %d\n\n", ret );
        goto exit;
    }
	
	if(strlen(opt.mail_cc) > 0)
	{
		len = sprintf( (char *) buf, "RCPT TO:<%s>\r\n", opt.mail_cc );
		ret = write_ssl_and_get_response( &ssl, buf, len );
		if( ret < 200 || ret > 299 )
		{
			mbedtls_printf( " failed\n mail cc can not rcpt to ! server responded with %d\n\n", ret );
			//goto exit;
		}
	}
	
	if(strlen(opt.mail_bcc) > 0)
	{
		len = sprintf( (char *) buf, "RCPT TO:<%s>\r\n", opt.mail_bcc );
		ret = write_ssl_and_get_response( &ssl, buf, len );
		if( ret < 200 || ret > 299 )
		{
			mbedtls_printf( " failed\n mail bcc can not rcpt to! server responded with %d\n\n", ret );
			//goto exit;
		}
	}

    mbedtls_printf(" ok\n" );

    mbedtls_printf( "  > Write DATA to server:" );
    fflush( stdout );

    len = sprintf( (char *) buf, "DATA\r\n" );
    ret = write_ssl_and_get_response( &ssl, buf, len );
    if( ret < 200 || ret > 399 )
    {
        mbedtls_printf( " failed\n  ! server responded with %d\n\n", ret );
        goto exit;
    }
	
    mbedtls_printf( "  > Write content to server:\n" );
    fflush( stdout );

	ret = write_mail_DATA(&opt, &ssl, &server_fd);
	if(ret != 0)
		mbedtls_printf( " failed\n  ! write_mail_DATA: %d\n\n", ret );

    len = sprintf( (char *) buf, "\r\n.\r\n");
    ret = write_ssl_and_get_response( &ssl, buf, len );
    if( ret < 200 || ret > 399 )//250 ok, queue as
    {
        mbedtls_printf( " failed\n  ! server responded with %d\n\n", ret );
        goto exit;
    }
	
    exit_code = MAIL_SEND_SUCCESS;

	
exit:
	len = sprintf( (char *) buf, "QUIT\r\n");
    ret = write_ssl_data(&ssl, buf, len);

    mbedtls_ssl_close_notify( &ssl );

    mbedtls_net_free( &server_fd );
    mbedtls_x509_crt_free( &clicert );
    mbedtls_x509_crt_free( &cacert );
    mbedtls_pk_free( &pkey );
    mbedtls_ssl_free( &ssl );
    mbedtls_ssl_config_free( &conf );
    mbedtls_ctr_drbg_free( &ctr_drbg );
    mbedtls_entropy_free( &entropy );

    return( exit_code );
}

int main( int argc, char *argv[] )
{
    int i;
    char *p, *q;
	char *files = NULL;
    const int *list;

    if( argc == 1 )
    {
    usage:
        mbedtls_printf( USAGE );

        list = mbedtls_ssl_list_ciphersuites();
        while( *list )
        {
            mbedtls_printf("    %s\n", mbedtls_ssl_get_ciphersuite_name( *list ) );
            list++;
        }
        mbedtls_printf("\n");
		return -9999;
    }

	memset(&opt, 0, sizeof(options));
    opt.server_name         = DFL_SERVER_NAME;
    opt.server_port         = DFL_SERVER_PORT;
    opt.authentication      = DFL_AUTHENTICATION;
    opt.mode                = DFL_MODE;
    opt.user_name           = DFL_USER_NAME;
    opt.user_pwd            = DFL_USER_PWD;
    opt.mail_from           = DFL_MAIL_FROM;
    opt.mail_to             = DFL_MAIL_TO;
	opt.mail_cc				= DFL_MAIL_CC;
	opt.mail_bcc			= DFL_MAIL_BCC;
	opt.mail_subject		= DFL_MAIL_SUBJECT;
	opt.mail_content		= DFL_MAIL_CONTENT;
    opt.ca_file             = DFL_CA_FILE;
    opt.crt_file            = DFL_CRT_FILE;
    opt.key_file            = DFL_KEY_FILE;
    opt.force_ciphersuite[0]= DFL_FORCE_CIPHER;

    for( i = 1; i < argc; i++ )
    {
        p = argv[i];
        if( ( q = strchr( p, '=' ) ) == NULL )
            goto usage;
        *q++ = '\0';

        if( strcmp( p, "server_name" ) == 0 )
            opt.server_name = q;
        else if( strcmp( p, "server_port" ) == 0 )
            opt.server_port = q;
        else if( strcmp( p, "mode" ) == 0 )
        {
            opt.mode = atoi( q );
            if( opt.mode < -1 || opt.mode > 1 )
                goto usage;
        }
        else if( strcmp( p, "authentication" ) == 0 )
        {
            opt.authentication = atoi( q );
            if( opt.authentication < 0 || opt.authentication > 1 )
                goto usage;
        }
        else if( strcmp( p, "user_name" ) == 0 )
            opt.user_name = q;
        else if( strcmp( p, "user_pwd" ) == 0 )
            opt.user_pwd = q;
        else if( strcmp( p, "mail_from" ) == 0 )
            opt.mail_from = q;
        else if( strcmp( p, "mail_to" ) == 0 )
            opt.mail_to = q;
		else if( strcmp( p, "mail_cc" ) == 0 )
            opt.mail_cc = q;
		else if( strcmp( p, "mail_bcc" ) == 0 )
            opt.mail_bcc = q;
		else if( strcmp( p, "mail_subject" ) == 0 )
            opt.mail_subject = q;
		else if( strcmp( p, "mail_content" ) == 0 )
            opt.mail_content = q;
		else if( strcmp( p, "attachments" ) == 0 )
        {
			files = q;
        }
        else if( strcmp( p, "ca_file" ) == 0 )
            opt.ca_file = q;
        else if( strcmp( p, "crt_file" ) == 0 )
            opt.crt_file = q;
        else if( strcmp( p, "key_file" ) == 0 )
            opt.key_file = q;
        else if( strcmp( p, "force_ciphersuite" ) == 0 )
        {
            opt.force_ciphersuite[0] = -1;

            opt.force_ciphersuite[0] = mbedtls_ssl_get_ciphersuite_id( q );

            if( opt.force_ciphersuite[0] <= 0 )
                goto usage;

            opt.force_ciphersuite[1] = 0;
        }
        else
            goto usage;
    }
	
	//split files by ','
	if(files != NULL)
	{
		int cnt = 0;
		p = q = files;
		do{
			q = strchr(p, ',');
			if(q == NULL)
			{
				opt.attachments[cnt++] = p;
				break;
			}
			else
			{
				*q = '\0';
				q++;
				opt.attachments[cnt++] = p;
				p = q;
			}
		}while(1);
		opt.attachments_cnt = cnt;
		
		mbedtls_printf( " found attachments:\n" );
		for(int i = 0; i < cnt; ++i)
		{
			mbedtls_printf( " -- %s\n", opt.attachments[i]);
		}
	}
	
	if(opt.mode == MODE_NO_SSL)
	{
		return send_mail_nossl(opt);
	}
	else
	{
		return send_mail_ssl(opt);
	}
}
