#include "th_file_utils.h"
#include <stdio.h>
#include <stdlib.h>

char* th_load_file(const char* file_path){
	printf("Loading file...\n");

	FILE* file = fopen(file_path, "rb");
	if(file == NULL){
		perror("th_load_file: cannot open file");
		return NULL;
	}

	if(fseek(file, 0, SEEK_END) != 0){
		perror("th_load_file: fseek");
		fclose(file);
		return NULL;
	}
	long size = ftell(file);
	if(size < 0){
		perror("th_load_file: ftell");
		fclose(file);
		return NULL;
	}
	rewind(file);

	char* file_content = malloc((size_t)size + 1);
	if(file_content == NULL){
		perror("th_load_file: malloc");
		fclose(file);
		return NULL;
	}

	size_t read = fread(file_content, 1, (size_t)size, file);
	file_content[read] = '\0';
	fclose(file);

	return file_content;
}
