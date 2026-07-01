#include <stdio.h>
#include <stdlib.h>
#include <curl/curl.h>
#include "upload.h"

#define DEFAULT_JPEG_PATH "/root/projects/video2lcd/capture.jpg"
#define DEFAULT_DETECT_URL "http://192.168.250.1:8080/detect"
#define RESPONSE_BUF_SIZE 8192

int main(int argc, char **argv)
{
	const char *jpeg_path = DEFAULT_JPEG_PATH;
	const char *detect_url = DEFAULT_DETECT_URL;
	unsigned long frame_id = 1;
	long long capture_ts_ms = 0;
	char response_buf[RESPONSE_BUF_SIZE];
	int ret;

	if (argc > 1)
		jpeg_path = argv[1];
	if (argc > 2)
		detect_url = argv[2];
	if (argc > 3)
		frame_id = strtoul(argv[3], NULL, 10);
	if (argc > 4)
		capture_ts_ms = strtoll(argv[4], NULL, 10);

	printf("upload_test: file=%s\n", jpeg_path);
	printf("upload_test: url=%s\n", detect_url);
	printf("upload_test: frame_id=%lu capture_ts_ms=%lld\n",
	       frame_id, capture_ts_ms);

	if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK)
	{
		printf("upload_test: curl_global_init failed\n");
		return 1;
	}

	ret = UploadFileToServer(jpeg_path,
	                         detect_url,
	                         frame_id,
	                         capture_ts_ms,
	                         response_buf,
	                         sizeof(response_buf));

	printf("upload_test: ret=%d\n", ret);
	printf("upload_test: response=%s\n", response_buf);

	curl_global_cleanup();
	return (ret == 0) ? 0 : 1;
}