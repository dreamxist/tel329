/*
 * TEL329 — Tarea 1, Actividad 4: configuracion de TSCH.
 */
#ifndef PROJECT_CONF_H_
#define PROJECT_CONF_H_

/* Habilitar TSCH en la capa MAC. */
#define MAC_CONF_WITH_TSCH 1

/*
 * Apagar el horario 6TiSCH Minimal.
 * Sin esto, Contiki-NG crea solo un slotframe con UNA ranura compartida
 * (TX|RX|SHARED) y el schedule manual queda tapado: los nodos volverian a
 * competir por el medio y no se podria demostrar el determinismo.
 */
#define TSCH_SCHEDULE_CONF_WITH_6TISCH_MINIMAL 0

/* Reducir el tiempo de escaneo inicial para que la simulacion asocie rapido. */
#define TSCH_CONF_EB_PERIOD (CLOCK_SECOND / 2)
#define TSCH_CONF_MAX_EB_PERIOD (CLOCK_SECOND)

#endif /* PROJECT_CONF_H_ */
