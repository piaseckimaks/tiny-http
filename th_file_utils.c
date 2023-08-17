#include "th_file_utils.h"
#include <stdio.h>
#include <stdlib.h>

char* th_load_file(const char* file_path, char* file_content){
	printf("Loading file...\n");
	if (file_content != NULL) {
	    perror("th_load_file: Pointer for file content should be NULL pointer");
	    return NULL;
	}

	FILE* file = fopen(file_path, "r");
	
	if(file == NULL){
		perror("th_load_file: file doesn't exists");
		return NULL;
	}

	char* buffer = malloc(sizeof(char));
	int char_count = 1;
	while(fread(buffer, sizeof(char), 1, file)){
        file_content = realloc(file_content, char_count * sizeof(char));
		file_content[char_count] = *buffer;
	}

	return file_content; 
}
