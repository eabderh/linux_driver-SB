

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/fcntl.h>
#include <linux/ioport.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/dma-mapping.h>
#include <linux/mm.h>
#include <asm/page.h>
#include <asm/signal.h>
#include <asm/system.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/dma.h>

#include <linux/delay.h>

#define SB16_BASE		0x220
#define SB16_DSP_RESET		(SB16_BASE + 0x6)
#define SB16_DSP_READ		(SB16_BASE + 0xA)
#define SB16_DSP_WRITE		(SB16_BASE + 0xC)
#define SB16_DSP_WSTATUS	(SB16_BASE + 0xC)
#define SB16_DSP_RSTATUS	(SB16_BASE + 0xE)
#define SB16_DSP_READY		0xAA
#define SB16_VERSION_COM	0xE1
#define DSPIO_SET_OUTRATE_COM 	0x41
#define DSPIO_OUT_IO_COM 	0xC0
#define DSPIO_TRANSFER_MODE 	0x00

#define SB16_RBUF_EMPTY()	(!(inb(SB16_DSP_RSTATUS) & 0x80))
#define SB16_WBUF_FULL()	(inb(SB16_DSP_WSTATUS) & 0x80)
#define SAMPLING_RATE 		11025
#define CHANNEL 		1

#define CHAR_FILE 		"sb"
#define MAJOR_NUMBER 		240

#define ORDER 			4
#define PAGESIZE() 		4096//getpagesize()
#define DMA_BYTES 		(PAGESIZE() * 1 << ORDER)
//#define DMA_BYTES_MAX 		(DMA_BYTES - 1)


#define HIBYTE(ptr) 		(((unsigned char*) &ptr)[1])
#define LOBYTE(ptr) 		(((unsigned char*) &ptr)[0])

//#define HIBYTE(ptr) ((ptr >> 8) & 0xFF)
//#define LOBYTE(ptr) (ptr & 0xFF)

int sb16drv_open(struct inode* inode, struct file* fileptr);
int sb16drv_release(struct inode* inode, struct file* fileptr);
ssize_t sb16drv_read(	struct file *fileptr, char* buf,
			size_t count, loff_t* f_pos);
ssize_t sb16drv_write(	struct file *fileptr, const char* buf,
			size_t count, loff_t* f_pos);



unsigned char* dma_buffer = NULL;
int dma_count = 0;

struct file_operations sb16drv_fops = {
	read: 		sb16drv_read,
	write: 		sb16drv_write,
	open: 		sb16drv_open,
	release: 	sb16drv_release
};






unsigned char read_dsp(void)
{
	while (SB16_RBUF_EMPTY());
	return inb(SB16_DSP_READ);
}

void write_dsp(unsigned char data)
{
	while (SB16_WBUF_FULL());
	outb(data, SB16_DSP_WRITE);
}

void reset_dsp(void)
{
	outb((unsigned) 0x1, SB16_DSP_RESET);
	udelay(3);//wait
	outb((unsigned) 0x0, SB16_DSP_RESET);
	while (!(read_dsp() == SB16_DSP_READY));
}

void print_version_dsp(void)
{
	unsigned char version_major;
	unsigned char version_minor;
	write_dsp(SB16_VERSION_COM);
	version_major = read_dsp();
	version_minor = read_dsp();
	printk(KERN_ALERT "version: %d-%d\n",
		(int) version_major,
		(int) version_minor);
}







int dad_dma_prepare(	int channel, int mode,
			unsigned char* buf, unsigned int count)
{
	unsigned long flags;
	flags = claim_dma_lock();
	disable_dma(channel);
	clear_dma_ff(channel);
	set_dma_mode(channel, mode);
	set_dma_addr(channel, virt_to_bus(buf));
	set_dma_count(channel, count);
	enable_dma(channel);
	release_dma_lock(flags);
	return 0;
}




void init_dmactl(void)
{
	dad_dma_prepare(CHANNEL, DMA_MODE_WRITE, dma_buffer, DMA_BYTES);
}




void init_dspdma(void)
{
	int rate;
	int count;

	rate = SAMPLING_RATE;
	count = DMA_BYTES - 1;
	write_dsp(DSPIO_SET_OUTRATE_COM);
	write_dsp(HIBYTE(rate));
	write_dsp(LOBYTE(rate));
//	printk("<1> high rate %x \n", HIBYTE(rate));
//	printk("<1> low rate %x \n", LOBYTE(rate));
	write_dsp(DSPIO_OUT_IO_COM);
	write_dsp(DSPIO_TRANSFER_MODE);
	write_dsp(LOBYTE(count));
	write_dsp(HIBYTE(count));
//	printk("<1> high count %x \n", HIBYTE(count));
//	printk("<1> low count %x \n", LOBYTE(count));
}




void start_play(void)
{
	printk("<1> started play\n");
	init_dmactl();
	init_dspdma();
	//mdelay(2000);
}



void sb16drv_exit(void)
{
	printk(KERN_INFO "sb16drv uninitializing\n");
	if (dma_buffer)
		free_pages( (long) dma_buffer, ORDER);
	unregister_chrdev(MAJOR_NUMBER, CHAR_FILE);
	reset_dsp();
	printk("<1> Removed sb16drv module\n");
}



int sb16drv_init(void)
{
	int result;

	reset_dsp();
	print_version_dsp();

	result = register_chrdev(MAJOR_NUMBER, "CHAR_FILE", &sb16drv_fops);
	if (result < 0)
		goto fail;

	dma_buffer = (unsigned char*) __get_free_pages(GFP_DMA, ORDER);
	if (!dma_buffer)
		goto fail;

	printk("<1> Insertion of sb16drv module: SUCCESS\n");
	printk("<1> DMA size: %d\n", DMA_BYTES);
	return 0;
fail:
	printk("<1> Insertion of sb16drv module: FAIL\n");
	sb16drv_exit();
	return -1;
}







int sb16drv_open(struct inode* inode, struct file* fileptr)
{
	printk("<1> Device opened\n");
	return 0;
}




int sb16drv_release(struct inode* inode, struct file* fileptr)
{
	printk("<1> Device released\n");
	return 0;
}




ssize_t sb16drv_read(	struct file *fileptr, char* buf,
			size_t count, loff_t* f_pos)
{
	return 0;
}




ssize_t sb16drv_write(	struct file *fileptr, const char* buf,
			size_t count, loff_t* f_pos)
{
	int dma_space;
	int count_transfer;
	printk("<1> sb16drv_write\n");
	printk("<1> write count: %d\n", count);
	printk("<1> write f_pos: %d\n", (int) *f_pos);

	if (*f_pos < DMA_BYTES) {
		dma_space = DMA_BYTES - *f_pos;
		// smaller of the two
		if (count < dma_space)
			count_transfer = count;
		else
			count_transfer = dma_space;
		printk("<1> write count_transfer: %d\n", count_transfer);
		copy_from_user(dma_buffer + *f_pos, buf, count_transfer);
		*f_pos += count_transfer;
		dma_count = *f_pos;
		printk("<1> bytes placed in buffer: %d\n", dma_count);
		if (dma_count >= DMA_BYTES) {
			start_play();
			*f_pos = 0;
		}
		return count_transfer;
	} else {
		printk(KERN_ALERT "write no space\n");
		return -ENOSPC;
	}
}



MODULE_LICENSE("GPL"); 
module_init(sb16drv_init);
module_exit(sb16drv_exit);








