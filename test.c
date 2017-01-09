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
	int fd;
};

darray(struct dm_dev) dm_devs = darray_new();

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
		struct dm_dev dev;

		names = (struct dm_names *)((char *) names + next);

		dev.dm_name = strdup(names->name);
		dev.fd = -1;
		darray_append(dm_devs, dev);

		next = names->next;
	} while (next);

      out:
	dm_task_destroy(task);
	return r;
}

int set_dev_event(struct dm_dev *dev)
{
	struct dm_ioctl ioc;

	memset(&ioc, 0, sizeof(ioc));

	ioc.version[0] = 4;
	ioc.version[1] = 30;
	ioc.version[2] = 0;

	ioc.data_start = sizeof(ioc);
	ioc.data_size = sizeof(ioc);

	snprintf(ioc.name, sizeof(ioc.name), "%s", dev->dm_name);

	if (ioctl(dev->fd, DM_DEV_ASSOC, &ioc) < 0) {
		printf("error: %m\n");
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
	struct pollfd *pollfds;
	int fd;

	get_names();

	darray_foreach(dev, dm_devs) {
		fd = open("/dev/mapper/control", O_NONBLOCK);
		if (fd < 0) {
			printf("Could not open\n");
			return 1;
		}

		dev->fd = fd;

		printf("dm name = %s fd = %d\n", dev->dm_name, dev->fd);
		print_status(dev->dm_name);

		if (set_dev_event(dev) < 0) {
			printf("set_dev_event failed\n");
			return 1;
		}

	}

	pollfds = calloc(sizeof(*pollfds), darray_size(dm_devs));
	if (!pollfds) {
		printf("could not alloc pollfds[], running this on a wristwatch???\n");
		return 1;
	}

	do {
		int i = 0;
		darray_foreach(dev, dm_devs) {
			pollfds[i].fd = dev->fd;
			pollfds[i].events = POLLIN;
			pollfds[i].revents = 0;
			i++;
		}

		printf("entering poll()\n");
		poll(pollfds, darray_size(dm_devs), -1);
		printf("returned from poll()\n");

		i = 0;
		darray_foreach(dev, dm_devs) {
			if (pollfds[i].revents & POLLIN) {
				printf("got POLLIN\n");
				print_status(dev->dm_name);
			}
			i++;
		}
	} while (1);

	return 0;
}
