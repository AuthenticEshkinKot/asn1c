/*
 * A collection of data members of unspecified types.
 */
#ifndef	ASN1_PARSER_EXPR_H
#define	ASN1_PARSER_EXPR_H

/*
 * Meta type of the ASN expression.
 */
typedef enum asn1p_expr_meta {
	AMT_INVALID,
	AMT_TYPE,		/* Type1 ::= INTEGER */
	AMT_TYPEREF,		/* Type2 ::= Type1 */
	AMT_PARAMTYPE,		/* Type3{Parameter} ::= SET { ... } */
	AMT_VALUE,		/* value1 Type1 ::= 1 */
	AMT_VALUESET,		/* ValueSet Type1 ::= { value1 } */
	AMT_OBJECT,		/* FUNCTION ::= CLASS {...} */
	AMT_OBJECTSET,		/* Functions FUNCTION ::= {...} */
	AMT_OBJECTFIELD,	/* ... */
	AMT_EXPR_META_MAX
} asn1p_expr_meta_e;

/*
 * ASN type of the expression.
 */
typedef enum asn1p_expr_type {
	/*
	 * Internal types.
	 */
	A1TC_INVALID,		/* Invalid type */
	A1TC_REFERENCE,		/* Reference to the type defined elsewhere */
	A1TC_EXPORTVAR,		/* We're exporting this definition */
	A1TC_UNIVERVAL,		/* A value of an ENUMERATED, INTEGER or BS */
	A1TC_BOOLBITPOS,	/* A bit position in a BIT STRING */
	A1TC_BITVECTOR,		/* A plain collection of bits */
	A1TC_OPAQUE,		/* Opaque data encoded as a bitvector */
	A1TC_EXTENSIBLE,	/* An extension marker "..." */
	A1TC_PARAMETRIZED,	/* A parametrized type declaration */
	A1TC_VALUESET,		/* Value set definition */
	A1TC_CLASSDEF,		/* Information Object Class */
	A1TC_CLASSFIELD,	/* Information Object Class field */
	A1TC_INSTANCE,		/* Instance of Object Class */
	A1TC_TYPEID,		/* Type identifier */
	/*
	 * ASN.1 Constructed types
	 */
#define	ASN_CONSTR_MASK		0x10	/* Every constructed type */
	ASN_CONSTR_SEQUENCE	= ASN_CONSTR_MASK,	/* SEQUENCE */
	ASN_CONSTR_CHOICE,		/* CHOICE */
	ASN_CONSTR_SET,			/* SET */
	ASN_CONSTR_SEQUENCE_OF,		/* SEQUENCE OF */
	ASN_CONSTR_SET_OF,		/* SET OF */
	ASN_CONSTR_ANY,			/* ANY (deprecated) */
	/*
	 * ASN.1 Basic types
	 */
#define	ASN_BASIC_MASK		0x20	/* Every basic type */
	ASN_BASIC_BOOLEAN	= ASN_BASIC_MASK,
	ASN_BASIC_NULL,
	ASN_BASIC_INTEGER,
	ASN_BASIC_REAL,
	ASN_BASIC_ENUMERATED,
	ASN_BASIC_BIT_STRING,
	ASN_BASIC_OCTET_STRING,
	ASN_BASIC_OBJECT_IDENTIFIER,
	ASN_BASIC_RELATIVE_OID,
	ASN_BASIC_EXTERNAL,
	ASN_BASIC_EMBEDDED_PDV,
	ASN_BASIC_CHARACTER_STRING,
	ASN_BASIC_UTCTime,
	ASN_BASIC_GeneralizedTime,
	/*
	 * ASN.1 String types
	 */
#define	ASN_STRING_MASK		0x40	/* Every string type */
	ASN_STRING_BMPString	= ASN_STRING_MASK,
	ASN_STRING_GeneralString,
	ASN_STRING_GraphicString,
	ASN_STRING_IA5String,
	ASN_STRING_ISO646String,
	ASN_STRING_NumericString,
	ASN_STRING_PrintableString,
	ASN_STRING_TeletexString,
	ASN_STRING_T61String,
	ASN_STRING_UniversalString,
	ASN_STRING_UTF8String,
	ASN_STRING_VideotexString,
	ASN_STRING_VisibleString,
	ASN_STRING_ObjectDescriptor,
	ASN_EXPR_TYPE_MAX
} asn1p_expr_type_e;

#include "asn1p_expr_str.h"
#include "asn1p_expr2uclass.h"

/*
 * A named collection of types.
 */
typedef struct asn1p_expr_s {

	/*
	 * Human readable name.
	 */
	char *Identifier;

	/*
	 * Meta type of the expression (type, value, value set, etc).
	 */
	asn1p_expr_meta_e meta_type;

	/*
	 * ASN type of the expression.
	 */
	asn1p_expr_type_e expr_type;

	/*
	 * Referenced type, if defined elsewhere.
	 * (If expr_type == A1TC_REFERENCE)
	 */
	asn1p_ref_t	*reference;

	/*
	 * Constraints for the type.
	 */
	asn1p_constraint_t *constraints;

	/*
	 * A list of parameters for parametrized type declaration
	 * (AMT_PARAMTYPE).
	 */
	asn1p_paramlist_t *params;

	/*
	 * The actual value (DefinedValue or inlined value).
	 */
	asn1p_value_t *value;

	/*
	 * The WITH SYHTAX clause.
	 */
	asn1p_wsyntx_t *with_syntax;

	/*
	 * A tag.
	 */
	struct asn1p_type_tag_s {
		enum {
			TC_NOCLASS,
			TC_UNIVERSAL,
			TC_APPLICATION,
			TC_CONTEXT_SPECIFIC,
			TC_PRIVATE,
		} tag_class;
		enum {
			TM_DEFAULT,
			TM_IMPLICIT,
			TM_EXPLICIT,
		} tag_mode;
		asn1_integer_t tag_value;
	} tag;

	/*
	 * Whether automatic tagging is applicable for subtypes.
	 */
	int auto_tags_OK;

	enum asn1p_expr_marker_e {
		EM_NOMARK,
		EM_OPTIONAL,
		EM_DEFAULT,	/* FIXME: store the value somewhere. */
	} marker;
	int unique;	/* UNIQUE */

	/*
	 * Members of the constructed type.
	 */
	TQ_HEAD(struct asn1p_expr_s)	members;

	/*
	 * Next expression in the list.
	 */
	TQ_ENTRY(struct asn1p_expr_s)	next;

	/*
	 * Line number where this structure is defined in the original
	 * grammar source.
	 */
	int _lineno;
	/*
	 * Marks are used for various purposes.
	 * Here are some predefined ones.
	 */
	enum {
		TM_NOMARK,
		TM_RECURSION,	/* Used to break recursion */
	} _mark;

	/*
	 * Opaque data may be attached to this structure,
	 * probably by compiler.
	 */
	void *data;
	void (*data_free)(void *data);
} asn1p_expr_t;


/*
 * Constructor and destructor.
 */
asn1p_expr_t *asn1p_expr_new(int _lineno);
asn1p_expr_t *asn1p_expr_clone(asn1p_expr_t *);
void asn1p_expr_free(asn1p_expr_t *expr);

#endif	/* ASN1_PARSER_EXPR_H */