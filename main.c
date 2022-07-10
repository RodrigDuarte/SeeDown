#include <curl/curl.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
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
#define DEDICATED_MANGA 2
#define DEDICATED_CHAPTER 2
#define THREADS (DEDICATED_MANGA * DEDICATED_CHAPTER)

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

typedef struct thread_space {
    MANGA *manga;

    pthread_mutex_t *mutex_manga;
} SPACE_T;

void get_chapters(MANGA *manga);
void download_chapter(MANGA *manga, CHAPTER *chapter);
void *producer(void *args);
void *organize(void *args);
void *consumer(void *args);

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

ARRAY manga_index;
QUEUE download;
int manga_count = 1;

pthread_mutex_t mutex_organizer;
pthread_cond_t cond_organizer;

pthread_mutex_t mutex_consumer;
pthread_cond_t cond_consumer;

bool pause_organize = true;
bool pause_consumer = true;

SPACE_T *spaces[THREADS];

int main(int argc, char *argv[]) {
    argv = argv;
    curl_global_init(CURL_GLOBAL_ALL);

    struct MemoryStruct chunk;
    chunk.memory = malloc(1);   /* will be grown as needed by the realloc above */ 
    chunk.size = 0;             /* no data at this point */

    // Start MangaSee database crawler [https://mangasee123.com/]
    Curl(&chunk, "https://www.mangasee123.com/search/?name=", WRITE_MEMORY_CALLBACK);

    char *json_start = strstr(chunk.memory, "vm.Directory = ") + 15;
    char *json_end = strstr(json_start, "];");
    *(json_end + 1) = '\0';

    manga_index = create_array(100);

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



    if (argc == 1) {
        pthread_mutex_init(&mutex_organizer, NULL);
        pthread_cond_init(&cond_organizer, NULL);

        pthread_mutex_init(&mutex_consumer, NULL);
        pthread_cond_init(&cond_consumer, NULL);

        download = create_queue();

        pthread_t producer_thread;
        pthread_t organize_thread;
        pthread_t consumer_thread[THREADS];

        int thread_ids[THREADS];
        for (int i = 0; i < THREADS; i++) {
            thread_ids[i] = i;
        }

        if (pthread_create(&producer_thread, NULL, producer, NULL) != 0) {
            printf("Error creating producer thread\n");
            exit(1);
        }
        if (pthread_create(&organize_thread, NULL, organize, NULL) != 0) {
            printf("Error creating organizer thread\n");
            exit(1);
        }
        for (int i = 0; i < THREADS; i++) {
            if (pthread_create(&consumer_thread[i], NULL, consumer, (void *) &thread_ids[i]) != 0) {
                printf("Error creating consumer thread\n");
                exit(1);
            }
        }
        
        pthread_join(producer_thread, NULL);
        pthread_join(organize_thread, NULL);
        for (int i = 0; i < THREADS; i++) {
            pthread_join(consumer_thread[i], NULL);
        }
    }
    
    free(manga_index);

    destroy_list((LIST) download);

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

void download_chapter(MANGA *manga, CHAPTER *chapter) {
    char *chapter_url = chapter->chapter_url;
    char *chapter_name = chapter->chapter_name;
    float id = chapter->id;

    char id_string[11];
    sprintf(id_string, "%0*.5f", 10, id);

    // We hide this code cause it is garbage
    for (int i = 5; i < 10; i++) {
        if (id_string[i] == '0') {
            if (i == 5) {
                id_string[i - 1] = '\0';
                break;
            }

            id_string[i] = '\0';
            break;
        }
    }

    // Create path for chapter -> ./manga/manga_name/chapter_number/
    int dir_name_length = 8 + (int) strlen(manga->manga_name) + 1 + (int) sizeof(id_string);
    char *dir_name = (char *) malloc(dir_name_length);
    snprintf(dir_name, dir_name_length, "./manga/%s/%s", manga->manga_name, id_string);

    int result = mkdir(dir_name, 0777);
    if (result == -1) printf("Error creating directory or already existent: %s\n", dir_name);

    struct MemoryStruct chunk;

    chunk.memory = malloc(1);   /* will be grown as needed by the realloc above */
    chunk.size = 0;             /* no data at this point */

    Curl(&chunk, chapter_url, WRITE_MEMORY_CALLBACK);

    // Get the number of pages in this chapter [vm.CurChapter = {..."Page":"<%d>"]
    char *page_number = strstr(chunk.memory, "vm.CurChapter = ") + 16;
    page_number = strstr(page_number, "Page\":\"") + 7;
    int page_total = atoi(page_number);

    // Get the chapter domain host [vm.CurPathName = "<%s>"]
    char *domain_host = strstr(chunk.memory, "vm.CurPathName = ") + 18;
    char *domain_end = strstr(domain_host, "\";");
    *domain_end = '\0';

    // Create the url -> https://<domain_host>/manga/<manga_link>/0000.0-
    int url_length = 9 + (int) strlen(domain_host) + 7 + (int) strlen(manga->manga_url) + 25 + 1;
    char *domain_url = (char *) malloc(url_length);
    snprintf(domain_url, url_length, "https://%s/manga/%s/%s-", domain_host, manga->manga_url, id_string);

    free(chunk.memory);
    chunk.memory = NULL;
    chunk.size = 0;

    for (int page_count = 1; page_count <= page_total; page_count++) {
        // Create the url -> https://<domain_host>/manga/<manga_link>/0000.0-000.png
        int page_url_length = (int) strlen(domain_url) + 3 + 5;
        char *page_url = (char *) malloc(page_url_length);
        snprintf(page_url, page_url_length, "%s%03d.png", domain_url, page_count);
        //printf("%s\n", page_url);

        // Create the path -> ./manga/<dir_name>/<chapter_number> - <page_number>.png
        int img_filename_length = strlen(dir_name) + 1 + strlen(chapter_name) + 1 + 3 + 4;
        char *img_filename = (char *) malloc(img_filename_length);
        snprintf(img_filename, img_filename_length, "%s/%s-%03d.png", dir_name, id_string, page_count);
        
        Curl((void *) img_filename, page_url, WRITE_MEMORY_FILE);

        free(page_url);
        free(img_filename);
    }

    free(domain_url);
    free(dir_name);
}


/* Thread section */
void *producer(void *args) {
    sleep(1);
    args = args;

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
            pthread_mutex_lock(&mutex_organizer);
            pause_organize = false;
            pthread_cond_broadcast(&cond_organizer);
            pthread_mutex_unlock(&mutex_organizer);
            continue;
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

        // Get the best match for the manga name
        for (int i = 0; i < manga_index->size; i++) {
            char *database_single = lowercase(create_string(manga_index->data[i]));
            ARRAY database_tokens = tokenize(database_single, "-", 1);


            int local_matches = 0;

            for (int j = 0; j < manga->input_tokens->size; j++) {
                for (int k = 0; k < database_tokens->size; k++) {
                    if (strstr((char *) database_tokens->data[k],
                            (char *) manga->input_tokens->data[j]) != NULL) {
                        local_matches++;
                        //printf("\t\t\t\t\t\t%s | %d\n", database, local_matches);
                        break;
                    }
                }
            }

            if (local_matches != 0 && local_matches > manga->number_matches) {
                //printf("\t\t\t\t\t\tClearing and adding to best matches\n");
                manga->best_matches = clear_array(manga->best_matches);
                manga->number_matches = local_matches;
                manga->best_matches = add_array(manga->best_matches,
                        (void *) create_string((char *) manga_index->data[i]));
            }
            else if (local_matches != 0 && local_matches == manga->number_matches) {
                //printf("\t\t\t\t\t\tAdding to best matches\n");
                manga->best_matches = add_array(manga->best_matches,
                        (void *) create_string((char *) manga_index->data[i]));
            }
            
            free(database_single);
            destroy_array(database_tokens);
        }
        // End of best match search

        if (manga->best_matches->size == 0) {
            printf("No matches found for \"");
            for (int i = 0; i < manga->input_tokens->size; i++) {
                if (i != manga->input_tokens->size - 1) {
                    printf("%s ", (char *) manga->input_tokens->data[i]);
                }
                else {
                    printf("%s\"\n", (char *) manga->input_tokens->data[i]);
                }
            }

            destroy_array(manga->input_tokens);
            destroy_array(manga->best_matches);
            free(manga);
            printf("\t\t\t\t\t\t\t\t   Producer : input invalid\n");
            continue;
        }
        else if (manga->best_matches->size == 1) {
            printf("%d: %s\n", manga->id, (char *) manga->best_matches->data[0]);

            // https://mangasee123.com/manga/<manga_name>
            int manga_url_len = 30 + strlen((char *) manga->best_matches->data[0]) + 1;
            manga->manga_url = create_string_extra("", manga_url_len);
            snprintf(manga->manga_url, manga_url_len, "https://mangasee123.com/manga/%s",
                    (char *) manga->best_matches->data[0]);

            manga->manga_name = create_string((char *) manga->best_matches->data[0]);
            replace_char(manga->manga_name, '-', ' ');
        }
        else {
            int choice = -1;

            do {
                for (int i = 0; i < manga->best_matches->size; i++) {
                    printf("%d: %d: %s\n", manga->id, i + 1, (char *) manga->best_matches->data[i]);
                }
                
                printf("Enter the number of the manga you want to download: ");
                scanf("%d", &choice);
            } while (choice <= 0 || choice >= manga->best_matches->size);        

            // https://mangasee123.com/manga/<manga_name>
            int manga_url_len = 30 + strlen((char *) manga->best_matches->data[choice - 1]) + 1;
            manga->manga_url = create_string_extra("", manga_url_len);
            snprintf(manga->manga_url, manga_url_len, "https://mangasee123.com/manga/%s",
                    (char *) manga->best_matches->data[choice - 1]);

            manga->manga_name = create_string((char *) manga->best_matches->data[choice - 1]);
            replace_char(manga->manga_name, '-', ' ');
        }

        get_chapters(manga);
        if (enqueue(download, (void *) manga) == -1) {
            printf("Manga couldn't be added\n");
        }
    }

    return NULL;
}

void *organize(void *args) {
    args = args;

    while (true) {
        pthread_mutex_lock(&mutex_organizer);
        while (pause_organize) {
            printf("Organizer : waiting for a book to be produced or finished\n");
            pthread_cond_wait(&cond_organizer, &mutex_organizer);
            pause_consumer = true;
        }

        // My code

        if (download->size == 0) {
            printf("Organizer : no books to download\n");
            pause_organize = true;
            pthread_mutex_unlock(&mutex_organizer);
            continue;
        }

        /* Test to see chapter numbers
        LIST_NODE node = (LIST_NODE) download->head;
        while (node != NULL) {

            MANGA *manga = (MANGA *) node->data;
            LIST_NODE chapter = (LIST_NODE) manga->chapters->head;
            printf("Organizer : %s [", manga->manga_name);
            while (chapter != NULL) {
                if (chapter->next == NULL) {
                    printf("%.2f]\n", ((CHAPTER *) chapter->data)->id);
                }
                else {
                    printf("%.2f, ", ((CHAPTER *) chapter->data)->id);
                }
                
                chapter = chapter->next;
            }

            node = node->next;
        }
        */

        LIST_NODE node = (LIST_NODE) download->head;
        int minimum = (download->size > DEDICATED_MANGA) ? DEDICATED_MANGA : download->size;
        for (int i = 0; i < THREADS; i++) {
            if (i < minimum) {
                SPACE_T *space = (SPACE_T *) malloc(sizeof(SPACE_T));
                space->manga = (MANGA *) node->data;
                space->mutex_manga = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
                pthread_mutex_init(space->mutex_manga, NULL);

                spaces[i] = space;
                node = node->next;
            } else {
                spaces[i] = spaces[i - minimum];
            }
        }

        for (int i = 0; i < THREADS; i++) {

            if (spaces[i]->manga == NULL) printf("\t\t\t\t\t\t\t\t   Organizer : space %d, book is NULL\n", i);
            printf("Organizer : manga \"%s\" added to space %d\n", spaces[i]->manga->manga_name, i);
            // printf("Organizer : book added to space %d\n", i);

        }

        // My code end

        pause_organize = true;
        
        pthread_mutex_lock(&mutex_consumer);
        pause_consumer = false;
        pthread_cond_broadcast(&cond_consumer);
        pthread_mutex_unlock(&mutex_consumer);

        pthread_mutex_unlock(&mutex_organizer);
    }

    return NULL;
}

void *consumer(void *args) {
    int thread_id = *(int *) args;

    while (true) {
        pthread_mutex_lock(&mutex_consumer);
        while (pause_consumer) {
            printf("Consumer %d: waiting for book\n", thread_id);
            pthread_cond_wait(&cond_consumer, &mutex_consumer);
        }

        pthread_mutex_unlock(&mutex_consumer);
        // My code

        if (spaces[thread_id] == NULL) {
            printf("Consumer %d: space is empty\n", thread_id);
            pause_consumer = true;
            continue;
        }

        pthread_mutex_t *mutex_manga = spaces[thread_id]->mutex_manga;

        pthread_mutex_lock(mutex_manga);
        
        if (spaces[thread_id]->manga == NULL) {
            printf("Consumer %d: space has no book\n", thread_id);
            pause_consumer = true;
            pthread_mutex_unlock(mutex_manga);
            continue;
        }

        MANGA *manga = spaces[thread_id]->manga;
        if (spaces[thread_id]->manga->chapters->size == 0) {
            printf("Consumer %d: space has a book, but no chapters\n", thread_id);
            pause_consumer = true;

            remove_list((LIST) download, (void *) manga);
            manga = NULL;

            pthread_mutex_lock(&mutex_organizer);
            pause_organize = false;
            pthread_cond_broadcast(&cond_organizer);
            pthread_mutex_unlock(&mutex_organizer);

            pthread_mutex_unlock(mutex_manga);
            continue;
        }

        CHAPTER *chapter = (CHAPTER *) dequeue(manga->chapters);

        pthread_mutex_unlock(mutex_manga);

        if (chapter != NULL) {
            sleep(1);
            printf("Consumer %d: space has downloaded chapter %.2f from book %s\n", thread_id, chapter->id, manga->manga_name);
            download_chapter(manga, chapter);
        }

        // My code end
    }

    return NULL;
}
