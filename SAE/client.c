#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080
#define TAILLE_GRILLE 10

void erreur(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

void afficher_grille(char grille[TAILLE_GRILLE][TAILLE_GRILLE]) {
    printf("  A B C D E F G H I J\n");
    for (int i = 0; i < TAILLE_GRILLE; i++) {
        printf("%d ", i);
        for (int j = 0; j < TAILLE_GRILLE; j++) {
            printf("%c ", grille[i][j]);
        }
        printf("\n");
    }
}

void afficher_historique_tirs(char historique[][3], int nb_tirs) {
    printf("Historique des tirs :\n");
    for (int i = 0; i < nb_tirs; i++) {
        printf("%s\n", historique[i]);
    }
}

int verifier_tir_repetitif(char historique[][3], int nb_tirs, char *tir) {
    for (int i = 0; i < nb_tirs; i++) {
        if (strncmp(historique[i], tir, 2) == 0) {
            return 1;
        }
    }
    return 0;
}

int main() {
    const char *server_ip = "172.20.10.8";  
    int sockfd;
    struct sockaddr_in serv_addr;
    char buffer[512];
    char grille[TAILLE_GRILLE][TAILLE_GRILLE];
    char historique[100][3];
    int nb_tirs = 0;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        erreur("Erreur d'ouverture de la socket");

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, server_ip, &serv_addr.sin_addr) <= 0) {
        erreur("Adresse IP invalide ou non supportée");
    }

    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        erreur("Erreur de connexion");

    printf("Entrez votre pseudo: ");
    memset(buffer, 0, sizeof(buffer));
    fgets(buffer, sizeof(buffer), stdin);
    write(sockfd, buffer, strlen(buffer));

    for (int i = 0; i < TAILLE_GRILLE; i++) {
        for (int j = 0; j < TAILLE_GRILLE; j++) {
            grille[i][j] = '.';
        }
    }

    int jeu_en_cours = 1;
    while (jeu_en_cours) {
        memset(buffer, 0, sizeof(buffer));
        int lus = read(sockfd, buffer, sizeof(buffer) - 1);
        if (lus <= 0) {
            if (lus == 0) {
                printf("Connexion fermée par le serveur.\n");
            } else {
                erreur("Erreur de lecture du serveur");
            }
            break;
        }
        buffer[lus] = '\0'; 
        printf("Message reçu: %s\n", buffer);

        if (strncmp(buffer, "FIN", 3) == 0) {
            printf("Le jeu est terminé.\n");
            break;
        }

        if (strncmp(buffer, "PERDU", 5) == 0) {
            printf("Vous avez perdu!\n");
            jeu_en_cours = 0;
            break;
        }

        if (strncmp(buffer, "VOTRE TOUR", 10) == 0) {
            afficher_grille(grille);
            afficher_historique_tirs(historique, nb_tirs);
            while (1) {
                printf("Entrez vos coordonnées de tir (ex: A0): ");
                memset(buffer, 0, sizeof(buffer));
                fgets(buffer, sizeof(buffer), stdin);
                buffer[strcspn(buffer, "\n")] = '\0'; 

                if (verifier_tir_repetitif(historique, nb_tirs, buffer)) {
                    printf("Vous avez déjà tiré sur cette position, veuillez réessayer.\n");
                    continue;
                }

                if (strlen(buffer) != 2 || buffer[0] < 'A' || buffer[0] > 'J' || buffer[1] < '0' || buffer[1] > '9') {
                    printf("Coordonnées invalides, veuillez réessayer.\n");
                    continue;
                }

                write(sockfd, buffer, 2);
                strncpy(historique[nb_tirs++], buffer, 2);
                historique[nb_tirs - 1][2] = '\0';

                memset(buffer, 0, sizeof(buffer));
                lus = read(sockfd, buffer, sizeof(buffer) - 1);
                if (lus <= 0) {
                    if (lus == 0) {
                        printf("Connexion fermée par le serveur.\n");
                    } else {
                        erreur("Erreur de lecture du serveur");
                    }
                    break;
                }
                buffer[lus] = '\0'; 
                printf("Resultat du tir: %s\n", buffer);

                if (strncmp(buffer, "ENTREE INVALIDE", 15) == 0) {
                    printf("Coordonnées invalides, veuillez réessayer.\n");
                    continue;  
                }

                if (strncmp(buffer, "TOUCHE", 6) == 0) {
                    int ligne = buffer[7] - '0';
                    int colonne = buffer[8] - 'A';
                    grille[ligne][colonne] = 'X';
                    break; 
                } else if (strncmp(buffer, "A L'EAU", 7) == 0) {
                    int ligne = buffer[7] - '0';
                    int colonne = buffer[8] - 'A';
                    grille[ligne][colonne] = 'O';
                    break;  
                } else if (strncmp(buffer, "GAGNE", 5) == 0) {
                    printf("Vous avez gagné!\n");
                    jeu_en_cours = 0;
                    break;
                } else {
                    printf("%s\n", buffer);
                    break;
                }
            }
        }
    }

    close(sockfd);
    return 0;
}
