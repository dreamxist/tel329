#!/usr/bin/env python3
"""
Tiempos de respuesta MQTT (Actividad 3) a partir del Mote output de Cooja.

Uso: analizar.py a2-mqtt.log a2-tiempos-mqtt.csv a2-resumen-mqtt.md [a2-mqtt-pcap.txt]

Lee lineas "<tiempo_us> n<id> A2,PUB,<id>,<seq>,<mid>,<t_ms>,<rank>,<padre>" y
"<tiempo_us> n<id> A2,PUBACK,<id>,<mid>,<t_ms>", las empareja por (id, mid) y
deja una fila por publicacion con el RTT en ms de reloj SIMULADO (el tiempo
del Mote output, no el del mote: tiene resolucion de microsegundos).

Los saltos salen de la cadena de padres RPL que cada cliente reporta en su
PUB: padre 1 es la raiz (1 salto); si no, 1 + saltos del padre al momento de
la publicacion. Si viene el texto del pcap (tcpdump -v), se cuenta ademas
cuantos paquetes TCP llegaron a tun0 y con que hop limit, como contraste.
"""
import csv
import re
import statistics
import sys
from collections import defaultdict

RAIZ = 1


def percentil(xs, p):
    xs = sorted(xs)
    if not xs:
        return float("nan")
    k = (len(xs) - 1) * p
    f, c = int(k), min(int(k) + 1, len(xs) - 1)
    return xs[f] + (xs[c] - xs[f]) * (k - f)


def resumen(xs):
    if not xs:
        return dict(n=0, media="", mediana="", p95="", minimo="", maximo="")
    return dict(n=len(xs), media=round(statistics.mean(xs), 1),
                mediana=round(statistics.median(xs), 1),
                p95=round(percentil(xs, 0.95), 1),
                minimo=round(min(xs), 1), maximo=round(max(xs), 1))


def main():
    log, csv_out, md_out = sys.argv[1:4]
    pcap_txt = sys.argv[4] if len(sys.argv) > 4 else None

    pubs = []                      # (t_us, id, seq, mid, rank, padre)
    acks = defaultdict(list)       # (id, mid) -> [t_us]
    padres = defaultdict(list)     # id -> [(t_us, padre)]
    eventos = defaultdict(list)    # id -> [(t_us, evento)]
    t_fin = 0
    pat = re.compile(r"^(\d+)\s+n(\d+)\s+A2,(\w+),(.*)$")
    for linea in open(log, errors="replace"):
        m = pat.match(linea.strip())
        if not m:
            continue
        t_us, nodo, ev, resto = int(m[1]), int(m[2]), m[3], m[4].split(",")
        t_fin = max(t_fin, t_us)
        if ev == "PUB":
            nid, seq, mid, _t_ms, rank, padre = map(int, resto[:6])
            pubs.append((t_us, nid, seq, mid, rank, padre))
            padres[nid].append((t_us, padre))
        elif ev == "PUBACK":
            nid, mid = int(resto[0]), int(resto[1])
            acks[(nid, mid)].append(t_us)
        else:
            eventos[nodo].append((t_us, ev))

    def padre_en(nid, t_us):
        p = 0
        for t, pa in padres.get(nid, []):
            if t <= t_us:
                p = pa
        return p

    def saltos(nid, t_us, vistos=()):
        if nid == RAIZ:
            return 0
        p = padre_en(nid, t_us)
        if p == 0 or p in vistos:
            return -1
        s = saltos(p, t_us, vistos + (nid,))
        return -1 if s < 0 else 1 + s

    filas = []
    for t_pub, nid, seq, mid, rank, padre in pubs:
        candidatos = [t for t in acks.get((nid, mid), []) if t >= t_pub]
        t_ack = min(candidatos) if candidatos else None
        filas.append(dict(
            cliente=nid, seq=seq, mid=mid,
            t_publish_ms=round(t_pub / 1000, 3),
            t_puback_ms=round(t_ack / 1000, 3) if t_ack else "",
            rtt_ms=round((t_ack - t_pub) / 1000, 3) if t_ack else "",
            saltos=saltos(nid, t_pub), rank_dag=rank, padre=padre))

    with open(csv_out, "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(filas[0].keys()) if filas else
                           ["cliente", "seq", "mid", "t_publish_ms", "t_puback_ms",
                            "rtt_ms", "saltos", "rank_dag", "padre"])
        w.writeheader()
        w.writerows(filas)

    # --- resumen ---
    por_cliente = defaultdict(list)
    por_saltos = defaultdict(list)
    sin_ack = defaultdict(int)
    for r in filas:
        if r["rtt_ms"] != "":
            por_cliente[r["cliente"]].append(r["rtt_ms"])
            por_saltos[r["saltos"]].append(r["rtt_ms"])
        else:
            sin_ack[r["cliente"]] += 1
    todos = [x for xs in por_cliente.values() for x in xs]

    out = []
    out.append(f"Fuente: `{log.split('/')[-1]}` ({t_fin / 1e6:.0f} s simulados, "
               f"{len(pubs)} PUBLISH QoS 1, {sum(len(v) for v in acks.values())} PUBACK).\n")
    out.append("| cliente | saltos | n | media (ms) | mediana (ms) | p95 (ms) | min (ms) | max (ms) | sin PUBACK |")
    out.append("|---|---|---|---|---|---|---|---|---|")
    for nid in sorted(por_cliente):
        s = resumen(por_cliente[nid])
        hs = sorted({r["saltos"] for r in filas if r["cliente"] == nid and r["rtt_ms"] != ""})
        out.append(f"| {nid} | {'/'.join(map(str, hs))} | {s['n']} | {s['media']} | {s['mediana']} | "
                   f"{s['p95']} | {s['minimo']} | {s['maximo']} | {sin_ack.get(nid, 0)} |")
    s = resumen(todos)
    out.append(f"| **global** | | {s['n']} | {s['media']} | {s['mediana']} | {s['p95']} | "
               f"{s['minimo']} | {s['maximo']} | {sum(sin_ack.values())} |")
    out.append("")
    out.append("| saltos | n | media (ms) | mediana (ms) | p95 (ms) | min (ms) | max (ms) |")
    out.append("|---|---|---|---|---|---|---|")
    for h in sorted(por_saltos):
        s = resumen(por_saltos[h])
        out.append(f"| {h} | {s['n']} | {s['media']} | {s['mediana']} | {s['p95']} | {s['minimo']} | {s['maximo']} |")
    out.append("")

    # primer CONNECTED por cliente y desconexiones
    out.append("| cliente | START (s) | primer CONNECTED (s) | primer PUB (s) | desconexiones |")
    out.append("|---|---|---|---|---|")
    for nid in sorted(set(list(eventos) + list(por_cliente))):
        evs = eventos.get(nid, [])
        start = next((t for t, e in evs if e == "START"), None)
        conn = next((t for t, e in evs if e == "CONNECTED"), None)
        disc = sum(1 for _, e in evs if e == "DISCONNECTED")
        pub1 = next((t for t, n, *_ in pubs if n == nid), None)
        fmt = lambda t: f"{t / 1e6:.1f}" if t is not None else "-"
        out.append(f"| {nid} | {fmt(start)} | {fmt(conn)} | {fmt(pub1)} | {disc} |")
    out.append("")

    if pcap_txt:
        try:
            txt = open(pcap_txt, errors="replace").read()
            hl = re.findall(r"hlim (\d+).*?\n?\s*(fd00::[0-9a-f:]+)\.(\d+) > (fd00::[0-9a-f:]+)\.(\d+)", txt)
            paquetes = len(re.findall(r"^\d+\.\d+ IP6", txt, re.M))
            hacia_broker = defaultdict(list)
            for h, src, sport, dst, dport in hl:
                if dport == "1883":
                    hacia_broker[src].append(int(h))
            out.append(f"Captura en tun0 (`{pcap_txt.split('/')[-1]}`): {paquetes} paquetes TCP al/desde :1883.")
            out.append("")
            out.append("| origen (mote) | paquetes hacia el broker | hop limit visto en tun0 | saltos = 64 - hlim |")
            out.append("|---|---|---|---|")
            for src in sorted(hacia_broker, key=lambda s: int(s.split(":")[-1], 16)):
                hs = sorted(set(hacia_broker[src]))
                out.append(f"| {src} | {len(hacia_broker[src])} | {'/'.join(map(str, hs))} | "
                           f"{'/'.join(str(64 - h) for h in hs)} |")
            out.append("")
        except FileNotFoundError:
            out.append(f"(sin pcap: {pcap_txt} no existe)\n")

    texto = "\n".join(out)
    open(md_out, "w").write(texto)
    print(texto)


if __name__ == "__main__":
    main()
