#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define TABLE_SIZE 1000

typedef struct Node {
    unsigned long hash;
    long position;
    struct Node *next;
} Node;

typedef struct {
    Node *buckets[TABLE_SIZE];
} HashTable;

/**
 * Normaliza un título:
 * - Convierte a minúsculas
 * - Elimina acentos comunes
 * - Quita caracteres no alfabéticos ni numéricos (excepto espacios)
 */
void normalize_title(char *dest, const char *src) {
    const unsigned char *p = (const unsigned char *)src;
    char *d = dest;

    while (*p) {
        if (p[0] == 0xC3) { // inicio de caracter acentuado en UTF-8
            switch (p[1]) {
                case 0xA1: case 0xA0: case 0xA2: case 0xA4: *d++ = 'a'; break; // áàâä
                case 0xA9: case 0xA8: case 0xAA: case 0xAB: *d++ = 'e'; break; // éèêë
                case 0xAD: case 0xAC: case 0xAF: case 0xAE: *d++ = 'i'; break; // íìîï
                case 0xB3: case 0xB2: case 0xB4: case 0xB6: *d++ = 'o'; break; // óòôö
                case 0xBA: case 0xB9: case 0xBB: case 0xBC: *d++ = 'u'; break; // úùûü
                case 0xB1: *d++ = 'n'; break; // ñ
            }
            p += 2;
            continue;
        }

        unsigned char c = tolower(*p);
        if (isalnum(c) || c == ' ')
            *d++ = c;

        p++;
    }

    *d = '\0';
}


/**
 * Hash DJB2
 */
unsigned long hashFunction(const char *str) {
    unsigned long hash = 5381;
    int c;
    while ((c = *str++))
        hash = ((hash << 5) + hash) + c;  // hash * 33 + c
    return hash;
}

void insert(HashTable *table, unsigned long hash, long position) {
    int index = hash % TABLE_SIZE;
    Node *newNode = (Node *)malloc(sizeof(Node));
    newNode->hash = hash;
    newNode->position = position;
    newNode->next = table->buckets[index];
    table->buckets[index] = newNode;
}

void writeIndexToFile(HashTable *table, const char *filename) {
    FILE *file = fopen(filename, "wb");
    if (!file) {
        perror("Error creando archivo binario");
        exit(1);
    }

    for (int i = 0; i < TABLE_SIZE; i++) {
        Node *current = table->buckets[i];
        while (current != NULL) {
            fwrite(&(current->hash), sizeof(unsigned long), 1, file);
            fwrite(&(current->position), sizeof(long), 1, file);
            current = current->next;
        }
    }

    fclose(file);
}

/**
 * Programa principal
 */
int main() {
    FILE *dataFile = fopen("books_data.csv", "r");
    if (!dataFile) {
        printf("No se pudo abrir books_data.csv\n");
        return 1;
    }

    HashTable table = {0};
    char line[1024];
    char title[256];
    char normalized_title[256];
    long position;

    // saltar encabezado
    fgets(line, sizeof(line), dataFile);

    while ((position = ftell(dataFile)), fgets(line, sizeof(line), dataFile)) {
        // suponiendo que el título está en la primera columna
        sscanf(line, "%255[^,]", title);

        normalize_title(normalized_title, title);
        unsigned long hash = hashFunction(normalized_title);
        insert(&table, hash, position);
    }

    writeIndexToFile(&table, "index_books.bin");
    fclose(dataFile);

    printf("✅ Indexación completada. Archivo 'index_books.bin' creado.\n");
    return 0;
}
