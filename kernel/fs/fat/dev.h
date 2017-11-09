#include <zjunix/type.h>
#include <zjunix/fs/fscache.h>

struct device
{
	u32 d_blocks;
	u32 d_blocksize;
	int (*d_open)(struct device *dev, uint32_t open_flags);
    int (*d_close)(struct device *dev);
    //int (*d_io)(struct device *dev, BUF_512 iob, bool write); /* iobuf 读入写入缓冲区 */
};

void dev_init(void);