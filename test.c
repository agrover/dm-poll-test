#include <stdio.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <poll.h>

#include <libdevmapper.h>

#include "dm-ioctl.h"
#include "darray.h"

struct dm_dev {
	char *dm_name;
	uint32_t event_nr;
};

typedef darray(struct dm_dev) darray_dmdev;

/*
 * Round up the ptr to an 8-byte boundary.
 */
#define ALIGN_MASK 7
static inline void *align_ptr(void *ptr)
{
	return (void *) (((size_t) (ptr + ALIGN_MASK)) & ~ALIGN_MASK);
}

static int get_names(darray_dmdev *out_array)
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
		struct dm_dev dev;
		uint32_t *event_nr_ptr;

		names = (struct dm_names *)((char *) names + next);

		dev.dm_name = strdup(names->name);
		event_nr_ptr = align_ptr(((void *) (names + 1))
					 + strlen(dev.dm_name) + 1);
		dev.event_nr = *event_nr_ptr;
		darray_append(*out_array, dev);

		next = names->next;
	} while (next);

      out:
	dm_task_destroy(task);
	return r;
}

int arm_poll(int fd)
{
	struct dm_ioctl ioc;

	memset(&ioc, 0, sizeof(ioc));

	ioc.version[0] = 4;
	ioc.version[1] = 30;
	ioc.version[2] = 0;

	ioc.data_start = sizeof(ioc);
	ioc.data_size = sizeof(ioc);

	if (ioctl(fd, DM_DEV_ARM_POLL, &ioc) < 0) {
		printf("ARM_POLL error: %m\n");
		return -1;
	}

	return 0;
}

int print_status(char* dm_name)
{
	struct dm_task *task;
	void *next = NULL;
	int r = 1;
	uint64_t start, length;
	char *type = NULL;
	char *params = NULL;


	if (!(task = dm_task_create(DM_DEVICE_STATUS)))
		return 0;

	if (!dm_task_set_name(task, dm_name))
		return 0;

	if (!dm_task_run(task)) {
		r = 0;
		goto out;
	}

	/* Fetch targets and print 'em */
	do {
		next = dm_get_next_target(task, next, &start, &length,
					  &type, &params);
		printf("status: %" PRIu64 " %" PRIu64 " %s %s\n",
		       start, length, type, params);

	} while (next);

out:
	dm_task_destroy(task);
	return r;
}

int main()
{
	struct dm_dev *dev;
	struct pollfd pollfd;
	int fd;
	darray_dmdev dm_devs = darray_new();
	int ret;

	ret = get_names(&dm_devs);
	if (ret < 0) {
		printf("get_names failed\n");
		return 1;
	}

	fd = open("/dev/mapper/control", O_NONBLOCK);
	if (fd < 0) {
		printf("Could not open\n");
		return 1;
	}

	darray_foreach(dev, dm_devs) {
		printf("dm name = %s ", dev->dm_name);
		print_status(dev->dm_name);
	}

	do {
		int i = 0;
		darray_dmdev new_dm_devs = darray_new();

		pollfd.fd = fd;
		pollfd.events = POLLIN;
		pollfd.revents = 0;

		printf("entering poll()\n");
		poll(&pollfd, 1, -1);
		printf("returned from poll()\n");

		ret = get_names(&new_dm_devs);
		if (ret < 0) {
			printf("get_names for new_dm_devs failed\n");
			return 1;
		}

		// NB: Assumes order and length of items in array
		// returned from get_names() remains constant
		for (i = 0; i < darray_size(dm_devs); i++) {
			uint32_t old_nr = darray_item(dm_devs, i).event_nr;
			uint32_t new_nr = darray_item(new_dm_devs, i).event_nr;
			if (old_nr != new_nr) {
				printf("old nr %u new nr %u\n", old_nr, new_nr);
				print_status(darray_item(dm_devs, i).dm_name);
			}
		}

		arm_poll(fd);

		darray_free(dm_devs);
		dm_devs = new_dm_devs;

	} while (1);

	return 0;
}
