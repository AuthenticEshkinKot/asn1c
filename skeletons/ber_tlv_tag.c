/*-
 * Copyright (c) 2003, 2004 Lev Walkin <vlm@lionet.info>. All rights reserved.
 * Redistribution and modifications are permitted subject to BSD license.
 */
#include <constr_TYPE.h>
#include <ber_tlv_tag.h>
#include <errno.h>

ssize_t
ber_fetch_tag(void *ptr, size_t size, ber_tlv_tag_t *tag_r) {
	ber_tlv_tag_t val;
	ber_tlv_tag_t tclass;
	ssize_t skipped;

	if(size == 0)
		return 0;

	val = *(uint8_t *)ptr;
	tclass = (val >> 6);
	if((val &= 31) != 31) {
		/*
		 * Simple form: everything encoded in a single octet.
		 * Tag Class is encoded using two least significant bits.
		 */
		*tag_r = (val << 2) | tclass;
		return 1;
	}

	/*
	 * Each octet contains 7 bits of useful information.
	 * The MSB is 0 if it is the last octet of the tag.
	 */
	for(val = 0, ptr++, skipped = 2; skipped < size; ptr++, skipped++) {
		unsigned oct = *(uint8_t *)ptr;
		if(oct & 0x80) {
			val = (val << 7) | (oct & 0x7F);
			/*
			 * Make sure there are at least 9 bits spare
			 * at the MS side of a value.
			 */
			if(val >> ((8 * sizeof(val)) - 9)) {
				/*
				 * We would not be able to accomodate
				 * any more tag bits.
				 */
				return -1;
			}
		} else {
			*tag_r = (val << 9) | (oct << 2) | tclass;
			return skipped;
		}
	}

	return 0;	/* Want more */
}


ssize_t
ber_tlv_tag_fwrite(ber_tlv_tag_t tag, FILE *f) {
	char buf[sizeof("[APPLICATION ]") + 32];
	ssize_t ret;

	ret = ber_tlv_tag_snprint(tag, buf, sizeof(buf));
	if(ret >= sizeof(buf) || ret < 2) {
		errno = EPERM;
		return -1;
	}

	return fwrite(buf, 1, ret, f);
}

ssize_t
ber_tlv_tag_snprint(ber_tlv_tag_t tag, char *buf, size_t size) {
	char *type = 0;
	int ret;

	switch(tag & 0x3) {
	case ASN_TAG_CLASS_UNIVERSAL:	type = "UNIVERSAL ";	break;
	case ASN_TAG_CLASS_APPLICATION:	type = "APPLICATION ";	break;
	case ASN_TAG_CLASS_CONTEXT:	type = "";		break;
	case ASN_TAG_CLASS_PRIVATE:	type = "PRIVATE ";	break;
	}

	ret = snprintf(buf, size, "[%s%u]", type, ((unsigned)tag) >> 2);
	if(ret <= 0 && size) buf[0] = '\0';	/* against broken libc's */

	return ret;
}

char *
ber_tlv_tag_string(ber_tlv_tag_t tag) {
	static char buf[sizeof("[APPLICATION ]") + 32];

	(void)ber_tlv_tag_snprint(tag, buf, sizeof(buf));

	return buf;
}


ssize_t
der_tlv_tag_serialize(ber_tlv_tag_t tag, void *bufp, size_t size) {
	int tclass = BER_TAG_CLASS(tag);
	ber_tlv_tag_t tval = BER_TAG_VALUE(tag);
	uint8_t *buf = bufp;
	uint8_t *end;
	ssize_t computed_size;
	int i;

	if(tval <= 30) {
		/* Encoded in 1 octet */
		if(size) buf[0] = (tclass << 6) | tval;
		return 1;
	} else if(size) {
		*buf++ = (tclass << 6) | 0x1F;
		size--;
	}

	/*
	 * Compute the size of the subsequent bytes.
	 * The routine is written so every floating-point
	 * operation is done at compile time.
	 * Note, there is a subtle problem lurking here,
	 * could you guess where it is? :)
	 * Hint: what happens when ((8*sizeof(tag))%7) == 0?
	 */
	computed_size = 1 + 8 * sizeof(tag) / 7;
	for(i = (8*sizeof(tag)) - ((8*sizeof(tag))%7); i >= 7; i -= 7) {
		if((tval >> i) & 0x7F) break;
		computed_size--;
	}

	/*
	 * Fill in the buffer, space permitting.
	 */
	if(size > computed_size)
		end = buf + computed_size;
	else
		end = buf + size;
	for((void)i; buf < end; i -= 7, buf++) {
		*buf = 0x80 | ((tval>>i) & 0x7F);
	}

	return computed_size + 1;
}
