import socket
import threading

# Fonction pour écouter les messages UDP
def udp_listener(host, port):
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as udp_socket:
            udp_socket.bind((host, port))
            print(f"UDP Listener démarré sur {host}:{port}", flush=True)
            
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

# Fonction pour écouter les messages TCP
def tcp_listener(sock):
    try:
        while True:
            data = sock.recv(1024)
            if data:
                print("Message reçu via TCP:", data.decode(), flush=True)
            else:
                break  # Sortir de la boucle si la connexion est fermée
    except socket.error as e:
        print("Erreur lors de la réception TCP :", e, flush=True)

def main():
    tcp_host = '127.0.0.1'
    tcp_port = 12347  # Port TCP/IPC
    udp_port = 23456  # Port UDP pour recevoir des messages de jeu

    # Démarrer le thread d'écoute UDP
    udp_thread = threading.Thread(target=udp_listener, args=(tcp_host, udp_port))
    udp_thread.daemon = True  # Permet de fermer le thread quand le programme principal se termine
    udp_thread.start()

    # Connexion TCP/IPC
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.connect((tcp_host, tcp_port))
            print("Connecté au serveur IPC.", flush=True)

            # Démarrer le thread d'écoute TCP
            tcp_thread = threading.Thread(target=tcp_listener, args=(s,))
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
    main()
