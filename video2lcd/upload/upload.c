#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <curl/curl.h>
#include "upload.h"

/* 保存返回数据的上下文：buf指针 + 已写入长度 + 缓冲区总大小 */
typedef struct {
	char *buf;
	int written;
	int buf_size;
} T_ResponseCtx;

/* libcurl 回调：服务端返回数据时被调用，将数据追加到 buf 中 */
static size_t WriteCallback(void *ptr, size_t size, size_t nmemb, void *userdata)
{
	size_t total = size * nmemb;
	T_ResponseCtx *ctx = (T_ResponseCtx *)userdata;

	/* 防止写入超出缓冲区 */
	if (ctx->written + total >= ctx->buf_size)
		total = ctx->buf_size - ctx->written - 1;

	memcpy(ctx->buf + ctx->written, ptr, total);
	ctx->written += total;
	ctx->buf[ctx->written] = '\0';

	return size * nmemb;
}

/* 上传文件到服务端: multipart POST 文件 + frame_id + 可选采集时间戳, 服务端返回内容写入 response_buf, 0 成功 -1 失败 */
int UploadFileToServer(const char *filename,
                       const char *url,
                       unsigned long frame_id,
                       long long capture_ts_ms,
                       char *response_buf,
                       int buf_size)
{
	CURL *curl = NULL;
	CURLcode res;
	curl_mime *mime = NULL;
	curl_mimepart *part = NULL;
	long http_code = 0;
	T_ResponseCtx ctx;
	char frame_id_str[32];
	char ts_str[32];
	int ret = -1;

	/* 上传是主链路之外的扩展功能, 失败只通过返回值通知上层 */
	if (!filename || !url || !response_buf || buf_size <= 0)
	{
		printf("UploadFileToServer: invalid parameter\n");
		return -1;
	}

	ctx.buf = response_buf;
	ctx.written = 0;
	ctx.buf_size = buf_size;
	memset(response_buf, 0, buf_size);

	curl = curl_easy_init();
	if (!curl)
	{
		printf("UploadFileToServer: curl_easy_init failed\n");
		return -1;
	}

	mime = curl_mime_init(curl);
	if (!mime)
	{
		printf("UploadFileToServer: curl_mime_init failed\n");
		goto cleanup;
	}

	part = curl_mime_addpart(mime);
	if (!part ||
	    curl_mime_name(part, "file") != CURLE_OK ||
	    curl_mime_filedata(part, filename) != CURLE_OK ||
	    curl_mime_type(part, "image/jpeg") != CURLE_OK)
	{
		printf("UploadFileToServer: add file field failed\n");
		goto cleanup;
	}

	snprintf(frame_id_str, sizeof(frame_id_str), "%lu", frame_id);
	part = curl_mime_addpart(mime);
	if (!part ||
	    curl_mime_name(part, "frame_id") != CURLE_OK ||
	    curl_mime_data(part, frame_id_str, CURL_ZERO_TERMINATED) != CURLE_OK)
	{
		printf("UploadFileToServer: add frame_id field failed\n");
		goto cleanup;
	}

	if (capture_ts_ms >= 0)
	{
		snprintf(ts_str, sizeof(ts_str), "%lld", capture_ts_ms);
		part = curl_mime_addpart(mime);
		if (!part ||
		    curl_mime_name(part, "capture_ts_ms") != CURLE_OK ||
		    curl_mime_data(part, ts_str, CURL_ZERO_TERMINATED) != CURLE_OK)
		{
			printf("UploadFileToServer: add capture_ts_ms field failed\n");
			goto cleanup;
		}
	}

	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);       /* multipart POST */
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);  /* 返回数据写入 ctx */
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ctx);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);                  /* 超时10秒 */

	res = curl_easy_perform(curl);
	if (res != CURLE_OK)
	{
		printf("UploadFileToServer: %s\n", curl_easy_strerror(res));
		goto cleanup;
	}

	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
	printf("UploadFileToServer: HTTP %ld, response: %s\n", http_code, response_buf);
	ret = (http_code == 200) ? 0 : -1;

cleanup:
	if (mime)
		curl_mime_free(mime);
	if (curl)
		curl_easy_cleanup(curl);

	return ret;
}
