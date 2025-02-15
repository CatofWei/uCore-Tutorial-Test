#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

int getchar()
{
	char byte = 0;
	if (1 == read(stdin, &byte, 1)) {
		return byte;
	} else {
		return EOF;
	}
}

#define __LINE_WIDTH 256

static char buffer[__LINE_WIDTH];
static int buffer_len;
static int buffer_lock;
int buffer_lock_enabled = 0;

// Returns: number of chars written, negative for failure
// Warn: buffer_len[f] will not be changed
int __write_buffer()
{
	if (buffer_len == 0)
		return 0;
	int r = write(stdout, buffer, buffer_len);
	return r;
}

// Clear buffer_len[f]
void __clear_buffer()
{
	buffer_len = 0;
}

int __fflush()
{
	int r = __write_buffer();
	__clear_buffer();
	return r >= 0 ? 0 : r;
}

int fflush(int fd)
{
	if (fd == 1) {
		int len = 0;
		if (buffer_lock_enabled == 1) {
			// for multiple threads io
			mutex_lock(buffer_lock);
			len = __fflush();
			mutex_unlock(buffer_lock);
		} else {
			len = __fflush();
		}
		return len;
	}
	return 0;
}

// enable_thread_io_buffer must be called before use mutiple thread stdout
void enable_thread_io_buffer()
{
	// Assume it's in single thread so there is no consistency problem
	// for buffer_lock and buffer_lock_enabled
	assert((buffer_lock = mutex_create()) >= 0);
	buffer_lock_enabled = 1;
}

static int out_unlocked(const char *s, size_t l)
{
	int ret = 0;
	for (size_t i = 0; i < l; i++) {
		char c = s[i];
		buffer[buffer_len++] = c;
		if (buffer_len >= __LINE_WIDTH || c == '\n') {
			// check overflow
			assert(buffer_len <= __LINE_WIDTH);
			int r = __write_buffer();
			__clear_buffer();
			if (r < 0) {
				return r;
			}
			if (r < buffer_len) {
				return ret + r;
			}
			ret += r;
		}
	}
	return ret;
}

static int out(int f, const char *s, size_t l)
{
	if (f != stdout){
		const char * btr ="mark this\n";
		write(stdout, btr, strlen(btr));
		return write(f, s, l);
	}
//	const char * btr ="do not call write directly\n";
//	write(stdout, btr, strlen(btr));
	int len = 0;
	if (buffer_lock_enabled == 1) {
		// for multiple threads io
		mutex_lock(buffer_lock);
		len = out_unlocked(s, l);
		mutex_unlock(buffer_lock);
	} else {
		len = out_unlocked(s, l);
	}
	return len;
}

int putchar(int c)
{
	char byte = c;
	return out(stdout, &byte, 1);
}

int puts(const char *s)
{
	int r;
	r = -(out(stdout, s, strlen(s)) < 0 || putchar('\n') < 0);
	return r;
}

static char digits[] = "0123456789abcdef";

static void printint(int xx, int base, int sign)
{
	char buf[16 + 1];
	int i;
	uint x;

	if (sign && (sign = xx < 0))
		x = -xx;
	else
		x = xx;

	buf[16] = 0;
	i = 15;
	do {
		buf[i--] = digits[x % base];
	} while ((x /= base) != 0);

	if (sign)
		buf[i--] = '-';
	i++;
	if (i < 0)
		puts("printint error");
	out(stdout, buf + i, 16 - i);
}

static void printptr(uint64 x)
{
	int i = 0, j;
	char buf[32 + 1];
	buf[i++] = '0';
	buf[i++] = 'x';
	for (j = 0; j < (sizeof(uint64) * 2); j++, x <<= 4)
		buf[i++] = digits[x >> (sizeof(uint64) * 8 - 4)];
	buf[i] = 0;
	out(stdout, buf, i);
}

// Print to the console. only understands %d, %x, %p, %s.
void printf(const char *fmt, ...)
{
	va_list ap;
	int l = 0;
	char *a, *z, *s = (char *)fmt;
	int f = stdout;

	va_start(ap, fmt);
	for (;;) {
		if (!*s)
			break;
		for (a = s; *s && *s != '%'; s++)
			;
		for (z = s; s[0] == '%' && s[1] == '%'; z++, s += 2)
			;
		l = z - a;
		out(f, a, l);
		if (l)
			continue;
		if (s[1] == 0)
			break;
		switch (s[1]) {
		case 'd':
			printint(va_arg(ap, int), 10, 1);
			break;
		case 'x':
			printint(va_arg(ap, int), 16, 1);
			break;
		case 'p':
			printptr(va_arg(ap, uint64));
			break;
		case 's':
			if ((a = va_arg(ap, char *)) == 0)
				a = "(null)";
			l = strnlen(a, 200);
			out(f, a, l);
			break;
		default:
			// Print unknown % sequence to draw attention.
			putchar('%');
			putchar(s[1]);
			break;
		}
		s += 2;
	}
	va_end(ap);
}