/*-
 * Copyright (c) 2003, 2004 Lev Walkin <vlm@lionet.info>. All rights reserved.
 * Redistribution and modifications are permitted subject to BSD license.
 */
#ifndef	_CONSTR_CHOICE_H_
#define	_CONSTR_CHOICE_H_

#include <constr_TYPE.h>

/*
 * A single element of the CHOICE type.
 */
typedef struct asn1_CHOICE_element_s {
	int memb_offset;		/* Offset of the element */
	int optional;			/* Whether the element is optional */
	ber_tlv_tag_t tag;		/* Outmost (most immediate) tag */
	int tag_mode;		/* IMPLICIT/no/EXPLICIT tag at current level */
	asn1_TYPE_descriptor_t
		*type;			/* Member type descriptor */
	char *name;			/* ASN.1 identifier of the element */
} asn1_CHOICE_element_t;

typedef struct asn1_CHOICE_tag2member_s {
	ber_tlv_tag_t el_tag;	/* Outmost tag of the member */
	int el_no;		/* Index of the associated member, base 0 */
} asn1_CHOICE_tag2member_t;

typedef struct asn1_CHOICE_specifics_s {
	/*
	 * Target structure description.
	 */
	int struct_size;	/* Size of the target structure. */
	int ctx_offset;		/* Offset of the ber_dec_ctx_t member */
	int pres_offset;	/* Identifier of the present member */
	int pres_size;		/* Size of the identifier (enum) */

	/*
	 * Members of the CHOICE structure.
	 */
	asn1_CHOICE_element_t *elements;
	int elements_count;

	/*
	 * Tags to members mapping table.
	 */
	asn1_CHOICE_tag2member_t *tag2el;
	int tag2el_count;

	/*
	 * Extensions-related stuff.
	 */
	int extensible;			/* Whether CHOICE is extensible */
} asn1_CHOICE_specifics_t;

/*
 * A set specialized functions dealing with the CHOICE type.
 */
asn_constr_check_f CHOICE_constraint;
ber_type_decoder_f CHOICE_decode_ber;
der_type_encoder_f CHOICE_encode_der;
asn_outmost_tag_f CHOICE_outmost_tag;
asn_struct_print_f CHOICE_print;
asn_struct_free_f CHOICE_free;

#endif	/* _CONSTR_CHOICE_H_ */