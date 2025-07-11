/*
 * Copyright (c) 2005-2014 Lev Walkin <vlm@lionet.info>.
 * All rights reserved.
 * Redistribution and modifications are permitted subject to BSD license.
 */
#include <asn_system.h>
#include <asn_internal.h>
#include <per_support.h>

char *
per_data_string(asn_per_data_t *pd) {
	static char buf[2][32];
	static int n;
	n = (n+1) % 2;
	snprintf(buf[n], sizeof(buf[n]),
		"{m=%ld span %+ld[%d..%d] (%d)}",
		(long)pd->moved,
		(((long)pd->buffer) & 0xf),
		(int)pd->nboff, (int)pd->nbits,
		(int)(pd->nbits - pd->nboff));
	return buf[n];
}

void
per_get_undo(asn_per_data_t *pd, int nbits) {
	if((ssize_t)pd->nboff < nbits) {
		assert((ssize_t)pd->nboff < nbits);
	} else {
		pd->nboff -= nbits;
		pd->moved -= nbits;
	}
}

int32_t
aper_get_align(asn_per_data_t *pd) {

	if(pd->nboff & 0x7) {
		ASN_DEBUG("Aligning %ld bits", 8 - ((unsigned long)pd->nboff & 0x7));
		return per_get_few_bits(pd, 8 - (pd->nboff & 0x7));
	}
	return 0;
}

/*
 * Extract a small number of bits (<= 31) from the specified PER data pointer.
 */
int32_t
per_get_few_bits(asn_per_data_t *pd, int nbits) {
	size_t off;	/* Next after last bit offset */
	ssize_t nleft;	/* Number of bits left in this stream */
	uint32_t accum;
	const uint8_t *buf;

	if(nbits < 0)
		return -1;

	nleft = pd->nbits - pd->nboff;
	if(nbits > nleft) {
		int32_t tailv, vhead;
		if(!pd->refill || nbits > 31) return -1;
		/* Accumulate unused bytes before refill */
		ASN_DEBUG("Obtain the rest %d bits (want %d)",
			(int)nleft, (int)nbits);
		tailv = per_get_few_bits(pd, nleft);
		if(tailv < 0) return -1;
		/* Refill (replace pd contents with new data) */
		if(pd->refill(pd))
			return -1;
		nbits -= nleft;
		vhead = per_get_few_bits(pd, nbits);
		/* Combine the rest of previous pd with the head of new one */
		tailv = (tailv << nbits) | vhead;  /* Could == -1 */
		return tailv;
	}

	/*
	 * Normalize position indicator.
	 */
	if(pd->nboff >= 8) {
		pd->buffer += (pd->nboff >> 3);
		pd->nbits  -= (pd->nboff & ~0x07);
		pd->nboff  &= 0x07;
	}
	pd->moved += nbits;
	pd->nboff += nbits;
	off = pd->nboff;
	buf = pd->buffer;

	/*
	 * Extract specified number of bits.
	 */
	if(off <= 8)
		accum = nbits ? (buf[0]) >> (8 - off) : 0;
	else if(off <= 16)
		accum = ((buf[0] << 8) + buf[1]) >> (16 - off);
	else if(off <= 24)
		accum = ((buf[0] << 16) + (buf[1] << 8) + buf[2]) >> (24 - off);
	else if(off <= 31)
		accum = ((buf[0] << 24) + (buf[1] << 16)
			+ (buf[2] << 8) + (buf[3])) >> (32 - off);
	else if(nbits <= 31) {
		asn_per_data_t tpd = *pd;
		/* Here are we with our 31-bits limit plus 1..7 bits offset. */
		per_get_undo(&tpd, nbits);
		/* The number of available bits in the stream allow
		 * for the following operations to take place without
		 * invoking the ->refill() function */
		accum  = per_get_few_bits(&tpd, nbits - 24) << 24;
		accum |= per_get_few_bits(&tpd, 24);
	} else {
		per_get_undo(pd, nbits);
		return -1;
	}

	accum &= (((uint32_t)1 << nbits) - 1);

	ASN_DEBUG("  [PER got %2d<=%2d bits => span %d %+ld[%d..%d]:%02x (%d) => 0x%02x]",
		(int)nbits, (int)nleft,
		(int)pd->moved,
		(((long)pd->buffer) & 0xf),
		(int)pd->nboff, (int)pd->nbits,
		nbits ? pd->buffer[0] : 0,
		(int)(pd->nbits - pd->nboff),
		(int)accum);

	return accum;
}

/*
 * Extract a large number of bits from the specified PER data pointer.
 */
int
per_get_many_bits(asn_per_data_t *pd, uint8_t *dst, int alright, int nbits) {
	int32_t value;

	ASN_DEBUG("align: %s, nbits %d", alright ? "YES":"NO", nbits);

	if(alright && (nbits & 7)) {
		/* Perform right alignment of a first few bits */
		value = per_get_few_bits(pd, nbits & 0x07);
		if(value < 0) return -1;
		*dst++ = value;	/* value is already right-aligned */
		nbits &= ~7;
	}

	while(nbits) {
		if(nbits >= 24) {
			value = per_get_few_bits(pd, 24);
			if(value < 0) return -1;
			*(dst++) = value >> 16;
			*(dst++) = value >> 8;
			*(dst++) = value;
			nbits -= 24;
		} else {
			value = per_get_few_bits(pd, nbits);
			if(value < 0) return -1;
			if(nbits & 7) {	/* implies left alignment */
				value <<= 8 - (nbits & 7),
				nbits += 8 - (nbits & 7);
				if(nbits > 24)
					*dst++ = value >> 24;
			}
			if(nbits > 16)
				*dst++ = value >> 16;
			if(nbits > 8)
				*dst++ = value >> 8;
			*dst++ = value;
			break;
		}
	}

	return 0;
}

/*
 * Get the length "n" from the stream.
 */
ssize_t
uper_get_length(asn_per_data_t *pd, int ebits, int *repeat) {
	ssize_t value;

	*repeat = 0;

	if(ebits >= 0) return per_get_few_bits(pd, ebits);

	value = per_get_few_bits(pd, 8);
	if(value < 0) return -1;
	if((value & 128) == 0)	/* #10.9.3.6 */
		return (value & 0x7F);
	if((value & 64) == 0) {	/* #10.9.3.7 */
		value = ((value & 63) << 8) | per_get_few_bits(pd, 8);
		if(value < 0) return -1;
		return value;
	}
	value &= 63;	/* this is "m" from X.691, #10.9.3.8 */
	if(value < 1 || value > 4)
		return -1;
	*repeat = 1;
	return (16384 * value);
}

ssize_t
aper_get_length_set_of(asn_per_data_t *pd, ssize_t lb, ssize_t ub,
		int ebits, int *repeat) {
	int constrained = (lb >= 0) && (ub >= 0);
	ssize_t value;

	*repeat = 0;

	if (constrained && ub < 65536) {
		return aper_get_constrained_whole_number(pd, lb, ub);
	}

	if (aper_get_align(pd) < 0)
		return -1;

	if(ebits >= 0) return per_get_few_bits(pd, ebits);

	value = per_get_few_bits(pd, 8);
	if(value < 0) return -1;
	if((value & 128) == 0)  /* #11.9.3.6 */
		return (value & 0x7F);
	if((value & 64) == 0) { /* #11.9.3.7 */
		value = ((value & 63) << 8) | per_get_few_bits(pd, 8);
		if(value < 0) return -1;
		return value;
	}
	value &= 63;	/* this is "m" from X.691, #11.9.3.8 */
	if(value < 1 || value > 4)
		return -1;
	*repeat = 1;
	return (16384 * value);
}

/* X.691 2002 10.5 - Decoding of a constrained whole number */
long
aper_get_constrained_whole_number(asn_per_data_t *pd, long lb, long ub) {
	assert(ub >= lb);
	long range = ub - lb + 1;
	int range_len;
	int value_len;
	long value;

	ASN_DEBUG("aper get constrained_whole_number with lb %ld and ub %ld", lb, ub);

	/* X.691 2002 10.5.4 */
	if (range == 1)
		return lb;

	/* X.691 2002 10.5.7.1 - The bit-field case. */
	if (range <= 255) {
		int bitfield_size = 8;
		for (bitfield_size = 8; bitfield_size >= 2; bitfield_size--)
			if ((range - 1) & (1 << (bitfield_size-1)))
				break;
		value = per_get_few_bits(pd, bitfield_size);
		if (value < 0 || value >= range)
			return -1;
		return value + lb;
	}

	/* X.691 2002 10.5.7.2 - The one-octet case. */
	if (range == 256) {
		if (aper_get_align(pd))
			return -1;
		value = per_get_few_bits(pd, 8);
		if (value < 0 || value >= range)
			return -1;
		return value + lb;
	}

	/* X.691 2002 10.5.7.3 - The two-octet case. */
	if (range <= 65536) {
		if (aper_get_align(pd))
			return -1;
		value = per_get_few_bits(pd, 16);
		if (value < 0 || value >= range)
			return -1;
		return value + lb;
	}

	/* X.691 2002 10.5.7.4 - The indefinite length case. */
	/* since we limit input to be 'long' we don't handle all numbers */
	/* and so length determinant is retrieved as X.691 2002 10.9.3.3 */
	/* number of bytes to store the range */
	for (range_len = 3; ; range_len++) {
		long bits = ((long)1) << (8 * range_len);
		if (range - 1 < bits)
			break;
	}
	value_len = aper_get_constrained_whole_number(pd, 1, range_len);
	if (value_len == -1)
		return -1;
	if (value_len > 4) {
		ASN_DEBUG("todo: aper_get_constrained_whole_number: value_len > 4");
		return -1;
	}
	if (aper_get_align(pd))
		return -1;
	value = per_get_few_bits(pd, value_len * 8);
	if (value < 0 || value >= range)
		return -1;
	return value + lb;
}

ssize_t
aper_get_length(asn_per_data_t *pd, int range, int ebits, int *repeat) {
	ssize_t value;

	*repeat = 0;

	if (range <= 65536 && range >= 0)
		return aper_get_nsnnwn(pd, range);

	if (aper_get_align(pd) < 0)
		return -1;

	if(ebits >= 0) return per_get_few_bits(pd, ebits);

	value = per_get_few_bits(pd, 8);
	if(value < 0) return -1;
	if((value & 128) == 0)  /* #10.9.3.6 */
		return (value & 0x7F);
	if((value & 64) == 0) { /* #10.9.3.7 */
		value = ((value & 63) << 8) | per_get_few_bits(pd, 8);
		if(value < 0) return -1;
		return value;
	}
	value &= 63;	/* this is "m" from X.691, #10.9.3.8 */
	if(value < 1 || value > 4)
		return -1;
	*repeat = 1;
	return (16384 * value);
}

/*
 * Get the normally small length "n".
 * This procedure used to decode length of extensions bit-maps
 * for SET and SEQUENCE types.
 */
ssize_t
uper_get_nslength(asn_per_data_t *pd) {
	ssize_t length;

	ASN_DEBUG("Getting normally small length");

	if(per_get_few_bits(pd, 1) == 0) {
		length = per_get_few_bits(pd, 6) + 1;
		if(length <= 0) return -1;
		ASN_DEBUG("l=%d", (int)length);
		return length;
	} else {
		int repeat;
		length = uper_get_length(pd, -1, &repeat);
		if(length >= 0 && !repeat) return length;
		return -1; /* Error, or do not support >16K extensions */
	}
}

ssize_t
aper_get_nslength(asn_per_data_t *pd) {
	ssize_t length;

	ASN_DEBUG("Getting normally small length");

	if(per_get_few_bits(pd, 1) == 0) {
		length = per_get_few_bits(pd, 6) + 1;
		if(length <= 0) return -1;
		ASN_DEBUG("l=%lld", (long long)length);
		return length;
	} else {
		int repeat;
		length = aper_get_length(pd, -1, -1, &repeat);
		if(length >= 0 && !repeat) return length;
		return -1; /* Error, or do not support >16K extensions */
	}
}

/*
 * Get the normally small non-negative whole number.
 * X.691, #10.6
 */
ssize_t
uper_get_nsnnwn(asn_per_data_t *pd) {
	ssize_t value;

	value = per_get_few_bits(pd, 7);
	if(value & 64) {	/* implicit (value < 0) */
		value &= 63;
		value <<= 2;
		value |= per_get_few_bits(pd, 2);
		if(value & 128)	/* implicit (value < 0) */
			return -1;
		if(value == 0)
			return 0;
		if(value >= 3)
			return -1;
		value = per_get_few_bits(pd, 8 * value);
		return value;
	}

	return value;
}

ssize_t
aper_get_nsnnwn(asn_per_data_t *pd, int range) {
	ssize_t value;
	int bytes = 0;

	ASN_DEBUG("getting nsnnwn with range %d", range);

	if(range <= 255) {
		if (range < 0) return -1;
		/* 1 -> 8 bits */
		int i;
		for (i = 1; i <= 8; i++) {
			int upper = 1 << i;
			if (upper >= range)
				break;
		}
		value = per_get_few_bits(pd, i);
		return value;
	} else if (range == 256){
		/* 1 byte */
		bytes = 1;
		return -1;
	} else if (range <= 65536) {
		/* 2 bytes */
		bytes = 2;
	} else {
		int length;

		/* handle indefinite range */
		length = per_get_few_bits(pd, 1);
		if (length == 0)
		    return per_get_few_bits(pd, 6);

		if (aper_get_align(pd) < 0)
		    return -1;

		length = per_get_few_bits(pd, 8);
		/* the length is not likely to be that big */
		if (length > 4)
		    return -1;
		value = 0;
		if (per_get_many_bits(pd, (uint8_t *)&value, 0, length * 8) < 0)
		    return -1;
		return value;
	}
	if (aper_get_align(pd) < 0)
		return -1;
	value = per_get_few_bits(pd, 8 * bytes);
	return value;
}

/*
 * X.691-11/2008, #11.6
 * Encoding of a normally small non-negative whole number
 */
int
uper_put_nsnnwn(asn_per_outp_t *po, int n) {
	int bytes;

		ASN_DEBUG("uper put nsnnwn n %d", n);
	if(n <= 63) {
		if(n < 0) return -1;
		return per_put_few_bits(po, n, 7);
	}
	if(n < 256)
		bytes = 1;
	else if(n < 65536)
		bytes = 2;
	else if(n < 256 * 65536)
		bytes = 3;
	else
		return -1;	/* This is not a "normally small" value */
	if(per_put_few_bits(po, bytes, 8))
		return -1;

	return per_put_few_bits(po, n, 8 * bytes);
}


/* X.691-2008/11, #11.5.6 -> #11.3 */
int uper_get_constrained_whole_number(asn_per_data_t *pd, unsigned long long *out_value, int nbits) {
	unsigned long long lhalf;    /* Lower half of the number*/
	long long half;

	if(nbits <= 31) {
		half = per_get_few_bits(pd, nbits);
		if(half < 0) return -1;
		*out_value = half;
		return 0;
	}

	if((size_t)nbits > 8 * sizeof(*out_value))
		return -1;  /* RANGE */

	half = per_get_few_bits(pd, 31);
	if(half < 0) return -1;

	if(uper_get_constrained_whole_number(pd, &lhalf, nbits - 31))
		return -1;

	*out_value = ((unsigned long long)half << (nbits - 31)) | lhalf;
	return 0;
}


/* X.691-2008/11, #11.5.6 -> #11.3 */
int uper_put_constrained_whole_number_s(asn_per_outp_t *po, long long v, int nbits) {
	/*
	 * Assume signed number can be safely coerced into
	 * unsigned of the same range.
	 * The following testing code will likely be optimized out
	 * by compiler if it is true.
	 */
	unsigned long long uvalue1 = ULONG_MAX;
	         long long svalue  = uvalue1;
	unsigned long long uvalue2 = svalue;
	assert(uvalue1 == uvalue2);
	return uper_put_constrained_whole_number_u(po, v, nbits);
}

int uper_put_constrained_whole_number_u(asn_per_outp_t *po, unsigned long long v, int nbits) {
	if(nbits <= 31) {
		return per_put_few_bits(po, v, nbits);
	} else {
		/* Put higher portion first, followed by lower 31-bit */
		if(uper_put_constrained_whole_number_u(po, v >> 31, nbits - 31))
			return -1;
		return per_put_few_bits(po, v, 31);
	}
}

/*
 * Put a small number of bits (<= 31).
 */
int
per_put_few_bits(asn_per_outp_t *po, uint32_t bits, int obits) {
	size_t off;	/* Next after last bit offset */
	size_t omsk;	/* Existing last byte meaningful bits mask */
	uint8_t *buf;

	if(obits <= 0 || obits >= 32) return obits ? -1 : 0;

	ASN_DEBUG("[PER put %d bits %x to %p+%d bits]",
			obits, (int)bits, po->buffer, (int)po->nboff);

	/*
	 * Normalize position indicator.
	 */
	if(po->nboff >= 8) {
		po->buffer += (po->nboff >> 3);
		po->nbits  -= (po->nboff & ~0x07);
		po->nboff  &= 0x07;
	}

	/*
	 * Flush whole-bytes output, if necessary.
	 */
	if(po->nboff + obits > po->nbits) {
		int complete_bytes = (po->buffer - po->tmpspace);
		ASN_DEBUG("[PER output %ld complete + %ld]",
			(long)complete_bytes, (long)po->flushed_bytes);
		if(po->outper(po->tmpspace, complete_bytes, po->op_key) < 0)
			return -1;
		if(po->nboff)
			po->tmpspace[0] = po->buffer[0];
		po->buffer = po->tmpspace;
		po->nbits = 8 * sizeof(po->tmpspace);
		po->flushed_bytes += complete_bytes;
	}

	/*
	 * Now, due to sizeof(tmpspace), we are guaranteed large enough space.
	 */
	buf = po->buffer;
	omsk = ~((1 << (8 - po->nboff)) - 1);
	off = (po->nboff + obits);

	/* Clear data of debris before meaningful bits */
	bits &= (((uint32_t)1 << obits) - 1);

	ASN_DEBUG("[PER out %d %u/%x (t=%d,o=%d) %x&%x=%x]", obits,
		(int)bits, (int)bits,
		(int)po->nboff, (int)off,
		buf[0], (int)(omsk&0xff),
		(int)(buf[0] & omsk));

	if(off <= 8)	/* Completely within 1 byte */
		po->nboff = off,
		bits <<= (8 - off),
		buf[0] = (buf[0] & omsk) | bits;
	else if(off <= 16)
		po->nboff = off,
		bits <<= (16 - off),
		buf[0] = (buf[0] & omsk) | (bits >> 8),
		buf[1] = bits;
	else if(off <= 24)
		po->nboff = off,
		bits <<= (24 - off),
		buf[0] = (buf[0] & omsk) | (bits >> 16),
		buf[1] = bits >> 8,
		buf[2] = bits;
	else if(off <= 31)
		po->nboff = off,
		bits <<= (32 - off),
		buf[0] = (buf[0] & omsk) | (bits >> 24),
		buf[1] = bits >> 16,
		buf[2] = bits >> 8,
		buf[3] = bits;
	else {
		per_put_few_bits(po, bits >> (obits - 24), 24);
		per_put_few_bits(po, bits, obits - 24);
	}

	ASN_DEBUG("[PER out %u/%x => %02x buf+%ld]",
		(int)bits, (int)bits, buf[0],
		(long)(po->buffer - po->tmpspace));

	return 0;
}

int
aper_put_nsnnwn(asn_per_outp_t *po, int range, int number) {
	int bytes;

    ASN_DEBUG("aper put nsnnwn %d with range %d", number, range);
	/* 10.5.7.1 X.691 */
	if(range < 0) {
		int i;
		for (i = 1; ; i++) {
			int bits = 1 << (8 * i);
			if (number <= bits)
				break;
		}
		bytes = i;
		assert(i <= 4);
	}
	if(range <= 255) {
		int i;
		for (i = 1; i <= 8; i++) {
			int bits = 1 << i;
			if (range <= bits)
				break;
		}
		return per_put_few_bits(po, number, i);
	} else if(range == 256) {
		bytes = 1;
	} else if(range <= 65536) {
		bytes = 2;
	} else { /* Ranges > 64K */
		int i;
		for (i = 1; ; i++) {
			int bits = 1 << (8 * i);
			if (range <= bits)
				break;
		}
		assert(i <= 4);
		bytes = i;
	}
	if(aper_put_align(po) < 0) /* Aligning on octet */
		return -1;
// 	if(per_put_few_bits(po, bytes, 8))
// 		return -1;

    return per_put_few_bits(po, number, 8 * bytes);
}

int aper_put_align(asn_per_outp_t *po) {

	if(po->nboff & 0x7) {
		ASN_DEBUG("Aligning %ld bits", 8 - ((unsigned long)po->nboff & 0x7));
		if(per_put_few_bits(po, 0x00, (8 - (po->nboff & 0x7))))
			return -1;
	}
	return 0;
}

/*
 * Output a large number of bits.
 */
int
per_put_many_bits(asn_per_outp_t *po, const uint8_t *src, int nbits) {

	while(nbits) {
		uint32_t value;

		if(nbits >= 24) {
			value = (src[0] << 16) | (src[1] << 8) | src[2];
			src += 3;
			nbits -= 24;
			if(per_put_few_bits(po, value, 24))
				return -1;
		} else {
			value = src[0];
			if(nbits > 8)
				value = (value << 8) | src[1];
			if(nbits > 16)
				value = (value << 8) | src[2];
			if(nbits & 0x07)
				value >>= (8 - (nbits & 0x07));
			if(per_put_few_bits(po, value, nbits))
				return -1;
			break;
		}
	}

	return 0;
}

/*
 * Put the length "n" (or part of it) into the stream.
 */
ssize_t
uper_put_length(asn_per_outp_t *po, size_t length) {

	ASN_DEBUG("UPER put length %zu", length);

	if(length <= 127)	/* #10.9.3.6 */
		return per_put_few_bits(po, length, 8)
			? -1 : (ssize_t)length;
	else if(length < 16384)	/* #10.9.3.7 */
		return per_put_few_bits(po, length|0x8000, 16)
			? -1 : (ssize_t)length;

	length >>= 14;
	if(length > 4) length = 4;

	return per_put_few_bits(po, 0xC0 | length, 8)
			? -1 : (ssize_t)(length << 14);
}

ssize_t
aper_put_length(asn_per_outp_t *po, int range, size_t length) {

	ASN_DEBUG("APER put length %zu with range %d", length, range);

	/* 10.9 X.691 Note 2 */
	if (range <= 65536 && range >= 0)
		return aper_put_nsnnwn(po, range, length);

	if (aper_put_align(po) < 0)
		return -1;

	if(length <= 127)	   /* #10.9.3.6 */{
		return per_put_few_bits(po, length, 8)
		? -1 : (ssize_t)length;
	}
	else if(length < 16384) /* #10.9.3.7 */
		return per_put_few_bits(po, length|0x8000, 16)
		? -1 : (ssize_t)length;

	length >>= 14;
	if(length > 4) length = 4;

	return per_put_few_bits(po, 0xC0 | length, 8)
	? -1 : (ssize_t)(length << 14);
}


/*
 * Put the normally small length "n" into the stream.
 * This procedure used to encode length of extensions bit-maps
 * for SET and SEQUENCE types.
 */
int
uper_put_nslength(asn_per_outp_t *po, size_t length) {

	if(length <= 64) {
		/* #10.9.3.4 */
		if(length == 0) return -1;
		return per_put_few_bits(po, length-1, 7) ? -1 : 0;
	} else {
		if(uper_put_length(po, length) != (ssize_t)length) {
			/* This might happen in case of >16K extensions */
			return -1;
		}
	}

	return 0;
}

int
aper_put_nslength(asn_per_outp_t *po, size_t length) {

	if(length <= 64) {
		/* #10.9.3.4 */
		if(length == 0) return -1;
		return per_put_few_bits(po, length-1, 7) ? -1 : 0;
	} else {
		if(aper_put_length(po, -1, length) != (ssize_t)length) {
			/* This might happen in case of >16K extensions */
			return -1;
		}
	}

	return 0;
}
