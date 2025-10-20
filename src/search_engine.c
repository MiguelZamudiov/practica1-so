// src/search_engine.c
// Compilar: gcc -O2 -std=c11 -Wall -o worker search_engine.c -lrt

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <sys/resource.h>
#include <errno.h>
#include <ctype.h>
#include <unistd.h>

// ------------ CONFIGURABLE -------------
#define FALLBACK_PAIR_SIZE (sizeof(uint64_t)+sizeof(int64_t))
// If your hashreviews.c used a TABLE_SIZE fixed (e.g., 6000007), setlo aquí si quieres
#define KNOWN_TABLE_SIZE 6000007
// ---------------------------------------

typedef unsigned long ulong;

static void normalize_simple(const char *src, char *dst, size_t dstsz) {
    size_t j=0;
    for (size_t i=0; src[i] && j+1<dstsz; ++i) {
        unsigned char c = src[i];
        // simple: lower + keep alnum and spaces and common punctuation
        if (c == '"' || c == '\'') continue;
        if (c >= 'A' && c <= 'Z') c = c - 'A' + 'a';
        if ( (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c==' ' || c=='-' || c==':' )
            dst[j++] = c;
    }
    while (j>0 && dst[j-1]==' ') j--;
    dst[j]='\0';
}

static unsigned long djb2_hash(const char *str) {
    unsigned long h = 5381;
    int c;
    while ((c = *str++)) h = ((h<<5) + h) + (unsigned char)c;
    return h;
}

// parse request line into fields (modifies input)
static void parse_req(char *line, char **key, char **etype, char **pmin, char **pmax, char **dfrom, char **dto) {
    *key = strtok(line, "|");
    *etype = strtok(NULL, "|");
    *pmin = strtok(NULL, "|");
    *pmax = strtok(NULL, "|");
    *dfrom = strtok(NULL, "|");
    *dto  = strtok(NULL, "|\n");
}

// Read a CSV line at offset pos. Caller must free returned ptr. Returns NULL on error/EOF
static char *read_csv_line_at(FILE *csv, long pos) {
    if (fseek(csv, pos, SEEK_SET) != 0) return NULL;
    size_t cap = 8192;
    char *buf = malloc(cap);
    if (!buf) return NULL;
    if (!fgets(buf, cap, csv)) { free(buf); return NULL; }
    // If line was truncated, extend until newline
    if (strchr(buf, '\n') == NULL) {
        size_t len = strlen(buf);
        while (1) {
            if (len + 8192 > cap) { cap *= 2; buf = realloc(buf, cap); if (!buf) return NULL; }
            if (!fgets(buf + len, cap - len, csv)) break;
            len = strlen(buf);
            if (strchr(buf, '\n')) break;
        }
    }
    return buf;
}

// Attempt header-based lookup: detect a magic header and perform bucket lookup.
// We implement a *simple* header detection: first 4 bytes "HTAB" (ASCII). If present, read header.
// Header format (worker expects):
// 4 bytes magic "HTAB"
// uint32_t version
// uint32_t n_buckets
// uint32_t n_nodes
// uint64_t buckets_offset
// uint64_t nodes_offset
// buckets array: n_buckets * int32 (node_index or -1)
// nodes array: n_nodes * node_struct { uint64_t hash; int64_t pos; int32_t next; (padding) }
// If not found or header mismatch, return -1.
static int search_with_header(FILE *indexf, ulong want_hash, long **out_positions, size_t *out_count) {
    rewind(indexf);
    char magic[5] = {0};
    if (fread(magic,1,4,indexf) != 4) return -1;
    if (strncmp(magic, "HTAB",4) != 0) return -1;
    uint32_t version=0, n_buckets=0, n_nodes=0;
    uint64_t buckets_off=0, nodes_off=0;
    if (fread(&version, sizeof(uint32_t), 1, indexf)!=1) return -1;
    fread(&n_buckets, sizeof(uint32_t), 1, indexf);
    fread(&n_nodes, sizeof(uint32_t), 1, indexf);
    fread(&buckets_off, sizeof(uint64_t), 1, indexf);
    fread(&nodes_off, sizeof(uint64_t), 1, indexf);
    // compute bucket
    uint32_t bucket = (uint32_t)(want_hash % n_buckets);
    // read bucket value (int32)
    int32_t node_idx = -1;
    if (fseek(indexf, (long)buckets_off + (long)bucket * sizeof(int32_t), SEEK_SET) != 0) return -1;
    if (fread(&node_idx, sizeof(int32_t), 1, indexf) != 1) return -1;
    // traverse node chain
    size_t found = 0;
    size_t cap = 16;
    long *positions = malloc(sizeof(long) * cap);
    while (node_idx != -1) {
        // read node at nodes_off + node_idx * sizeof(node)
        long node_pos = (long)nodes_off + (long)node_idx * (sizeof(uint64_t) + sizeof(int64_t) + sizeof(int32_t));
        if (fseek(indexf, node_pos, SEEK_SET) != 0) break;
        uint64_t h; int64_t pos; int32_t next;
        if (fread(&h, sizeof(uint64_t), 1, indexf) != 1) break;
        if (fread(&pos, sizeof(int64_t), 1, indexf) != 1) break;
        if (fread(&next, sizeof(int32_t), 1, indexf) != 1) break;
        if (h == (uint64_t)want_hash) {
            if (found >= cap) { cap *= 2; positions = realloc(positions, cap * sizeof(long)); }
            positions[found++] = (long)pos;
        }
        node_idx = next;
    }
    if (found == 0) { free(positions); *out_positions = NULL; *out_count = 0; return 0; }
    *out_positions = positions; *out_count = found;
    return 0;
}

// Fallback: linear scan of index file for pairs (uint64 hash, int64 pos)
static int search_scan_pairs(FILE *indexf, ulong want_hash, long **out_positions, size_t *out_count) {
    rewind(indexf);
    // Try to read pairs until EOF
    size_t cap = 32;
    long *positions = malloc(sizeof(long) * cap);
    size_t found = 0;
    while (1) {
        uint64_t h;
        int64_t pos;
        size_t r1 = fread(&h, sizeof(uint64_t), 1, indexf);
        size_t r2 = fread(&pos, sizeof(int64_t), 1, indexf);
        if (r1!=1 || r2!=1) break;
        if (h == (uint64_t)want_hash) {
            if (found >= cap) { cap *= 2; positions = realloc(positions, cap*sizeof(long)); }
            positions[found++] = (long)pos;
        }
    }
    if (found == 0) { free(positions); *out_positions = NULL; *out_count = 0; return 0; }
    *out_positions = positions; *out_count = found;
    return 0;
}

static void send_response_fifo(FILE *fifo_out, char **results, size_t n) {
    if (n==0) {
        fprintf(fifo_out, "NA\n");
    } else {
        for (size_t i=0;i<n;i++) {
            fprintf(fifo_out, "%s", results[i]);
            if (results[i][strlen(results[i])-1] != '\n') fprintf(fifo_out, "\n");
        }
    }
    fprintf(fifo_out, "__END__\n");
    fflush(fifo_out);
}

// main worker routine
int main(int argc, char **argv) {
    if (argc < 5) {
        fprintf(stderr, "Uso: %s index.bin csv_file fifo_req fifo_resp\n", argv[0]);
        return 1;
    }
    const char *index_path = argv[1];
    const char *csv_path = argv[2];
    const char *fifo_req = argv[3];
    const char *fifo_resp = argv[4];

    FILE *indexf = fopen(index_path, "rb");
    if (!indexf) { perror("open index"); return 1; }
    FILE *csvf = fopen(csv_path, "r");
    if (!csvf) { perror("open csv"); fclose(indexf); return 1; }

    // Open FIFOs
    FILE *fin = fopen(fifo_req, "r");
    if (!fin) { perror("open fifo_req"); fclose(indexf); fclose(csvf); return 1; }
    FILE *fout = fopen(fifo_resp, "w");
    if (!fout) { perror("open fifo_resp"); fclose(indexf); fclose(csvf); fclose(fin); return 1; }

    char reqline[4096];
    while (fgets(reqline, sizeof(reqline), fin)) {
        // remove trailing newline
        char *nl = strchr(reqline, '\n'); if (nl) *nl = 0;
        if (strlen(reqline) == 0) continue;
        // parse
        char *key,*etype,*pmin,*pmax,*dfrom,*dto;
        parse_req(reqline, &key,&etype,&pmin,&pmax,&dfrom,&dto);
        char norm[1024]; normalize_simple(key, norm, sizeof(norm));
        unsigned long h = djb2_hash(norm);

        // timing start
        struct timespec t0, t1;
        clock_gettime(CLOCK_MONOTONIC, &t0);

        long *positions = NULL; size_t pos_count = 0;
        int r = search_with_header(indexf, h, &positions, &pos_count);
        if (r != 0) {
            // fallback scan
            search_scan_pairs(indexf, h, &positions, &pos_count);
        }

        // prepare results array
        char **results = NULL; size_t rcap = 0, rcount = 0;
        if (pos_count > 0) {
            rcap = pos_count;
            results = calloc(rcap, sizeof(char*));
            for (size_t i=0;i<pos_count;i++) {
                char *line = read_csv_line_at(csvf, positions[i]);
                if (!line) continue;
                // TODO: apply secondary filters (event_type, price range, dates) here.
                // For now we return the CSV line; you can implement CSV parsing to filter fields.
                results[rcount++] = line;
            }
        }

        // timing end
        clock_gettime(CLOCK_MONOTONIC, &t1);
        double elapsed = (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec)/1e9;
        // memory usage
        struct rusage ru; getrusage(RUSAGE_SELF, &ru);
        long mem_kb = ru.ru_maxrss;

        // write a small log line to stderr (sustentacion)
        fprintf(stderr, "[worker] key='%s' found=%zu elapsed=%.6fs mem_kb=%ld\n", key, rcount, elapsed, mem_kb);

        // Send response
        send_response_fifo(fout, results, rcount);

        // free
        if (positions) free(positions);
        if (results) {
            for (size_t i=0;i<rcount;i++) free(results[i]);
            free(results);
        }
    }

    fclose(fout); fclose(fin);
    fclose(csvf); fclose(indexf);
    return 0;
}

