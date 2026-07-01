
#ifndef _PIC_MANAGER_H
#define _PIC_MANAGER_H

#include <config.h>
#include <pic_operation.h>
#include <page_manager.h>
#include <file.h>

/* 注册图片文件解析模块 (从 BMP/JPG 等文件解析出像素数据) */
int RegisterPicFileParser(PT_PicFileParser ptPicFileParser);

/* 显示本程序支持的图片文件解析模块 */
void ShowPicFmts(void);

/* 调用各图片文件解析模块的初始化函数, 即注册它们 */
int PicFmtsInit(void);

/* 注册 JPG 文件解析模块 */
int JPGParserInit(void);

/* 注册 BMP 文件解析模块 */
int BMPParserInit(void);

/* 根据名字取出指定的图片文件解析模块 */
PT_PicFileParser Parser(char *pcName);

/* 找到能支持指定文件的图片文件解析模块 */
PT_PicFileParser GetParser(PT_FileMap ptFileMap);

#endif /* _PIC_MANAGER_H */

