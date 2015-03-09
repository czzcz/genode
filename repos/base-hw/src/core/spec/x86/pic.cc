#include <port_io.h>

#include "pic.h"

using namespace Genode;

enum {
	PIC1          = 0x20,      /* IO base address for master PIC */
	PIC2          = 0xa0,      /* IO base address for slave PIC  */
	PIC1_COMMAND  = PIC1,
	PIC1_DATA     = (PIC1+1),
	PIC2_COMMAND  = PIC2,
	PIC2_DATA     = (PIC2+1),
	PIC_EOI       = 0x20,      /* End-of-interrupt command code  */
	ICW1_INIT     = 0x10,      /* Initialization                 */
	ICW1_ICW4     = 0x01,      /* Enable ICW4                    */
	ICW4_8086     = 0x01,      /* 8086/88 (MCS-80/85) mode       */
	OCW3_READ_ISR = 0x0b,      /* OCW3 irq service               */

	MASTER_VEC_OFFSET = 0x20,
	SLAVE_VEC_OFFSET  = 0x28,
};


/* Return PIC1/PIC2 combined result for given command */
static uint16_t pic_get_combined(int const cmd)
{
	outb(PIC1_COMMAND, cmd);
	outb(PIC2_COMMAND, cmd);
	return (inb(PIC2_COMMAND) << 8) | inb(PIC1_COMMAND);
}

/* Returns the combined value of the cascaded PICs ISR */
static uint16_t pic_get_isr(void)
{
	return pic_get_combined(OCW3_READ_ISR);
}

/* Determine lowest pending request vector from given ISR bitmask */
static unsigned pic_get_lowest_vector(unsigned const bitmask)
{
	return __builtin_ffs(bitmask) - 1;
}


Pic::Pic()
{
	unsigned char a1, a2;

	/* Save masks */
	a1 = inb(PIC1_DATA);
	a2 = inb(PIC2_DATA);

	/* Start initialization sequence in cascade mode */
	outb(PIC1_COMMAND, ICW1_INIT+ICW1_ICW4);
	outb(PIC2_COMMAND, ICW1_INIT+ICW1_ICW4);

	/* ICW2: Master PIC vector offset (32) */
	outb(PIC1_DATA, MASTER_VEC_OFFSET);
	/* ICW2: Slave PIC vector offset (40) */
	outb(PIC2_DATA, SLAVE_VEC_OFFSET);

	/* ICW3: Tell Master PIC that there is a slave PIC at IRQ2 */
	outb(PIC1_DATA, 4);

	/* ICW3: Tell Slave PIC its cascade identity */
	outb(PIC2_DATA, 2);

	/* ICW4: Enable 8086 mode */
	outb(PIC1_DATA, ICW4_8086);
	outb(PIC2_DATA, ICW4_8086);

	/* Restore saved masks */
	outb(PIC1_DATA, a1);
	outb(PIC2_DATA, a2);
}

bool Pic::take_request(unsigned &irq)
{
	unsigned const isr = pic_get_isr();
	if (isr == 0) {
		return false;
	}

	irq = pic_get_lowest_vector(isr);
	return true;
}

void Pic::finish_request()
{
	outb(PIC1_COMMAND, PIC_EOI);
	outb(PIC2_COMMAND, PIC_EOI);
}

void Pic::mask(unsigned const i)
{
	unsigned port, bitpos;
	uint8_t value;

	bitpos = i;

	if (i < 8) {
		port = PIC1_DATA;
	} else {
		port = PIC2_DATA;
		bitpos -= i;
	}

	value = inb(port) | (1 << bitpos);
	outb(port, value);
}
