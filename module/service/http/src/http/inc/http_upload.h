#ifndef __HTTP_UPLOAD_H__
#define __HTTP_UPLOAD_H__


int  http_upload_form_status_get();
void http_upload_form_status_set(int status);

char *http_base64_encode(const char *data, int data_len);
int   http_base64_decode(const char *base64, unsigned char * bindata);

int http_recv_form_file(void *pInst, 
                            const char *post_path,
                            FormDataBoundary *pFormDataBoundary, 
                            char *md5_str,
                            int socket, 
                            char *buf, 
                            unsigned int buflen, 
                            int *datalen);


/* add 20180622 by yajie.wang
以下添加内容用于接收ONVIF http post提交的文件
post_path 			用于判断提交文件的URL	
返回值			1:接收完成; 0:正在接收; -1:不是的文件URL
*/
int http_recv_post_file(void *pInst, 
                    const char *post_path, 
                    HttpPostFileInfo *pPostFileInfo,
                    const char *file_path,
                    int socket, 
                    char *buf, 
                    unsigned int buflen, 
                    int *datalen);


#endif

