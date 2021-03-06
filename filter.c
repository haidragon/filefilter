#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>   
#include <linux/init.h>  
#include <linux/sched.h>  
#include <asm/unistd.h> 
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/list.h>
#include <asm/uaccess.h>

unsigned long *sys_call_table=NULL;   
asmlinkage ssize_t (*sys_read)(int,void *,size_t);
asmlinkage ssize_t (*sys_write)(int,void *,size_t);
static char * filename="test.c";
module_param(filename,charp,S_IRUGO);
static char * keyword="key";
module_param(keyword,charp,S_IRUGO);
int orig_cr0;
#define STRLEN 1024
char tran_buf[STRLEN];
int len=0;

struct _idt   
{   
unsigned short offset_low,segment_sel;   
unsigned char reserved,flags;   
unsigned short offset_high;   
};   
  
unsigned long *getscTable()
{   
        unsigned char idtr[6],*shell,*sort;   
        struct _idt *idt;   
        unsigned long system_call,sct;   
        unsigned short offset_low,offset_high;   
        char *p;   
        int i;   
  
  
        /* get the interrupt descriptor table */  
  
  
        __asm__("sidt %0" : "=m" (idtr));   
  
  
        /* get the address of system_call */  
        idt=(struct _idt*)(*(unsigned long*)&idtr[2]+8*0x80);   
        offset_low = idt->offset_low;   
        offset_high = idt->offset_high;   
        system_call=(offset_high<<16)|offset_low;   
  
  
        shell=(char *)system_call;   
        sort="\xff\x14\x85";   
  
  
        /* get the address of sys_call_table */  
  
        for(i=0;i<(100-2);i++)   
                if(shell[i]==sort[0]&&shell[i+1]==sort[1]&&shell[i+2]==sort[2])   
                        break;   
  
        p=&shell[i];   
        p+=3;   
        sct=*(unsigned long*)p;   
  
        return (unsigned long*)(sct);   
}   

unsigned int clear_and_return_cr0(void)
{
    unsigned int cr0 = 0;
    unsigned int ret;

    asm volatile ("movl %%cr0, %%eax"
            : "=a"(cr0)
	);
    ret = cr0;

    /*clear the 20th bit of CR0,*/
    cr0 &= 0xfffeffff;
    asm volatile ("movl %%eax, %%cr0"
            :
            : "a"(cr0)
         );
    return ret;
}

void setback_cr0(unsigned int val)
{
    asm volatile ("movl %%eax, %%cr0"
            :
            : "a"(val)
         );
}

void encrypt(char * tran_buf,char * keyword)
{
	int keylen=strlen(keyword);
	printk("keylen %d\n",keylen);
	int tlen=strlen(tran_buf);
	printk("tlen %d\n",tlen);
	int i=0;
	char temp;
	while(keylen!=0)
       {
		temp=tran_buf[0];
		for(i=1;i<tlen-1;i++)
		{
			tran_buf[i-1]=tran_buf[i];
		}
		tran_buf[tlen-2]=temp;
		keylen--;
	}	
}

void decrypt(char * tran_buf,char * keyword)
{
	int keylen=strlen(keyword);
	int tlen=strlen(tran_buf);
	int i=0;
	char temp=0;
	while(keylen!=0)
       {
		temp=tran_buf[tlen-2];
		for(i=tlen-3;i>=0;i--)
		{
			tran_buf[i+1]=tran_buf[i];
		}
		tran_buf[0]=temp;
		keylen--;
	}
		
}



asmlinkage ssize_t filefilter_read(unsigned int fd, char * buf, size_t count)
{   
	
   struct file * file;
	int num=sys_read(fd,buf,count);
	file = fget(fd);
	if(file==NULL)
		return sys_read(fd,buf,count);	
	
	if(!strcmp(file->f_dentry->d_name.name,filename))
	{
		if(count>STRLEN)
			len=STRLEN-1;
		else
 			len=count;
		copy_from_user(tran_buf,buf,len);
		decrypt(tran_buf,keyword);
		copy_to_user(buf,tran_buf,len);
	}	
	fput(file);
   return num;
}   

asmlinkage ssize_t filefilter_write(unsigned int fd, char * buf, size_t count)
{   
	struct file * file;
	file = fget(fd);
	if(file==NULL)
		return sys_write(fd,buf,count);		
	if(!strcmp(file->f_dentry->d_name.name,filename))
	{
		//printk("name %s\n",file->f_dentry->d_name.name);
		if(count>STRLEN)
			len=STRLEN-1;
		else
 			len=count;
		copy_from_user(tran_buf,buf,len);
		encrypt(tran_buf,keyword);
		copy_to_user(buf,tran_buf,len);
	}	
	fput(file);
   return sys_write(fd,buf,count);
}   


static int filefilter_init(void)
{
	sys_call_table = getscTable();
	printk("sys_call_table addr %x!\n",sys_call_table);
	sys_read=(ssize_t(*)(int,void *,size_t))sys_call_table[__NR_read];
	sys_write=(ssize_t(*)(int,void *,size_t))sys_call_table[__NR_write];
	orig_cr0=clear_and_return_cr0();
	sys_call_table[__NR_read]=(unsigned long)filefilter_read;
	sys_call_table[__NR_write]=(unsigned long)filefilter_write;
	setback_cr0(orig_cr0);
	printk("installed!\n");
	return 0;
}

static int filefilter_exit(void)
{
	orig_cr0=clear_and_return_cr0();
	sys_call_table[__NR_read]=(unsigned long)sys_read;
	sys_call_table[__NR_write]=(unsigned long)sys_write;
	setback_cr0(orig_cr0);
	printk("uninstalled!\n");
	return 0;
}


module_init(filefilter_init);
module_exit(filefilter_exit);
MODULE_LICENSE("GPL");
