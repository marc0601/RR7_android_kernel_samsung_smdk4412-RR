/*
 * fs/sdcardfs/packagelist.c
 *
 * Copyright (c) 2013 Samsung Electronics Co. Ltd
 *   Authors: Daeho Jeong, Woojoong Lee, Seunghwan Hyun,
 *               Sunghwan Yun, Sungjong Seo
 *
 * This program has been developed as a stackable file system based on
 * the WrapFS which written by
 *
 * Copyright (c) 1998-2011 Erez Zadok
 * Copyright (c) 2009     Shrikar Archak
 * Copyright (c) 2003-2011 Stony Brook University
 * Copyright (c) 2003-2011 The Research Foundation of SUNY
 *
 * This file is dual licensed.  It may be redistributed and/or modified
 * under the terms of the Apache 2.0 License OR version 2 of the GNU
 * General Public License.
 */

#include "sdcardfs.h"
#include <linux/delay.h>
#include <linux/hashtable.h>
#include <linux/init.h>
#include <linux/inotify.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/syscalls.h>

#include <linux/configfs.h>

#define STRING_BUF_SIZE		(512)

#ifdef CONFIG_SDCARD_FS_ANDROID_PKGLIST
/* Path to system-provided mapping of package name to appIds */
static const char* const kpackageslist_file = "/data/system/packages.list";

/* Supplementary groups to execute with */
static const gid_t kgroups[1] = { AID_PACKAGE_INFO };
#endif

struct hashtable_entry {
        struct hlist_node hlist;
        void *key;
	unsigned int value;
};

struct sb_list {
	struct super_block *sb;
	struct list_head list;
};

struct packagelist_data {
	DECLARE_HASHTABLE(package_to_appid,8);
	spinlock_t hashtable_lock;
#ifdef CONFIG_SDCARD_FS_ANDROID_PKGLIST
	struct task_struct *thread_id;
#endif
	char read_buf[STRING_BUF_SIZE];
	char event_buf[STRING_BUF_SIZE];
	char app_name_buf[STRING_BUF_SIZE];
	char gids_buf[STRING_BUF_SIZE];
};

static struct packagelist_data *pkgl_data_all;

static struct kmem_cache *hashtable_entry_cachep;

static unsigned int str_hash(const char *key) {
	int i;
	unsigned int h = strlen(key);
	char *data = (char *)key;

	for (i = 0; i < strlen(key); i++) {
		h = h * 31 + *data;
		data++;
	}
	return h;
}

appid_t get_appid(void *pkgl_id, const char *app_name)
{
	struct packagelist_data *pkgl_dat = pkgl_data_all;
	struct hashtable_entry *hash_cur;
	struct hlist_node *h_n;
	unsigned int hash = str_hash(app_name);
	appid_t ret_id;

	spin_lock(&pkgl_dat->hashtable_lock);
	hash_for_each_possible(pkgl_dat->package_to_appid, hash_cur, h_n, hlist, hash) {
		if (!strcasecmp(app_name, hash_cur->key)) {
			ret_id = (appid_t)hash_cur->value;
			spin_unlock(&pkgl_dat->hashtable_lock);
			return ret_id;
		}
	}
	spin_unlock(&pkgl_dat->hashtable_lock);
	return 0;
}

/* Kernel has already enforced everything we returned through
 * derive_permissions_locked(), so this is used to lock down access
 * even further, such as enforcing that apps hold sdcard_rw. */
int check_caller_access_to_name(struct inode *parent_node, const char* name) {

	/* Always block security-sensitive files at root */
	if (parent_node && SDCARDFS_I(parent_node)->perm == PERM_ROOT) {
		if (!strcasecmp(name, "autorun.inf")
			|| !strcasecmp(name, ".android_secure")
			|| !strcasecmp(name, "android_secure")) {
			return 0;
		}
	}

	/* Root always has access; access for any other UIDs should always
	 * be controlled through packages.list. */
	if (current_fsuid() == 0) {
		return 1;
	}

	/* No extra permissions to enforce */
	return 1;
}

/* This function is used when file opening. The open flags must be
 * checked before calling check_caller_access_to_name() */
int open_flags_to_access_mode(int open_flags) {
	if((open_flags & O_ACCMODE) == O_RDONLY) {
		return 0; /* R_OK */
	} else if ((open_flags & O_ACCMODE) == O_WRONLY) {
		return 1; /* W_OK */
	} else {
		/* Probably O_RDRW, but treat as default to be safe */
		return 1; /* R_OK | W_OK */
	}
}

static int insert_str_to_int_lock(struct packagelist_data *pkgl_dat, char *key,
		unsigned int value)
{
	struct hashtable_entry *hash_cur;
	struct hashtable_entry *new_entry;
	struct hlist_node *h_n;
	unsigned int hash = str_hash(key);

	hash_for_each_possible(pkgl_dat->package_to_appid, hash_cur, h_n, hlist, hash) {
		if (!strcasecmp(key, hash_cur->key)) {
			hash_cur->value = value;
			return 0;
		}
	}
	new_entry = kmem_cache_alloc(hashtable_entry_cachep, GFP_ATOMIC);
	if (!new_entry)
		return -ENOMEM;
	new_entry->key = kstrdup(key, GFP_ATOMIC);
	new_entry->value = value;
	hash_add(pkgl_dat->package_to_appid, &new_entry->hlist, hash);
	return 0;
}

static void fixup_perms(struct super_block *sb, const char *key) {
	if (sb && sb->s_magic == SDCARDFS_SUPER_MAGIC) {
		fixup_perms_recursive(sb->s_root, key, strlen(key));
	}
}

static int insert_str_to_int(struct packagelist_data *pkgl_dat, char *key,
		unsigned int value) {
	int ret;
	struct sdcardfs_sb_info *sbinfo;
	mutex_lock(&sdcardfs_super_list_lock);
	spin_lock(&pkgl_dat->hashtable_lock);
	ret = insert_str_to_int_lock(pkgl_dat, key, value);
	spin_unlock(&pkgl_dat->hashtable_lock);

	list_for_each_entry(sbinfo, &sdcardfs_super_list, list) {
		if (sbinfo) {
			fixup_perms(sbinfo->sb, key);
		}
	}
	mutex_unlock(&sdcardfs_super_list_lock);
	return ret;
}

static void remove_str_to_int_lock(struct hashtable_entry *h_entry) {
	kfree(h_entry->key);
	hash_del(&h_entry->hlist);
	kmem_cache_free(hashtable_entry_cachep, h_entry);
}

static void remove_str_to_int(struct packagelist_data *pkgl_dat, const char *key)
{
	struct sdcardfs_sb_info *sbinfo;
	struct hashtable_entry *hash_cur;
	struct hlist_node *h_n;
	unsigned int hash = str_hash(key);
	mutex_lock(&sdcardfs_super_list_lock);
	spin_lock(&pkgl_data_all->hashtable_lock);
	hash_for_each_possible(pkgl_dat->package_to_appid, hash_cur, h_n, hlist, hash) {
		if (!strcasecmp(key, hash_cur->key)) {
			remove_str_to_int_lock(hash_cur);
			break;
		}
	}
	spin_unlock(&pkgl_data_all->hashtable_lock);
	list_for_each_entry(sbinfo, &sdcardfs_super_list, list) {
		if (sbinfo) {
			fixup_perms(sbinfo->sb, key);
		}
	}
	mutex_unlock(&sdcardfs_super_list_lock);
	return;
}

static void remove_all_hashentries_locked(struct packagelist_data *pkgl_dat)
{
	struct hashtable_entry *hash_cur;
	struct hlist_node *h_n;
	struct hlist_node *h_t;
	int i;
	hash_for_each_safe(pkgl_dat->package_to_appid, i, h_t, h_n, hash_cur, hlist)
                remove_str_to_int_lock(hash_cur);
	hash_init(pkgl_dat->package_to_appid);
}

#ifdef CONFIG_SDCARD_FS_ANDROID_PKGLIST
static int read_package_list(struct packagelist_data *pkgl_dat) {

	int ret;
	int fd;
	int read_amount;

	printk(KERN_INFO "sdcardfs: read_package_list\n");

	spin_lock(&pkgl_dat->hashtable_lock);

	remove_all_hashentries_locked(pkgl_dat);

	fd = sys_open(kpackageslist_file, O_RDONLY, 0);
	if (fd < 0) {
		printk(KERN_ERR "sdcardfs: failed to open package list\n");
		spin_unlock(&pkgl_dat->hashtable_lock);
		return fd;
	}

	while ((read_amount = sys_read(fd, pkgl_dat->read_buf,
					sizeof(pkgl_dat->read_buf))) > 0) {
		int appid;
		int one_line_len = 0;
		int additional_read;

		while (one_line_len < read_amount) {
			if (pkgl_dat->read_buf[one_line_len] == '\n') {
				one_line_len++;
				break;
			}
			one_line_len++;
		}
		additional_read = read_amount - one_line_len;
		if (additional_read > 0)
			sys_lseek(fd, -additional_read, SEEK_CUR);

		if (sscanf(pkgl_dat->read_buf, "%s %d %*d %*s %*s %s",
				pkgl_dat->app_name_buf, &appid,
				pkgl_dat->gids_buf) == 3) {
			ret = insert_str_to_int_lock(pkgl_dat, pkgl_dat->app_name_buf, appid);
			if (ret) {
				sys_close(fd);
				spin_unlock(&pkgl_dat->hashtable_lock);
				return ret;
			}
		}
	}

	sys_close(fd);
	spin_unlock(&pkgl_dat->hashtable_lock);
	return 0;
}

static int packagelist_reader(void *thread_data)
{
	struct packagelist_data *pkgl_dat = (struct packagelist_data *)thread_data;
	struct inotify_event *event;
	bool active = false;
	int event_pos;
	int event_size;
	int res = 0;
	int nfd;

	allow_signal(SIGINT);

	nfd = sys_inotify_init();
	if (nfd < 0) {
		printk(KERN_ERR "sdcardfs: inotify_init failed: %d\n", nfd);
		return nfd;
	}

	while (!kthread_should_stop()) {
		if (signal_pending(current)) {
			ssleep(1);
			continue;
		}

		if (!active) {
			res = sys_inotify_add_watch(nfd, kpackageslist_file, IN_DELETE_SELF);
			if (res < 0) {
				if (res == -ENOENT || res == -EACCES) {
				/* Framework may not have created yet, sleep and retry */
					printk(KERN_ERR "sdcardfs: missing packages.list; retrying\n");
					ssleep(2);
					printk(KERN_ERR "sdcardfs: missing packages.list_end; retrying\n");
					continue;
				} else {
					printk(KERN_ERR "sdcardfs: inotify_add_watch failed: %d\n", res);
					goto interruptable_sleep;
				}
			}
			/* Watch above will tell us about any future changes, so
			 * read the current state. */
			res = read_package_list(pkgl_dat);
			if (res) {
				printk(KERN_ERR "sdcardfs: read_package_list failed: %d\n", res);
				goto interruptable_sleep;
			}
			active = true;
		}

		event_pos = 0;
		res = sys_read(nfd, pkgl_dat->event_buf, sizeof(pkgl_dat->event_buf));
		if (res < (int) sizeof(*event)) {
			if (res == -EINTR)
				continue;
			printk(KERN_ERR "sdcardfs: failed to read inotify event: %d\n", res);
			goto interruptable_sleep;
		}

		while (res >= (int) sizeof(*event)) {
			event = (struct inotify_event *) (pkgl_dat->event_buf + event_pos);

			printk(KERN_INFO "sdcardfs: inotify event: %08x\n", event->mask);
			if ((event->mask & IN_IGNORED) == IN_IGNORED) {
				/* Previously watched file was deleted, probably due to move
				 * that swapped in new data; re-arm the watch and read. */
				active = false;
			}

			event_size = sizeof(*event) + event->len;
			res -= event_size;
			event_pos += event_size;
		}
		continue;

interruptable_sleep:
		set_current_state(TASK_INTERRUPTIBLE);
		schedule();
	}
	flush_signals(current);
	sys_close(nfd);
	return res;
}

struct packagelist_data * package_reader_create(struct packagelist_data *pkgl_dat)
{
	struct task_struct *packagelist_thread;

	packagelist_thread = kthread_run(packagelist_reader, (void *)pkgl_dat, "pkgld");
	if (IS_ERR(packagelist_thread)) {
		printk(KERN_ERR "sdcardfs: creating kthread failed\n");
		kfree(pkgl_dat);
		goto out;
	}
	pkgl_dat->thread_id = packagelist_thread;

	printk(KERN_INFO "sdcardfs: created packagelist pkgld/%d\n",
				(int)pkgl_dat->thread_id->pid);

out:
	return pkgl_dat;
}

void package_reader_destroy(struct packagelist_data * pkgl_dat)
{
	force_sig_info(SIGINT, SEND_SIG_PRIV, pkgl_dat->thread_id);
	kthread_stop(pkgl_dat->thread_id);
}
#endif

static struct packagelist_data * packagelist_create(void)
{
	struct packagelist_data *pkgl_dat;

	pkgl_dat = kmalloc(sizeof(*pkgl_dat), GFP_KERNEL | __GFP_ZERO);
	if (!pkgl_dat) {
		printk(KERN_ERR "sdcardfs: Failed to create hash\n");
		return ERR_PTR(-ENOMEM);
	}

	spin_lock_init(&pkgl_dat->hashtable_lock);
	hash_init(pkgl_dat->package_to_appid);

#ifdef CONFIG_SDCARD_FS_ANDROID_PKGLIST
	return package_reader_create(pkgl_dat);
#else
	return pkgl_dat;
#endif
}

static void packagelist_destroy(struct packagelist_data *pkgl_dat)
{
	spin_lock(&pkgl_dat->hashtable_lock);
#ifdef CONFIG_SDCARD_FS_ANDROID_PKGLIST
	package_reader_destroy(pkgl_dat);
#endif
	remove_all_hashentries_locked(pkgl_dat);
	spin_unlock(&pkgl_dat->hashtable_lock);
	printk(KERN_INFO "sdcardfs: destroyed packagelist pkgld\n");
	kfree(pkgl_dat);
}

struct package_appid {
	struct config_item item;
	int add_pid;
};

static inline struct package_appid *to_package_appid(struct config_item *item)
{
	return item ? container_of(item, struct package_appid, item) : NULL;
}

static struct configfs_attribute package_appid_attr_add_pid = {
	.ca_owner = THIS_MODULE,
	.ca_name = "appid",
	.ca_mode = S_IRUGO | S_IWUGO,
};

static struct configfs_attribute *package_appid_attrs[] = {
	&package_appid_attr_add_pid,
	NULL,
};

static ssize_t package_appid_attr_show(struct config_item *item,
				      struct configfs_attribute *attr,
				      char *page)
{
	ssize_t count;
	count = sprintf(page, "%d\n", get_appid(pkgl_data_all, item->ci_name));
	return count;
}

static ssize_t package_appid_attr_store(struct config_item *item,
				       struct configfs_attribute *attr,
				       const char *page, size_t count)
{
	struct package_appid *package_appid = to_package_appid(item);
	unsigned long tmp;
	char *p = (char *) page;
	int ret;

	tmp = simple_strtoul(p, &p, 10);
	if (!p || (*p && (*p != '\n')))
		return -EINVAL;

	if (tmp > INT_MAX)
		return -ERANGE;
	ret = insert_str_to_int(pkgl_data_all, item->ci_name, (unsigned int)tmp);
	package_appid->add_pid = tmp;
	if (ret)
		return ret;

	return count;
}

static void package_appid_release(struct config_item *item)
{
	printk(KERN_INFO "sdcardfs: removing %s\n", item->ci_dentry->d_name.name);
	/* item->ci_name is freed already, so we rely on the dentry */
	remove_str_to_int(pkgl_data_all, item->ci_dentry->d_name.name);
	kfree(to_package_appid(item));
}

static struct configfs_item_operations package_appid_item_ops = {
	.release		= package_appid_release,
	.show_attribute		= package_appid_attr_show,
	.store_attribute	= package_appid_attr_store,
};

static struct config_item_type package_appid_type = {
	.ct_item_ops	= &package_appid_item_ops,
	.ct_attrs	= package_appid_attrs,
	.ct_owner	= THIS_MODULE,
};

struct sdcardfs_packages {
	struct config_group group;
};

static inline struct sdcardfs_packages *to_sdcardfs_packages(struct config_item *item)
{
	return item ? container_of(to_config_group(item), struct sdcardfs_packages, group) : NULL;
}

static struct config_item *sdcardfs_packages_make_item(struct config_group *group, const char *name)
{
	struct package_appid *package_appid;

	package_appid = kzalloc(sizeof(struct package_appid), GFP_KERNEL);
	if (!package_appid)
		return ERR_PTR(-ENOMEM);

	config_item_init_type_name(&package_appid->item, name,
				   &package_appid_type);

	package_appid->add_pid = 0;

	return &package_appid->item;
}

static struct configfs_attribute sdcardfs_packages_attr_description = {
	.ca_owner = THIS_MODULE,
	.ca_name = "packages_gid.list",
	.ca_mode = S_IRUGO,
};

static struct configfs_attribute *sdcardfs_packages_attrs[] = {
	&sdcardfs_packages_attr_description,
	NULL,
};

static ssize_t packages_attr_show(struct config_item *item,
					 struct configfs_attribute *attr,
					 char *page)
{
	struct hashtable_entry *hash_cur;
	struct hlist_node *h_t;
	struct hlist_node *h_n;
	int i;
	int count = 0, written = 0;
	char errormsg[] = "<truncated>\n";

	spin_lock(&pkgl_data_all->hashtable_lock);
	hash_for_each_safe(pkgl_data_all->package_to_appid, i, h_t, h_n, hash_cur, hlist) {
		written = scnprintf(page + count, PAGE_SIZE - sizeof(errormsg) - count, "%s %d\n", (char *)hash_cur->key, hash_cur->value);
		if (count + written == PAGE_SIZE - sizeof(errormsg)) {
			count += scnprintf(page + count, PAGE_SIZE - count, errormsg);
			break;
		}
		count += written;
	}
	spin_unlock(&pkgl_data_all->hashtable_lock);


	return count;
}

static void sdcardfs_packages_release(struct config_item *item)
{
	printk(KERN_INFO "sdcardfs: destroyed something?\n");
	kfree(to_sdcardfs_packages(item));
}

static struct configfs_item_operations sdcardfs_packages_item_ops = {
	.release	= sdcardfs_packages_release,
	.show_attribute	= packages_attr_show,
};

/*
 * Note that, since no extra work is required on ->drop_item(),
 * no ->drop_item() is provided.
 */
static struct configfs_group_operations sdcardfs_packages_group_ops = {
	.make_item	= sdcardfs_packages_make_item,
};

static struct config_item_type sdcardfs_packages_type = {
	.ct_item_ops	= &sdcardfs_packages_item_ops,
	.ct_group_ops	= &sdcardfs_packages_group_ops,
	.ct_attrs	= sdcardfs_packages_attrs,
	.ct_owner	= THIS_MODULE,
};

static struct configfs_subsystem sdcardfs_packages_subsys = {
	.su_group = {
		.cg_item = {
			.ci_namebuf = "sdcardfs",
			.ci_type = &sdcardfs_packages_type,
		},
	},
};

static int configfs_sdcardfs_init(void)
{
	int ret;
	struct configfs_subsystem *subsys = &sdcardfs_packages_subsys;

	config_group_init(&subsys->su_group);
	mutex_init(&subsys->su_mutex);
	ret = configfs_register_subsystem(subsys);
	if (ret) {
		printk(KERN_ERR "Error %d while registering subsystem %s\n",
		       ret,
		       subsys->su_group.cg_item.ci_namebuf);
	}
	return ret;
}

static void configfs_sdcardfs_exit(void)
{
	configfs_unregister_subsystem(&sdcardfs_packages_subsys);
}

int packagelist_init(void)
{
	hashtable_entry_cachep =
		kmem_cache_create("packagelist_hashtable_entry",
					sizeof(struct hashtable_entry), 0, 0, NULL);
	if (!hashtable_entry_cachep) {
		printk(KERN_ERR "sdcardfs: failed creating pkgl_hashtable entry slab cache\n");
		return -ENOMEM;
	}

	pkgl_data_all = packagelist_create();
	configfs_sdcardfs_init();
	return 0;
}

void packagelist_exit(void)
{
	configfs_sdcardfs_exit();
	packagelist_destroy(pkgl_data_all);
	if (hashtable_entry_cachep)
		kmem_cache_destroy(hashtable_entry_cachep);
}
