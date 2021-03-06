#include <stdio.h>
#include <string.h>
#include <stdint.h>

#define	BUF		40

/*
 * Macros for converting digits to letters and vice versa
 */
#define	to_digit(c)	((c) - '0')
#define is_digit(c)	((unsigned)to_digit (c) <= 9)
#define	to_char(n)	((n) + '0')

/*
 * Flags used during conversion.
 */
#define	HEXPREFIX	0x002		/* add 0x or 0X prefix */
#define	LADJUST		0x004		/* left adjustment */
#define	LONGINT		0x010		/* long integer */
#define	SHORTINT	0x040		/* short integer */
#define	ZEROPAD		0x080		/* zero (as opposed to blank) pad */

#define _CONST const

#define PRINT(ptr, len) \
{ \
	int i; \
	for (i = 0; i < len; i++) { \
		int c = ((char *)ptr)[i]; \
		putchar(c); \
	} \
}

unsigned long get_number_arg(char **data, int flags)
{
	unsigned long ret;

	if (flags & LONGINT) {
		ret = (unsigned long)(*(unsigned short *)(*data));
		(*data) += sizeof(unsigned short);
	}
	else if (flags & SHORTINT) {
		ret = (*(unsigned long *)(*data));
		(*data) += sizeof(unsigned long);
	}
	else {
		ret = (unsigned long)(*(unsigned int *)(*data));
		(*data) += sizeof(unsigned int);
	}
	return ret;
}

char *get_string_arg(char **data)
{
	char *ret = *data;
	while (**data != 0)
		(*data)++;
	(*data)++;

	return ret;
}

char get_char_arg(char **data)
{
	char ret = **data;
	(*data)++;

	return ret;
}

#define	FLUSH() fflush(stdout)

void print(char *fmt0, char *data0)
{
	register char *fmt;	/* format string */
	char *data;
	register int ch;	/* character from fmt */
	register int n, m;	/* handy integers (short term usage) */
	register char *cp;	/* handy char pointer (short term usage) */
	register int flags;	/* flags as above */
	char *fmt_anchor;       /* current format spec being processed */
	int ret;		/* return value accumulator */
	int width;		/* width from format (%8d), or 0 */
	int prec;		/* precision from format (%.3d), or -1 */
	char sign;		/* sign prefix (' ', '+', '-', or \0) */
	unsigned long _uquad;	/* integer arguments %[diouxX] */
	enum { DEC, HEX } base;/* base for [diouxX] conversion */
	int dprec;		/* a copy of prec if [diouxX], 0 otherwise */
	int realsz;		/* field size expanded by dprec */
	int size;		/* size of converted field or string */
	char *xdigs = NULL;	/* digits for [xX] conversion */
	char buf[BUF];		/* space for %c, %S, %[diouxX], %[aA] */
	char ox[2];		/* space for 0x hex-prefix */

	/*
	 * Choose PADSIZE to trade efficiency vs. size.  If larger printf
	 * fields occur frequently, increase PADSIZE and make the initialisers
	 * below longer.
	 */
#define	PADSIZE	16		/* pad chunk size */
	static _CONST char blanks[PADSIZE] =
	 {' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' '};
	static _CONST char zeroes[PADSIZE] =
	 {'0','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0'};

	/*
	 * To extend shorts properly, we need both signed and unsigned
	 * argument extraction methods.
	 */

#define	PAD(howmany, with) { \
	if ((n = (howmany)) > 0) { \
		while (n > PADSIZE) { \
			PRINT (with, PADSIZE); \
			n -= PADSIZE; \
		} \
		PRINT (with, n); \
	} \
}

	fmt = (char *)fmt0;
	data = data0;

	/*
	 * Scan the format for conversions (`%' character).
	 */
	for (;;) {
		cp = fmt;

		while (*fmt != '\0' && *fmt != '%')
			fmt += 1;
			
		if ((m = fmt - cp) != 0) {
			PRINT (cp, m);
			ret += m;
		}

		if (*fmt == '\0')
			goto done;

		fmt_anchor = fmt;
		fmt++;		/* skip over '%' */

		flags = 0;
		dprec = 0;
		width = 0;
		prec = -1;
		sign = '\0';

rflag:		ch = *fmt++;
reswitch:	switch (ch) {
		case ' ':
			/*
			 * ``If the space and + flags both appear, the space
			 * flag will be ignored.''
			 *	-- ANSI X3J11
			 */
			if (!sign)
				sign = ' ';
			goto rflag;
		case '0':
			/*
			 * ``Note that 0 is taken as a flag, not as the
			 * beginning of a field width.''
			 *	-- ANSI X3J11
			 */
			flags |= ZEROPAD;
			goto rflag;
		case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			n = 0;
			do {
				n = 10 * n + to_digit (ch);
				ch = *fmt++;
			} while (is_digit (ch));

			width = n;
			goto reswitch;
		case 'h':
				flags |= SHORTINT;
			goto rflag;
		case 'l':
				flags |= LONGINT;
			goto rflag;
		case 'c':
			cp = buf;
			{
				*cp = get_char_arg(&data);

				size = 1;
			}
			sign = '\0';
			break;
		case 'd':
		case 'i':
			_uquad = get_number_arg(&data, flags);

			if ((long) _uquad < 0)
			{
				_uquad = -_uquad;
				sign = '-';
			}
			base = DEC;
			goto number;
		case 's':
			cp = get_string_arg(&data);

			sign = '\0';

			if (prec >= 0) {
				/*
				 * can't use strlen; can only look for the
				 * NUL in the first `prec' characters, and
				 * strlen () will go further.
				 */
				char *p = memchr (cp, 0, prec);

				if (p != NULL)
					size = p - cp;
				else
					size = prec;
			} else
				size = strlen (cp);

			break;
		case 'u':
			_uquad = get_number_arg(&data, flags);
			base = DEC;
			goto nosign;
		case 'X':
			xdigs = "0123456789ABCDEF";
			goto hex;
		case 'x':
			xdigs = "0123456789abcdef";
hex:
			_uquad = get_number_arg(&data, flags);
			base = HEX;

			/* unsigned conversions */
nosign:			sign = '\0';
			/*
			 * ``... diouXx conversions ... if a precision is
			 * specified, the 0 flag will be ignored.''
			 *	-- ANSI X3J11
			 */
number:			if ((dprec = prec) >= 0)
				flags &= ~ZEROPAD;

			/*
			 * ``The result of converting a zero value with an
			 * explicit precision of zero is no characters.''
			 *	-- ANSI X3J11
			 */
			cp = buf + BUF;
			if (_uquad != 0 || prec != 0) {
				/*
				 * Unsigned mod is hard, and unsigned mod
				 * by a constant is easier than that by
				 * a variable; hence this switch.
				 */
				switch (base) {
				case DEC:
					/* many numbers are 1 digit */
					if (_uquad < 10) {
						*--cp = to_char(_uquad);
						break;
					}

					do {
					  *--cp = to_char (_uquad % 10);
					  _uquad /= 10;
					} while (_uquad != 0);
					break;

				case HEX:
					do {
						*--cp = xdigs[_uquad & 15];
						_uquad >>= 4;
					} while (_uquad);
					break;

				default:
					cp = "bug in vfprintf: bad base";
					size = strlen (cp);
					goto skipsize;
				}
			}

			size = buf + BUF - cp;
		skipsize:
			break;
		default:	/* "%?" prints ?, unless ? is NUL */
			if (ch == '\0')
				goto done;
			/* pretend it was %c with argument ch */
			cp = buf;
			*cp = ch;
			size = 1;
			sign = '\0';
			break;
		}

		/*
		 * All reasonable formats wind up here.  At this point, `cp'
		 * points to a string which (if not flags&LADJUST) should be
		 * padded out to `width' places.  If flags&ZEROPAD, it should
		 * first be prefixed by any sign or other prefix; otherwise,
		 * it should be blank padded before the prefix is emitted.
		 * After any left-hand padding and prefixing, emit zeroes
		 * required by a decimal [diouxX] precision, then print the
		 * string proper, then emit zeroes required by any leftover
		 * floating precision; finally, if LADJUST, pad with blanks.
		 * If flags&FPT, ch must be in [aAeEfg].
		 *
		 * Compute actual size, so we know how much to pad.
		 * size excludes decimal prec; realsz includes it.
		 */
		realsz = dprec > size ? dprec : size;
		if (sign)
			realsz++;
		if (flags & HEXPREFIX)
			realsz+= 2;

		/* right-adjusting blank padding */
		if ((flags & (LADJUST|ZEROPAD)) == 0)
			PAD (width - realsz, blanks);

		/* prefix */
		if (sign)
			PRINT (&sign, 1);
		if (flags & HEXPREFIX)
			PRINT (ox, 2);

		/* right-adjusting zero padding */
		if ((flags & (LADJUST|ZEROPAD)) == ZEROPAD)
			PAD (width - realsz, zeroes);

		/* leading zeroes from decimal precision */
		PAD (dprec - size, zeroes);

		/* the string or number proper */
		PRINT (cp, size);
		/* left-adjusting padding (always blank) */
		if (flags & LADJUST)
			PAD (width - realsz, blanks);

		/* finally, adjust ret */
		ret += width > realsz ? width : realsz;

		FLUSH ();	/* copy out the I/O vectors */

	}
done:
	FLUSH ();
}
