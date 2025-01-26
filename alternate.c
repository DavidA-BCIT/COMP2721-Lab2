#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <ctype.h>
#include <poll.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <assert.h>

// GPIO slots we're plugging the LEDs into
#define LED_1  12
#define LED_2  16
static_assert(0 < LED_1 < 28 && 0 < LED_2 < 28, "Can only use GPIO slots 2-27");

// Frequency of flashes (in Hz)
#define DELAY 2

// Bools

// Consts
static volatile unsigned int GPIO_Base = 0x3F200000;     //GPIO base
static volatile uint32_t *GPIO;
static const unsigned int GPIO_Mask = 0xFFFFFFC0;


// Register values
const unsigned char GPIO_Register_1 = LED_1/10;
const unsigned char GPIO_Shift_1 = (LED_1%10)*3;

const unsigned char GPIO_Register_2 = LED_2/10;
const unsigned char GPIO_Shift_2 = (LED_2%10)*3;

const unsigned char GPSET_Register = 7;
const unsigned char GPCLR_Register = 10;
// -----------------------------------------------------------------------------

// IDK what this does
int failure (int fatal, const char *message, ...)
{
    va_list argp;
    char buffer[1024];

    if (!fatal) //  && wiringPiReturnCodes)
        return -1 ;

    va_start(argp, message);
    vsnprintf(buffer, 1023, message, argp);
    va_end(argp);

    fprintf (stderr, "%s", buffer) ;
    exit (EXIT_FAILURE) ;

    return 0 ;
}

/**
 * pin: a GPIO slot
 * state: 0 = set LED off, any other value = set LED on
 * returns: the new state (0 for off, 1 for on, -1 for error)
 */
unsigned char setLED(const unsigned int pin, const unsigned char state) {
    if ((pin & GPIO_Mask) != 0) return -1; // Invalid pin

    if (state) *(GPIO + GPSET_Register) = 1 << (pin & 31);
    else *(GPIO + GPCLR_Register) = 1 << (pin & 31);

    return state;
}

int main(void) {
    // Error stuff
    int fd;
    if (geteuid () != 0) fprintf (stderr, "setup: Must be root. (Did you forget sudo?)\n");

    if ((fd = open("/dev/mem", O_RDWR | O_SYNC | O_CLOEXEC)) < 0) {
        return failure(0, "setup: Unable to open /dev/mem: %s\n", strerror (errno));
    }

    GPIO = (uint32_t *)mmap(0, BLOCK_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, GPIO_Base);

    if ((int32_t)GPIO == -1) {
        return failure (0, "setup: mmap (GPIO) failed: %s\n", strerror (errno));
    }

    // Set GPIO pins to output
    *(GPIO + GPIO_Register_1) = *(GPIO + GPIO_Register_1) & ~(7 << GPIO_Shift_1); // Sets bits to one = output
    *(GPIO + GPIO_Register_2) = *(GPIO + GPIO_Register_2) & ~(7 << GPIO_Shift_2);


    unsigned char LED_1_State = 0, LED_2_State = 1;

    // Alternate the pins
    for (int i = 0; i < 10; ++i) {
        LED_1_State = setLED(LED_1, !LED_1_State);
        LED_2_State = setLED(LED_2, !LED_2_State);

        // INLINED delay
        {
            struct timespec sleeper, dummy ;

            sleeper.tv_sec  = (time_t)(1/DELAY) ;
            sleeper.tv_nsec = (long)(1/DELAY) * 1000000 ;

            nanosleep(&sleeper, &dummy) ;
        }
    }

    // Cleanup when done
    *(GPIO + 7) = 1 << (23 & 31);

    return 0;
}