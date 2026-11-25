def bubble_sort(lista, indice_clave):
    """Ordena `lista` en su lugar (de mayor a menor) usando el algoritmo Bubble Sort,
    comparando los elementos según la posición `indice_clave`.
    """
    n = len(lista)
    for i in range(n - 1):
        hubo_intercambio = False
        # En cada pasada el elemento más pequeño restante queda al final de la sublista
        for j in range(n - 1 - i):
            if lista[j][indice_clave] < lista[j + 1][indice_clave]:
                lista[j], lista[j + 1] = lista[j + 1], lista[j]
                hubo_intercambio = True
        # Si no hubo intercambios, la lista ya está ordenada
        if not hubo_intercambio:
            break


def mayores_generadoras(nombre_archivo, tipo_energia, energia):
    """Retorna una lista de listas [empresa, capacidad_total] con las 3 empresas
    de mayor capacidad de generación para la fuente de energía indicada,
    ordenadas de mayor a menor.
    """
    # Diccionario acumulador: {nombre_empresa: suma_capacidades_MW}
    capacidades_por_empresa = {}

    # Abrimos el archivo en modo lectura y luego lo cerramos explícitamente con close()
    archivo = open(nombre_archivo, "r", encoding="utf-8")
    for linea in archivo:
        # Eliminamos saltos de línea y espacios extremos; si queda vacía, se omite
        linea = linea.strip()
        if not linea:
            continue

        # Extraemos los 6 campos del CSV; '_' descarta id, nombre_planta y comuna
        _, empresa, _, tipo, capacidad_str, _ = linea.split(",")

        # Solo sumamos si el tipo de generación pertenece a la fuente solicitada
        if tipo_energia.get(tipo) == energia:
            capacidad = int(capacidad_str)
            # .get(empresa, 0) parte en 0 la primera vez que aparece la empresa
            capacidades_por_empresa[empresa] = (
                capacidades_por_empresa.get(empresa, 0) + capacidad
            )
    archivo.close()

    # Convertimos el diccionario en una lista de pares [empresa, capacidad] para ordenarla
    ranking = [
        [empresa, capacidad]
        for empresa, capacidad in capacidades_por_empresa.items()
    ]
    # Ordenamos de mayor a menor usando Bubble Sort comparando por item[1] (capacidad)
    bubble_sort(ranking, 1)

    # Retornamos solo los primeros 3 elementos (o menos si hay menos de 3 empresas)
    return ranking[:3]


def centrales_por_empresa(nombre_archivo, capacidad_minima):
    """Crea un archivo 'plantas_<Empresa>.txt' por cada empresa con sus centrales
    de capacidad >= capacidad_minima ordenadas de mayor a menor, y retorna
    el total de centrales escritas.
    """
    # Diccionario que agrupa por empresa una lista de tuplas con sus centrales válidas
    centrales_agrupadas = {}
    total_centrales = 0

    archivo = open(nombre_archivo, "r", encoding="utf-8")
    for linea in archivo:
        linea = linea.strip()
        if not linea:
            continue

        _, empresa, nombre_planta, tipo, capacidad_str, comuna = linea.split(",")
        capacidad = int(capacidad_str)

        # Filtramos únicamente las centrales que alcanzan al menos el mínimo indicado
        if capacidad >= capacidad_minima:
            if empresa not in centrales_agrupadas:
                centrales_agrupadas[empresa] = []
            # Guardamos 'capacidad' primero para facilitar el ordenamiento
            centrales_agrupadas[empresa].append(
                (capacidad, nombre_planta, tipo, comuna)
            )
            total_centrales += 1
    archivo.close()

    # Recorremos cada empresa filtrada para ordenar sus centrales y escribir su archivo
    for empresa, lista_centrales in centrales_agrupadas.items():
        # Ordenamos de mayor a menor usando Bubble Sort comparando por central[0] (capacidad)
        bubble_sort(lista_centrales, 0)
        nombre_salida = f"plantas_{empresa}.txt"

        salida = open(nombre_salida, "w", encoding="utf-8")
        salida.write(
            f"Centrales de {empresa} con capacidad mayor a {capacidad_minima}MW:\n"
        )
        # Desempaquetamos cada tupla para formatear la línea según el formato pedido
        for capacidad, nombre_planta, tipo, comuna in lista_centrales:
            salida.write(
                f"   Planta: {nombre_planta} ({tipo}), {capacidad}MW, ubicada en {comuna}.\n"
            )
        salida.close()

    return total_centrales
