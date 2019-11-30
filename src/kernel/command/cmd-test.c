/*
 * kernel/command/cmd-test.c
 */

#include <xboot.h>
#include <command/command.h>
#include <spi/spi.h>

#include "spi_sdcard_driver.h"
#include "spi_sdcard_driver_config.h"
#include "spi_sdcard_crc7.h"

static struct spi_device_t * spidev;

static void io_set_speed(uint32_t freq)
{
}
static void io_select(void)
{
	spi_device_select(spidev);
}
static void io_relese(void)
{
	spi_device_deselect(spidev);
}
static int io_is_present(void)
{
	return 1;
}

static uint8_t io_wr_rd_byte(uint8_t byte)
{
	struct spi_msg_t msg;
	uint8_t c;

	msg.type = spidev->type;
	msg.mode = spidev->mode;
	msg.bits = spidev->bits;
	msg.speed = spidev->speed;

	msg.txbuf = &byte;
	msg.rxbuf = &c;
	msg.len = 1;
	spidev->spi->transfer(spidev->spi, &msg);
	return c;
}

static void io_write(uint8_t *buffer, uint32_t size)
{
	spi_device_write_then_read(spidev, buffer, size, 0, 0);
}
static void io_read(uint8_t *buffer, uint32_t size)
{
	spi_device_write_then_read(spidev, 0, 0, buffer, size);
}

static spisd_interface_t io = {
	.set_speed = io_set_speed,
	.select = io_select,
	.relese = io_relese,
	.is_present = io_is_present,
	.wr_rd_byte = io_wr_rd_byte,
	.write = io_write,
	.read = io_read,
};

static void usage(void)
{
	printf("usage:\r\n");
	printf("    test [args ...]\r\n");
}

static int do_test(int argc, char ** argv)
{
	spisd_info_t cardinfo;

	spidev = spi_device_alloc("spi-gpio.0", 0, 0, 0, 8, 0);

	spisd_init(&io);
	spisd_get_card_info(&cardinfo);

	return 0;
}

static struct command_t cmd_test = {
	.name	= "test",
	.desc	= "debug command for programmer",
	.usage	= usage,
	.exec	= do_test,
};

static __init void test_cmd_init(void)
{
	register_command(&cmd_test);
}

static __exit void test_cmd_exit(void)
{
	unregister_command(&cmd_test);
}

command_initcall(test_cmd_init);
command_exitcall(test_cmd_exit);
