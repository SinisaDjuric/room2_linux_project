#include <linux/init.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/utsname.h>
#include <linux/moduleparam.h>
#include <linux/timekeeping.h>
#include <linux/fs.h> //for character device support
#include <linux/cdev.h>
#include <linux/uaccess.h> 
#include <linux/gfp.h>
#include <linux/string.h>
#include <linux/gpio.h> //for device GPIO support

#define mailbox_BUFFSIZE 51

/* Struct used for storing messages inside driver */
struct proj_data 
{
    char mailbox_buff[mailbox_BUFFSIZE];
    int lenght;
};

/* Struct holdion diode data */
static struct gpio diode = {47, GPIOF_OUT_INIT_HIGH, "led0"};

/* Input parameter */
int mode = 0; //default value encrypted
module_param(mode, int, S_IWUSR | S_IRUSR);

/* Key used for encryption */
static int key = 7;

/* Variables used for driver initialization */
static struct proj_data mailbox_data;
static int mailbox_count=1;
static dev_t mailbox_dev;
static struct cdev mailbox_cdev;

/* Turns diode on */
void turn_diode_on(void) 
{
	gpio_set_value(diode.gpio, 1);
}

/* Turns diode off */
void turn_diode_off(void) 
{
	gpio_set_value(diode.gpio, 0);
}

/* Function for calulcation CRC of message */
char calculate_crc(char* message, int lenght)
{
    char sum = 0;
    int i;
    for(i = 0; i < lenght; ++i)
    {
        sum += message[i];
    }
    return ~sum;
}

/* Encrypts message using Caesar Cipher (key of encryption is static variable) */
char* get_encrypted(char* message, int* length)
{
    int size = 0, i;
    char ch, *encrypted;
    size = strlen(message);
    /* Save message length due to encryption, meaybe output can be "mef\0fwr\0\0" */
    *length = size;
    encrypted = (char*)kmalloc_array(size + 1, sizeof(char), GFP_ATOMIC);
    if (encrypted == NULL) 
    {
        printk(KERN_ERR "Failed to allocate memmory inside %s function.\n", __FUNCTION__);
        return NULL;
    }
    strncpy(encrypted, message, size + 1);

    for(i = 0; encrypted[i] != '\0'; ++i)
    {
        ch = encrypted[i];
        if(ch >= 'a' && ch <= 'z')
        {
            ch = ch + key;
            if(ch > 'z')
            {
                ch = ch - 'z' + 'a' - 1;
            }
            encrypted[i] = ch;
        }
        else if(ch >= 'A' && ch <= 'Z')
        {
            ch = ch + key;
            if(ch > 'Z')
            {
                ch = ch - 'Z' + 'A' - 1;
            }
            encrypted[i] = ch;
        }
    }
    return encrypted;
}

/* Decrypts message using Caesar Cipher (key of encryption is static variable) */
char* get_decrypted(char* encrypted, int length)
{
    int i;
    char ch, *decrypted;
    decrypted = (char*)kmalloc_array(length + 1, sizeof(char), GFP_ATOMIC);
    if (decrypted == NULL) 
    {
        printk(KERN_ERR "Failed to allocate memmory inside %s function.\n", __FUNCTION__);
        return NULL;
    }
    strncpy(decrypted, encrypted, length + 1);

    for(i = 0; i < length; ++i)
    {
        ch = decrypted[i];
        if(ch >= 'a' && ch <= 'z')
        {
            ch = ch - key;
            if(ch < 'a')
            {
                ch = ch + 'z' - 'a' + 1;
            }
            decrypted[i] = ch;
        }
        else if(ch >= 'A' && ch <= 'Z')
        {
            ch = ch - key;
            if(ch < 'A')
            {
                ch = ch + 'Z' - 'A' + 1;
            }
            decrypted[i] = ch;
        }
    }
    return decrypted;
}

/* Appends CRC to encrypted message */
char* encrypt_append_crc_length(char* message, int* length, char* crc)
{
    char* encrypted, *message_crc;
    char sum = 0;
    int j = 0;

    encrypted = get_encrypted(message, length);
    sum = calculate_crc(message, *length);
    if (encrypted == NULL)
    {
        return NULL;
    }
    message_crc = (char*)kmalloc_array(*length + 3, sizeof(char), GFP_ATOMIC);
    if (message_crc == NULL) 
    {
        printk(KERN_ERR "Failed to allocate memmory inside %s function.\n", __FUNCTION__);
        return NULL;
    }
    message_crc[0] = (char)(*length);
    message_crc[1] = sum;
    for(j = 0; j < *length; j++)
    {
        message_crc[j+2] = encrypted[j];
    }
    message_crc[*length + 2] = '\0';
    *crc = sum;
    return message_crc;
}

/* Triggered when device file is opened */
int mailbox_open (struct inode *pnode, struct file *pfile)
{
    printk(KERN_DEBUG "%s called...\n", __FUNCTION__);
    turn_diode_off();
    return 0;
}

/* Triggered when data is read from device file */
ssize_t mailbox_read (struct file *pfile, char __user *buffer, size_t length, loff_t *offset)
{
    int msg_length = 0, data_size = 0;
    char crc;
    char *encrypted_crc;
    printk(KERN_DEBUG "%s called...\n", __FUNCTION__);

    /* If mode is encryption, then create message containing LENGHT, CRC and STRING */
    if (mode == 1)
    {
        encrypted_crc = encrypt_append_crc_length(mailbox_data.mailbox_buff, &msg_length, &crc);
        if(encrypted_crc == NULL)
        {
            return -EFAULT;
        }
        if (*offset == 0)
        {
            /* Send data to user space */
            data_size = msg_length + 3;
            printk(KERN_DEBUG "driver [encrypted] = '%s'\n", encrypted_crc + 2);
            if (copy_to_user(buffer, encrypted_crc, data_size) != 0)
            {
                return -EFAULT;
            }

            kfree(encrypted_crc);
            return data_size;
        }
        else
        {
            kfree(encrypted_crc);
            return 0;
        }
    }
    else if (mode == 0) 
    { 
        /* If mode is decryption, then create message containing only STRING */
        printk(KERN_DEBUG "driver [decrypted] = '%s'\n", mailbox_data.mailbox_buff);
        if (copy_to_user(buffer, mailbox_data.mailbox_buff, mailbox_data.lenght + 1) != 0)
        {
            return -EFAULT;
        } 
        return mailbox_data.lenght;
    } 
    else 
    {
        printk(KERN_DEBUG "Unknown encryption mode '%d'!\n", mode);
        return -EFAULT;
    }
}

/* Triggered when data is written to device file */
ssize_t mailbox_write (struct file *pfile, const char __user *buffer, size_t length, loff_t *offset)
{
    char* decrypted, *encrypted, msg_length, crc, dec_crc;
    printk(KERN_DEBUG "%s called...\n", __FUNCTION__);

    /* Check requested length */
    if (length > 50)
    {
        printk(KERN_DEBUG "Requested write size '%d' exceeds allowed buffer driver size '%d'!\n", length, mailbox_BUFFSIZE);
        return -EFAULT;
    }
    /* Reset memory. */
    memset(mailbox_data.mailbox_buff, 0, mailbox_BUFFSIZE);

    if (mode == 1)
    {
        if (copy_from_user(mailbox_data.mailbox_buff, buffer, length))
        {
            return -EFAULT;
        }
        mailbox_data.lenght = (int)length;
        return length;
    }
    else if (mode == 0)
    {
        /* If mode is decryption, then create message containing only STRING */
        if (copy_from_user(mailbox_data.mailbox_buff, buffer, length + 1))
        {
            return -EFAULT;
        }
        msg_length = mailbox_data.mailbox_buff[0];
        crc = mailbox_data.mailbox_buff[1];
        encrypted = mailbox_data.mailbox_buff + 2;
        decrypted = get_decrypted(encrypted, msg_length);
        if (decrypted == NULL)
        {
            return -EFAULT;
        }
        dec_crc = calculate_crc(decrypted, msg_length);
        memset(mailbox_data.mailbox_buff, 0, mailbox_BUFFSIZE);
        strncpy(mailbox_data.mailbox_buff, decrypted, msg_length + 1);
        mailbox_data.lenght = (int)msg_length + 1;

        if (crc == dec_crc)
        {
            printk(KERN_DEBUG "CRC sum match");
            turn_diode_on();
        } 
        else
        {
            printk(KERN_DEBUG "CRC sum mismatch");
            printk(KERN_DEBUG "CRC sum match");
            turn_diode_off();
        }
        kfree(decrypted);
        return length;
    }
    else
    {
        printk(KERN_DEBUG "Unknown encryption mode '%d'!\n", mode);
        return -EFAULT;
    }
}

/* Triggered when ioctl function is called from user space application */
 long mailbox_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
 {
    int ret = 0;
    printk(KERN_DEBUG "%s called...\n", __FUNCTION__);
    switch (cmd)
    {
    case 1:
        printk(KERN_DEBUG "Driver mode = 'Encryption'\n");
        mode = cmd;
        break;
    case 0:
        printk(KERN_DEBUG "Driver mode = 'Decryption'\n");
        mode = cmd;
        break;
    default:
        printk(KERN_DEBUG "Unknown encryption mode '%d'!\n", mode);
        ret = -EFAULT;
        break;
    }
    return ret;
}

/* Triggered when device file is closed */
int mailbox_close (struct inode *pnode, struct file *pfile)
{
    printk(KERN_DEBUG "%s called...\n", __FUNCTION__);      
    return 0;
}

/* Struct that defines device file operations and functions handling those operations */
struct file_operations mailbox_file_operations = {
    .owner = THIS_MODULE,
    .open = mailbox_open,
    .read = mailbox_read,
    .write = mailbox_write,
    .release = mailbox_close,
    .unlocked_ioctl = mailbox_ioctl,
};

/* Triggered when driver is insterted to kernel */
static int __init  mailbox_init(void)
{
    int err;
    printk(KERN_DEBUG "%s called...\n", __FUNCTION__);
    if (alloc_chrdev_region(&mailbox_dev, 0, mailbox_count, "mailbox_module"))
    {
        err=-ENODEV;
        goto err_dev_unregister;
    }
    cdev_init(&mailbox_cdev, &mailbox_file_operations);
    if (cdev_add(&mailbox_cdev, mailbox_dev, mailbox_count))
    {
        err=-ENODEV;
        goto err_dev_unregister;
    }
    gpio_request_array(&diode, 1);
    return 0;

    err_dev_unregister:
        unregister_chrdev_region(mailbox_dev, mailbox_count);
        return err;
}


/* Triggered when driver is removed from kernel */
static void __exit mailbox_exit(void)
{
    printk(KERN_DEBUG "%s called...\n", __FUNCTION__);
    cdev_del(&mailbox_cdev);
    unregister_chrdev_region(mailbox_dev, mailbox_count);
	turn_diode_off();
}

module_init(mailbox_init);
module_exit(mailbox_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("This module is used for encrypting data using Caesar Cipher.");
MODULE_AUTHOR("Room2");

