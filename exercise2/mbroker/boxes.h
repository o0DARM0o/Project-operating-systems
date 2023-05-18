bool does_box_exist(char *box_name);

int create_box(char *box_name);

int write_message(char *box_name, char *message);

int remove_box(char *box_name);

int read_box(char *dest, char * box_name);