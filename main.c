#include "tiny_http.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <threads.h>
#include "th_file_utils.h"

// my test function for undertanding pointer
void change_number(int* ptr_number, int to_number){
	*ptr_number = to_number;
}


void print_sum(const char* str){
	printf("%s: ", str);
    int sum = 0;
    for(int i = 0; i < strlen(str); i++){
        sum += (int)str[i];
    }
    printf("%i\n", sum);
}

void handle_root(){
    printf("Root accessed!\n");
}

int main(int argc, const char** argv){
	//const char* get = "GET";
	//const char* post = "POST";
	//const char* put = "PUT";
	//const char* delete = "DELETE";
	//print_sum(get);
	//print_sum(post);
	//print_sum(put);
	//print_sum(delete);
	//return 0;
	system("clear"); 
	char* file_content = NULL;
	th_load_file("./file", file_content);
    th_create_server("0.0.0.0", 8080);
    th_add_route("GET", "/", handle_root);	
    th_add_route("GET", "/login", handle_root);	
    th_add_route("GET", "/meme", handle_root);	
	th_server_listen();
	exit(EXIT_SUCCESS);
}




