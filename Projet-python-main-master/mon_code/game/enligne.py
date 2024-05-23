import socket
import threading
from game.game import *
from game.menu import *
from game.formulaire import *
from game.methods import *
from game.bob import *
class Game:
    def __init__(self, tcp_host, tcp_port, udp_port):
        self.tcp_host = tcp_host
        self.tcp_port = tcp_port
        self.udp_port = udp_port

    def udp_listener(self):
        try:
            with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as udp_socket:
                udp_socket.bind((self.tcp_host, self.udp_port))
                print(f"UDP Listener démarré sur {self.tcp_host}:{self.udp_port}", flush=True)
                
                while True:
                    try:
                        data, addr = udp_socket.recvfrom(1024)  # Recevoir des messages UDP
                        if data:
                            print(f"Message reçu via UDP de {addr}: {data.decode()}", flush=True)
                    except socket.error as e:
                        print("Erreur lors de la réception UDP :", e, flush=True)
                        break  # Sortir de la boucle si une erreur survient

        except Exception as e:
            print("Erreur lors de l'écoute UDP :", e, flush=True)

    def tcp_listener(self, sock):
        try:
            while True:
                data = sock.recv(1024)
                if data:
                    print("Message reçu via TCP:", data.decode(), flush=True)
                else:
                    break  # Sortir de la boucle si la connexion est fermée
        except socket.error as e:
            print("Erreur lors de la réception TCP :", e, flush=True)

    def start(self):
        # Démarrer le thread d'écoute UDP
        udp_thread = threading.Thread(target=self.udp_listener)
        udp_thread.daemon = True  # Permet de fermer le thread quand le programme principal se termine
        udp_thread.start()

        # Connexion TCP/IPC
        try:
            with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
                s.connect((self.tcp_host, self.tcp_port))
                print("Connecté au serveur IPC.", flush=True)

                # Démarrer le thread d'écoute TCP
                tcp_thread = threading.Thread(target=self.tcp_listener, args=(s,))
                tcp_thread.daemon = True  # Permet de fermer le thread quand le programme principal se termine
                tcp_thread.start()

                while True:
                    message = input("Entrez une commande à envoyer (ou 'quit' pour quitter) : ")
                    if message.lower() == 'quit':
                        break

                    try:
                        s.sendall(message.encode())  # Envoyer le message
                    except socket.error as e:
                        print("Erreur lors de l'envoi TCP :", e, flush=True)
                        break  # Quitter la boucle en cas d'erreur

        except Exception as e:
            print("Une erreur est survenue :", e, flush=True)

if __name__ == '__main__':
    game = Game('127.0.0.1', 12347, 23456)
    game.start()