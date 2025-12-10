/*
 * usbrelay_probe() in relay_driver.c
 *
 * High-level responsibilities:
 *
 * 1. Allocate and initialize per-device state (struct usbrelay):
 *    - Grab the underlying usb_device and usb_interface.
 *    - Initialize relay_state and locking.
 *    - Associate the state with this interface via usb_set_intfdata().
 *
 * 2. Discover the USB endpoints we care about:
 *    - Walk the interface's endpoint descriptors.
 *    - Find and store the bulk OUT endpoint (required to send relay bytes).
 *    - Optionally record the bulk IN endpoint for future use.
 *
 * 3. Expose the device to userspace as /dev/usbrelay0:
 *    - Allocate a char device number (major/minor).
 *    - Initialize and register a cdev with usbrelay_fops.
 *    - Create a device class and a device node (/dev/usbrelay0).
 *
 * 4. Configure the FTDI chip into bit-bang mode:
 *    - Send a vendor-specific control request (SET_BITMODE).
 *    - Configure all pins as outputs and enable bit-bang mode.
 *
 * 5. Initialize the hardware relay state:
 *    - Start with relay_state = 0x00 (all relays off).
 *    - Push that byte via bulk OUT so the board starts in a known safe state.
 *
 * On any failure:
 *    - Clean up allocated resources (char device, class, usb references, memory).
 *    - Return a negative errno so the USB core knows binding failed.
 */

