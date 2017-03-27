#ifdef RTE_EXEC_ENV_BSDAPP
	#define _WITH_GETLINE
#endif
#include <stdio.h>

#include <rte_malloc.h>

#include "cperf_options.h"
#include "cperf_test_vectors.h"
#include "cperf_test_vector_parsing.h"

int
free_test_vector(struct cperf_test_vector *vector, struct cperf_options *opts)
{
	if (vector == NULL || opts == NULL)
		return -1;

	rte_free(vector->iv.data);
	rte_free(vector->aad.data);
	rte_free(vector->digest.data);

	if (opts->test_file != NULL) {
		rte_free(vector->plaintext.data);
		rte_free(vector->cipher_key.data);
		rte_free(vector->auth_key.data);
		rte_free(vector->ciphertext.data);
	}

	rte_free(vector);

	return 0;
}

void
show_test_vector(struct cperf_test_vector *test_vector)
{
	const uint8_t wrap = 32;
	uint32_t i;

	if (test_vector == NULL)
		return;

	if (test_vector->plaintext.data) {
		printf("\nplaintext =\n");
		for (i = 0; i < test_vector->plaintext.length; ++i) {
			if ((i % wrap == 0) && (i != 0))
				printf("\n");
			if (i == test_vector->plaintext.length - 1)
				printf("0x%02x",
					test_vector->plaintext.data[i]);
			else
				printf("0x%02x, ",
					test_vector->plaintext.data[i]);
		}
		printf("\n");
	}

	if (test_vector->cipher_key.data) {
		printf("\ncipher_key =\n");
		for (i = 0; i < test_vector->cipher_key.length; ++i) {
			if ((i % wrap == 0) && (i != 0))
				printf("\n");
			if (i == (uint32_t)(test_vector->cipher_key.length - 1))
				printf("0x%02x",
					test_vector->cipher_key.data[i]);
			else
				printf("0x%02x, ",
					test_vector->cipher_key.data[i]);
		}
		printf("\n");
	}

	if (test_vector->auth_key.data) {
		printf("\nauth_key =\n");
		for (i = 0; i < test_vector->auth_key.length; ++i) {
			if ((i % wrap == 0) && (i != 0))
				printf("\n");
			if (i == (uint32_t)(test_vector->auth_key.length - 1))
				printf("0x%02x", test_vector->auth_key.data[i]);
			else
				printf("0x%02x, ",
					test_vector->auth_key.data[i]);
		}
		printf("\n");
	}

	if (test_vector->iv.data) {
		printf("\niv =\n");
		for (i = 0; i < test_vector->iv.length; ++i) {
			if ((i % wrap == 0) && (i != 0))
				printf("\n");
			if (i == (uint32_t)(test_vector->iv.length - 1))
				printf("0x%02x", test_vector->iv.data[i]);
			else
				printf("0x%02x, ", test_vector->iv.data[i]);
		}
		printf("\n");
	}

	if (test_vector->ciphertext.data) {
		printf("\nciphertext =\n");
		for (i = 0; i < test_vector->ciphertext.length; ++i) {
			if ((i % wrap == 0) && (i != 0))
				printf("\n");
			if (i == test_vector->ciphertext.length - 1)
				printf("0x%02x",
					test_vector->ciphertext.data[i]);
			else
				printf("0x%02x, ",
					test_vector->ciphertext.data[i]);
		}
		printf("\n");
	}

	if (test_vector->aad.data) {
		printf("\naad =\n");
		for (i = 0; i < test_vector->aad.length; ++i) {
			if ((i % wrap == 0) && (i != 0))
				printf("\n");
			if (i == (uint32_t)(test_vector->aad.length - 1))
				printf("0x%02x", test_vector->aad.data[i]);
			else
				printf("0x%02x, ", test_vector->aad.data[i]);
		}
		printf("\n");
	}

	if (test_vector->digest.data) {
		printf("\ndigest =\n");
		for (i = 0; i < test_vector->digest.length; ++i) {
			if ((i % wrap == 0) && (i != 0))
				printf("\n");
			if (i == (uint32_t)(test_vector->digest.length - 1))
				printf("0x%02x", test_vector->digest.data[i]);
			else
				printf("0x%02x, ", test_vector->digest.data[i]);
		}
		printf("\n");
	}
}

/* trim leading and trailing spaces */
static char *
trim_space(char *str)
{
	char *start, *end;

	for (start = str; *start; start++) {
		if (!isspace((unsigned char) start[0]))
			break;
	}

	for (end = start + strlen(start); end > start + 1; end--) {
		if (!isspace((unsigned char) end[-1]))
			break;
	}

	*end = 0;

	/* Shift from "start" to the beginning of the string */
	if (start > str)
		memmove(str, start, (end - start) + 1);

	return str;
}

/* tokenization test values separated by a comma */
static int
parse_values(char *tokens, uint8_t **data, uint32_t *data_length)
{
	uint32_t n_tokens;
	uint32_t data_size = 32;

	uint8_t *values, *values_resized;
	char *tok, *error = NULL;

	tok = strtok(tokens, CPERF_VALUE_DELIMITER);
	if (tok == NULL)
		return -1;

	values = (uint8_t *) rte_zmalloc(NULL, sizeof(uint8_t) * data_size, 0);
	if (values == NULL)
		return -1;

	n_tokens = 0;
	while (tok != NULL) {
		values_resized = NULL;

		if (n_tokens >= data_size) {
			data_size *= 2;

			values_resized = (uint8_t *) rte_realloc(values,
				sizeof(uint8_t) * data_size, 0);
			if (values_resized == NULL) {
				rte_free(values);
				return -1;
			}
			values = values_resized;
		}

		values[n_tokens] = (uint8_t) strtoul(tok, &error, 0);
		if ((error == NULL) || (*error != '\0')) {
			printf("Failed with convert '%s'\n", tok);
			rte_free(values);
			return -1;
		}

		tok = strtok(NULL, CPERF_VALUE_DELIMITER);
		if (tok == NULL)
			break;

		n_tokens++;
	}

	values_resized = (uint8_t *) rte_realloc(values,
		sizeof(uint8_t) * (n_tokens + 1), 0);

	if (values_resized == NULL) {
		rte_free(values);
		return -1;
	}

	*data = values_resized;
	*data_length = n_tokens + 1;

	return 0;
}

/* checks the type of key and assigns data */
static int
parse_entry(char *entry, struct cperf_test_vector *vector,
	struct cperf_options *opts, uint8_t tc_found)
{
	int status;
	uint32_t data_length;

	uint8_t *data = NULL;
	char *token, *key_token;

	if (entry == NULL) {
		printf("Expected entry value\n");
		return -1;
	}

	/* get key */
	token = strtok(entry, CPERF_ENTRY_DELIMITER);
	key_token = token;
	/* get values for key */
	token = strtok(NULL, CPERF_ENTRY_DELIMITER);

	if (key_token == NULL || token == NULL) {
		printf("Expected 'key = values' but was '%.40s'..\n", entry);
		return -1;
	}

	status = parse_values(token, &data, &data_length);
	if (status)
		return -1;

	/* compare keys */
	if (strstr(key_token, "plaintext")) {
		rte_free(vector->plaintext.data);
		vector->plaintext.data = data;
		if (tc_found)
			vector->plaintext.length = data_length;
		else {
			if (opts->max_buffer_size > data_length) {
				printf("Global plaintext shorter than "
					"buffer_sz\n");
				return -1;
			}
			vector->plaintext.length = opts->max_buffer_size;
		}

	} else if (strstr(key_token, "cipher_key")) {
		rte_free(vector->cipher_key.data);
		vector->cipher_key.data = data;
		if (tc_found)
			vector->cipher_key.length = data_length;
		else {
			if (opts->cipher_key_sz > data_length) {
				printf("Global cipher_key shorter than "
					"cipher_key_sz\n");
				return -1;
			}
			vector->cipher_key.length = opts->cipher_key_sz;
		}

	} else if (strstr(key_token, "auth_key")) {
		rte_free(vector->auth_key.data);
		vector->auth_key.data = data;
		if (tc_found)
			vector->auth_key.length = data_length;
		else {
			if (opts->auth_key_sz > data_length) {
				printf("Global auth_key shorter than "
					"auth_key_sz\n");
				return -1;
			}
			vector->auth_key.length = opts->auth_key_sz;
		}

	} else if (strstr(key_token, "iv")) {
		rte_free(vector->iv.data);
		vector->iv.data = data;
		vector->iv.phys_addr = rte_malloc_virt2phy(vector->iv.data);
		if (tc_found)
			vector->iv.length = data_length;
		else {
			if (opts->cipher_iv_sz > data_length) {
				printf("Global iv shorter than "
					"cipher_iv_sz\n");
				return -1;
			}
			vector->iv.length = opts->cipher_iv_sz;
		}

	} else if (strstr(key_token, "ciphertext")) {
		rte_free(vector->ciphertext.data);
		vector->ciphertext.data = data;
		if (tc_found)
			vector->ciphertext.length = data_length;
		else {
			if (opts->max_buffer_size > data_length) {
				printf("Global ciphertext shorter than "
					"buffer_sz\n");
				return -1;
			}
			vector->ciphertext.length = opts->max_buffer_size;
		}

	} else if (strstr(key_token, "aad")) {
		rte_free(vector->aad.data);
		vector->aad.data = data;
		vector->aad.phys_addr = rte_malloc_virt2phy(vector->aad.data);
		if (tc_found)
			vector->aad.length = data_length;
		else {
			if (opts->auth_aad_sz > data_length) {
				printf("Global aad shorter than "
					"auth_aad_sz\n");
				return -1;
			}
			vector->aad.length = opts->auth_aad_sz;
		}

	} else if (strstr(key_token, "digest")) {
		rte_free(vector->digest.data);
		vector->digest.data = data;
		vector->digest.phys_addr = rte_malloc_virt2phy(
			vector->digest.data);
		if (tc_found)
			vector->digest.length = data_length;
		else {
			if (opts->auth_digest_sz > data_length) {
				printf("Global digest shorter than "
					"auth_digest_sz\n");
				return -1;
			}
			vector->digest.length = opts->auth_digest_sz;
		}
	} else {
		printf("Not valid key: '%s'\n", trim_space(key_token));
		return -1;
	}

	return 0;
}

/* searches in the file for test keys and values */
static int
parse_file(struct cperf_test_vector *vector, struct cperf_options *opts)
{
	uint8_t tc_found = 0;
	uint8_t tc_data_start = 0;
	ssize_t read;
	size_t len = 0;
	int status = 0;

	FILE *fp;
	char *line = NULL;
	char *entry = NULL;

	fp = fopen(opts->test_file, "r");
	if (fp == NULL) {
		printf("File %s does not exists\n", opts->test_file);
		return -1;
	}

	while ((read = getline(&line, &len, fp)) != -1) {

		/* ignore comments and new lines */
		if (line[0] == '#' || line[0] == '/' || line[0] == '\n'
			|| line[0] == '\r' || line[0] == ' ')
			continue;

		trim_space(line);

		/* next test case is started */
		if (line[0] == '[' && line[strlen(line) - 1] == ']' && tc_found)
			break;
		/* test case section started, end of global data */
		else if (line[0] == '[' && line[strlen(line) - 1] == ']')
			tc_data_start = 1;

		/* test name unspecified, end after global data */
		if (tc_data_start && opts->test_name == NULL)
			break;
		/* searching for a suitable test */
		else if (tc_data_start && tc_found == 0) {
			if (!strcmp(line, opts->test_name)) {
				tc_found = 1;
				continue;
			} else
				continue;
		}

		/* buffer for multiline */
		entry = (char *) rte_realloc(entry,
					sizeof(char) * strlen(line) + 1, 0);
		if (entry == NULL)
			return -1;

		memset(entry, 0, strlen(line) + 1);
		strncpy(entry, line, strlen(line));

		/* check if entry ends with , or = */
		if (entry[strlen(entry) - 1] == ','
			|| entry[strlen(entry) - 1] == '=') {
			while ((read = getline(&line, &len, fp)) != -1) {
				trim_space(line);

				/* extend entry about length of new line */
				char *entry_extended = (char *) rte_realloc(
					entry, sizeof(char)
						* (strlen(line) + strlen(entry))
						+ 1, 0);

				if (entry_extended == NULL)
					goto err;
				entry = entry_extended;

				strncat(entry, line, strlen(line));

				if (entry[strlen(entry) - 1] != ',')
					break;
			}
		}
		status = parse_entry(entry, vector, opts, tc_found);
		if (status) {
			printf("An error occurred while parsing!\n");
			goto err;
		}
	}

	if (tc_found == 0 && opts->test_name != NULL) {
		printf("Not found '%s' case in test file\n", opts->test_name);
		goto err;
	}

	fclose(fp);
	free(line);
	rte_free(entry);

	return 0;

err:
	if (fp)
		fclose(fp);
	if (line)
		free(line);
	if (entry)
		rte_free(entry);

	return -1;
}

struct cperf_test_vector*
cperf_test_vector_get_from_file(struct cperf_options *opts)
{
	int status;
	struct cperf_test_vector *test_vector = NULL;

	if (opts == NULL || opts->test_file == NULL)
		return test_vector;

	test_vector = (struct cperf_test_vector *) rte_zmalloc(NULL,
		sizeof(struct cperf_test_vector), 0);
	if (test_vector == NULL)
		return test_vector;

	/* filling the vector with data from a file */
	status = parse_file(test_vector, opts);
	if (status) {
		free_test_vector(test_vector, opts);
		return NULL;
	}

	/* other values not included in the file */
	test_vector->data.cipher_offset = 0;
	test_vector->data.cipher_length = opts->max_buffer_size;

	test_vector->data.auth_offset = 0;
	test_vector->data.auth_length = opts->max_buffer_size;

	return test_vector;
}
