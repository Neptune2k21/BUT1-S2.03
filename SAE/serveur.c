#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define PORT 8080
#define TAILLE_GRILLE 10
#define MAX_CLIENTS 2

typedef struct {
    int sockfd;
    char grille[TAILLE_GRILLE][TAILLE_GRILLE];
    int bateaux_restants;
} DonneesClient;

DonneesClient *clients[MAX_CLIENTS];
int clients_connectes = 0;
int tour = 0;
pthread_mutex_t verrou = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t condition = PTHREAD_COND_INITIALIZER;

void erreur(const char *msg) {
    perror(msg);
    exit(1);
}

void init_grille(char grille[TAILLE_GRILLE][TAILLE_GRILLE], int *bateaux_restants) {
    for (int i = 0; i < TAILLE_GRILLE; i++) {
        for (int j = 0; j < TAILLE_GRILLE; j++) {
            grille[i][j] = '.';
        }
    }
    // Positionner les bateaux
    grille[1][1] = 'B';
    grille[2][2] = 'B';
    grille[3][3] = 'B';
    *bateaux_restants = 3;
}

void afficher_grille(char grille[TAILLE_GRILLE][TAILLE_GRILLE], char *buffer) {
    strcpy(buffer, "  A B C D E F G H I J \n");
    for (int i = 0; i < TAILLE_GRILLE; i++) {
        char ligne[32];
        snprintf(ligne, sizeof(ligne), "%d ", i);
        strcat(buffer, ligne);
        for (int j = 0; j < TAILLE_GRILLE; j++) {
            snprintf(ligne, sizeof(ligne), "%c ", grille[i][j]);
            strcat(buffer, ligne);
        }
        strcat(buffer, "\n");
    }
}

void *gerer_client(void *arg) {
    int id_joueur = *(int *)arg;
    int sockfd = clients[id_joueur]->sockfd;
    char (*grille)[TAILLE_GRILLE] = clients[id_joueur]->grille;
    int *bateaux_restants = &clients[id_joueur]->bateaux_restants;
    char buffer[512];
    char temp_buffer[1024]; 

    pthread_mutex_lock(&verrou);
    while (clients_connectes < MAX_CLIENTS) {
        pthread_cond_wait(&condition, &verrou);
    }
    pthread_mutex_unlock(&verrou);

    write(sockfd, "Tous les joueurs sont connectes. La partie commence !\n", 54);
    printf("Joueur %d informe que la partie commence.\n", id_joueur);

    while (1) {
        pthread_mutex_lock(&verrou);
        if (tour != id_joueur) {
            pthread_mutex_unlock(&verrou);
            usleep(100000);
            continue;
        }
        pthread_mutex_unlock(&verrou);

        write(sockfd, "VOTRE TOUR\n", 11);
        printf("C'est le tour du joueur %d.\n", id_joueur);

        memset(buffer, 0, sizeof(buffer));
        if (read(sockfd, buffer, sizeof(buffer) - 1) <= 0) {
            perror("Erreur de lecture du socket");
            break;
        }
        printf("Joueur %d a tiré sur %s.\n", id_joueur, buffer);

        // Vérifiez que les coordonnées sont valides
        if (strlen(buffer) != 2) {
            write(sockfd, "ENTREE INVALIDE\n", 16);
            continue;
        }

        char colonne_char = buffer[0];
        char ligne_char = buffer[1];

        if (colonne_char < 'A' || colonne_char > 'J' || ligne_char < '0' || ligne_char > '9') {
            write(sockfd, "ENTREE INVALIDE\n", 16);
            continue;
        }

        int colonne = colonne_char - 'A';
        int ligne = ligne_char - '0';

        if (grille[ligne][colonne] == 'X' || grille[ligne][colonne] == 'O') {
            write(sockfd, "COORDONNEES DEJA UTILISEES\n", 27);
            continue;
        }

        if (grille[ligne][colonne] == 'B') {
            grille[ligne][colonne] = 'X';
            (*bateaux_restants)--;
            write(sockfd, "TOUCHE\n", 7);
        } else if (grille[ligne][colonne] == '.') {
            grille[ligne][colonne] = 'O';
            write(sockfd, "A L'EAU\n", 7);
        }

        char grille_buffer[512];
        afficher_grille(grille, grille_buffer);
        snprintf(temp_buffer, sizeof(temp_buffer), "Bateaux restants: %d\n%s\n", *bateaux_restants, grille_buffer);

        if (strlen(temp_buffer) >= sizeof(buffer)) {
            strncpy(buffer, temp_buffer, sizeof(buffer) - 1);
            buffer[sizeof(buffer) - 1] = '\0'; 
        } else {
            strcpy(buffer, temp_buffer);
        }

        write(sockfd, buffer, strlen(buffer));

        pthread_mutex_lock(&verrou);
        tour = (tour + 1) % MAX_CLIENTS;
        pthread_mutex_unlock(&verrou);

        if (*bateaux_restants == 0) {
            write(sockfd, "GAGNE\n", 6);
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (i != id_joueur) {
                    write(clients[i]->sockfd, "PERDU\n", 6);
                }
            }
            break;
        }
    }

    write(sockfd, "FIN\n", 4); 
    close(sockfd);
    free(clients[id_joueur]);
    pthread_exit(NULL);
}


int main() {
    int sockfd, newsockfd;
    socklen_t clilen;
    struct sockaddr_in serv_addr, cli_addr;
    pthread_t threads[MAX_CLIENTS];
    int ids[MAX_CLIENTS];

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        erreur("Erreur d'ouverture de la socket");

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    serv_addr.sin_port = htons(PORT);

    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
        erreur("Erreur de liaison");

    if (listen(sockfd, 5) < 0)
        erreur("Erreur d'écoute");

    clilen = sizeof(cli_addr);

    while (clients_connectes < MAX_CLIENTS) {
        newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
        if (newsockfd < 0)
            erreur("Erreur d'acceptation");

        DonneesClient *donnees_client = malloc(sizeof(DonneesClient));
        if (!donnees_client)
            erreur("Erreur d'allocation mémoire");

        donnees_client->sockfd = newsockfd;
        init_grille(donnees_client->grille, &donnees_client->bateaux_restants);

        pthread_mutex_lock(&verrou);
        clients[clients_connectes] = donnees_client;
        ids[clients_connectes] = clients_connectes;
        clients_connectes++;
        printf("Client connecté. Nombre de clients : %d\n", clients_connectes);

        if (clients_connectes == MAX_CLIENTS) {
            printf("Tous les joueurs sont connectés.\n");
            pthread_cond_broadcast(&condition);
        }
        pthread_mutex_unlock(&verrou);

        if (pthread_create(&threads[clients_connectes - 1], NULL, gerer_client, &ids[clients_connectes - 1]) != 0)
            erreur("Erreur de création de thread");
    }

    for (int i = 0; i < MAX_CLIENTS; i++) {
        pthread_join(threads[i], NULL);
    }

    close(sockfd);
    return 0;
}
