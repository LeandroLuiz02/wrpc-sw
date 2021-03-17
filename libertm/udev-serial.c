#include <linux/limits.h>
#include <libudev.h>
#include <stdio.h>
#include <string.h>

#define SUBSYSTEM "usb"
#define SILICON_LABS_ID		"10c4"
#define CP2108_UART_TO_USB	"ea71"

struct sl_cp2108_port {
	char	devnode[PATH_MAX];
	char	symlink[PATH_MAX];
	int	port;
} sl_cp2108_port[4];

static void search_dongle(void)
{
    struct udev *udev = udev_new();
    struct udev_enumerate *enumerate = udev_enumerate_new(udev);
    struct udev_list_entry *devs, *ptr;

    udev_enumerate_add_match_subsystem(enumerate, "tty");
    udev_enumerate_add_match_property(enumerate, "ID_VENDOR_ID", SILICON_LABS_ID);
    udev_enumerate_add_match_property(enumerate, "ID_MODEL", CP2108_UART_TO_USB);
    udev_enumerate_scan_devices(enumerate);

    devs = udev_enumerate_get_list_entry(enumerate);
    udev_list_entry_foreach(ptr, devs) {
        const char *path = udev_list_entry_get_name(ptr);
        struct udev_device *dev = udev_device_new_from_syspath(udev, path);
	const char *devnode = udev_device_get_devnode(dev);

	if (devnode != NULL) {
		struct udev_list_entry *l, *links = udev_device_get_devlinks_list_entry(dev);
		int port;
		udev_list_entry_foreach(l, links) {
			const char *linkname = udev_list_entry_get_name(l);
			int i, len = strlen(linkname);
			const char *ports[]  = { "0-port0", "1-port0", "2-port0", "3-port0", };
			int trim = strlen(ports[0]);

			for (i = 0; i < 4; i++)
				if (!strcmp(ports[i], &linkname[len-trim])) {
					sl_cp2108_port[i].port = port = i;
					strcpy(sl_cp2108_port[i].symlink, path);
					strcpy(sl_cp2108_port[i].devnode, devnode);
					goto found;
				}
			goto notfound;
		}
found:
		printf("port%d: %20s %s\n", port, devnode, path);
	}
notfound:
	udev_device_unref(dev);
    }
    udev_enumerate_unref(enumerate);
}


int main(void)
{
    search_dongle();
    return 0;
}
