#undef	NDEBUG
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <sysexits.h>

#include "asn1fix.h"
#include "asn1fix_internal.h"

static int check(const char *fname,
	enum asn1p_flags parser_flags,
	enum asn1f_flags fixer_flags);
static int post_fix_check(asn1p_t *asn);
static int post_fix_check_element(asn1p_module_t *mod, asn1p_expr_t *expr);

int
main(int ac, char **av) {
	struct dirent *dp;
	DIR *dir;
	int failed = 0;
	int completed = 0;
	enum asn1p_flags parser_flags = A1P_NOFLAGS;
	enum asn1f_flags fixer_flags  = A1F_NOFLAGS;
	int ret;

	/*
	 * Just in case when one decides that some flags better be
	 * enabled during `ASN1_FIXER_FLAGS=1 make check` or some
	 * similar usage.
	 */
	if(getenv("ASN1_PARSER_FLAGS"))
		parser_flags = atoi(getenv("ASN1_PARSER_FLAGS"));
	if(getenv("ASN1_FIXER_FLAGS"))
		fixer_flags = atoi(getenv("ASN1_FIXER_FLAGS"));

	/*
	 * Go into a directory with tests.
	 */
	if(ac <= 1) {
		fprintf(stderr, "Testing in ./tests...\n");
		ret = chdir("../tests");
		assert(ret == 0);
		dir = opendir(".");
		assert(dir);
	} else {
		dir = 0;
	}

	/*
	 * Scan every *.asn1 file and try to parse and fix it.
	 */
	if(dir) {
		while((dp = readdir(dir))) {
			int len = strlen(dp->d_name);
			if(len && strcmp(dp->d_name + len - 5, ".asn1") == 0) {
				ret = check(dp->d_name,
					parser_flags, fixer_flags);
				if(ret) {
					fprintf(stderr,
						"FAILED: %s\n",
						dp->d_name);
					failed++;
				}
				completed++;
			}
		}
		closedir(dir);

		fprintf(stderr,
			"Tests COMPLETED: %d\n"
			"Tests FAILED:    %d\n"
			,
			completed, failed
		);
	} else {
		int i;
		for(i = 1; i < ac; i++) {
			ret = check(av[i], parser_flags, fixer_flags);
			if(ret) {
				fprintf(stderr, "FAILED: %s\n", av[i]);
				failed++;
			}
			completed++;
		}
	}

	if(completed == 0) {
		fprintf(stderr, "No tests defined?!\n");
		exit(EX_NOINPUT);
	}

	if(failed)
		exit(EX_DATAERR);
	return 0;
}

static int
check(const char *fname,
		enum asn1p_flags parser_flags,
		enum asn1f_flags fixer_flags) {
	asn1p_t *asn;
	int expected_parseable;		/* Is it expected to be parseable? */
	int expected_fix_code;		/* What code a fixer must return */
	int r_value = 0;

	/*
	 * Figure out how the processing should go by inferring
	 * expectations from the file name.
	 */
	if(strstr(fname, "-OK.")) {
		expected_parseable = 1;
		expected_fix_code  = 0;
	} else if(strstr(fname, "-NP.")) {
		expected_parseable = 0;
		expected_fix_code  = 123;	/* Does not matter */
	} else if(strstr(fname, "-SE.")) {
		expected_parseable = 1;
		expected_fix_code  = -1;	/* Semantically incorrect */
	} else if(strstr(fname, "-SW.")) {
		expected_parseable = 1;
		expected_fix_code  = 1;	/* Semantically suspicious */
	} else {
		fprintf(stderr, "%s: Invalid file name format\n", fname);
		return -1;
	}

	fprintf(stderr, "[=> %s]\n", fname);

	/*
	 * Perform low-level parsing.
	 */
	if(!expected_parseable)
		fprintf(stderr, "Expecting error...\n");
	asn = asn1p_parse_file(fname, parser_flags);
	if(asn == NULL) {
		if(expected_parseable) {
			fprintf(stderr, "Cannot parse file \"%s\"\n", fname);
			r_value = -1;
		} else {
			fprintf(stderr,
				"Previous error is EXPECTED, no worry\n");
		}
	} else if(!expected_parseable) {
		fprintf(stderr,
			"The file \"%s\" is not expected to be parseable, "
			"yet parsing was successfull!\n", fname);
		r_value = -1;
	}

	/*
	 * Perform semantical checks and fixes.
	 */
	if(asn && r_value == 0) {
		int ret;

		if(expected_fix_code)
			fprintf(stderr, "Expecting some problems...\n");

		ret = asn1f_process(asn, fixer_flags, 0);
		if(ret) {
			if(ret == expected_fix_code) {
				fprintf(stderr,
					"Previous error is EXPECTED, "
					"no worry\n");
			} else {
				fprintf(stderr,
					"Cannot process file \"%s\": %d\n",
					fname, ret);
				r_value = -1;
		}
		} else if(ret != expected_fix_code) {
			fprintf(stderr,
				"File \"%s\" is expected "
				"to be semantically incorrect, "
				"yet processing was successful!\n",
				fname);
			r_value = -1;
		}
	}

	/*
	 * Check validity of some values, if grammar has special
	 * instructions for that.
	 */
	if(asn && r_value == 0) {
		if(post_fix_check(asn))
			r_value = -1;
	}

	/*
	 * TODO: destroy the asn.
	 */

	return r_value;
}


static int
post_fix_check(asn1p_t *asn) {
	asn1p_module_t *mod;
	asn1p_expr_t *expr;
	int r_value = 0;

	TQ_FOR(mod, &(asn->modules), mod_next) {
		TQ_FOR(expr, &(mod->members), next) {
			assert(expr->Identifier);
			if(strncmp(expr->Identifier, "check-", 6) == 0) {
				if(post_fix_check_element(mod, expr))
					r_value = -1;
			}
		}
	}

	return r_value;
}


static int
post_fix_check_element(asn1p_module_t *mod, asn1p_expr_t *check_expr) {
	asn1p_expr_t *expr = NULL;
	char *name;
	asn1p_value_t *value;

	if(check_expr->expr_type != ASN_BASIC_INTEGER
	|| check_expr->meta_type != AMT_VALUE) {
		fprintf(stderr,
			"CHECKER: Unsupported type of \"%s\" value: "
			"%d at line %d of %s\n",
			check_expr->Identifier,
			check_expr->expr_type,
			check_expr->_lineno,
			mod->source_file_name
		);
		return -1;
	}

	assert(check_expr->meta_type == AMT_VALUE);

	value = check_expr->value;
	if(value == NULL || value->type != ATV_INTEGER) {
		fprintf(stderr,
			"CHECKER: Unsupported value type of \"%s\": "
			"%d at line %d of %s\n",
			check_expr->Identifier,
			value?value->type:-1,
			expr->_lineno,
			mod->source_file_name
		);
		return -1;
	}

	name = check_expr->Identifier + sizeof("check-") - 1;

	/*
	 * Scan in search for the original.
	 */
	TQ_FOR(expr, &(mod->members), next) {
		if(strcmp(expr->Identifier, name) == 0)
			break;
	}

	if(expr == NULL) {
		fprintf(stderr,
			"CHECKER: Value \"%s\" requested by "
			"\"check-%s\" at line %d of %s is not found!\n",
			name, name, check_expr->_lineno,
			mod->source_file_name
		);
		return -1;
	}

	if(0 && expr->expr_type != check_expr->expr_type) {
		fprintf(stderr,
			"CHECKER: Value type of \"%s\" (=%d) at line %d "
			"does not have desired type %d as requested by "
			"\"check-%s\" in %s\n",
			expr->Identifier,
			expr->expr_type,
			expr->_lineno,
			check_expr->expr_type,
			name,
			mod->source_file_name
		);
		return -1;
	}

	if(expr->value == NULL
	|| expr->value->type != value->type) {
		fprintf(stderr,
			"CHECKER: Value of \"%s\" (\"%s\", type=%d) at line %d "
			"does not have desired type %d as requested by "
			"\"check-%s\" in %s\n",
			expr->Identifier,
			asn1f_printable_value(expr->value),
			expr->value->type,
			expr->_lineno,
			value->type,
			name,
			mod->source_file_name
		);
		return -1;
	}

	assert(value->type = ATV_INTEGER);

	return 0;
}

