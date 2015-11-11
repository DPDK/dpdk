/*-
 * GPL LICENSE SUMMARY
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of version 2 of the GNU General Public License as
 *   published by the Free Software Foundation.
 *
 *   This program is distributed in the hope that it will be useful, but
 *   WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *   General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *   The full GNU General Public License is included in this distribution
 *   in the file called LICENSE.GPL.
 *
 *   Contact Information:
 *   Intel Corporation
 */

#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/file.h>
#include <linux/fdtable.h>
#include <linux/syscalls.h>

#include "eventfd_link.h"


/*
 * get_files_struct is copied from fs/file.c
 */
struct files_struct *
get_files_struct(struct task_struct *task)
{
	struct files_struct *files;

	task_lock(task);
	files = task->files;
	if (files)
		atomic_inc(&files->count);
	task_unlock(task);

	return files;
}

/*
 * put_files_struct is extracted from fs/file.c
 */
void
put_files_struct(struct files_struct *files)
{
	if (atomic_dec_and_test(&files->count))
		BUG();
}

static struct file *
fget_from_files(struct files_struct *files, unsigned fd)
{
	struct file *file;

	rcu_read_lock();
	file = fcheck_files(files, fd);
	if (file) {
		if (file->f_mode & FMODE_PATH ||
			!atomic_long_inc_not_zero(&file->f_count)) {

			file = NULL;
		}
	}
	rcu_read_unlock();

	return file;
}

static long
eventfd_link_ioctl_copy2(unsigned long arg)
{
	void __user *argp = (void __user *) arg;
	struct task_struct *task_target = NULL;
	struct file *file;
	struct files_struct *files;
	struct eventfd_copy2 eventfd_copy2;
	long ret = -EFAULT;

	if (copy_from_user(&eventfd_copy2, argp, sizeof(struct eventfd_copy2)))
		goto out;

	/*
	 * Find the task struct for the target pid
	 */
	ret = -ESRCH;

	task_target =
		get_pid_task(find_vpid(eventfd_copy2.pid), PIDTYPE_PID);
	if (task_target == NULL) {
		pr_info("Unable to find pid %d\n", eventfd_copy2.pid);
		goto out;
	}

	ret = -ESTALE;
	files = get_files_struct(task_target);
	if (files == NULL) {
		pr_info("Failed to get target files struct\n");
		goto out_task;
	}

	ret = -EBADF;
	file = fget_from_files(files, eventfd_copy2.fd);
	put_files_struct(files);

	if (file == NULL) {
		pr_info("Failed to get fd %d from target\n", eventfd_copy2.fd);
		goto out_task;
	}

	/*
	 * Install the file struct from the target process into the
	 * newly allocated file desciptor of the source process.
	 */
	ret = get_unused_fd_flags(eventfd_copy2.flags);
	if (ret < 0) {
		fput(file);
		goto out_task;
	}
	fd_install(ret, file);

out_task:
	put_task_struct(task_target);
out:
	return ret;
}

static long
eventfd_link_ioctl_copy(unsigned long arg)
{
	void __user *argp = (void __user *) arg;
	struct task_struct *task_target = NULL;
	struct file *file;
	struct files_struct *files;
	struct fdtable *fdt;
	struct eventfd_copy eventfd_copy;
	long ret = -EFAULT;

	if (copy_from_user(&eventfd_copy, argp, sizeof(struct eventfd_copy)))
		goto out;

	/*
	 * Find the task struct for the target pid
	 */
	ret = -ESRCH;

	task_target =
		get_pid_task(find_vpid(eventfd_copy.target_pid), PIDTYPE_PID);
	if (task_target == NULL) {
		pr_info("Unable to find pid %d\n", eventfd_copy.target_pid);
		goto out;
	}

	ret = -ESTALE;
	files = get_files_struct(current);
	if (files == NULL) {
		pr_info("Failed to get current files struct\n");
		goto out_task;
	}

	ret = -EBADF;
	file = fget_from_files(files, eventfd_copy.source_fd);

	if (file == NULL) {
		pr_info("Failed to get fd %d from source\n",
			eventfd_copy.source_fd);
		put_files_struct(files);
		goto out_task;
	}

	/*
	 * Release the existing eventfd in the source process
	 */
	spin_lock(&files->file_lock);
	fput(file);
	filp_close(file, files);
	fdt = files_fdtable(files);
	fdt->fd[eventfd_copy.source_fd] = NULL;
	spin_unlock(&files->file_lock);

	put_files_struct(files);

	/*
	 * Find the file struct associated with the target fd.
	 */

	ret = -ESTALE;
	files = get_files_struct(task_target);
	if (files == NULL) {
		pr_info("Failed to get target files struct\n");
		goto out_task;
	}

	ret = -EBADF;
	file = fget_from_files(files, eventfd_copy.target_fd);
	put_files_struct(files);

	if (file == NULL) {
		pr_info("Failed to get fd %d from target\n",
			eventfd_copy.target_fd);
		goto out_task;
	}

	/*
	 * Install the file struct from the target process into the
	 * file desciptor of the source process,
	 */

	fd_install(eventfd_copy.source_fd, file);
	ret = 0;

out_task:
	put_task_struct(task_target);
out:
	return ret;
}

static long
eventfd_link_ioctl(struct file *f, unsigned int ioctl, unsigned long arg)
{
	long ret = -ENOIOCTLCMD;

	switch (ioctl) {
	case EVENTFD_COPY:
		ret = eventfd_link_ioctl_copy(arg);
		break;
	case EVENTFD_COPY2:
		ret = eventfd_link_ioctl_copy2(arg);
		break;
	}

	return ret;
}

static const struct file_operations eventfd_link_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = eventfd_link_ioctl,
};


static struct miscdevice eventfd_link_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "eventfd-link",
	.fops = &eventfd_link_fops,
};

static int __init
eventfd_link_init(void)
{
	return misc_register(&eventfd_link_misc);
}

module_init(eventfd_link_init);

static void __exit
eventfd_link_exit(void)
{
	misc_deregister(&eventfd_link_misc);
}

module_exit(eventfd_link_exit);

MODULE_VERSION("0.0.1");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Anthony Fee");
MODULE_DESCRIPTION("Link eventfd");
MODULE_ALIAS("devname:eventfd-link");
