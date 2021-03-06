#include <vme/vme.h>
#include <vme/vme_api.h>
#include <stdio.h>

#define VME_ADDRESS       0x00040000
#define ADDRESS_SPACE     VME_A24
#define NBYTES            0x10
#define MB 1024*1024

int main()
{
        vme_bus_handle_t bus_handle;
        vme_slave_handle_t window_handle;
#ifdef EIGHTBIT
        uint8_t *ptr;
#else
	uint16_t *ptr;
	uint16_t count;
#endif
        int ii;
	int c;

        if (vme_init(&bus_handle)) {
                perror("Error initializing the VMEbus");
                return -1;
        }

        if (vme_slave_window_create(bus_handle, &window_handle,
#ifdef EIGHTBIT
                                    VME_ADDRESS, ADDRESS_SPACE, NBYTES,
#else
                                    VME_ADDRESS, ADDRESS_SPACE, NBYTES*2,
#endif
                                    VME_CTL_PWEN | VME_CTL_PREN, 
				    NULL)) {
				/*	(void *)(2044*MB))) {*/
                perror("Error creating the window");
                vme_term(bus_handle);
                return -1;
        }

        ptr = vme_slave_window_map(bus_handle, window_handle, 0);
        if (!ptr) {
                perror("Error mapping the window");
                vme_slave_window_release(bus_handle, window_handle);
                vme_term(bus_handle);
                return -1;
        }

	count=0x1234;
        /* Print the data */
        for (ii = 0; ii < NBYTES; ++ii, ++ptr) {

		*ptr=count;
		count+=4;
#ifdef EIGHTBIT
                printf("%.2x ", *ptr);
#else
                printf("%x ", *ptr);
#endif
                /* Add a newline every 16 bytes */
                if (!((ii + 1) % 0x10))
                        printf("\n");
        }
        printf("\n");

	printf ("Press <enter> to unmap window\n");
	c = getchar();

printf("unmapping the window\n");
        if (vme_slave_window_unmap(bus_handle, window_handle)) {
                perror("Error unmapping the window");
                vme_slave_window_release(bus_handle, window_handle);
                vme_term(bus_handle);
                return -1;
        }

printf("releasing the window\n");
        if (vme_slave_window_release(bus_handle, window_handle)) {
                perror("Error releasing the window");
                vme_term(bus_handle);
                return -1;
        }

printf("terminating the window\n");
        if (vme_term(bus_handle)) {
                perror("Error terminating");
                return -1;
        }

        return 0;
}

