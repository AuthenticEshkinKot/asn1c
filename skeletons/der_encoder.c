/*-
 * Copyright (c) 2003, 2004 Lev Walkin <vlm@lionet.info>. All rights reserved.
 * Redistribution and modifications are permitted subject to BSD license.
 */
#include <constr_TYPE.h>
#include <assert.h>

static ssize_t der_write_TL(ber_tlv_tag_t tag, ber_tlv_len_t len,
	asn_app_consume_bytes_f *cb, void *app_key, int constructed);

/*
 * The DER encoder of any type.
 */
der_enc_rval_t
der_encode(asn1_TYPE_descriptor_t *type_descriptor, void *struct_ptr,
	asn_app_consume_bytes_f *consume_bytes, void *app_key) {

	ASN_DEBUG("DER encoder invoked for %s",
		type_descriptor->name);

	/*
	 * Invoke type-specific encoder.
	 */
	return type_descriptor->der_encoder(type_descriptor,
		struct_ptr,	/* Pointer to the destination structure */
		0, 0,
		consume_bytes, app_key);
}

/*
 * Write out leading TL[v] sequence according to the type definition.
 */
ssize_t
der_write_tags(asn1_TYPE_descriptor_t *sd,
		size_t struct_length,
		int tag_mode,
		ber_tlv_tag_t tag,	/* EXPLICIT or IMPLICIT tag */
		asn_app_consume_bytes_f *cb,
		void *app_key) {
	ber_tlv_tag_t *tags;	/* Copy of tags stream */
	int tags_count;		/* Number of tags */
	size_t overall_length;
	ssize_t *lens;
	int i;

	if(tag_mode) {
		/*
		 * Instead of doing shaman dance like we do in ber_check_tags(),
		 * allocate a small array on the stack
		 * and initialize it appropriately.
		 */
		tags = alloca((sd->tags_count + (tag_mode?1:0))
			* sizeof(ber_tlv_tag_t));
		if(tags == NULL) return -1;	/* Impossible on i386 */
		tags_count = sd->tags_count
			+ 1	/* EXPLICIT or IMPLICIT tag is given */
			- ((tag_mode==-1)?sd->tags_impl_skip:0);
		/* Copy tags over */
		tags[0] = tag;
		for(i = 1; i < tags_count; i++)
			tags[i] = sd->tags[i - 1 + sd->tags_impl_skip];
	} else {
		tags = sd->tags;
		tags_count = sd->tags_count;
	}

	/* No tags to write */
	if(tags_count == 0)
		return 0;

	lens = alloca(tags_count * sizeof(lens[0]));
	if(lens == NULL) return -1;

	/*
	 * Array of tags is initialized.
	 * Now, compute the size of the TLV pairs, from right to left.
	 */
	overall_length = struct_length;
	for(i = tags_count - 1; i >= 0; --i) {
		lens[i] = der_write_TL(tags[i], overall_length, 0, 0, 0);
		if(lens[i] == -1) return -1;
		overall_length += lens[i];
		lens[i] = overall_length - lens[i];
	}

	if(!cb) return overall_length - struct_length;

	ASN_DEBUG("%s %s TL sequence (%d elements)",
		cb?"Encoding":"Estimating", sd->name, tags_count);

	/*
	 * Encode the TL sequence for real.
	 */
	for(i = 0; i < tags_count; i++) {
		ssize_t len;
		int _constr;

		/* If this one happens to be constructed, do it. */
		if(i < (tags_count - 1) || sd->last_tag_form == 1)
			_constr = 1;
		else	_constr = 0;

		len = der_write_TL(tags[i], lens[i], cb, app_key, _constr);
		if(len == -1) return -1;
	}

	return overall_length - struct_length;
}

static ssize_t
der_write_TL(ber_tlv_tag_t tag, ber_tlv_len_t len,
		asn_app_consume_bytes_f *cb, void *app_key,
		int constructed) {
	uint8_t buf[32];
	size_t size = 0;
	int buf_size = cb?sizeof(buf):0;
	ssize_t tmp;

	/* Serialize tag (T from TLV) into possibly zero-length buffer */
	tmp = der_tlv_tag_serialize(tag, buf, buf_size);
	if(tmp == -1 || tmp > sizeof(buf)) return -1;
	size += tmp;

	/* Serialize length (L from TLV) into possibly zero-length buffer */
	tmp = der_tlv_length_serialize(len, buf+size, buf_size?buf_size-size:0);
	if(tmp == -1) return -1;
	size += tmp;

	if(size > sizeof(buf))
		return -1;

	/*
	 * If callback is specified, invoke it, and check its return value.
	 */
	if(cb) {
		if(constructed) *buf |= 0x20;
		if(cb(buf, size, app_key) == -1) {
			return -1;
		}
	}

	return size;
}