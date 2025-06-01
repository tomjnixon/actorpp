import socket
import os


def parse_args():
    import argparse

    parser = argparse.ArgumentParser()
    parser.add_argument("--pid-file")
    parser.add_argument("--fork", action="store_true")
    return parser.parse_args()


def main(args):
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind(("localhost", 5001))
    s.listen(1)

    if args.fork:
        if os.fork() != 0:
            return

    if args.pid_file is not None:
        with open(args.pid_file, "w") as f:
            f.write(f"{os.getpid()}\n")

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


if __name__ == "__main__":
    main(parse_args())
