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
#include <linux/gpio.h>

#define PROJECT_BUFFSIZE 51

struct proj_data 
{
    char project_buff[PROJECT_BUFFSIZE];
    int lenght;
};

static struct gpio leds[] = {
	{47, GPIOF_OUT_INIT_HIGH, "led0"},
};

/* Input parameter */
int mode = 0; //default value encrypted
module_param(mode, int, S_IWUSR | S_IRUSR); //alow user to write and read

/* Key used for encryption */
static int key = 7;

static struct proj_data project_data;
static int project_count=1;
static dev_t project_dev;
static struct cdev project_cdev;

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

/* Appends CRC to encrypted message (decrypted or encrypted) */
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

int project_open (struct inode *pnode, struct file *pfile)
{
	int i;
    printk(KERN_DEBUG "%s called...\n", __FUNCTION__);
	for(i = 0; i < ARRAY_SIZE(leds); i++) 
    {
		gpio_set_value(leds[i].gpio, 0);
	}
    return 0;
}

ssize_t project_read (struct file *pfile, char __user *buffer, size_t length, loff_t *offset)
{
    int msg_length = 0, data_size = 0;
    char crc;
    char *encrypted_crc;
    printk(KERN_DEBUG "%s called...\n", __FUNCTION__);

    /* If mode is encryption, then create message containing LENGHT, CRC and STRING */
    if (mode == 1)
    {
        encrypted_crc = encrypt_append_crc_length(project_data.project_buff, &msg_length, &crc);
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
        printk(KERN_DEBUG "driver [decrypted] = '%s'\n", project_data.project_buff);
        if (copy_to_user(buffer, project_data.project_buff, project_data.lenght + 1) != 0)
        {
            return -EFAULT;
        } 
        return project_data.lenght;
    } 
    else 
    {
        printk(KERN_DEBUG "Unknown encryption mode '%d'!\n", mode);
        return -EFAULT;
    }
}

ssize_t project_write (struct file *pfile, const char __user *buffer, size_t length, loff_t *offset)
{
    char* decrypted, *encrypted, msg_length, crc, dec_crc;
    int i;
    printk(KERN_DEBUG "%s called...\n", __FUNCTION__);

    /* Check requested length */
    if (length > 50)
    {
        printk(KERN_DEBUG "Requested write size '%d' exceeds allowed buffer driver size '%d'!\n", length, PROJECT_BUFFSIZE);
        return -EFAULT;
    }
    /* Reset memory. */
    memset(project_data.project_buff, 0, PROJECT_BUFFSIZE);

    if (mode == 1)
    {
        if (copy_from_user(project_data.project_buff, buffer, length))
        {
            return -EFAULT;
        }
        project_data.lenght = (int)length;
        return length;
    }
    else if (mode == 0)
    {
        /* If mode is decryption, then create message containing only STRING */
        if (copy_from_user(project_data.project_buff, buffer, length + 1))
        {
            return -EFAULT;
        }
        msg_length = project_data.project_buff[0];
        crc = project_data.project_buff[1];
        encrypted = project_data.project_buff + 2;
        decrypted = get_decrypted(encrypted, msg_length);
        if (decrypted == NULL)
        {
            return -EFAULT;
        }
        dec_crc = calculate_crc(decrypted, msg_length);
        memset(project_data.project_buff, 0, PROJECT_BUFFSIZE);
        strncpy(project_data.project_buff, decrypted, msg_length + 1);
        project_data.lenght = (int)msg_length + 1;

        if (crc == dec_crc)
        {
            printk(KERN_DEBUG "CRC sum match");
            for(i = 0; i < ARRAY_SIZE(leds); i++) 
            {
                gpio_set_value(leds[i].gpio, 1);
            }
        } 
        else
        {
            printk(KERN_DEBUG "CRC sum mismatch");
            printk(KERN_DEBUG "CRC sum match");
            for(i = 0; i < ARRAY_SIZE(leds); i++) 
            {
                gpio_set_value(leds[i].gpio, 0);
            }
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

 long project_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
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

int project_close (struct inode *pnode, struct file *pfile)
{
    printk(KERN_DEBUG "%s called...\n", __FUNCTION__);      
    return 0;
}

struct file_operations project_file_operations =
{
    .owner = THIS_MODULE,
    .open = project_open,
    .read = project_read,
    .write = project_write,
    .release = project_close,
    .unlocked_ioctl = project_ioctl,
};

/* Add your code here */
static int __init  project_init(void)
{
    int err;
    printk(KERN_DEBUG "%s called...\n", __FUNCTION__);
    if (alloc_chrdev_region(&project_dev, 0, project_count, "mailbox_module"))
    {
        err=-ENODEV;
        goto err_dev_unregister;
    }
    cdev_init(&project_cdev, &project_file_operations);
    if (cdev_add(&project_cdev, project_dev, project_count))
    {
        err=-ENODEV;
        goto err_dev_unregister;
    }
    gpio_request_array(leds, ARRAY_SIZE(leds));
    return 0;

    err_dev_unregister:
        unregister_chrdev_region(project_dev, project_count);
        return err;
}

static void __exit project_exit(void)
{
    int i;
    printk(KERN_DEBUG "%s called...\n", __FUNCTION__);
    cdev_del(&project_cdev);
    unregister_chrdev_region(project_dev, project_count);
	for(i = 0; i < ARRAY_SIZE(leds); i++) 
    {
		gpio_set_value(leds[i].gpio, 0);
	}
}

module_init(project_init);
module_exit(project_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("This module is used for encrypting data using Caesar Cipher.");
MODULE_AUTHOR("Room2");

