/*
 * This work is part of the White Rabbit project
 *
 * Copyright (C) 2021 CERN (www.cern.ch)
 * Author: Tomasz Wlostowski <tomasz.wlostowski@cern.ch>
 * Author: Adam Wujek
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */
#include "irq.h"

void __attribute__((target("arch=+zicsr"))) disable_irq(void)
{
    unsigned long t;

    /* Disable irq: set mie.meie  */
    asm volatile ("csrrc %0, mie, %1" : "=r"(t) : "r"(1 << 11));
    /* Disable interrupts: set mstatus.mie  */
    asm volatile ("csrrci %0, mstatus, %1" : "=r"(t) : "i"(1 << 3));
}

void __attribute__((target("arch=+zicsr"))) enable_irq(void)
{
    unsigned long t;

    /* Enable irq: set mie.meie  */
    asm volatile ("csrrs %0, mie, %1" : "=r"(t) : "r"(1 << 11));
    /* Enable interrupts: set mstatus.mie  */
    asm volatile ("csrrsi %0, mstatus, %1" : "=r"(t) : "i"(1 << 3));
}
