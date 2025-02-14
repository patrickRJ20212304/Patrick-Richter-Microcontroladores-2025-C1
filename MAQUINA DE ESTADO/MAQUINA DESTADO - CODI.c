#include <stdio.h>

// Definición de estados
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

    printf("\nSimulación de máquina de estados - Control de puerta\n");
    printf("Ingrese A para abrir, C para cerrar, PP para detener, RST para resetear.\n");

    for (;;) {
        printf("\nEstado actual: %d\n", estado);
        printf("Ingrese señal: ");
        scanf(" %c", &entrada);

        if (entrada == 'A') {
            if (estado == CERRADO) {
                printf("Transición: CERRADO -> ABRIENDO\n");
                estado = ABRIENDO;
                LSA = 1;
            }
        } else if (entrada == 'C') {
            if (estado == ABIERTO) {
                printf("Transición: ABIERTO -> CERRANDO\n");
                estado = CERRANDO;
                LSC = 1;
            }
        } else if (entrada == 'P') {  // PP señal
            if (estado == ABRIENDO) {
                printf("Transición: ABRIENDO -> DETENIDA\n");
                estado = DETENIDA;
            }
        } else if (entrada == 'R') {  // RST señal
            if (estado == CERRANDO || estado == ABRIENDO) {
                printf("Transición a ERROR\n");
                estado = ERROR;
            }
        }

        // Transiciones
        if (estado == ABRIENDO && LSA) {
            printf("Transición: ABRIENDO -> ABIERTO\n");
            estado = ABIERTO;
        } else if (estado == CERRANDO && LSC) {
            printf("Transición: CERRANDO -> CERRADO\n");
            estado = CERRADO;
        } else if (estado == ERROR) {
            printf("Transición: ERROR -> CERRADO\n");
            estado = CERRADO;
        } else if (estado == DETENIDA) {
            printf("Transición: DETENIDA -> PARPADEO\n");
            estado = PARPADEO;
            printf("Estado PARPADEO, enviando RST...\n");
            printf("Transición: PARPADEO -> CERRADO\n");
            estado = CERRADO;
        }
    }

    return 0;
}

/*DATO:*/
/*EN ESTA MAQUINA DE ESTADO PARA FINES DE SIMULACION SE INGRESAN DE MANERA MANUAL, LAS VARIABLES PARA PASAR DE ESTADO A ESTADO*/
