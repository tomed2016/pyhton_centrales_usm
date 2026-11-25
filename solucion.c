#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Par clave-valor que reemplaza al diccionario `tipo_energia` de Python. */
typedef struct {
    const char *tipo;
    const char *fuente;
} TipoEnergia;

/* Acumulador de capacidad total por empresa; `orden` permite desempatar de forma estable. */
typedef struct {
    char *empresa;
    int capacidad_total;
    size_t orden;
} EmpresaCapacidad;

/* Representa una central filtrada lista para ordenarse y escribirse en el archivo de salida. */
typedef struct {
    int capacidad;
    char *nombre_planta;
    char *tipo;
    char *comuna;
    size_t orden;
} Central;

/* Arreglo dinámico de centrales pertenecientes a una misma empresa. */
typedef struct {
    char *empresa;
    Central *centrales;
    size_t cantidad;
    size_t capacidad_arreglo;
} EmpresaCentrales;

/* Crea una copia independiente en memoria dinámica (heap) de una cadena C. */
static char *duplicar_cadena(const char *s) {
    size_t len = strlen(s);
    char *copia = malloc(len + 1);
    if (copia != NULL) {
        memcpy(copia, s, len + 1);
    }
    return copia;
}

/* Recorta espacios en blanco al inicio y al final modificando la cadena en su lugar. */
static char *recortar_espacios(char *s) {
    while (*s != '\0' && isspace((unsigned char)*s)) {
        s++;
    }
    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1])) {
        s[--len] = '\0';
    }
    return s;
}

/*
 * Lee una línea completa de largo arbitrario desde `archivo`, ampliando el buffer
 * con `realloc` si hace falta. Elimina '\r' y '\n' finales.
 * Retorna NULL al llegar a EOF sin caracteres leídos o si falla la asignación.
 */
static char *leer_linea(FILE *archivo) {
    size_t capacidad = 256;
    size_t len = 0;
    char *buffer = malloc(capacidad);
    if (buffer == NULL) {
        return NULL;
    }

    int c;
    while ((c = fgetc(archivo)) != EOF) {
        if (c == '\n') {
            break;
        }
        if (len + 1 >= capacidad) {
            size_t nueva_capacidad = capacidad * 2;
            char *nuevo_buffer = realloc(buffer, nueva_capacidad);
            if (nuevo_buffer == NULL) {
                free(buffer);
                return NULL;
            }
            buffer = nuevo_buffer;
            capacidad = nueva_capacidad;
        }
        buffer[len++] = (char)c;
    }

    if (c == EOF && len == 0) {
        free(buffer);
        return NULL;
    }

    /* Manejo de saltos de línea estilo Windows (\r\n). */
    if (len > 0 && buffer[len - 1] == '\r') {
        len--;
    }
    buffer[len] = '\0';
    return buffer;
}

/*
 * Divide `linea` exactamente en 6 campos separados por ',' reemplazando cada coma
 * por '\0' (equivalente a `linea.split(",")` en Python).
 * Se usa `strchr` en vez de `strtok` para no colapsar campos vacíos consecutivos.
 * Retorna 1 si la línea tiene exactamente 6 campos, o 0 en caso contrario.
 */
static int dividir_csv(char *linea, char *campos[6]) {
    char *actual = linea;
    for (int i = 0; i < 5; i++) {
        char *coma = strchr(actual, ',');
        if (coma == NULL) {
            return 0;
        }
        *coma = '\0';
        campos[i] = actual;
        actual = coma + 1;
    }
    /* Si queda otra coma en el sexto campo, el formato no coincide. */
    if (strchr(actual, ',') != NULL) {
        return 0;
    }
    campos[5] = actual;
    return 1;
}

/* Busca linealmente el `tipo` en la tabla `tipo_energia` y retorna su fuente o NULL. */
static const char *buscar_fuente(const TipoEnergia *tipo_energia,
                                 size_t num_tipos,
                                 const char *tipo) {
    for (size_t i = 0; i < num_tipos; i++) {
        if (strcmp(tipo_energia[i].tipo, tipo) == 0) {
            return tipo_energia[i].fuente;
        }
    }
    return NULL;
}

/*
 * Comparador para `qsort`: ordena empresas de mayor a menor capacidad total.
 * En caso de empate, conserva el orden de aparición original usando `orden`.
 * (Retornar -1 coloca `a` antes que `b`; retornar 1 coloca `b` antes que `a`).
 */
static int comparar_empresas(const void *a, const void *b) {
    const EmpresaCapacidad *ea = (const EmpresaCapacidad *)a;
    const EmpresaCapacidad *eb = (const EmpresaCapacidad *)b;
    if (ea->capacidad_total < eb->capacidad_total) {
        return 1;
    }
    if (ea->capacidad_total > eb->capacidad_total) {
        return -1;
    }
    if (ea->orden < eb->orden) {
        return -1;
    }
    if (ea->orden > eb->orden) {
        return 1;
    }
    return 0;
}

/* Comparador para `qsort`: ordena centrales de mayor a menor capacidad (desempate por `orden`). */
static int comparar_centrales(const void *a, const void *b) {
    const Central *ca = (const Central *)a;
    const Central *cb = (const Central *)b;
    if (ca->capacidad < cb->capacidad) {
        return 1;
    }
    if (ca->capacidad > cb->capacidad) {
        return -1;
    }
    if (ca->orden < cb->orden) {
        return -1;
    }
    if (ca->orden > cb->orden) {
        return 1;
    }
    return 0;
}

/* Libera los nombres dinámicos almacenados en el arreglo de resultados del top 3. */
void liberar_mayores_generadoras(EmpresaCapacidad resultados[], int cantidad) {
    for (int i = 0; i < cantidad; i++) {
        free(resultados[i].empresa);
        resultados[i].empresa = NULL;
    }
}

/*
 * Llena `resultados` (hasta 3 elementos) con las empresas de mayor capacidad
 * total de generación para la fuente indicada ("Renovable" o "No Renovable"),
 * ordenadas de mayor a menor.
 * Retorna la cantidad de empresas encontradas (0..3), o -1 si ocurre un error.
 * El llamador debe liberar los nombres con `liberar_mayores_generadoras`.
 */
int mayores_generadoras(const char *nombre_archivo,
                        const TipoEnergia *tipo_energia,
                        size_t num_tipos,
                        const char *energia,
                        EmpresaCapacidad resultados[3]) {
    FILE *archivo = fopen(nombre_archivo, "r");
    if (archivo == NULL) {
        return -1;
    }

    EmpresaCapacidad *empresas = NULL;
    size_t num_empresas = 0;
    size_t cap_empresas = 0;
    size_t orden = 0;

    char *linea;
    while ((linea = leer_linea(archivo)) != NULL) {
        char *limpia = recortar_espacios(linea);
        if (*limpia == '\0') {
            free(linea);
            continue;
        }

        char *campos[6];
        if (!dividir_csv(limpia, campos)) {
            free(linea);
            continue;
        }

        /* Índices del CSV: 0=id, 1=empresa, 2=nombre_planta, 3=tipo, 4=capacidad, 5=comuna */
        const char *empresa = campos[1];
        const char *tipo = campos[3];
        int capacidad = (int)strtol(campos[4], NULL, 10);

        const char *fuente = buscar_fuente(tipo_energia, num_tipos, tipo);
        if (fuente != NULL && strcmp(fuente, energia) == 0) {
            /* Buscamos si la empresa ya existe en nuestro arreglo acumulador */
            size_t idx = num_empresas;
            for (size_t i = 0; i < num_empresas; i++) {
                if (strcmp(empresas[i].empresa, empresa) == 0) {
                    idx = i;
                    break;
                }
            }

            if (idx == num_empresas) {
                /* Empresa nueva: ampliamos el arreglo dinámico si está lleno */
                if (num_empresas == cap_empresas) {
                    size_t nueva_cap = cap_empresas == 0 ? 8 : cap_empresas * 2;
                    EmpresaCapacidad *nuevo =
                        realloc(empresas, nueva_cap * sizeof(*empresas));
                    if (nuevo == NULL) {
                        free(linea);
                        fclose(archivo);
                        for (size_t i = 0; i < num_empresas; i++) {
                            free(empresas[i].empresa);
                        }
                        free(empresas);
                        return -1;
                    }
                    empresas = nuevo;
                    cap_empresas = nueva_cap;
                }
                empresas[num_empresas].empresa = duplicar_cadena(empresa);
                if (empresas[num_empresas].empresa == NULL) {
                    free(linea);
                    fclose(archivo);
                    for (size_t i = 0; i < num_empresas; i++) {
                        free(empresas[i].empresa);
                    }
                    free(empresas);
                    return -1;
                }
                empresas[num_empresas].capacidad_total = capacidad;
                empresas[num_empresas].orden = orden++;
                num_empresas++;
            } else {
                /* Empresa ya registrada: acumulamos su capacidad */
                empresas[idx].capacidad_total += capacidad;
            }
        }

        free(linea);
    }

    fclose(archivo);

    /* Ordenamos todas las empresas acumuladas de mayor a menor capacidad */
    if (num_empresas > 1) {
        qsort(empresas, num_empresas, sizeof(*empresas), comparar_empresas);
    }

    /* Transferimos la propiedad de los primeros 3 nombres a `resultados` */
    int top = num_empresas < 3 ? (int)num_empresas : 3;
    for (int i = 0; i < top; i++) {
        resultados[i] = empresas[i];
    }
    /* Liberamos los nombres de las empresas que quedaron fuera del top 3 y el arreglo */
    for (size_t i = (size_t)top; i < num_empresas; i++) {
        free(empresas[i].empresa);
    }
    free(empresas);

    return top;
}

/* Libera toda la memoria asociada a los grupos de empresas y sus centrales. */
static void liberar_empresas_centrales(EmpresaCentrales *grupos,
                                       size_t num_grupos) {
    for (size_t i = 0; i < num_grupos; i++) {
        free(grupos[i].empresa);
        for (size_t j = 0; j < grupos[i].cantidad; j++) {
            free(grupos[i].centrales[j].nombre_planta);
            free(grupos[i].centrales[j].tipo);
            free(grupos[i].centrales[j].comuna);
        }
        free(grupos[i].centrales);
    }
    free(grupos);
}

/*
 * Crea un archivo "plantas_<Empresa>.txt" por cada empresa con sus centrales
 * de capacidad >= capacidad_minima ordenadas de mayor a menor, y retorna
 * el total de centrales escritas (o -1 si ocurre un error).
 */
int centrales_por_empresa(const char *nombre_archivo, int capacidad_minima) {
    FILE *archivo = fopen(nombre_archivo, "r");
    if (archivo == NULL) {
        return -1;
    }

    EmpresaCentrales *grupos = NULL;
    size_t num_grupos = 0;
    size_t cap_grupos = 0;
    int total_centrales = 0;
    size_t orden = 0;

    char *linea;
    while ((linea = leer_linea(archivo)) != NULL) {
        char *limpia = recortar_espacios(linea);
        if (*limpia == '\0') {
            free(linea);
            continue;
        }

        char *campos[6];
        if (!dividir_csv(limpia, campos)) {
            free(linea);
            continue;
        }

        const char *empresa = campos[1];
        const char *nombre_planta = campos[2];
        const char *tipo = campos[3];
        int capacidad = (int)strtol(campos[4], NULL, 10);
        const char *comuna = campos[5];

        /* Solo consideramos centrales que cumplan el mínimo de capacidad */
        if (capacidad >= capacidad_minima) {
            /* Buscamos o creamos el grupo correspondiente a la empresa */
            size_t idx = num_grupos;
            for (size_t i = 0; i < num_grupos; i++) {
                if (strcmp(grupos[i].empresa, empresa) == 0) {
                    idx = i;
                    break;
                }
            }

            if (idx == num_grupos) {
                if (num_grupos == cap_grupos) {
                    size_t nueva_cap = cap_grupos == 0 ? 8 : cap_grupos * 2;
                    EmpresaCentrales *nuevo =
                        realloc(grupos, nueva_cap * sizeof(*grupos));
                    if (nuevo == NULL) {
                        free(linea);
                        fclose(archivo);
                        liberar_empresas_centrales(grupos, num_grupos);
                        return -1;
                    }
                    grupos = nuevo;
                    cap_grupos = nueva_cap;
                }
                grupos[num_grupos].empresa = duplicar_cadena(empresa);
                if (grupos[num_grupos].empresa == NULL) {
                    free(linea);
                    fclose(archivo);
                    liberar_empresas_centrales(grupos, num_grupos);
                    return -1;
                }
                grupos[num_grupos].centrales = NULL;
                grupos[num_grupos].cantidad = 0;
                grupos[num_grupos].capacidad_arreglo = 0;
                num_grupos++;
            }

            /* Agregamos la central al arreglo dinámico de esa empresa */
            EmpresaCentrales *g = &grupos[idx];
            if (g->cantidad == g->capacidad_arreglo) {
                size_t nueva_cap =
                    g->capacidad_arreglo == 0 ? 8 : g->capacidad_arreglo * 2;
                Central *nuevo =
                    realloc(g->centrales, nueva_cap * sizeof(*g->centrales));
                if (nuevo == NULL) {
                    free(linea);
                    fclose(archivo);
                    liberar_empresas_centrales(grupos, num_grupos);
                    return -1;
                }
                g->centrales = nuevo;
                g->capacidad_arreglo = nueva_cap;
            }

            Central *c = &g->centrales[g->cantidad];
            c->capacidad = capacidad;
            c->nombre_planta = duplicar_cadena(nombre_planta);
            c->tipo = duplicar_cadena(tipo);
            c->comuna = duplicar_cadena(comuna);
            c->orden = orden++;

            if (c->nombre_planta == NULL || c->tipo == NULL ||
                c->comuna == NULL) {
                free(c->nombre_planta);
                free(c->tipo);
                free(c->comuna);
                free(linea);
                fclose(archivo);
                liberar_empresas_centrales(grupos, num_grupos);
                return -1;
            }

            g->cantidad++;
            total_centrales++;
        }

        free(linea);
    }

    fclose(archivo);

    /* Para cada empresa, ordenamos sus centrales y generamos "plantas_<Empresa>.txt" */
    for (size_t i = 0; i < num_grupos; i++) {
        EmpresaCentrales *g = &grupos[i];
        if (g->cantidad > 1) {
            qsort(g->centrales, g->cantidad, sizeof(*g->centrales),
                  comparar_centrales);
        }

        size_t nombre_len = strlen("plantas_") + strlen(g->empresa) + strlen(".txt") + 1;
        char *nombre_salida = malloc(nombre_len);
        if (nombre_salida == NULL) {
            liberar_empresas_centrales(grupos, num_grupos);
            return -1;
        }
        snprintf(nombre_salida, nombre_len, "plantas_%s.txt", g->empresa);

        FILE *salida = fopen(nombre_salida, "w");
        free(nombre_salida);
        if (salida == NULL) {
            liberar_empresas_centrales(grupos, num_grupos);
            return -1;
        }

        fprintf(salida,
                "Centrales de %s con capacidad mayor a %dMW:\n",
                g->empresa, capacidad_minima);
        for (size_t j = 0; j < g->cantidad; j++) {
            const Central *c = &g->centrales[j];
            fprintf(salida,
                    "   Planta: %s (%s), %dMW, ubicada en %s.\n",
                    c->nombre_planta, c->tipo, c->capacidad, c->comuna);
        }

        fclose(salida);
    }

    liberar_empresas_centrales(grupos, num_grupos);
    return total_centrales;
}

/* Compila con -DSIN_MAIN si deseas usar este archivo solo como librería sin `main`. */
#ifndef SIN_MAIN
int main(int argc, char *argv[]) {
    const char *archivo = argc > 1 ? argv[1] : "centrales.csv";

    /* Tabla equivalente al diccionario `tipo_energia` del enunciado */
    const TipoEnergia tipo_energia[] = {
        {"Petróleo", "No Renovable"},
        {"Gas", "No Renovable"},
        {"Carbón", "No Renovable"},
        {"Nuclear", "No Renovable"},
        {"Solar", "Renovable"},
        {"Eólica", "Renovable"},
        {"Hidroeléctrica de pasada", "Renovable"},
        {"Hidroeléctrica de embalse", "Renovable"},
        {"Geotérmica", "Renovable"},
        {"Biomasa", "Renovable"},
        {"Mareomotriz", "Renovable"},
    };
    size_t num_tipos = sizeof(tipo_energia) / sizeof(tipo_energia[0]);

    EmpresaCapacidad top[3];
    int n = mayores_generadoras(archivo, tipo_energia, num_tipos, "Renovable", top);
    if (n < 0) {
        fprintf(stderr, "No se pudo abrir el archivo '%s'.\n", archivo);
        return 1;
    }

    printf("Mayores generadoras (Renovable):\n");
    for (int i = 0; i < n; i++) {
        printf("  %s: %d MW\n", top[i].empresa, top[i].capacidad_total);
    }
    liberar_mayores_generadoras(top, n);

    int total = centrales_por_empresa(archivo, 350);
    if (total < 0) {
        fprintf(stderr, "Error al generar archivos por empresa.\n");
        return 1;
    }
    printf("Total de centrales escritas: %d\n", total);

    return 0;
}
#endif
