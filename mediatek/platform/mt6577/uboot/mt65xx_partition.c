
#include <common.h>
#include <exports.h>
#include <asm/errno.h>
#include "config.h"
#include "mt65xx_partition.h"
#include "pmt.h"

/* BUG BUG: temp */
#define DBGMSG(...)
#define ERRMSG(...)
#ifdef MTK_EMMC_SUPPORT
#define PMT 1
#else
#define PMT 1
#endif

static part_dev_t *mt6577_part_dev;
extern void part_init_pmt(unsigned long totalblks,part_dev_t *dev);

//static 
extern part_t partition_layout[];
static uchar *mt6577_part_buf;

#ifdef MTK_EMMC_SUPPORT
void mt6577_part_dump(void)
{
    part_t *part = &partition_layout[0];
    
    printf("\nPart Info from compiler.(1blk=%dB):\n", BLK_SIZE);
    printf("\nPart Info.(1blk=%dB):\n", BLK_SIZE);
    while (part->name) {
    	 printf("[0x%016llx-0x%016llx] (%.8ld blocks): \"%s\"\n", 
               (u64)part->startblk * BLK_SIZE, 
              (u64)(part->startblk + part->blknum) * BLK_SIZE - 1, 
			   				part->blknum, part->name);
        part++;
    }
    printf("\n");
}
#else
void mt6577_part_dump(void)
{
    part_t *part = &partition_layout[0];
    
    printf("\nPart Info from compiler.(1blk=%dB):\n", BLK_SIZE);
    printf("\nPart Info.(1blk=%dB):\n", BLK_SIZE);
    while (part->name) {
        printf("[0x%.8x-0x%.8x] (%.8ld blocks): \"%s\"\n", 
               part->startblk * BLK_SIZE, 
              (part->startblk + part->blknum) * BLK_SIZE - 1, 
			   part->blknum, part->name);
        part++;
    }
    printf("\n");
}
#endif
void mt6577_part_init(unsigned long totalblks)
{
    part_t *part = &partition_layout[0];
    unsigned long lastblk;

    mt6577_part_buf = (uchar*)malloc(BLK_SIZE * 2);
    
    if (!totalblks) return;

    /* updater the number of blks of first part. */
    if (totalblks <= part->blknum)
        part->blknum = totalblks;

    totalblks -= part->blknum;
    lastblk = part->startblk + part->blknum;

    while(totalblks) {
        part++;
        if (!part->name)
            break;

        if (part->flags & PART_FLAG_LEFT || totalblks <= part->blknum)
            part->blknum = totalblks;

        part->startblk = lastblk;
        totalblks -= part->blknum;
        lastblk = part->startblk + part->blknum;;
    }
}

#ifdef PMT
part_t tempart;
pt_resident lastest_part[PART_MAX_COUNT];
#endif
part_t* mt6577_part_get_partition(char *name)
{
	int index=0;
	printf ("[%s] %s\n", __FUNCTION__, name);
    part_t *part = &partition_layout[0];
	
	while (part->name)
	{
		if (!strcmp (name, part->name))
		{
#ifdef PMT
		tempart.name=part->name;
		//when download get partitin used new,orther wise used latest
		{
			tempart.startblk = BLK_NUM(lastest_part[index].offset);
			tempart.blknum=BLK_NUM(lastest_part[index].size);
		}
		tempart.flags=part->flags;
		printf ("[%s] %x\n", __FUNCTION__, tempart.startblk);
		return &tempart;
#endif
		return part;
		}
		index++;
		part++;
	}
    return NULL;
}
#ifdef MTK_EMMC_SUPPORT
int mt6577_part_generic_read(part_dev_t *dev, u64 src, uchar *dst, int size)
{
    int dev_id = dev->id;
    uchar *buf = &mt6577_part_buf[0];
    block_dev_desc_t *blkdev = dev->blkdev;
		u64 end, part_start, part_end, part_len, aligned_start, aligned_end;
    ulong blknr, blkcnt;

	if (!blkdev) {
        ERRMSG("No block device registered\n");
        return -ENODEV;
	}

	if (size == 0) 
		return 0;

	end = src + size;
	
	part_start    = src &  ((u64)BLK_SIZE - 1);
	part_end      = end &  ((u64)BLK_SIZE - 1);
	aligned_start = src & ~((u64)BLK_SIZE - 1);
	aligned_end   = end & ~((u64)BLK_SIZE - 1);
 
	if (part_start) {
	    blknr = aligned_start >> BLK_BITS;	
		part_len = BLK_SIZE - part_start;
		if ((blkdev->block_read(dev_id, blknr, 1, (unsigned long*)buf)) != 1)
			return -EIO;
		memcpy(dst, buf + part_start, part_len);
		dst += part_len;
        src += part_len;
    }
    
	aligned_start = src & ~((u64)BLK_SIZE - 1);
	blknr  = aligned_start >> BLK_BITS;
	blkcnt = (aligned_end - aligned_start) >> BLK_BITS;
	
	if ((blkdev->block_read(dev_id, blknr, blkcnt, (unsigned long *)(dst))) != blkcnt)
		return -EIO;

    src += (blkcnt << BLK_BITS);
    dst += (blkcnt << BLK_BITS);

	if (part_end && src < end) {
	    blknr = aligned_end >> BLK_BITS;	
		if ((blkdev->block_read(dev_id, blknr, 1, (unsigned long*)buf)) != 1)
			return -EIO;
		memcpy(dst, buf, part_end);
	}
	return size;
}

static int mt6577_part_generic_write(part_dev_t *dev, uchar *src, u64 dst, int size)
{
    int dev_id = dev->id;
    uchar *buf = &mt6577_part_buf[0];
    block_dev_desc_t *blkdev = dev->blkdev;
		u64 end, part_start, part_end, part_len, aligned_start, aligned_end;
    ulong blknr, blkcnt;

	if (!blkdev) {
        ERRMSG("No block device registered\n");
        return -ENODEV;
	}

	if (size == 0) 
		return 0;

	end = dst + size;
	
	part_start    = dst &  ((u64)BLK_SIZE - 1);
	part_end      = end &  ((u64)BLK_SIZE - 1);
	aligned_start = dst & ~((u64)BLK_SIZE - 1);
	aligned_end   = end & ~((u64)BLK_SIZE - 1);
 
	if (part_start) {
	    blknr = aligned_start >> BLK_BITS;	
		part_len = BLK_SIZE - part_start;
		if ((blkdev->block_read(dev_id, blknr, 1, (unsigned long*)buf)) != 1)
			return -EIO;
		memcpy(buf + part_start, src, part_len);
    	if ((blkdev->block_write(dev_id, blknr, 1, (unsigned long*)buf)) != 1)
        	return -EIO;
		dst += part_len;
        src += part_len;
    }
    
	aligned_start = dst & ~((u64)BLK_SIZE - 1);
	blknr  = aligned_start >> BLK_BITS;
	blkcnt = (aligned_end - aligned_start) >> BLK_BITS;

	if ((blkdev->block_write(dev_id, blknr, blkcnt, (unsigned long *)(dst))) != blkcnt)
		return -EIO;
    
    src += (blkcnt << BLK_BITS);
    dst += (blkcnt << BLK_BITS);

	if (part_end && dst < end) {
	    blknr = aligned_end >> BLK_BITS;	
		if ((blkdev->block_read(dev_id, blknr, 1, (unsigned long*)buf)) != 1) {
			return -EIO;
		}
		memcpy(buf, src, part_end);
    	if ((blkdev->block_write(dev_id, blknr, 1, (unsigned long*)buf)) != 1) {
            return -EIO;
    	}
	}
	return size;
}

int mt6577_part_register_device(part_dev_t *dev)
{
    if (!mt6577_part_dev) {
        if (!dev->read)
            dev->read = mt6577_part_generic_read;
        if (!dev->write)
            dev->write = mt6577_part_generic_write;
        mt6577_part_dev = dev;

        #ifdef CFG_NAND_BOOT
        if (dev->id == CFG_MT65XX_NAND_ID)
	{
#ifdef PMT
		// printf("call mt6575_part_init_pmt");
		part_init_pmt(BLK_NUM(1 * GB),dev);
#else
            mt6577_part_init(BLK_NUM(1 * GB));
#endif
	}
        else
        {
#ifdef PMT
		// printf("call mt6516_part_init_pmt");
		part_init_pmt(BLK_NUM(1 * GB),dev);
#else
            mt6577_part_init((unsigned long)dev->blkdev->lba);
#endif /* CFG_NAND_BOOT */
        }
	#endif /* CFG_NAND_BOOT */

#ifdef CFG_MMC_BOOT
#ifdef PMT
	part_init_pmt((unsigned long)dev->blkdev->lba,dev);
#else
	mt6577_part_init((unsigned long)dev->blkdev->lba);
#endif
#endif
    }
    return 0;
}
#else
int mt6577_part_generic_read(part_dev_t *dev, ulong src, uchar *dst, int size)
{
    int dev_id = dev->id;
    uchar *buf = &mt6577_part_buf[0];
    block_dev_desc_t *blkdev = dev->blkdev;
	ulong end, part_start, part_end, part_len, aligned_start, aligned_end;
    ulong blknr, blkcnt;

	if (!blkdev) {
        ERRMSG("No block device registered\n");
        return -ENODEV;
	}

	if (size == 0) 
		return 0;

	end = src + size;
	
	part_start    = src &  (BLK_SIZE - 1);
	part_end      = end &  (BLK_SIZE - 1);
	aligned_start = src & ~(BLK_SIZE - 1);
	aligned_end   = end & ~(BLK_SIZE - 1);
 
	if (part_start) {
	    blknr = aligned_start >> BLK_BITS;	
		part_len = BLK_SIZE - part_start;
		if ((blkdev->block_read(dev_id, blknr, 1, (unsigned long*)buf)) != 1)
			return -EIO;
		memcpy(dst, buf + part_start, part_len);
		dst += part_len;
        src += part_len;
    }
    
	aligned_start = src & ~(BLK_SIZE - 1);
	blknr  = aligned_start >> BLK_BITS;
	blkcnt = (aligned_end - aligned_start) >> BLK_BITS;
	
	if ((blkdev->block_read(dev_id, blknr, blkcnt, (unsigned long *)(dst))) != blkcnt)
		return -EIO;

    src += (blkcnt << BLK_BITS);
    dst += (blkcnt << BLK_BITS);

	if (part_end && src < end) {
	    blknr = aligned_end >> BLK_BITS;	
		if ((blkdev->block_read(dev_id, blknr, 1, (unsigned long*)buf)) != 1)
			return -EIO;
		memcpy(dst, buf, part_end);
	}
	return size;
}

static int mt6577_part_generic_write(part_dev_t *dev, uchar *src, ulong dst, int size)
{
    int dev_id = dev->id;
    uchar *buf = &mt6577_part_buf[0];
    block_dev_desc_t *blkdev = dev->blkdev;
	ulong end, part_start, part_end, part_len, aligned_start, aligned_end;
    ulong blknr, blkcnt;

	if (!blkdev) {
        ERRMSG("No block device registered\n");
        return -ENODEV;
	}

	if (size == 0) 
		return 0;

	end = dst + size;
	
	part_start    = dst &  (BLK_SIZE - 1);
	part_end      = end &  (BLK_SIZE - 1);
	aligned_start = dst & ~(BLK_SIZE - 1);
	aligned_end   = end & ~(BLK_SIZE - 1);
 
	if (part_start) {
	    blknr = aligned_start >> BLK_BITS;	
		part_len = BLK_SIZE - part_start;
		if ((blkdev->block_read(dev_id, blknr, 1, (unsigned long*)buf)) != 1)
			return -EIO;
		memcpy(buf + part_start, src, part_len);
    	if ((blkdev->block_write(dev_id, blknr, 1, (unsigned long*)buf)) != 1)
        	return -EIO;
		dst += part_len;
        src += part_len;
    }
    
	aligned_start = dst & ~(BLK_SIZE - 1);
	blknr  = aligned_start >> BLK_BITS;
	blkcnt = (aligned_end - aligned_start) >> BLK_BITS;

	if ((blkdev->block_write(dev_id, blknr, blkcnt, (unsigned long *)(dst))) != blkcnt)
		return -EIO;
    
    src += (blkcnt << BLK_BITS);
    dst += (blkcnt << BLK_BITS);

	if (part_end && dst < end) {
	    blknr = aligned_end >> BLK_BITS;	
		if ((blkdev->block_read(dev_id, blknr, 1, (unsigned long*)buf)) != 1) {
			return -EIO;
		}
		memcpy(buf, src, part_end);
    	if ((blkdev->block_write(dev_id, blknr, 1, (unsigned long*)buf)) != 1) {
            return -EIO;
    	}
	}
	return size;
}

int mt6577_part_register_device(part_dev_t *dev)
{
    if (!mt6577_part_dev) {
        if (!dev->read)
            dev->read = mt6577_part_generic_read;
        if (!dev->write)
            dev->write = mt6577_part_generic_write;
        mt6577_part_dev = dev;

        #ifdef CFG_NAND_BOOT
        if (dev->id == CFG_MT6577_NAND_ID)
	{
#ifdef PMT
		// printf("call mt6577_part_init_pmt");
		part_init_pmt(BLK_NUM(1 * GB),dev);
#else
            mt6577_part_init(BLK_NUM(1 * GB));
#endif
	}
        else
        {
#ifdef PMT
		// printf("call mt6516_part_init_pmt");
		part_init_pmt(BLK_NUM(1 * GB),dev);
#else
            mt6577_part_init((unsigned long)dev->blkdev->lba);
#endif /* CFG_NAND_BOOT */
        }
	#endif /* CFG_NAND_BOOT */
    }
    return 0;
}
#endif
part_dev_t *mt6577_part_get_device(void)
{
    if (mt6577_part_dev && !mt6577_part_dev->init && mt6577_part_dev->init_dev) 
    {
        mt6577_part_dev->init_dev(mt6577_part_dev->id);
        mt6577_part_dev->init = 1;
    }
    return mt6577_part_dev;
}

