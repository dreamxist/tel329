#!/usr/bin/env python3
"""
Tiempos de respuesta CoAP a partir del Mote output de Cooja (a1-coap.log).

Cada linea del log es "<tiempo_us> n<id> <mensaje>". El cliente imprime
"TX id=<n> seq=<k> mid=<m> ..." al enviar y "RX id=<n> seq=<k> code=<c> mid=<m>
rtt_ms=<ms> 'ok hops=<h>'" al recibir la respuesta; el servidor imprime
"REG id=<n> seq=<k> temp=<t> hops=<h> ...". Se emparejan por (id, seq) y el RTT
es la diferencia de los tiempos simulados que Cooja antepone (precision de
microsegundos, se reporta en ms con tres decimales).

Uso: tiempos.py a1-coap.log [--csv salida.csv] [--resumen salida.md] [--pcap a.pcap]
"""
import argparse
import csv
import re
import statistics
import struct
import sys

LINEA = re.compile(r"^(\d+)\s+n(\d+)\s+(.*)$")
TX = re.compile(r"\] TX id=(\d+) seq=(\d+) mid=(\d+) len=(\d+)")
RX = re.compile(r"\] RX id=(\d+) seq=(\d+) code=(\S+) mid=(\d+) rtt_ms=(\d+) '(?:ok hops=(\d+))?")
TIMEOUT = re.compile(r"\] TIMEOUT id=(\d+) seq=(\d+)")
SKIP = re.compile(r"\] SKIP id=(\d+) seq=(\d+)")
REG = re.compile(r"\] REG id=(\d+) seq=(\d+) temp=(\S+) hops=(\d+) ttl=(\d+) mid=(\d+)")
RUTA = re.compile(r"\] TX id=")


def leer(path):
    tx, rx, reg = {}, {}, {}
    timeouts, skips = [], []
    primera_tx = {}
    with open(path, encoding="utf-8", errors="replace") as f:
        for raw in f:
            m = LINEA.match(raw.rstrip("\n"))
            if not m:
                continue
            t_us, nodo, msg = int(m.group(1)), int(m.group(2)), m.group(3)
            if (mm := TX.search(msg)):
                clave = (int(mm.group(1)), int(mm.group(2)))
                tx[clave] = (t_us, int(mm.group(3)), int(mm.group(4)))
                primera_tx.setdefault(nodo, t_us)
            elif (mm := RX.search(msg)):
                clave = (int(mm.group(1)), int(mm.group(2)))
                rx[clave] = (t_us, mm.group(3), int(mm.group(4)), int(mm.group(5)),
                             int(mm.group(6)) if mm.group(6) else None)
            elif (mm := REG.search(msg)):
                clave = (int(mm.group(1)), int(mm.group(2)))
                reg[clave] = (t_us, mm.group(3), int(mm.group(4)), int(mm.group(5)))
            elif (mm := TIMEOUT.search(msg)):
                timeouts.append((int(mm.group(1)), int(mm.group(2))))
            elif (mm := SKIP.search(msg)):
                skips.append((int(mm.group(1)), int(mm.group(2))))
    return tx, rx, reg, timeouts, skips, primera_tx


def filas(tx, rx, reg):
    out = []
    for clave in sorted(tx):
        t_tx, mid, largo = tx[clave]
        r = rx.get(clave)
        g = reg.get(clave)
        saltos = (r[4] if r and r[4] is not None else (g[2] if g else None))
        out.append({
            "cliente": clave[0],
            "seq": clave[1],
            "t_envio_ms": f"{t_tx / 1000:.3f}",
            "t_respuesta_ms": f"{r[0] / 1000:.3f}" if r else "",
            "rtt_ms": f"{(r[0] - t_tx) / 1000:.3f}" if r else "",
            "saltos": saltos if saltos is not None else "",
            "codigo": r[1] if r else "sin respuesta",
            "mid": mid,
            "payload_bytes": largo,
            "temp": g[1] if g else "",
            "t_registro_raiz_ms": f"{g[0] / 1000:.3f}" if g else "",
            "rtt_mote_ms": r[3] if r else "",
        })
    return out


def p95(valores):
    # Percentil 95 por rango mas cercano (nearest-rank): el valor que deja el
    # 95 % de la muestra por debajo o igual.
    v = sorted(valores)
    k = max(1, int(round(0.95 * len(v) + 0.5)))
    return v[min(k, len(v)) - 1]


def resumen(rows, timeouts, skips):
    grupos = {}
    for r in rows:
        if r["rtt_ms"] == "":
            continue
        grupos.setdefault(r["cliente"], []).append(float(r["rtt_ms"]))
    todos = [x for v in grupos.values() for x in v]

    def fila(nombre, v, saltos, n_timeout, n_skip):
        if not v:
            return f"| {nombre} | {saltos} | 0 | - | - | - | - | - | {n_timeout} | {n_skip} |"
        return (f"| {nombre} | {saltos} | {len(v)} | {statistics.mean(v):.1f} | "
                f"{statistics.median(v):.1f} | {p95(v):.1f} | {min(v):.1f} | {max(v):.1f} | "
                f"{n_timeout} | {n_skip} |")

    lineas = [
        "| cliente | saltos | n | media ms | mediana ms | p95 ms | min ms | max ms | timeouts | saltadas |",
        "|---|---|---|---|---|---|---|---|---|---|",
    ]
    for c in sorted(grupos):
        saltos_c = sorted({r["saltos"] for r in rows if r["cliente"] == c and r["saltos"] != ""})
        lineas.append(fila(f"n{c}", grupos[c], "/".join(str(s) for s in saltos_c) or "?",
                           sum(1 for t in timeouts if t[0] == c), sum(1 for s in skips if s[0] == c)))
    lineas.append(fila("**global**", todos, "1-2", len(timeouts), len(skips)))
    return "\n".join(lineas), grupos, todos


def resumen_pcap(path):
    with open(path, "rb") as f:
        cab = f.read(24)
        if len(cab) < 24:
            return None
        magic = struct.unpack(">I", cab[:4])[0]
        orden = ">" if magic == 0xA1B2C3D4 else "<"
        linktype = struct.unpack(orden + "I", cab[20:24])[0]
        n, bytes_, t0, t1 = 0, 0, None, None
        while True:
            h = f.read(16)
            if len(h) < 16:
                break
            s, us, incl, _ = struct.unpack(orden + "IIII", h)
            data = f.read(incl)
            if len(data) < incl:
                break
            n += 1
            bytes_ += incl
            t = s + us / 1e6
            t0 = t if t0 is None else t0
            t1 = t
        return {"tramas": n, "bytes": bytes_, "linktype": linktype,
                "desde_s": t0, "hasta_s": t1}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("log")
    ap.add_argument("--csv")
    ap.add_argument("--resumen")
    ap.add_argument("--pcap")
    args = ap.parse_args()

    tx, rx, reg, timeouts, skips, primera_tx = leer(args.log)
    rows = filas(tx, rx, reg)
    if not rows:
        print("no hay lineas TX en el log", file=sys.stderr)
        return 1

    if args.csv:
        with open(args.csv, "w", newline="") as f:
            w = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
            w.writeheader()
            w.writerows(rows)

    tabla, grupos, todos = resumen(rows, timeouts, skips)
    sin_resp = [r for r in rows if r["rtt_ms"] == ""]
    partes = [
        "# Tiempos de respuesta CoAP (Actividad 3, mitad CoAP)",
        "",
        f"Fuente: `{args.log.split('/')[-1]}` (Cooja headless, tiempo simulado). "
        f"{len(rows)} peticiones POST confirmables, {len(todos)} con respuesta 2.04, "
        f"{len(timeouts)} timeouts, {len(skips)} rondas saltadas por peticion en vuelo, "
        f"{len(sin_resp) - len(timeouts)} en vuelo al cortar.",
        "",
        "RTT = tiempo simulado de la linea RX menos el de la linea TX del mismo (cliente, seq).",
        "",
        tabla,
    ]
    if primera_tx:
        partes += ["", "Primer envio por cliente (ruta a la raiz disponible): " +
                   ", ".join(f"n{c} a los {t / 1e6:.1f} s" for c, t in sorted(primera_tx.items()))]
    if args.pcap:
        try:
            p = resumen_pcap(args.pcap)
        except OSError:
            p = None
        if p:
            partes += ["", f"pcap: {p['tramas']} tramas 802.15.4 ({p['bytes']} bytes de trama), "
                       f"linktype {p['linktype']}, de {p['desde_s']:.2f} s a {p['hasta_s']:.2f} s."]
    texto = "\n".join(partes) + "\n"
    if args.resumen:
        with open(args.resumen, "w") as f:
            f.write(texto)
    print(texto)
    return 0


if __name__ == "__main__":
    sys.exit(main())
