# NRF52833/LibOpenCM3 GPIO Port CLarity

Written by: Jack Rathert

## The Problem

If you're using libopencm3, you may have noticed your radio isn't doing much of anything. This is because libopencm3 does not actually treat the nrf52833 as having discrete ports.

From `libopencm3/include/libopencm3/nrf/common/gpio.c`:

```C++
void gpio_mode_setup(uint32_t gpioport, uint32_t mode, uint32_t pull_up_down,uint32_t gpios)
{
	(void) gpioport;
	uint8_t i = 0;
	while (gpios) {
		if (gpios & 1) {
			GPIO_PIN_CNF(i) = (
					GPIO_PIN_CNF(i) &
					~((GPIO_CNF_MODE_MASK << GPIO_CNF_MODE_SHIFT)
						| (GPIO_CNF_PUPD_MASK << GPIO_CNF_PUPD_SHIFT)
					 )
				) | (mode << GPIO_CNF_MODE_SHIFT)
				| (pull_up_down << GPIO_CNF_PUPD_SHIFT);
		}
		++i;
		gpios >>= 1;
	}
}
```

As you can see, your port is literally dropped into the void.

As a result, to the best of this programmer's understanding, when you input `gpio_mode_setup(P1,GPIO9)` to setup your RF frontend, you are essentially just inputting `GPIO9`.

Similarly for `gpio_set`:
```
void gpio_clear(uint32_t gpioport, uint32_t gpios)
{
	(void) gpioport;
	GPIO_OUTCLR = gpios;
}
```

To add insult to injury, nothing is going to happen when you set P0.09 to HIGH, because there P0.09 is meant to be routed to an NFC antenna (which we do not have anyways).

## The Solution

I was able to overcome this fault by writing directly to the requisite registers.

in com.h, you will need to manually define registers for the base of the port, clears and sets, and the pin configurations themselves.

```C
#define P0                  (GPIO_BASE)
#define P1_BASE             (0x50000300)
#define P1_PIN_CNF(N)       (*(volatile uint32_t *)(P1_BASE + 0x700 + 0x4 * (N)))
#define P1_OUTSET           (*(volatile uint32_t *)(P1_BASE + 0x508))
#define P1_OUTCLR           (*(volatile uint32_t *)(P1_BASE + 0x50C))
```

Then to set this pin high or low, you can do the following

```C
void enable_rf(void) {
    P1_OUTSET = (1UL << 9);
}

void disable_rf(void) {
    P1_OUTCLR = (1 << 9);
}
```
Please slack me if you find a more refined solution or have questions about the included code or how I solved this. I haven't posted an issue to their github about this yet, but I might try to contibute a fix for this when I have bandwidth later in the semester.
