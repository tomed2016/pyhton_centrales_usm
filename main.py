import os
import sys
import tempfile

from solucion import centrales_por_empresa, mayores_generadoras

# Diccionario completo con los tipos de generación mostrados en el enunciado
TIPO_ENERGIA = {
    "Petróleo": "No Renovable",
    "Gas": "No Renovable",
    "Carbón": "No Renovable",
    "Nuclear": "No Renovable",
    "Solar": "Renovable",
    "Eólica": "Renovable",
    "Hidroeléctrica de pasada": "Renovable",
    "Hidroeléctrica de embalse": "Renovable",
    "Geotérmica": "Renovable",
    "Biomasa": "Renovable",
    "Mareomotriz": "Renovable",
}

# Datos de ejemplo (extracto del certamen) por si aún no existe 'centrales.csv'
DATOS_EJEMPLO = """\
1024,EnergíaPatagónica,Geotérmica Mapu,Geotérmica,163,Copiapó
1004,Sol y Sombra,Gas Mapu,Gas,408,Puerto Montt
1012,Geotérmica Ñamku,Biomasa Nueva,Biomasa,691,Mejillones
1014,EcoEnerChile,Mareomotriz Nueva,Mareomotriz,158,María Elena
1006,SurVoltaje,Hidroeléctrica Austral,Hidroeléctrica de embalse,235,Rancagua
1027,Voltaje Total,Nuclear Cordillera,Nuclear,582,Valdivia
1000,Gasnova,Eólica Verde,Eólica,125,Copiapó
1011,PetroEner,Carbón Austral,Carbón,699,Alto Biobío
1140,Luz Austral,Hidroeléctrica Sur,Hidroeléctrica de pasada,552,Mejillones
1200,Gasnova,Geotérmica EnergíaLibre,Geotérmica,737,La Serena
1201,Gasnova,Solar Horizonte,Solar,628,Copiapó
1202,Gasnova,Solar Nueva,Solar,618,La Serena
"""


def ejecutar_pruebas(ruta_csv, capacidad_minima=350):
    """Ejecuta las dos funciones de solucion.py y muestra sus resultados."""
    print("=== Prueba 1: mayores_generadoras (Renovable) ===")
    top_renovable = mayores_generadoras(ruta_csv, TIPO_ENERGIA, "Renovable")
    print(top_renovable)

    print("\n=== Prueba 2: mayores_generadoras (No Renovable) ===")
    top_no_renovable = mayores_generadoras(ruta_csv, TIPO_ENERGIA, "No Renovable")
    print(top_no_renovable)

    print(f"\n=== Prueba 3: centrales_por_empresa (mínimo {capacidad_minima} MW) ===")
    total = centrales_por_empresa(ruta_csv, capacidad_minima)
    print(f"Total de centrales escritas: {total}")


def main():
    # Si se entrega una ruta por consola o existe 'centrales.csv', se usa ese archivo
    if len(sys.argv) > 1:
        ruta_csv = sys.argv[1]
        ejecutar_pruebas(ruta_csv)
        return

    if os.path.exists("centrales.csv"):
        ejecutar_pruebas("centrales.csv")
        return

    # Si no existe 'centrales.csv', probamos en una carpeta temporal con datos de ejemplo
    print("No se encontró 'centrales.csv'; ejecutando pruebas con datos de ejemplo.\n")
    carpeta_original = os.getcwd()
    with tempfile.TemporaryDirectory() as tmpdir:
        os.chdir(tmpdir)
        try:
            ruta_ejemplo = os.path.join(tmpdir, "centrales_ejemplo.csv")
            archivo_ejemplo = open(ruta_ejemplo, "w", encoding="utf-8")
            archivo_ejemplo.write(DATOS_EJEMPLO)
            archivo_ejemplo.close()

            ejecutar_pruebas(ruta_ejemplo)

            print("\n=== Archivos generados (ejemplo: plantas_Gasnova.txt) ===")
            archivo_muestra = os.path.join(tmpdir, "plantas_Gasnova.txt")
            if os.path.exists(archivo_muestra):
                muestra = open(archivo_muestra, "r", encoding="utf-8")
                print(muestra.read(), end="")
                muestra.close()
        finally:
            os.chdir(carpeta_original)


if __name__ == "__main__":
    main()
