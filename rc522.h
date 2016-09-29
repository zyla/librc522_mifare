// Interface to MCRF522 PICC via Linux spidev.

#include <stdint.h>

typedef struct rc522_dev {
  int fd;
} rc522_dev;

/**
 * Open the spidev device.
 *
 * Example:
 *    rc522_dev dev;
 *    rc522_open(&dev, "/dev/spidev0.0");
 *
 * Returns: 0 on success, -1 on failure.
 *   Sets errno on failure.
 */
int rc522_open(rc522_dev *dev, const char *device);

/**
 * Initialize the device for communicating with tags.
 *
 * Returns: 0 on success, -1 on failure.
 */
int rc522_init(rc522_dev *dev);

/**
 * Issue the Transceive command (MFRC522, section 10.3.1.8).
 *
 * TODO document parameters
 */
ssize_t rc522_transceive(rc522_dev *dev,
    const uint8_t *input, size_t input_len,
    uint8_t tx_last_bits,
    uint8_t *output, size_t output_len);
