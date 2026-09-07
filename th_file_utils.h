#ifndef TH_FILE_UTILS_H
#define TH_FILE_UTILS_H

/*
* Reads the whole file into a newly allocated, NUL-terminated buffer.
* Returns NULL on failure. The caller must free the result.
*/
char* th_load_file(const char* file_path);

#endif
