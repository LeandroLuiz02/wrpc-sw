#include <libudev.h>
#include <stdio.h>

#define SUBSYSTEM "usb"
#define SILICON_LABS_ID		"10c4"
#define CP2108_UART_TO_USB	"ea71"

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
		printf("%s\n", devnode);
	}
	udev_device_unref(dev);
    }
    udev_enumerate_unref(enumerate);
}


int main(void)
{
    search_dongle();
    return 0;
}
