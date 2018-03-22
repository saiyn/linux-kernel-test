typedef struct pciv_preview_buf
{
	unsigned long phy;
	unsigned long size;
	unsigned long fd;

}pciv_preview_buf_t;




#define PCIV_EXPOERTE_MALLOC  _IOWR(IOC_TYPE_PCIV, IOC_NR_, pciv_preview_buf_t)
