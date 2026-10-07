/* Bare-metal failure handling for the full-suite newlib port.
 * Embench uses its own beebs heap; no hidden OS or extra benchmark memory.
 */
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

void *_sbrk(ptrdiff_t increment)
{
    (void)increment;
    errno = ENOMEM;
    return (void *)-1;
}

__attribute__((noreturn)) void _exit(int status)
{
    (void)status;
    /* Only start.S, after main's verifier, may report success. */
    *(volatile uint32_t *)0x3ffc0 = 3;
    for (;;) {}
}
