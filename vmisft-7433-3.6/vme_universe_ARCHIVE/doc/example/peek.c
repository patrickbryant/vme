
#include <vme/vme.h>
#include <vme/vme_api.h>
#include <stdio.h>

#define VME_ADDRESS       0x08000000
#define ADDRESS_MODIFIER  VME_A32SD
#define NBYTES            0x40

int main()
{
	vme_bus_handle_t bus_handle;
	vme_master_handle_t window_handle;
	uint8_t *ptr;
	int ii;

	if (vme_init(&bus_handle)) {
		perror("Error initializing the VMEbus");
		return -1;
	}

	if (vme_master_window_create(bus_handle, &window_handle,
				     VME_ADDRESS, ADDRESS_MODIFIER, NBYTES,
				     VME_CTL_PWEN, NULL)) {
		perror("Error creating the window");
		vme_term(bus_handle);
		return -1;
	}

	ptr = vme_master_window_map(bus_handle, window_handle, 0);
	if (!ptr) {
		perror("Error mapping the window");
		vme_master_window_release(bus_handle, window_handle);
		vme_term(bus_handle);
		return -1;
	}

	/* Print the VMEbus data */
	for (ii = 0; ii < NBYTES; ++ii, ++ptr) {
		printf("%.2x ", *ptr);
		/* Add a newline every 16 bytes */
		if (!((ii + 1) % 0x10))
			printf("\n");
	}
	printf("\n");

	if (vme_master_window_unmap(bus_handle, window_handle)) {
		perror("Error unmapping the window");
		vme_master_window_release(bus_handle, window_handle);
		vme_term(bus_handle);
		return -1;
	}

	if (vme_master_window_release(bus_handle, window_handle)) {
		perror("Error releasing the window");
		vme_term(bus_handle);
		return -1;
	}

	if (vme_term(bus_handle)) {
		perror("Error terminating");
		return -1;
	}

	return 0;
}
