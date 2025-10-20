# Protocolo IPC (FIFOs)

FIFOs:
- fifo_req  : UI -> worker
- fifo_resp : worker -> UI

Formato petición:
key|event_type|price_min|price_max|date_from|date_to\n
(Para pruebas puede usarse solo la clave: "its only art if its well hung||||\n")

Formato respuesta:
- 0..N líneas con resultados (cada línea: la línea completa del CSV o campos escogidos)
- Al final: __END__\n
- Si no hay resultados: NA\n__END__\n
