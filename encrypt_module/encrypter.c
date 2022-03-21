#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <asm/errno.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/ctype.h>
#include <linux/fcntl.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/slab.h>

#define ENCRYPT             (1)
#define DECRYPT             (0)
#define ERROR_INJECTION     (1)

 /* Declaring kernel module settings: */
MODULE_LICENSE("Dual BSD/GPL");

MODULE_AUTHOR("Embedded Linux course example");

MODULE_DESCRIPTION("Tiny encryption module. The module takes up to 80 "
                    "characters from the user and applies an encryption " 
                    "function. The module returns to the user up to 80 "
                    "characters (encrypted). For the debug purposes the "
                    "encryption functionality can be disabled using ioctl "
                    "interface.");

#define BUF_LEN 50

/* Encryption switch */
int encryption_enable = 1; /* Enable encryption. */
long error_injection = 0; /* Error injection. */

/* Declaration of encrypter.c functions */
static int encryptVigenere(const char* in, const char* key, char* out);
static int decryptVigenere(const char* in, const char* key, char* out);
void encrypter_exit(void);
int encrypter_init(void);
static int encrypter_open(struct inode *, struct file *);
static int encrypter_release(struct inode *, struct file *);
static ssize_t encrypter_read(struct file *, char *buf, size_t , loff_t *);
static ssize_t encrypter_write(struct file *, const char *buf, size_t , loff_t *);
static long encrypter_ioctl (struct file *file, unsigned int cmd, unsigned long arg);

/* Structure that declares the usual file access functions. */
struct file_operations encrypter_fops =
{
    read    :   encrypter_read,
    write   :   encrypter_write,
    open    :   encrypter_open,
    release :   encrypter_release,
    unlocked_ioctl : encrypter_ioctl
};

/* Declaration of the init and exit functions. */
module_init(encrypter_init);
module_exit(encrypter_exit);


/* Global variables of the driver */

/* Major number. */
int encrypter_major = 60;

/* Buffer to store data. */
char *encrypter_buffer;

static int encryptVigenere(const char* in, const char *key, char* out){
	int inLength = strlen(in);
	int keyLength = strlen(key);
	int i=0, j=0;
	char c;
	char k;

	for(i=0; i<inLength; i++){
		if(j>=keyLength){
			j=0;
		}
		c = in[i];
		k = key[j];
		if((c>='a' && c<='z') || (c>='A' && c<='Z')){
			c= tolower(c);
			k= tolower(k);
			out[i] = ((c -'a') + (key[j] -'a')) % 26 + 'a';
			j++;
		}else{
			out[i] = in[i];
		}
	}

	return 1;
}

static int decryptVigenere(const char* in, const char *key, char* out){
	int inLength = strlen(in);
	int keyLength = strlen(key);
	int i=0, j=0;
	char c;
	char k;

	for(i=0; i<inLength; i++){
		if(j>=keyLength){
			j=0;
		}
		c = in[i];
		k = key[j];
		if((c>='a' && c<='z') || (c>='A' && c<='Z')){
			c= tolower(c);
			k= tolower(k);
			if((c = (c -'a') - (k -'a')) < 0){
				c += 26;
			}
			out[i] = c % 26 + 'a';
			j++;
		}else{
			out[i] = in[i];
		}
	}

	return 1;
}

/*
 * Initialization:
 *  1. Register device driver
 *  2. Allocate buffer
 *  3. Initialize buffer
 */
int encrypter_init(void)
{
    int result;

    printk(KERN_INFO "Inserting encrypter module (major number %d)\n", encrypter_major);

    /* Registering device. */
    result = register_chrdev(encrypter_major, "encrypter", &encrypter_fops);
    if (result < 0)
    {
        printk(KERN_INFO "encrypter: cannot obtain major number %d\n", encrypter_major);
        return result;
    }



    /* Allocating memory for the buffer. */
    encrypter_buffer = kmalloc(BUF_LEN, GFP_KERNEL);
    if (!encrypter_buffer)
    {
        result = -ENOMEM;
        goto fail_no_mem;
    }

    memset(encrypter_buffer, 0, BUF_LEN);

    return 0;

fail_no_mem:
    unregister_chrdev(encrypter_major, "encrypter");
    return result;
}

/*
 * Cleanup:
 *  1. Free buffer
 *  2. Unregister device driver
 */
void encrypter_exit(void)
{
    printk(KERN_INFO "Removing encrypter module\n");

    /* Freeing buffer memory. */
    if (encrypter_buffer)
    {
        kfree(encrypter_buffer);
    }

    /* Freeing the major number. */
    unregister_chrdev(encrypter_major, "encrypter");
}

/* File open function. */
static int encrypter_open(struct inode *inode, struct file *filp)
{
    /* Initialize driver variables here. */

    /* Reset the device here. */

    /* Success. */
    return 0;
}

/* File close function. */
static int encrypter_release(struct inode *inode, struct file *filp)
{
    /* Free memory allocated in encrypter_open here. */

    /* Success. */
    return 0;
}

/*
 * File read function
 *  Parameters:
 *   filp  - a type file structure;
 *   buf   - a buffer, from which the user space function (fread) will read;
 *   len - a counter with the number of bytes to transfer, which has the same
 *           value as the usual counter in the user space function (fread);
 *   f_pos - a position of where to start reading the file;
 *  Operation:
 *   The encrypter_read function transfers data from the driver buffer (encrypter_buffer)
 *   to user space with the function copy_to_user.
 */
static ssize_t encrypter_read(struct file *filp, char *buf, size_t len, loff_t *f_pos)
{
    /* Size of valid data in memory - data to send in user space. */
    int data_size = 0;

    if (*f_pos == 0)
    {
        /* Get size of valid data. */
        data_size = strlen(encrypter_buffer);

        /* Send data to user space. */
        if (copy_to_user(buf, encrypter_buffer, data_size) != 0)
        {
            return -EFAULT;
        }
        else
        {
            (*f_pos) += data_size;

            return data_size;
        }
    }
    else
    {
        return 0;
    }
}

/*
 * File write function
 *  Parameters:
 *   filp  - a type file structure;
 *   buf   - a buffer in which the user space function (fwrite) will write;
 *   len - a counter with the number of bytes to transfer, which has the same
 *           values as the usual counter in the user space function (fwrite);
 *   f_pos - a position of where to start writing in the file;
 *  Operation:
 *   The function copy_from_user transfers the data from user space to kernel space.
 */
static ssize_t encrypter_write(struct file *filp, const char *buf, size_t len, loff_t *f_pos)
{
    int i = 0;

    /* Reset memory. */
    memset(encrypter_buffer, 0, BUF_LEN);

    /* Get data from user space.*/
    if (copy_from_user(encrypter_buffer, buf, len) != 0)
    {
        return -EFAULT;
    }
    else
    {
        if(encryption_enable){
                /* Encrypt received data. */      	
                for(i=0; i < len; ++i){
                    encrypter_buffer[i] = ~encrypter_buffer[i];
                }
        }
        return len;
    }
}

/* Encryption switch controller
 *  Parameters:
 *   file  - a type file structure;
 *   cmd  - user command;
 *   arg  - an optional argument depended on the driver
 *
 * cmd values: 
 *   0 to disable encryption
 *   1 (or else) to enable encryption
 *
 * NOTE: After changes the current data in the buffer MUST be discarded.
 */
static long encrypter_ioctl(struct file *file, unsigned int cmd, unsigned long arg){
    int i = 0;
	encryption_enable = cmd;
	error_injection = arg;
    printk(KERN_INFO "Encryption enable updated\n");
     if (arg == ERROR_INJECTION){
         for(i=0; i < BUF_LEN; ++i){
            encrypter_buffer[i] = ~encrypter_buffer[i];
        }
    }
    
	return 0;
}
