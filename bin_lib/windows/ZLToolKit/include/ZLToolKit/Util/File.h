/*
 * File.h
 *
 *  Created on: 2016年9月22日
 *      Author: xzl
 */

#ifndef SRC_UTIL_FILE_H_
#define SRC_UTIL_FILE_H_

#if defined(_WIN32)
#include <WinSock2.h>   
#pragma comment (lib,"WS2_32")
#endif // WIN32
#include <stdio.h>

#if defined(_WIN32)
#ifndef PATH_MAX
#define PATH_MAX 256
#endif // !PATH_MAX

struct dirent{
	long d_ino;              /* inode number*/
	off_t d_off;             /* offset to this dirent*/
	unsigned short d_reclen; /* length of this d_name*/
	unsigned char d_type;    /* the type of d_name*/
	char d_name[1];          /* file name (null-terminated)*/
};
typedef struct _dirdesc {
	int     dd_fd;      /** file descriptor associated with directory */
	long    dd_loc;     /** offset in current buffer */
	long    dd_size;    /** amount of data returned by getdirentries */
	char    *dd_buf;    /** data buffer */
	int     dd_len;     /** size of data buffer */
	long    dd_seek;    /** magic cookie returned by getdirentries */
	HANDLE handle;
	struct dirent *index;
} DIR;
# define __dirfd(dp)    ((dp)->dd_fd)

int mkdir(const char *path, int mode);
DIR *opendir(const char *);
int closedir(DIR *);
struct dirent *readdir(DIR *);

#endif // defined(_WIN32)

namespace ZL {
namespace Util {

class File {
public:
	// 可读普通文件
	static bool isrfile(const char *path) ;
	//新建文件，目录文件夹自动生成
	static bool createfile_path(const char *file, unsigned int mod);
	static FILE *createfile_file(const char *file,const char *mode);
	//判断是否为目录
	static bool is_dir(const char *path) ;
	//判断是否为常规文件
	static bool is_file(const char *path) ;
	//判断是否是特殊目录（. or ..）
	static bool is_special_dir(const char *path);
	//删除目录或文件
	static void delete_file(const char *path) ;
	static bool rm_empty_dir(const char *path);
private:
	File();
	virtual ~File();
};

} /* namespace Util */
} /* namespace ZL */

#endif /* SRC_UTIL_FILE_H_ */
