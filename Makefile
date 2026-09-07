tiny-http:
	gcc -Wall -Wextra -pthread main.c queue.c header_list.c console_utils.c tiny_http.c th_file_utils.c th_threading.c th_handle_connection.c
