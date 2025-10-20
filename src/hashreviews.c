#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TABLE_SIZE 6000007   // número primo grande
#define MAX_NODES 3100000    // un poco más que 3M

typedef struct {
    unsigned long hash;
    long position;
    int next;   // índice del siguiente nodo o -1
} Node;

typedef struct {
    int *buckets;      // tamaño TABLE_SIZE (inicializados a -1)
    Node *nodes;       // tamaño MAX_NODES
    int count;
} HashTable;

// --- HASH DJB2 ---
unsigned long hashFunction(const char *str) {
    unsigned long hash = 5381;
    int c;
    while ((c = *str++))
        hash = ((hash << 5) + hash) + c;
    return hash;
}

// --- NORMALIZADOR SIMPLE ---
void normalize_title(const char *src, char *dst, size_t dst_size) {
    size_t j = 0;
    for (size_t i = 0; src[i] != '\0' && j + 1 < dst_size; i++) {
        char c = src[i];
        if (c == '\r' || c == '\n') break;
        if (c == '"' || c == '\'') continue;
        if (c >= 'A' && c <= 'Z') c = c - 'A' + 'a';
        dst[j++] = c;
    }
    while (j > 0 && dst[j-1] == ' ') j--;
    dst[j] = '\0';
}

// --- FUNCIONES HASH TABLE ---
void initTable(HashTable *ht) {
    ht->buckets = malloc(sizeof(int) * TABLE_SIZE);
    ht->nodes = malloc(sizeof(Node) * MAX_NODES);
    if (!ht->buckets || !ht->nodes) {
        fprintf(stderr, "No hay memoria suficiente\n");
        exit(1);
    }
    for (int i = 0; i < TABLE_SIZE; ++i) ht->buckets[i] = -1;
    ht->count = 0;
}

void freeTable(HashTable *ht) {
    free(ht->buckets);
    free(ht->nodes);
}

void insertReview(HashTable *ht, unsigned long hash, long position) {
    if (ht->count >= MAX_NODES) {
        fprintf(stderr, "Se alcanzó MAX_NODES (%d)\n", MAX_NODES);
        exit(1);
    }
    int bucket = (int)(hash % TABLE_SIZE);
    int idx = ht->count++;
    ht->nodes[idx].hash = hash;
    ht->nodes[idx].position = position;
    ht->nodes[idx].next = ht->buckets[bucket];
    ht->buckets[bucket] = idx;
}

void writeIndexToFile(HashTable *ht, const char *filename) {
    FILE *f = fopen(filename, "wb");
    if (!f) { perror("fopen"); exit(1); }
    fwrite(&ht->count, sizeof(int), 1, f);
    fwrite(ht->buckets, sizeof(int), TABLE_SIZE, f);
    fwrite(ht->nodes, sizeof(Node), ht->count, f);
    fclose(f);
}

// --- FUNCIÓN PRINCIPAL ---
int main() {
    FILE *csv = fopen("Books_rating.csv", "r");
    if (!csv) { perror("reviews.csv"); return 1; }

    HashTable ht;
    initTable(&ht);

    char line[8192];
    char raw_title[1024];
    char title[1024];
    long pos;

    // Saltar header
    fgets(line, sizeof(line), csv);

    while ((pos = ftell(csv)), fgets(line, sizeof(line), csv)) {
        // Parsear título del segundo campo
        // Ejemplo: id,"Book Title","Review text",rating...
        char *token = strtok(line, ",");  // primer campo
        token = strtok(NULL, ",");        // segundo campo (el título)
        if (!token) continue;

        // Eliminar posibles comillas
        if (token[0] == '"') {
            memmove(token, token + 1, strlen(token));
            char *end = strchr(token, '"');
            if (end) *end = '\0';
        }

        normalize_title(token, title, sizeof(title));

        unsigned long h = hashFunction(title);
        insertReview(&ht, h, pos);
    }

    writeIndexToFile(&ht, "reviews_index.bin");
    printf("✅ Indexación completada — nodos: %d\n", ht.count);

    freeTable(&ht);
    fclose(csv);
    return 0;
}
