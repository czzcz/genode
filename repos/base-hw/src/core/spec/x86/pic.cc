#include <port_io.h>

#include "pic.h"

using namespace Genode;

enum {
	PIC1         = 0x20,      /* IO base address for master PIC */
	PIC2         = 0xa0,      /* IO base address for slave PIC  */
	PIC1_COMMAND = PIC1,
	PIC1_DATA    = (PIC1+1),
	PIC2_COMMAND = PIC2,
	PIC2_DATA    = (PIC2+1),
	PIC_EOI      = 0x20,      /* End-of-interrupt command code  */
	ICW1_INIT    = 0x10,      /* Initialization                 */
	ICW1_ICW4    = 0x01,      /* Enable ICW4                    */
	ICW4_8086    = 0x01,      /* 8086/88 (MCS-80/85) mode       */
};


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
	outb(PIC1_DATA, 0x20);
	/* ICW2: Slave PIC vector offset (40) */
	outb(PIC2_DATA, 0x28);

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
