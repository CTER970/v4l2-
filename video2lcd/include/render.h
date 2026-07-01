
#ifndef _RENDER_H
#define _RENDER_H

#include <pic_operation.h>
#include <disp_manager.h>

/* 近邻取样插值方法缩放图片; 函数内 malloc 分配输出内存, 调用者负责 free */
int PicZoom(PT_PixelDatas ptOriginPic, PT_PixelDatas ptZoomPic);

/* 把小图片合并入大图片的指定区域 (iX,iY 为左上角座标) */
int PicMerge(int iX, int iY, PT_PixelDatas ptSmallPic, PT_PixelDatas ptBigPic);


#endif /* _RENDER_H */

