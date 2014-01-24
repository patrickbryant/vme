/*
===============================================================================
                            COPYRIGHT NOTICE

    Copyright (C) 2003-2005 GE Fanuc
    International Copyright Secured.  All Rights Reserved.

-------------------------------------------------------------------------------

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

   o Redistributions of source code must retain the above copyright notice, this
     list of conditions and the following disclaimer.
   o Redistributions in binary form must reproduce the above copyright notice,
     this list of conditions and the following disclaimer in the documentation
     and/or other materials provided with the distribution.
   o Neither the name of GE Fanuc nor the names of its contributors may be used
     to endorse or promote products derived from this software without specific
     prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

===============================================================================
*/


#include <linux/config.h>
#ifdef CONFIG_SMP
#define __SMP__
#endif

#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/reboot.h>
#include <linux/watchdog.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include "vmiwdt-7760.h"


#ifdef DEBUG
#define DPRINTF(x...)  printk(KERN_DEBUG MOD_NAME": "x)
#else
#define DPRINTF(x...)
#endif


/* Older 2.4.x kernel versions did not have *TIMEOUT operations in "watchdog.h"
 */
#ifndef WDIOC_SETTIMEOUT
#define WDIOC_SETTIMEOUT        _IOWR(WATCHDOG_IOCTL_BASE, 6, int)
#define WDIOC_GETTIMEOUT        _IOR(WATCHDOG_IOCTL_BASE, 7, int)
#define WDIOF_SETTIMEOUT        0x0080
#endif				/* WDIOC_SETTIMEOUT */


MODULE_AUTHOR("GE Fanuc <www.gefanuc.com/embedded>");
MODULE_DESCRIPTION("GE Fanuc Watchdog Timer Driver");
#ifdef MODULE_LICENSE
MODULE_LICENSE("Dual BSD/GPL");
#endif


static int vmiwdt_open(struct inode *inode, struct file *file_ptr);
static int vmiwdt_close(struct inode *inode, struct file *file_ptr);
static ssize_t vmiwdt_write(struct file *file_ptr, const char *buffer,
			    size_t nbytes, loff_t * off);
static int vmiwdt_ioctl(struct inode *inode, struct file *file_ptr,
			unsigned int cmd, unsigned long arg);
static int vmiwdt_notify(struct notifier_block *notify, unsigned long event,
			 void *reserved);
static struct file_operations file_ops = {
	.owner = THIS_MODULE,
	.write = vmiwdt_write,
	.ioctl = vmiwdt_ioctl,
	.open = vmiwdt_open,
	.release = vmiwdt_close
};


static struct miscdevice miscdev = {
	WATCHDOG_MINOR,
	MOD_NAME,
	&file_ops
};

static struct notifier_block vmiwdt_notifier = {
	vmiwdt_notify,
	NULL,
	0
};

static int vmiwdt_count = 0;
static spinlock_t vmiwdt_lock = SPIN_LOCK_UNLOCKED;
static int vmiwdt_timeout = VMIWDT_WCSR__TO__60s;


/*============================================================================
 * Kick the dog
 */
static inline void __wdt_keepalive(void)
{
	uint8_t csr;

	csr = inb(VMIWDT_WCSR);
	outb(csr | VMIWDT_WCSR__KEEPALIVE, VMIWDT_WCSR);
	outb(csr & ~VMIWDT_WCSR__KEEPALIVE, VMIWDT_WCSR);
	DPRINTF("Keepalive\n");
}


/*============================================================================
 * Kick the dog
 */
static void wdt_keepalive(void)
{
	spin_lock(&vmiwdt_lock);
	__wdt_keepalive();
	spin_unlock(&vmiwdt_lock);
}


/*============================================================================
 * Get timeout - returns a VMIWDT_WCSR__TO__ value.
 */
static uint8_t __wdt_get_timeout(void)
{
	uint8_t timeout;

	spin_lock(&vmiwdt_lock);
	timeout = inb(VMIWDT_WCSR) & VMIWDT_WCSR__TO;
	spin_unlock(&vmiwdt_lock);

	return timeout;
}


/*============================================================================
 * Get timeout - returns timeout value in number of milliseconds
 */
static int wdt_get_timeout(void)
{
	int timeout;

	timeout = __wdt_get_timeout();

	switch (timeout) {
	case VMIWDT_WCSR__TO__100ms:
		timeout = 100;
		break;
	case VMIWDT_WCSR__TO__1s:
		timeout = 1000;
		break;
	case VMIWDT_WCSR__TO__10s:
		timeout = 10000;
		break;
	case VMIWDT_WCSR__TO__60s:
		timeout = 60000;
		break;
	}

	return timeout;
}


/*============================================================================
 * Set timeout - input is a VMIWDT_WCSR__TO__ value.
 */
static inline int __wdt_set_timeout(int timeout)
{
	uint8_t csr;

	switch (timeout) {
	case VMIWDT_WCSR__TO__100ms:
	case VMIWDT_WCSR__TO__1s:
	case VMIWDT_WCSR__TO__10s:
	case VMIWDT_WCSR__TO__60s:
	case VMIWDT_WCSR__DISABLE:
		break;
	default:
		return -EINVAL;
	}

	csr = inb(VMIWDT_WCSR);
	outb((csr & ~VMIWDT_WCSR__TO) | timeout, VMIWDT_WCSR);
	vmiwdt_timeout = timeout;

	DPRINTF("WCSR = %#x\n", inb(VMIWDT_WCSR));

	return 0;
}


/*============================================================================
 * Set timeout - returns the actual timeout value set.
 */
static int wdt_set_timeout(int *timeout)
{
	int timeoutval;
	int rval;

	if (*timeout <= 100) {
		timeoutval = VMIWDT_WCSR__TO__100ms;
		*timeout = 100;
	} else if (*timeout <= 1000) {
		timeoutval = VMIWDT_WCSR__TO__1s;
		*timeout = 1000;
	} else if (*timeout <= 10000) {
		timeoutval = VMIWDT_WCSR__TO__10s;
	} else {
		timeoutval = VMIWDT_WCSR__TO__60s;
		*timeout = 60000;
	}

	DPRINTF("Setting timeout value to %dms\n", *timeout);

	spin_lock(&vmiwdt_lock);
	rval = __wdt_set_timeout(timeoutval);
	spin_unlock(&vmiwdt_lock);

	return rval;
}


/*============================================================================
 * Enable the WDT
 */
static void wdt_enable(void)
{
	spin_lock(&vmiwdt_lock);
	__wdt_set_timeout(vmiwdt_timeout);
	__wdt_keepalive();
	spin_unlock(&vmiwdt_lock);

	DPRINTF("Enabled watchdog timeout\n");
}


/*============================================================================
 * Disable the WDT
 */
static inline void __wdt_disable(void)
{
	__wdt_set_timeout(VMIWDT_WCSR__DISABLE);
	__wdt_keepalive();

	DPRINTF("Disabled watchdog timer\n");
}


/*============================================================================
 * Disable the WDT
 */
static void wdt_disable(void)
{
#ifndef CONFIG_WATCHDOG_NOWAYOUT
	spin_lock(&vmiwdt_lock);
	__wdt_disable();
	spin_unlock(&vmiwdt_lock);
#endif				/* CONFIG_WATCHDOG_NOWAYOUT */
}


/*============================================================================
 * Hook for notification of system events
 */
static int vmiwdt_notify(struct notifier_block *notify, unsigned long event,
			 void *reserved)
{
	/* Disable the timer upon shutdown or reboot regardless of 
	   whether CONFIG_WATCHDOG_NOWAYOUT is enabled or not. */
	if ((SYS_DOWN == event) || (SYS_HALT == event)) {
		__wdt_disable();
	}

	return NOTIFY_DONE;
}


/*============================================================================
 * Hook for the write file operation
 */
static ssize_t vmiwdt_write(struct file *file_ptr, const char *buffer,
			    size_t nbytes, loff_t * off)
{
	wdt_keepalive();
	return 1;
}


/*============================================================================
 * Hook to the ioctl file operation
 */
int vmiwdt_ioctl(struct inode *inode, struct file *file_ptr, uint32_t cmd,
		 unsigned long arg)
{
	int timeout, status = 0;
	static struct watchdog_info wdt_info = {
		.options = WDIOF_SETTIMEOUT,
		.firmware_version = 0,
		.identity = MOD_NAME
	};

	switch (cmd) {
	case WDIOC_GETSUPPORT:
		if (copy_to_user((void *) arg, &wdt_info, sizeof (wdt_info)))
			return -EFAULT;
		break;
	case WDIOC_GETSTATUS:
		if (copy_to_user((void *) arg, &status, sizeof (int)))
			return -EFAULT;
		break;
	case WDIOC_KEEPALIVE:
		wdt_keepalive();
		break;
	case WDIOC_SETTIMEOUT:
		if (copy_from_user(&timeout, (void *) arg, sizeof (int)))
			return -EFAULT;
		/* The wdt_set_timeout function expects milliseconds */
		timeout *= 1000;
		wdt_set_timeout(&timeout);
		timeout /= 1000;
		if (copy_to_user((void *) arg, &timeout, sizeof (int)))
			return -EFAULT;
		break;
#ifdef WDIOC_SETTIMEOUT_MS
	case WDIOC_SETTIMEOUT_MS:
#endif
		if (copy_from_user(&timeout, (void *) arg, sizeof (int)))
			return -EFAULT;
		wdt_set_timeout(&timeout);
		if (copy_to_user((void *) arg, &timeout, sizeof (int)))
			return -EFAULT;
		break;
	case WDIOC_GETTIMEOUT:
		/* The wdt_set_timeout function returns milliseconds */
		timeout = wdt_get_timeout();
		timeout /= 1000;
		if (copy_to_user((void *) arg, &timeout, sizeof (timeout)))
			return -EFAULT;
		break;
#ifdef WDIOC_GETTIMEOUT_MS
	case WDIOC_GETTIMEOUT_MS:
#endif
		timeout = wdt_get_timeout();
		if (copy_to_user((void *) arg, &timeout, sizeof (timeout)))
			return -EFAULT;
		break;
	default:
		return -ENOTTY;
	}

	return 0;
}


/*============================================================================
 * Hook for the open file operation
 */
static int vmiwdt_open(struct inode *inode, struct file *file_ptr)
{
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,5,0)
	MOD_INC_USE_COUNT;
#endif
	if (1 == ++vmiwdt_count) {
		wdt_enable();
	}

	return 0;
}


/*============================================================================
 * Hook for the close file operation
 */
static int vmiwdt_close(struct inode *inode, struct file *file_ptr)
{
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,5,0)
	MOD_DEC_USE_COUNT;
#endif
	if (0 == --vmiwdt_count) {
		wdt_disable();
	}

	return 0;
}


/*===========================================================================
 * Module initialization routine
 */
static int __init vmiwdt_init(void)
{
	int rval;

	if (!request_region(VMIWDT_WCSR, 1, MOD_NAME)) {
		printk(KERN_ERR MOD_NAME ": Error requesting region\n");
		return -EBUSY;
	}

	__wdt_disable();
	__wdt_set_timeout(vmiwdt_timeout);

	register_reboot_notifier(&vmiwdt_notifier);

	rval = misc_register(&miscdev);
	if (rval) {
		printk(KERN_ERR MOD_NAME ": Failed registering device\n");
		goto err_register;
	}

	printk(KERN_NOTICE MOD_NAME
	       ": Installed VMIC watchdog timer module version: %s\n",
	       MOD_VERSION);

	return 0;

      err_register:
	unregister_reboot_notifier(&vmiwdt_notifier);

	release_region(VMIWDT_WCSR, 1);

	return rval;
}


/*===========================================================================
 * Module exit routine
 */
static void __exit vmiwdt_cleanup(void)
{
	misc_deregister(&miscdev);

	/* If we unload the module, the watchdog gets disabled regardless of
	   whether CONFIG_WATCHDOG_NOWAYOUT is enabled or not. */
	__wdt_disable();

	unregister_reboot_notifier(&vmiwdt_notifier);

	release_region(VMIWDT_WCSR, 1);

	printk(KERN_NOTICE MOD_NAME
	       ": Exiting %s module version: %s\n", MOD_NAME, MOD_VERSION);
}


module_init(vmiwdt_init);
module_exit(vmiwdt_cleanup);
