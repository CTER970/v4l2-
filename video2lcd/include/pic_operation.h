
#ifndef _PIC_OPERATION_H
#define _PIC_OPERATION_H

/* 项目内统一使用的像素数据描述结构, 摄像头帧/转换输出/framebuffer 等均包装成此结构 */
typedef struct PixelDatas {
	int iWidth;
	int iHeight;
	int iBpp;
	int iLineBytes;
	int iTotalBytes;
	unsigned char *aucPixelDatas;
}T_PixelDatas, *PT_PixelDatas;


#endif /* _PIC_OPERATION_H */
