# INSTRUCCIONES PARA EJECUTAR EL PROYECTO

## Datasets requeridos

Este proyecto requiere descargar los siguientes datasets de Kaggle:

**Amazon Books Reviews Dataset:**
- URL: https://www.kaggle.com/datasets/mohamedbakhet/amazon-books-reviews
- Archivos necesarios:
  - `books_data.csv` → colocar en `data/books_data.csv`
  - `Books_rating.csv` → colocar en `data/Books_rating.csv`

## Pasos para ejecutar:

1. **Descargar y colocar datasets:**
   ```bash
   # Descargar desde Kaggle y colocar en:
   data/books_data.csv      # (212K libros)
   data/Books_rating.csv    # (3M reviews)
   ```

2. **Compilar:**
   ```bash
   make
   ```

3. **Crear índices:**
   ```bash
   ./src/indexador data/books_data.csv data/index_books.bin
   ./src/indexador data/Books_rating.csv data/reviews_index.bin
   ```

4. **Ejecutar:**
   ```bash
   mkfifo fifo_req fifo_resp
   ./src/search_engine data/index_books.bin data/books_data.csv data/reviews_index.bin data/Books_rating.csv fifo_req fifo_resp &
   ```

5. **Probar:**
   ```bash
   echo "dr seuss american icon|yes" > fifo_req
   cat fifo_resp
   ```

## Nota importante
Los datasets no están incluidos en el repositorio debido a su tamaño (>2GB total).