
#include <sys/types.h>          /* See NOTES */
#include <sys/socket.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>

/* 调试辅助工具:
 * 通过 UDP 给 netprint 服务端发送控制命令, 或进入 show 模式接收远端打印。
 * 它不是 video2lcd 主采集链路的一部分。
 */

#define SERVER_PORT 5678


/* 用法: ./netprint_client <server_ip> <命令>, 命令可为 dbglevel=<0-9> / stdout=0|1 / netprint=0|1 / show */

int main(int argc, char **argv)
{
	int iSocketClient;
	struct sockaddr_in tSocketServerAddr;
	
	int iRet;
	unsigned char ucRecvBuf[1000];
	int iSendLen;
	int iRecvLen;
	int iAddrLen;

	if (argc != 3)
	{
		printf("Usage:\n");
		printf("%s <server_ip> dbglevel=<0-9>\n", argv[0]);
		printf("%s <server_ip> stdout=0|1\n", argv[0]);
		printf("%s <server_ip> netprint=0|1\n", argv[0]);
		printf("%s <server_ip> show\n", argv[0]);
		return -1;
	}

	iSocketClient = socket(AF_INET, SOCK_DGRAM, 0);

	tSocketServerAddr.sin_family      = AF_INET;
	tSocketServerAddr.sin_port        = htons(SERVER_PORT);  /* host to net, short */
 	if (0 == inet_aton(argv[1], &tSocketServerAddr.sin_addr))
 	{
		printf("invalid server_ip\n");
		return -1;
	}
	memset(tSocketServerAddr.sin_zero, 0, 8);

	if (strcmp(argv[2], "show") == 0)
	{
		iAddrLen = sizeof(struct sockaddr);
		iSendLen = sendto(iSocketClient, "setclient", 9, 0,
							  (const struct sockaddr *)&tSocketServerAddr, iAddrLen);

		while (1)
		{
			iAddrLen = sizeof(struct sockaddr);
			iRecvLen = recvfrom(iSocketClient, ucRecvBuf, 999, 0, (struct sockaddr *)&tSocketServerAddr, &iAddrLen);
			if (iRecvLen > 0)
			{
				ucRecvBuf[iRecvLen] = '\0';
				printf("%s\n", ucRecvBuf);
			}
		}
	}
	else
	{
		iAddrLen = sizeof(struct sockaddr);
		iSendLen = sendto(iSocketClient, argv[2], strlen(argv[2]), 0,
							  (const struct sockaddr *)&tSocketServerAddr, iAddrLen);
	}

	return 0;
}

