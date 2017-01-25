#include <stdio.h>

#include <rte_malloc.h>
#include "cperf_options.h"
#include "cperf_test_vectors.h"
#include "cperf_verify_parser.h"

int
free_test_vector(struct cperf_test_vector *vector, struct cperf_options *opts)
{
	if (vector == NULL || opts == NULL)
		return -1;

	if (opts->test_file == NULL) {
		if (vector->iv.data)
			rte_free(vector->iv.data);
		if (vector->aad.data)
			rte_free(vector->aad.data);
		if (vector->digest.data)
			rte_free(vector->digest.data);
		rte_free(vector);

	} else {
		if (vector->plaintext.data)
			rte_free(vector->plaintext.data);
		if (vector->cipher_key.data)
			rte_free(vector->cipher_key.data);
		if (vector->auth_key.data)
			rte_free(vector->auth_key.data);
		if (vector->iv.data)
			rte_free(vector->iv.data);
		if (vector->ciphertext.data)
			rte_free(vector->ciphertext.data);
		if (vector->aad.data)
			rte_free(vector->aad.data);
		if (vector->digest.data)
			rte_free(vector->digest.data);
		rte_free(vector);
	}

	return 0;
}

/* trim leading and trailing spaces */
static char *
trim(char *str)
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
	uint8_t n_tokens;
	uint32_t data_size = 32;
	uint8_t *values;
	char *tok, *error = NULL;

	tok = strtok(tokens, VALUE_DELIMITER);
	if (tok == NULL)
		return -1;

	values = (uint8_t *) rte_zmalloc(NULL, sizeof(uint8_t) * data_size, 0);
	if (values == NULL)
		return -1;

	n_tokens = 0;
	while (tok != NULL) {
		uint8_t *values_extended = NULL;

		if (n_tokens >= data_size) {

			data_size *= 2;

			values_extended = (uint8_t *) rte_realloc(values,
				sizeof(uint8_t) * data_size, 0);
			if (values_extended == NULL) {
				rte_free(values);
				return -1;
			}

			values = values_extended;
		}

		values[n_tokens] = (uint8_t) strtoul(tok, &error, 0);
		if ((error == NULL) || (*error != '\0')) {
			printf("Failed with convert '%s'\n", tok);
			rte_free(values);
			return -1;
		}

		tok = strtok(NULL, VALUE_DELIMITER);
		if (tok == NULL)
			break;

		n_tokens++;
	}

	uint8_t *resize_values = (uint8_t *) rte_realloc(values,
		sizeof(uint8_t) * (n_tokens + 1), 0);

	if (resize_values == NULL) {
		rte_free(values);
		return -1;
	}

	*data = resize_values;
	*data_length = n_tokens + 1;

	return 0;
}

/* checks the type of key and assigns data */
static int
parse_entry(char *entry, struct cperf_test_vector *vector)
{
	char *token, *key_token;
	uint8_t *data = NULL;
	int status;
	uint32_t data_length;

	/* get key */
	token = strtok(entry, ENTRY_DELIMITER);
	key_token = token;

	/* get values for key */
	token = strtok(NULL, ENTRY_DELIMITER);

	if (token == NULL) {
		printf("Expected 'key = values' but was '%.40s'..\n",
			key_token);
		return -1;
	}

	status = parse_values(token, &data, &data_length);
	if (status)
		return -1;

	/* compare keys */
	if (strstr(key_token, "plaintext")) {
		if (vector->plaintext.data)
			rte_free(vector->plaintext.data);
		vector->plaintext.data = data;
		vector->plaintext.length = data_length;
	} else if (strstr(key_token, "cipher_key")) {
		if (vector->cipher_key.data)
			rte_free(vector->cipher_key.data);
		vector->cipher_key.data = data;
		vector->cipher_key.length = data_length;
	} else if (strstr(key_token, "auth_key")) {
		if (vector->auth_key.data)
			rte_free(vector->auth_key.data);
		vector->auth_key.data = data;
		vector->auth_key.length = data_length;
	} else if (strstr(key_token, "iv")) {
		if (vector->iv.data)
			rte_free(vector->iv.data);
		vector->iv.data = data;
		vector->iv.phys_addr = rte_malloc_virt2phy(vector->iv.data);
		vector->iv.length = data_length;
	} else if (strstr(key_token, "ciphertext")) {
		if (vector->ciphertext.data)
			rte_free(vector->ciphertext.data);
		vector->ciphertext.data = data;
		vector->ciphertext.length = data_length;
	} else if (strstr(key_token, "aad")) {
		if (vector->aad.data)
			rte_free(vector->aad.data);
		vector->aad.data = data;
		vector->aad.phys_addr = rte_malloc_virt2phy(vector->aad.data);
		vector->aad.length = data_length;
	} else if (strstr(key_token, "digest")) {
		if (vector->digest.data)
			rte_free(vector->digest.data);
		vector->digest.data = data;
		vector->digest.phys_addr = rte_malloc_virt2phy(
			vector->digest.data);
		vector->digest.length = data_length;
	} else {
		printf("Not valid key: '%s'\n", trim(key_token));
		return -1;
	}

	return 0;
}

/* searches in the file for registry keys and values */
static int
parse_file(struct cperf_test_vector *v_vec, const char *path)
{
	FILE *fp;
	char *line = NULL, *entry = NULL;
	ssize_t read;
	size_t len = 0;
	int status = 0;

	fp = fopen(path, "r");
	if (fp == NULL) {
		printf("File %s does not exists\n", path);
		return -1;
	}

	while ((read = getline(&line, &len, fp)) != -1) {
		/* ignore comments and new lines */
		if (line[0] == '#' || line[0] == '/' || line[0] == '\n'
			|| line[0] == '\r' || line[0] == ' ')
			continue;

		trim(line);

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
				trim(line);

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
		status = parse_entry(entry, v_vec);
		if (status) {
			printf("An error occurred while parsing!\n");
			goto err;
		}
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
	status = parse_file(test_vector, opts->test_file);
	if (status) {
		free_test_vector(test_vector, opts);
		return NULL;
	}

	/* other values not included in the file */
	test_vector->data.cipher_offset = 0;
	test_vector->data.cipher_length = opts->buffer_sz;

	test_vector->data.auth_offset = 0;
	test_vector->data.auth_length = opts->buffer_sz;

	return test_vector;
}
