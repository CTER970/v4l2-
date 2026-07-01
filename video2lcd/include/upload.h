#ifndef _UPLOAD_H
#define _UPLOAD_H

/* 使用 HTTP multipart/form-data 上传本地文件, response_buf 保存服务端返回内容 */
int UploadFileToServer(const char *filename,
                       const char *url,
                       unsigned long frame_id,
                       long long capture_ts_ms,
                       char *response_buf,
                       int buf_size);

#endif /* _UPLOAD_H */
