#include <linux/module.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/string.h>
#include <linux/poll.h>
#include <linux/miscdevice.h>
#include <linux/version.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/dma-buf.h>
#include <linux/dma-direction.h>
#include "pciv_exporter.h"


static atomic_t pciv_export_usrRef = ATOMIC_INIT(0); 



struct pciv_buf{
	struct device        		*dev;
	void 		     		*vaddr;
	struct page          		**pages;
	struct vm_area_struct 		*vma;
	dma_addr_t 			dma_addr;
	enum dma_data_direction 	dma_dir;
	unsigned long 			size;
	unsigned int 			nb_pages;
	uintptr_t 			paddr;
	atomic_t 			refcount;
	struct pciv_vmarea_handler 	handler;
	struct dma_buf 			*dbuf;
	int 				alloc_flag;
};





struct pciv_attachment{
	struct sg_table sgt;
	enum dma_data_direction dma_dir;
};



static int pciv_dmabuf_ops_attach(struct dma_buf *dbuf, struct device *dev, struct dma_buf_attachment *dbuf_attach)
{
	struct pciv_attachment *attach;
	struct pciv_buf *buf = dbuf->priv;
	int num_pages = PAGE_ALIGN(buf->size) / PAGE_SIZE;
	struct sg_table *sgt;
	struct scatterlist *sg;
	void *vaddr = buf->vaddr;
	int ret;
	int i;

	attach = kzalloc(sizeof(*attach), GFP_KERNEL);
	if(!attach){
		return -ENOEME;
	}	

	sgt = &attach->sgt;

	ret = sg_alloc_tale(sgt, num_pages, GFP_KERNEL);
	if(ret){
		kfree(attach);
		return ret;
	} 

	for_each_sg(sgt->sgl, sg, sgt->nents, i){
		struct page *page = virt_to_page(vaddr);

		if(!page){

		}

		sg_set_page(sg, page, PAGE_SIZE, 0);
		vaddr += PAGE_SIZE;		
	}
	

	attach->dma_dir = DMA_NONE;

	dbuf_attach->priv = attach;

	return 0;
}


static void pciv_dmabuf_ops_detach(struct dma_buf *dbuf, struct dma_buf_attachment *db_attach)
{
	struct pciv_attachment *attach = db_attach->priv;
	struct sg_table *sgt;

	sgt = &attach->sgt;

	/**
 	 * release the scatterlist cache
 	 */	

	if(attach->dma_dir != DMA_NONE){
		dma_umap_sg(db_attach->dev, sgt->sgl, sgt->orig_nents, attach->dma_dir);
	}

	sg_free_table(sgt);
	kfree(attach);
	db_attach->priv = NULL;
}



static struct sg_table *pciv_dmabuf_ops_map(struct dma_buf_attachment *db_attach, enum dma_data_direction dma_dir)
{
	struct pciv_attachment *attach = db_attach->priv;
	struct mutex *lock = &db_attach->dmabuf->lock;
	struct sg_table *sgt;

	mutex_lock(lock);

	sgt = &attach->sgt;

	if(attach->dma_dir == dma_dir){
		mutex_unlock(lock);
		return sgt;
	}


	/**
 	 * release any previous cached
 	 */	
	if(attach->dma_dir != DMA_NONE){
		dma_umap_sg(db_attach->dev, sgt->sgl, sgt->orig_nents, attach->dma_dir);

		attach->dma_dir = DMA_NONE;
	}

	/**
 	 * mapping to the client with new direction
 	 */
	sgt->nents = dma_map_sg(db_attach->dev, sgt->sgl, sgt->orig_nents, dma_dir);

	if(!sgt->nents){
		mutex_unlock(lock);
		return ERR_PTR(-EIO);
	}

	
	attach->dma_dir = dma_dir;

	mutex_unlock(lock);

	return sgt;
}


static void pciv_common_vm_open(struct vm_area_struct *vma)
{
	struct pciv_vmarea_handler *h = NULL;
	
	if(vma == NULL){
	
	}

	h = vma->vm_private_data;

	if(h){
		
	}else{
	
		return;
	}

	atomic_inc(h->refcount);
}


static void pciv_common_vm_close(struct vm_area_struct *vma)
{
	struct pciv_vmarea_handler *h = NULL;
	
	if(!vma){

	}

	h = vma->vm_private_data;

	if(h){
				
	}else{
		
		return;
	}


	h->put(h->arg);	
}



const struct vm_operations_struct pciv_common_vm_ops = {
	.open = pciv_common_vm_open,
	.close = pciv_common_vm_close,
};


static int pciv_mmap(void *buf_priv, struct vm_area_struct *vma)
{
	struct pciv_buf *buf = buf_priv;
	unsigned long size = vma->vm_end - vma->vm_start;
	int ret;

	if(!buf){

	}

	vma->vm_pgoff = buf->paddr >> PAGE_SHIFT;

	vma->vm_page_prot = PAGE_SHARED;

	ret = remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff, size, vma->vm_page_prot);
	if(ret){

	}

	
	/**
 	 * make sure that vm_areas for 2 buffers won't be merged together
 	 */
	vma->vm_flags |= VM_DONTEXPAND;


	/**
    	 * use common vm_area opreations to track buffer refcount
    	 */
	vma->vm_private_data  = &buf->handler;
	vma->vm_ops           = &pciv_common_vm_ops;

	vma->vm_ops->open(vma);
	
	return 0;
}


static int pciv_dmabuf_ops_mamp(struct dma_buf *dbuf, struct vm_area_struct *vma)
{
	return pciv_mmap(dbuf->priv, vma);
}


static void pciv_dmabuf_ops_release(struct dma_buf *dbuf)
{
	struct pciv_buf *buf = dbuf->priv;
	
	if(atmoic_dec_and_test(&buf->refcount)){
		if(1 == buf->buf_kalloc_flag){
			if(buf->addr){
				kfree(buf->addr);	
			}else{
					
			}
		}
		
		kfree(buf);
	}
	
}

static const struct dma_buf_ops pciv_dmabuf_ops = {
	.attach = pciv_dmabuf_ops_attach,
	.detach = pciv_dmabuf_ops_detach,
	.map_dma_buf = pciv_dmabuf_ops_map,
	.mmap   = pciv_dmabuf_ops_mmap,	
	.release = pciv_dmabuf_ops_release,
};




static void pciv_dmabuf_put(void *buf_priv)
{
	struct pciv_buf *buf = buf_priv;

	pciv_dmabuf_ops_release(buf->dbuf);

}

static struct dma_buf *pciv_get_dmabuf(void *buf_priv, unsigned long flags)
{
	struct pciv_buf *buf = buf_priv;
	struct dma_buf *dbuf;

	DEFINE_DMA_BUF_EXPORT_INFO(exp_info);

	exp_info.ops = &pciv_dmabuf_ops;
	exp_info.size = buf->size;
	exp_info.flags = flags;
	exp_info.priv = buf;

	if(WARN_ON(!buf->vaddr))
		return NULL;

	dbuf = dma_buf_export(&exp_info);

	if(IS_ERR(dbuf))
		return NULL;

	buf->handler.refcount = &buf->refcount;
	buf->handler.put      = pciv_dmabuf_put;
	buf->handler.arg      = buf;

	atomic_inc(&buf->refcount);

	buf->dbuf = dbuf;

	return dbuf;
}



static long pciv_export_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = -ENOIOCTLCMD;
	void __user *parg = (void __user *)arg;
	
	atomic_inc(pciv_export_usrRef);

	switch(cmd)
	{
		case PCIV_EXPORT_MALLOC:
		{
			pciv_preview_buf_t prevBuf;
			pciv_preview_buf_t *pusrPreBuf = (pciv_preview_buf_t *)parg;
			struct dma_buf *dbuf = NULL;
			struct pciv_buf *pbuf = NULL;
			long fd;

			if(copy_from_user(&prevBuf, parg, sizeof(pciv_preview_buf_t)))
			{
				
			}

			if(!prevBuf.size){

			}

			pbuf = kzalloc(sizeof(struct pciv_buf), GFP_KERNEL);
			pbuf->dev = pciv_dev.this_device;

			prevBuf.size = roundup(prevBuf.size, PAGE_SIZE);
			if(!prevBuf.size)}

			}

			pbuf->size = prevBuf.size;

			if(0 == prevBuf.phy){

				pbuf->vaddr = kzalloc(pbuf->size, GFP_KERNEL);
				if(pbuf->vaddr){
					pbuf->paddr = virt_to_phys((char *)pbuf->vaddr);
				}else{

				}

				pbuf->alloc_flag = 1;

			}else{
				pbuf->paddr = (uintptr_t)prevBuf.phy;
				pbuf->vaddr = phys_to_virt(prevBuf.phy);
				pbuf->alloc_flag = 1;				
			}
			
			dbuf = pciv_get_dma_buf(pbuf, O_RDWR | O_CLOEXEC);
			if(!dbuf){
				printk("dma buf export failed\n");
				ret = -EFAULT;
				break;
			}
			else{
				printk("pbuf : %p dbuf : %p pbuf->priv : %p\n", pbuf, dbuf, pbuf->dbuf);
			}

			fd = dma_buf_fd(dbuf, O_RDWR & ~O_ACCMODE);
			if(fd < 0){
				printk("dma_buf_fd failed!\n");
				dma_buf_put(dbuf);
				ret = -EFAULT;
				break;
			}
			
			prevBuf.fd = fd;
			prevBuf.pPhy = pbuf->paddr;

			if(copy_to_user(pusrPreBuf, &prevBuf, sizeof(pciv_preview_buf_t))){

			}

			ret = 0;			
			break;
		}

		default:
			ret = -ENOIOCTLCMD;
		break;

	}

	atmoic_dec(&pciv_export_usrRef);
	
	return ret;
}



static struct file_operations pciv_fops = {
	.owner          = THIS_MODULE,
	.open           = pciv_export_open,
	.unlocked_ioctl = pciv_export_ioctl, 
	.release        = pciv_export_close,
};



static struct miscdevice pciv_dev = {
	.minor     = MISC_DYNAMIC_MINOR,
	.name      = "pciv_exporter",
	.fops      = &pciv_fops,
};




static int __init pciv_exporter_init(void)
{
	int ret = 0;

	ret = misc_register(&pciv_dev);
	
	if(ret){
		printk("pciv exporter init fail\n");
		return -1;
	}

	return 0;
}


static void __exit pciv_exporter_exit(void)
{
	misc_deregister(&pciv_dev);
}


module_init(pciv_exporter_init);

module_exit(pciv_exporter_exit);


MODULE_AUTHOR("saiyn");








