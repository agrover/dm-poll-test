#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <libdevmapper.h>

#include "dm-ioctl.h"
#include "darray.h"

static int get_names(void)
{
	struct dm_task *task;
	struct dm_names *names;
	unsigned next = 0;
	int r = 1;

	if (!(task = dm_task_create(DM_DEVICE_LIST)))
		return 0;

	if (!dm_task_run(task)) {
		r = 0;
		goto out;
	}

	if (!(names = dm_task_get_names(task))) {
		r = 0;
		goto out;
	}

	if (!names->dev)
		goto out;

	do {
		names = (struct dm_names *)((char *) names + next);
		printf("dm name = %s\n", names->name);
		/* if (!dm_task_set_name(dmt, names->name)) { */
		/* 	r = 0; */
		/* 	goto out; */
		/* } */
		/* if (!dm_task_run(dmt)) */
		/* 	r = 0; */
		next = names->next;
	} while (next);

      out:
	dm_task_destroy(task);
	return r;
}


struct dm_dev {
	char *filename;
	int fd;
};

darray(struct dm_dev) dm_devs = darray_new();

static int is_handler(const struct dirent *dirent)
{
	if (strncmp(dirent->d_name, "dm-", 3))
		return 0;

	return 1;
}

static int open_devs(void)
{
	struct dirent **dirent_list;
	int i;
	char *dev_path = "/dev";

	int num_dm_devs = scandir(dev_path, &dirent_list, is_handler, alphasort);

	if (num_dm_devs == -1)
		return -1;

	printf("num handlers %d\n", num_dm_devs);

	for (i = 0; i < num_dm_devs; i++)
		free(dirent_list[i]);
	free(dirent_list);

	return num_dm_devs;
}

int main()
{
	return get_names();
}
