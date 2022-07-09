#include <curl/curl.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "./libs/list_utils.h"
#include "./libs/queue_utils.h"
#include "./libs/array_utils.h"
#include "./libs/utilitiesRD.h"


#define DEBUG_MODE 0
#define WRITE_MEMORY_CALLBACK   0
#define WRITE_MEMORY_FILE       1


struct MemoryStruct {
  char *memory;
  size_t size;
};

typedef struct chapter {
    float id;

    char *chapter_name;
    char *chapter_url;
} CHAPTER;

typedef struct manga {
    int id;
    char *manga_name;
    char *manga_url;

    ARRAY input_tokens;
    ARRAY best_matches;
    int number_matches;

    QUEUE chapters;
} MANGA;


void get_chapters(MANGA *manga);

size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *) userp;

    mem->memory = realloc(mem->memory, mem->size + realsize + 1);
    if (mem->memory == NULL) {
        /* out of memory! */
        printf("not enough memory (realloc returned NULL)\n");
        return 0;
    }

    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;

    return realsize;
}

size_t WriteMemoryFile(void *contents, size_t size, size_t nmemb, void *temp) {
    FILE *img = open_file((char *) temp, "ab");

    size_t realsize = fwrite(contents, size, nmemb, img);
    fclose(img);

    return realsize;
}

void *WriteMemory[2] = {WriteMemoryCallback, WriteMemoryFile};

void Curl(void *chunk, char *url, int mode) {
    CURL *curl_handle;
    CURLcode res;

    curl_handle = curl_easy_init();
    if(curl_handle) {
        curl_easy_setopt(curl_handle, CURLOPT_URL, url);
        curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteMemory[mode]);
        curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *) chunk);
        res = curl_easy_perform(curl_handle);
        if(res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        }
        curl_easy_cleanup(curl_handle);
    }
}

QUEUE download;
int manga_count = 1;

int main(int argc, char *argv[]) {
    curl_global_init(CURL_GLOBAL_ALL);

    struct MemoryStruct chunk;
    chunk.memory = malloc(1);   /* will be grown as needed by the realloc above */ 
    chunk.size = 0;             /* no data at this point */

    // Start MangaSee database crawler [https://mangasee123.com/]
    Curl(&chunk, "https://www.mangasee123.com/search/?name=", WRITE_MEMORY_CALLBACK);

    char *json_start = strstr(chunk.memory, "vm.Directory = ") + 15;
    char *json_end = strstr(json_start, "];");
    *(json_end + 1) = '\0';

    ARRAY manga_index = create_array(100);

    // Create the manga index array from the json string
    while ((json_start = strstr(json_start, "\"i\"")) != NULL) {
        char *name_start = json_start + 5;
        char *name_end = strstr(name_start, "\"");
        *name_end = '\0';

        add_array(manga_index, (void *) create_string(name_start));

        json_start = ++name_end;
    }

    free(chunk.memory);
    chunk.memory = NULL;
    chunk.size = 0;
    // End MangaSee database crawler

    download = create_queue();

    // Create the download queue
    //printf("Create the download queue\n");
    if (argc == 1) {
        char *input = create_string_extra("", 201);

        while (true) {
            printf("Enter manga name: ");
            fgets(input, 200, stdin);
            input = lowercase(input);

            if (strncmp(input, "!quit\n", 6) == 0) {
                free(input);
                exit(1);
            }
            else if (strncmp(input, "!do\n", 4) == 0) {
                free(input);
                break;
            }
            else if (input[0] == '\n') {
                continue;
            }
            else if (strncmp(input, "!help\n", 6) == 0) {
                printf("\n");
                printf("!quit - Quit the program\n");
                printf("!do   - Start the download\n");
                printf("!help - Show this help\n");
                printf("\n");
            }

            MANGA *manga = (MANGA *) malloc(sizeof(struct manga));
            manga->input_tokens = tokenize(input, " ", 1);
            manga->best_matches = create_array(3);
            manga->number_matches = 0;
            manga->id = manga_count++;

            if (enqueue(download, (void *) manga) == -1) {
                printf("Manga couldn't be added\n");
            }
        }
    }
    else {
        for (int i = 0; i < argc; i++) {
            MANGA *manga = (MANGA *) malloc(sizeof(struct manga));
            manga->input_tokens = tokenize(argv[i], " ", 1);
            manga->best_matches = create_array(3);
            manga->id = manga_count++;
            manga->number_matches = 0;
            enqueue(download, (void *) manga);
        }
    }

    // Search for the manga in the database
    //printf("Search for the manga in the database\n");
    for (int i = 0; i < manga_index->size; i++) {
        char *database = lowercase(create_string(manga_index->data[i]));
        ARRAY database_tokens = tokenize(database, "-", 1);

        QUEUE_NODE download_node = download->head;

        while (download_node != NULL) {
            MANGA *dl_manga = (MANGA *) download_node->data;
            int local_matches = 0;

            for (int j = 0; j < dl_manga->input_tokens->size; j++) {
                for (int k = 0; k < database_tokens->size; k++) {
                    if (strstr((char *) database_tokens->data[k],
                            (char *) dl_manga->input_tokens->data[j]) != NULL) {
                        local_matches++;
                        //printf("\t\t\t\t\t\t%s | %d\n", database, local_matches);
                        break;
                    }
                }
            }

            if (local_matches != 0 && local_matches > dl_manga->number_matches) {
                //printf("\t\t\t\t\t\tClearing and adding to best matches\n");
                dl_manga->best_matches = clear_array(dl_manga->best_matches);
                dl_manga->number_matches = local_matches;
                dl_manga->best_matches = add_array(dl_manga->best_matches,
                        (void *) create_string((char *) manga_index->data[i]));
            }
            else if (local_matches != 0 && local_matches == dl_manga->number_matches) {
                //printf("\t\t\t\t\t\tAdding to best matches\n");
                dl_manga->best_matches = add_array(dl_manga->best_matches,
                        (void *) create_string((char *) manga_index->data[i]));
            }
            
            download_node = download_node->next;
        }
        
        free(database);
        destroy_array(database_tokens);
    }
    
    // Confirm the manga to be downloaded
    //printf("Confirm the manga to be downloaded\n");
    QUEUE_NODE download_node = download->head;
    while (download_node != NULL) {
        MANGA *dl_manga = (MANGA *) download_node->data;
        ARRAY best_matches = dl_manga->best_matches;

        if (best_matches->size == 0) {
            printf("No matches found for \"");
            for (int i = 0; i < dl_manga->input_tokens->size; i++) {
                if (i != dl_manga->input_tokens->size - 1) {
                    printf("%s ", (char *) dl_manga->input_tokens->data[i]);
                }
                else {
                    printf("%s\"\n", (char *) dl_manga->input_tokens->data[i]);
                }
            }
            
            remove_list((LIST) download, (void *) dl_manga);
        }
        else if (best_matches->size == 1) {
            printf("%d: %s\n", dl_manga->id, (char *) best_matches->data[0]);

            // https://mangasee123.com/manga/<manga_name>
            int manga_url_len = 30 + strlen((char *) best_matches->data[0]) + 1;
            dl_manga->manga_url = create_string_extra("", manga_url_len);
            snprintf(dl_manga->manga_url, manga_url_len, "https://mangasee123.com/manga/%s",
                    (char *) best_matches->data[0]);

            dl_manga->manga_name = create_string((char *) best_matches->data[0]);
            replace_char(dl_manga->manga_name, '-', ' ');

            get_chapters(dl_manga);
        }
        else {
            int choice = -1;

            do {
                for (int i = 0; i < best_matches->size; i++) {
                    printf("%d: %d: %s\n", dl_manga->id, i + 1, (char *) best_matches->data[i]);
                }
                
                printf("Enter the number of the manga you want to download: ");
                scanf("%d", &choice);
            } while (choice <= 0 || choice >= best_matches->size);        

            // https://mangasee123.com/manga/<manga_name>
            int manga_url_len = 30 + strlen((char *) best_matches->data[choice - 1]) + 1;
            dl_manga->manga_url = create_string_extra("", manga_url_len);
            snprintf(dl_manga->manga_url, manga_url_len, "https://mangasee123.com/manga/%s",
                    (char *) best_matches->data[choice - 1]);

            dl_manga->manga_name = create_string((char *) best_matches->data[choice - 1]);
            replace_char(dl_manga->manga_name, '-', ' ');

            get_chapters(dl_manga);
        }

        download_node = download_node->next;
    }
    

    // Download the manga
    LIST_NODE download_list_node = (LIST_NODE) download->head;
    while (download_list_node != NULL) {
        printf("%s\n", ((MANGA *) download_list_node->data)->manga_name);
        download_list_node = download_list_node->next;
    }
    
    destroy_list((LIST) download);
    
    free(manga_index);

    return 0;
}

void get_chapters(MANGA *manga) {
    char *manga_url = manga->manga_url;
    char *manga_name = manga->manga_name;
    manga->chapters = create_queue();

    // Create manga path ./manga/<manga_name>/
    int manga_dir_len = strlen(manga_name) + strlen("./manga/") + 1;
    char *manga_dir = (char *) malloc(manga_dir_len * sizeof(char));
    snprintf(manga_dir, manga_dir_len, "./manga/%s/", manga_name);
    
    // Create the folder to store the manga if it doesn't exist
    int ret = mkdir(manga_dir, 0777);
    if (ret == -1 && errno != EEXIST) {
        fprintf(stderr, "Couldn't create directory or already exists %s\n", manga_dir);
    }

    struct MemoryStruct chunk;
    chunk.memory = malloc(1);   /* will be grown as needed by the realloc above */ 
    chunk.size = 0;             /* no data at this point */

    Curl(&chunk, manga_url, WRITE_MEMORY_CALLBACK);

    char *xml = chunk.memory;
    char *xml_start = strstr(xml, "title=\"RSS Feed\"") + 23;
    char *xml_end = strstr(xml_start, "\"");
    *xml_end = '\0';
    char *xml_url = create_string(xml_start);

    free(chunk.memory);
    chunk.memory = NULL;
    chunk.size = 0;

    Curl(&chunk, xml_url, WRITE_MEMORY_CALLBACK);

    free(xml_url);

    xml = chunk.memory;
    xml_start = strstr(xml, "<item>") + 6;

    char *position = xml_start;
    while ((position = strstr(position, "<title>")) != NULL) {
        CHAPTER *chapter = (CHAPTER *)malloc(sizeof(CHAPTER));

        xml_start = position + 7;
        xml_end = strstr(xml_start, "</");
        *xml_end = '\0';

        chapter->chapter_name = create_string(xml_start);

        xml_start = strstr(xml_end + 1, "<link>") + 6;
        xml_end = strstr(xml_start, "</");
        *xml_end = '\0';

        chapter->chapter_url = create_string(xml_start);

        xml_start = strstr(strstr(xml_end + 1, "<guid"), ">") + 1;
        xml_end = strstr(xml_start, "</");
        *xml_end = '\0';

        sscanf(xml_start + strlen(manga->manga_name) + 1, "%f", &(chapter->id));

        enqueue(manga->chapters, (void *) chapter);

        position = xml_end + 1;
    }
    
    free(xml);
    free(manga_dir);

    /* Debugging purposes
    LIST_NODE chapter_list_node = (LIST_NODE) manga->chapters->head;
    while (chapter_list_node != NULL) {
        CHAPTER *chapter = (CHAPTER *) chapter_list_node->data;
        printf("%f\n", chapter->id);
        chapter_list_node = chapter_list_node->next;
    }
    */

    return;
}

