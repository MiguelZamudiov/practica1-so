# Formato de index_books.bin

Cada entrada ocupa 16 bytes (little-endian):

- uint64_t hash   (8 bytes) — DJB2 del título normalizado
- int64_t  offset (8 bytes) — offset (ftell) en bytes dentro de `books_data.csv` donde comienza la línea

Interpretación:
- Leer bloques de 16 bytes.
- Para cada entrada: si hash == hash_busqueda -> fseek(csv, offset, SEEK_SET); fgets(line,...)
