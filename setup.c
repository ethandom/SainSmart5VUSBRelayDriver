#include <stdio.h>
#include <unistd.h>
#include <ftdi.h>

int main(void) {
    struct ftdi_context *ftdi;
    int ret;

    // Create context
    ftdi = ftdi_new();
    if (!ftdi) {
        fprintf(stderr, "ftdi_new failed\n");
        return 1;
    }

    // Open the FTDI device (vendor 0x0403, product 0x6001)
    ret = ftdi_usb_open(ftdi, 0x0403, 0x6001);
    if (ret < 0) {
        fprintf(stderr, "unable to open FTDI device: %d (%s)\n",
                ret, ftdi_get_error_string(ftdi));
        ftdi_free(ftdi);
        return 1;
    }

    // Reset the device
    if ((ret = ftdi_usb_reset(ftdi)) < 0) {
        fprintf(stderr, "usb reset failed: %d (%s)\n",
                ret, ftdi_get_error_string(ftdi));
        goto done;
    }

    // Set bitbang mode on lower 4 bits (one bit per relay)
    unsigned char direction = 0x0F;  // bits 0–3 = outputs
    ret = ftdi_set_bitmode(ftdi, direction, BITMODE_BITBANG);
    if (ret < 0) {
        fprintf(stderr, "set_bitmode failed: %d (%s)\n",
                ret, ftdi_get_error_string(ftdi));
        goto done;
    }

    // All relays OFF (assumes active-high; if your board is active-low,
    // this will actually turn them ON – we can flip it after testing).
    unsigned char data = 0x00;
    ret = ftdi_write_data(ftdi, &data, 1);
    if (ret < 0) {
        fprintf(stderr, "write (all off) failed: %d (%s)\n",
                ret, ftdi_get_error_string(ftdi));
        goto done;
    }

    printf("All relays OFF (data = 0x%02X)\n", data);
    sleep(1);

    // Turn relay 1 ON: set bit 0 = 1 (data = 0x01)
    data = 0x01;
    ret = ftdi_write_data(ftdi, &data, 1);
    if (ret < 0) {
        fprintf(stderr, "write (relay1 on) failed: %d (%s)\n",
                ret, ftdi_get_error_string(ftdi));
        goto done;
    }

    printf("Relay 1 ON (data = 0x%02X)\n", data);
    sleep(2);

    // Turn relay 1 OFF again
    data = 0x00;
    ret = ftdi_write_data(ftdi, &data, 1);
    if (ret < 0) {
        fprintf(stderr, "write (relay1 off) failed: %d (%s)\n",
                ret, ftdi_get_error_string(ftdi));
        goto done;
    }

    printf("Relay 1 OFF (data = 0x%02X)\n", data);

done:
    // Disable bitbang and clean up
    ftdi_disable_bitbang(ftdi);
    ftdi_usb_close(ftdi);
    ftdi_free(ftdi);
    return 0;
}
