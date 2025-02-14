#include <stdio.h>

// Definici�n de estados
typedef enum {
    CERRADO = 1,
    ABRIENDO,
    ABIERTO,
    CERRANDO,
    ERROR,
    PARPADEO,
    DETENIDA
} Estado;

int main() {
    Estado estado = CERRADO;
    char entrada;
    int LSA = 0, LSC = 1, FTC = 0;

    printf("\nSimulaci�n de m�quina de estados - Control de puerta\n");
    printf("Ingrese A para abrir, C para cerrar, PP para detener, RST para resetear.\n");

    for (;;) {
        printf("\nEstado actual: %d\n", estado);
        printf("Ingrese se�al: ");
        scanf(" %c", &entrada);

        if (entrada == 'A') {
            if (estado == CERRADO) {
                printf("Transici�n: CERRADO -> ABRIENDO\n");
                estado = ABRIENDO;
                LSA = 1;
            }
        } else if (entrada == 'C') {
            if (estado == ABIERTO) {
                printf("Transici�n: ABIERTO -> CERRANDO\n");
                estado = CERRANDO;
                LSC = 1;
            }
        } else if (entrada == 'P') {  // PP se�al
            if (estado == ABRIENDO) {
                printf("Transici�n: ABRIENDO -> DETENIDA\n");
                estado = DETENIDA;
            }
        } else if (entrada == 'R') {  // RST se�al
            if (estado == CERRANDO || estado == ABRIENDO) {
                printf("Transici�n a ERROR\n");
                estado = ERROR;
            }
        }

        // Transiciones
        if (estado == ABRIENDO && LSA) {
            printf("Transici�n: ABRIENDO -> ABIERTO\n");
            estado = ABIERTO;
        } else if (estado == CERRANDO && LSC) {
            printf("Transici�n: CERRANDO -> CERRADO\n");
            estado = CERRADO;
        } else if (estado == ERROR) {
            printf("Transici�n: ERROR -> CERRADO\n");
            estado = CERRADO;
        } else if (estado == DETENIDA) {
            printf("Transici�n: DETENIDA -> PARPADEO\n");
            estado = PARPADEO;
            printf("Estado PARPADEO, enviando RST...\n");
            printf("Transici�n: PARPADEO -> CERRADO\n");
            estado = CERRADO;
        }
    }

    return 0;
}

/*DATO:*/
/*EN ESTA MAQUINA DE ESTADO PARA FINES DE SIMULACION SE INGRESAN DE MANERA MANUAL, LAS VARIABLES PARA PASAR DE ESTADO A ESTADO*/
