import socket

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
s.bind(("localhost", 5001))
s.listen(1)

while True:
    conn, addr = s.accept()
    while 1:
        data = conn.recv(4)
        if data == b"ping":
            conn.send(b"pong")
        elif data == b"exit" or data == b"":
            break
        else:
            print("unknown:", data)
            break
    conn.close()
