#!/usr/bin/env python3
"""Puente TCP minimo: escucha en 0.0.0.0:PORT (IPv4, publicable con docker -p)
y reenvia cada conexion al mote IPv6 del tun del contenedor. Sirve para que el
middleware corra en el host (Node 22) mientras el tun vive dentro del contenedor.
Uso: python3 tcp-forward.py 8080 fd00::302:304:506:708 80"""
import socket, sys, threading

listen_port, target_host, target_port = int(sys.argv[1]), sys.argv[2], int(sys.argv[3])

def pipe(src, dst):
    try:
        while (data := src.recv(4096)):
            dst.sendall(data)
    except OSError:
        pass
    finally:
        try: dst.shutdown(socket.SHUT_WR)
        except OSError: pass

def handle(conn):
    with conn, socket.create_connection((target_host, target_port), timeout=5) as remote:
        t = threading.Thread(target=pipe, args=(remote, conn), daemon=True); t.start()
        pipe(conn, remote); t.join()

srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
srv.bind(("0.0.0.0", listen_port)); srv.listen()
print(f"forward 0.0.0.0:{listen_port} -> [{target_host}]:{target_port}", flush=True)
while True:
    c, _ = srv.accept()
    threading.Thread(target=handle, args=(c,), daemon=True).start()
