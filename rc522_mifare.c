#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>
#include <assert.h>

#include "rc522.h"
#include "constants.h"

// Sources:
//
// [ISO] Final draft of ISO/IEC 14443-3
// http://wg8.de/wg8n1496_17n3613_Ballot_FCD14443-3.pdf
//
// [NXP] MFRC522 datasheet
// http://www.nxp.com/documents/data_sheet/MFRC522.pdf

static int read_reg(rc522_dev *dev, uint8_t reg);
static int write_reg(rc522_dev *dev, uint8_t reg, uint8_t val);
static int set_bits(rc522_dev *dev, uint8_t reg, uint8_t mask);

int rc522_open(rc522_dev *dev, const char *device) {
  int fd = open(device, O_RDWR);
  if(fd < 0) {
    return -1;
  }

  dev->fd = fd;
  return 0;
}

int rc522_init(rc522_dev *dev) {
  write_reg(dev, CommandReg, PCD_SoftReset);

  // [NXP 9.3.3.10] TModeReg and TPrescalerReg registers
  //   These registers define the timer settings.
  //
  // TAuto: timer starts automatically at the end of the transmission in all
  // communication modes at all speeds
  //
  // TPrescalerHi, TPrescalerLo: high/low bits of the prescaler value
  //
  // [NXP 8.10] Timer unit
  const uint16_t prescaler = 0xd3e;
  write_reg(dev, TModeReg, TModeReg_TAuto | TModeReg_TPrescaler_Hi(prescaler));
  write_reg(dev, TPrescalerReg, TPrescalerReg_TPrescaler_Lo(prescaler));
  // TODO
  write_reg(dev, TReloadRegL, 0x1e);
  write_reg(dev, TReloadRegH, 0x00);
  

  // [NXP 9.3.2.6] TxASKReg register
  //   Controls transmit modulation settings.
  //
  // Force100ASK: forces a 100% ASK modulation independent of the ModGsPReg
  //   register setting
  write_reg(dev, TxASKReg, TxASKReg_Force100ASK);

  // [NXP 9.3.2.2] ModeReg register
  //   Defines general mode settings for transmitting and receiving.
  // TODO
  write_reg(dev, ModeReg, 0x3d);

  // [NXP 9.3.2.5] TxControlReg register
  // 
  // Tx{1,2}RFEn - output signal on pin TX{1,2} delivers the 13.56 MHz energy
  //   carrier modulated by the transmission data
  set_bits(dev, TxControlReg, TxControlReg_Tx1RFEn | TxControlReg_Tx2RFEn);

  return 0;
}

ssize_t rc522_transceive(rc522_dev *dev,
      const uint8_t *input, size_t input_len,
      uint8_t tx_last_bits,
      uint8_t *output, size_t output_len) {

  assert(tx_last_bits <= 7);

  // Before we issue this command, we need to:
  //  - clear the FIFO buffer
  //  - fill the FIFO buffer with our data
  //  - clear interrupts, because we need to check for them later

  // [NXP 9.3.1.11] FIFOLevelReg register
  // FlushBuffer: immediately clears the internal FIFO buffer’s read and write
  //   pointer and ErrorReg register’s BufferOvfl bit
  write_reg(dev, FIFOLevelReg, FIFOLevelReg_FlushBuffer);

  size_t i;
  for(i = 0; i < input_len; i++) {
    write_reg(dev, FIFODataReg, input[i]);
  }

  // Clear all interrupt flags
  write_reg(dev, ComIrqReg, 0x00);

  // [NXP 10.3.1.8] Transceive
  //   This command continuously repeats the transmission of data from the FIFO buffer and the 
  //   reception of data from the RF field. The first action is transmit and after transmission the 
  //   command is changed to receive a data stream.
  //
  // NB: this doesn't actually start the transmission; the next write does.
  write_reg(dev, CommandReg, PCD_Transceive);

  // [NXP 9.3.1.14] BitFramingReg register
  // StartSend: starts the transmission of data
  // TxLastBits: used for transmission of bit oriented frames: defines the
  //   number of bits of the last byte that will be transmitted
  //   000b indicates that all bits of the last byte will be transmitted
  write_reg(dev, BitFramingReg, BitFramingReg_StartSend | BitFramingReg_TxLastBits(tx_last_bits));

  uint8_t interrupts;
  do {
    interrupts = read_reg(dev, ComIrqReg);
    // TODO delay
    printf("ComIrqReg = %02x\n", interrupts);
  } while(!(interrupts & ComIrqReg_RxIRq));

  // TODO error check

  size_t num_bytes = read_reg(dev, FIFOLevelReg);
  if(num_bytes > output_len) {
    num_bytes = output_len;
  }

  for(i = 0; i < num_bytes; i++) {
    output[i] = read_reg(dev, FIFODataReg);
  }

  return num_bytes;
}

// reading/writing registers
// TODO document

static int read_reg(rc522_dev *dev, uint8_t reg) {
  uint8_t buf[1] = { (reg << 1) | 0x80 };

  if(write(dev->fd, buf, 1) < 0)
    return -1;

  if(read(dev->fd, buf, 1) < 0)
    return -1;

  return buf[0];
}

static int write_reg(rc522_dev *dev, uint8_t reg, uint8_t val) {
  uint8_t buf[2];
  buf[0] = reg << 1;
  buf[1] = val;
  if(write(dev->fd, buf, 2) < 0)
    return -1;

  return 0;
}

static int set_bits(rc522_dev *dev, uint8_t reg, uint8_t mask) {
  write_reg(dev, reg, read_reg(dev, reg) | mask);
}

int main() {
  rc522_dev dev;
  if(rc522_open(&dev, "/dev/spidev0.0") < 0) perror("open");

  rc522_init(&dev);

  printf("RC522 initialized\n");

  uint8_t inbuf[] = { PICC_REQIDL };
  uint8_t outbuf[16];

  int len = rc522_transceive(&dev, inbuf, sizeof(inbuf), 7, outbuf, sizeof(outbuf));
  printf("Received %d bytes:", len);
  for(int i = 0; i < len; i++)
    printf(" %02x", outbuf[i]);
  printf("\n");

  return 0;
}
