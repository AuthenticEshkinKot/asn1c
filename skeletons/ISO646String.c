/*-
 * Copyright (c) 2003, 2004 Lev Walkin <vlm@lionet.info>. All rights reserved.
 * Redistribution and modifications are permitted subject to BSD license.
 */
#include <ISO646String.h>

/*
 * ISO646String basic type description.
 */
static ber_tlv_tag_t asn1_DEF_ISO646String_tags[] = {
	(ASN_TAG_CLASS_UNIVERSAL | (26 << 2))
};
asn1_TYPE_descriptor_t asn1_DEF_ISO646String = {
	"ISO646String",
	VisibleString_constraint,
	OCTET_STRING_decode_ber,    /* Implemented in terms of OCTET STRING */
	OCTET_STRING_encode_der,    /* Implemented in terms of OCTET STRING */
	OCTET_STRING_print_ascii,   /* ASCII subset */
	OCTET_STRING_free,
	0, /* Use generic outmost tag fetcher */
	asn1_DEF_ISO646String_tags,
	sizeof(asn1_DEF_ISO646String_tags)
	  / sizeof(asn1_DEF_ISO646String_tags[0]),
	1,	/* Single UNIVERSAL tag may be implicitly overriden */
	-1,	/* Both ways are fine */
};
