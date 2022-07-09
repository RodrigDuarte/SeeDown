// Creates a string using malloc and returns it.
char *create_string(char *string) {
    char *p = (char *) malloc(sizeof(char) * strlen(string) + 1);
    strcpy(p, string);
    return p;
}

// Creates a string with extra memory using malloc and returns it.
char *create_string_extra(char *string, int extra) {
    char *p = (char *) malloc(sizeof(char) * strlen(string) + extra + 1);
    strcpy(p, string);
    return p;
}

// Removes the first '\n' of a string.
void remove_newline(char *string) {
    char *pos = strchr(string, '\n');
    if (pos != NULL) { *pos = '\0'; }   
}

// Returns the string after removing the first '\n' of the string.
char *remove_newline_i(char *string) {
    char *pos = strchr(string, '\n');
    if (pos != NULL) { *pos = '\0'; }

    return string;
}

// Replaces every character with another character in the string.
void replace_char(char *string, char find, char replace) {
    for (int i = 0; i < (int) strlen(string); i++) {
        if (string[i] == find) {
            string[i] = replace;
        }
    }
}

// Returns the string lowercase.
char *lowercase(char *string) {
    for (int i = 0; i < (int) strlen(string); i++) {
        string[i] = tolower(string[i]);
    }

    return string;
}

// Splits a string into 2 different strings. Given a delimiter and the lentgh of both strings.
int split_string(char *source, char **dest_1, char **dest_2, int delimiter, int desired_length) {
    *dest_1 = (char *) malloc(sizeof(char) * desired_length + 1);
    *dest_2 = (char *) malloc(sizeof(char) * desired_length + 1);

    if (*dest_1 == NULL || *dest_2 == NULL) {
        return 0;
    } else {
        strncpy(*dest_1, source, desired_length);
        char *pos = strchr(*dest_1, delimiter);
        *pos = '\0';
        
        pos = strchr(source, delimiter);
        strncpy(*dest_2, pos + 1, desired_length);
        return 1;
    }
}

// Splits a string into an array of strings. Given a delimiter and a pointer to a size.
ARRAY tokenize(char *string, char *delimiter, int step) {
    ARRAY array = create_array((step <= 0) ? 1 : step);

    char *copy = create_string(string);
    char *token = strtok(copy, delimiter);

    while (token != NULL) {
        add_array(array, (void *) create_string(token));
        token = strtok(NULL, delimiter);
    }

    remove_newline((char *) array->data[array->size - 1]);

    return array;
}


// Centers a string from the left side.
char *center_string(char *string, int length) {
    char *result = (char *) malloc(sizeof(char) * length + 1);
    int string_length = strlen(string);
    int spaces = (length - string_length) / 2;
    int i;
    for (i = 0; i < spaces; i++) {
        result[i] = ' ';
    }
    strcpy(result + i, string);
    return result;
}

// Makes a folder path relative to the current folder of the program.
char *make_folder_path(char *directory, char *folder_name) {
    char *folder_path = (char *) malloc(strlen("./") + strlen(directory) + strlen(folder_name) + 2);
    strcpy(folder_path, "./");
    strcat(folder_path, directory);
    strcat(folder_path, folder_name);
    strcat(folder_path, "/");
    replace_char(folder_path, ' ', '_');
    return folder_path;
}

// Makes a file path relative to the current folder of the program.
char *make_file_path(char *directory, char *file_name) {
    char *file_path = (char *) malloc(strlen(directory) + strlen(file_name) + 1);
    strcpy(file_path, directory);
    strcat(file_path, file_name);
    return file_path;
}

// Opens a file and returns the file pointer.
FILE *open_file(char *fileName, char *mode) {
    FILE *b;

    b = fopen(fileName, mode);

    if (b == NULL) {
        fprintf(stderr, "There was a problem opening %s", fileName);
        exit(EXIT_FAILURE);
    }

    return b;
}

// Closes a file.
void close_file(FILE *b,char *fileName) {
    if (fclose(b) == EOF) {
        fprintf(stderr, "There was a problem closing %s", fileName);
        exit(EXIT_FAILURE);
    }
}