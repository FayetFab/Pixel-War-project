#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <unistd.h> /* pour close et sleep */
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h> /* pour memset */
#include <netinet/in.h> /* pour struct sockaddr_in */
#include <arpa/inet.h> /* pour htons et inet_aton */

#include <poll.h>
#define PORT IPPORT_USERRESERVED // = 5000
#define LG_MESSAGE 256

#define parametre_PORT "-p"
#define parametre_SIZE "-s"
#define parametre_LIMIT "-l"

#define commande_matrix "/getMatrix"
#define commande_size "/getSize"
#define commande_limits "/getLimits"
#define commande_version "/getVersion"
#define commande_WaitTime "/getWaitTime"

typedef struct Parametre{
	int PORT;
	int limite;
	int *dimensions;
}Parametre;



typedef struct User User;
struct User{
	int socketClient;
	struct sockaddr_in *sockin;
	User *suivant;
};

Parametre parametrage(int nbArgs, char *args[]){

	//si le parametre args est vide, le port et la matrice sont initialiser avec les valeurs suivantes :
	Parametre parametre;
	parametre.PORT = 5000;
	parametre.limite = 10;
	parametre.dimensions = (int*) calloc(2, sizeof(int));
	parametre.dimensions[0] = 80;
	parametre.dimensions[1] = 40;
	
	if(nbArgs > 5){
		printf("la ligne de commande contient trop de paramètres\n");
		exit(0);
	}
	for(int i=1; i < nbArgs; i++){
		//i commence a 1 car args[0] == ./serveur
		if( strcmp(args[i], parametre_PORT) == 0){
			i++;
			parametre.PORT = atoi(args[i]);
			//atoi permet de convertir des strings en integers
		}
		else if( strcmp(args[i], parametre_LIMIT) == 0){
			i++;
			parametre.limite = atoi(args[i]);
			//atoi permet de convertir des strings en integers
		}
		else if( strcmp(args[i], parametre_SIZE) == 0){
			char *dimensions_string[2] = {"", ""};
			i++;
			dimensions_string[0] = strtok(args[i], "x");
			dimensions_string[1] = strtok(NULL, "x");
			//strtok permet de separer une chaine de charactere selon un charactere donné
			for(int j=0; j<2; j++) parametre.dimensions[j] = atoi( dimensions_string[j] );
		}
	
	}
	
	return parametre;
}

void affiche_liste(User *liste){
	User *tmp = liste;
	if(tmp == NULL) printf("liste vide\n");
	while(tmp != NULL){
		printf(" socket : %d\n", tmp->socketClient);
		tmp = tmp->suivant;
	}
	printf("\n");
}

User *ajout_Client(User *liste, int socketEcoute, socklen_t *address_len){

	//affiche_liste(liste); //pour verifier 
	struct sockaddr_in pointDeRencontreDistant;

	User *client_suivant;
	client_suivant = malloc(sizeof(User));
	client_suivant->socketClient = accept(socketEcoute, (struct sockaddr *)&pointDeRencontreDistant, address_len);
	client_suivant->sockin = &pointDeRencontreDistant;
	//client_suivant->suivant = NULL;
	
	
	if(liste == NULL){
		liste = client_suivant;
	}
	else if(liste != NULL){
		User *parcour = liste;
		while(parcour->suivant != NULL){
			parcour = parcour->suivant;
		}
		parcour->suivant = client_suivant;
		parcour->suivant->suivant = NULL;
	}
	//affiche_liste(liste); //pour verifier	
	return liste;
}

User* supprime_Client(User *liste, int socket_Client){
	User *tmp = liste;
	if(tmp->socketClient == socket_Client){
		tmp = tmp->suivant;
		liste = tmp;
		return liste;
	
	}
	else{
		while(tmp->suivant != NULL){
			if(tmp->suivant->socketClient == socket_Client){
				User *rest = tmp->suivant->suivant;
				free(tmp->suivant);
				tmp->suivant = rest; 
				liste = tmp;	
				return liste;		
			}
			else tmp = tmp->suivant;
		}
		
	}
	printf("error : socket non trouvé...\n\n");
	return liste;
}

struct pollfd *supprime_poll(struct pollfd *liste, int index, int nbPoll){
	for(int i = index; i <= nbPoll-1; i++){
		liste[i] = liste[i+1];
	}
	return liste;
}

void reception_message(struct pollfd *poll_message, int *nbClient, User **liste){

	char messageEnvoi[LG_MESSAGE]; /* le message de la couche Application ! */
	char messageRecu[LG_MESSAGE]; /* le message de la couche Application ! */
	int ecrits, lus; /* nb d’octets ecrits et lus */

	memset(messageEnvoi, 0x00, LG_MESSAGE*sizeof(char));
	memset(messageRecu, 0x00, LG_MESSAGE*sizeof(char));
	
	
	
	for(int i=1; i<=*nbClient; i++){
		if( poll(&poll_message[i], 1, 100) == 1 ){
			lus = read(poll_message[i].fd, messageRecu, LG_MESSAGE*sizeof(char)); // ici appel bloquant
			switch(lus)
			{
				case -1 : // une erreur ! 
					perror("read");
					close(poll_message[i].fd);
					exit(-5);
				case 0 : // la socket est fermée 
					fprintf(stderr, "La socket a été fermée par le client !\n\n");
					*liste = supprime_Client(*liste, poll_message[i].fd);
					poll_message = supprime_poll(poll_message, i, *nbClient);
					*nbClient -= 1;
					break;
					//return 0;
				default: printf("Message reçu du client : %s (%d octets)\n\n", messageRecu, lus);
					break;
			}
			if(lus >0){
				// On envoie des données vers le client (cf. protocole)
				sprintf(messageEnvoi, "message reçu\n");
				ecrits = write(poll_message[i].fd, messageEnvoi, strlen(messageEnvoi));
				switch(ecrits)
				{
					case -1 : // une erreur ! 
						perror("write");
						close(poll_message[i].fd);
						
						exit(-6);
					case 0 : // la socket est fermée 
						fprintf(stderr, "La socket a été fermée par le client !\n\n");
						*liste = supprime_Client(*liste, poll_message[i].fd);
						poll_message = supprime_poll(poll_message, i, *nbClient);
						*nbClient -= 1;
						break;
						//return 0;
					default: // envoi de n octets 
						printf("Message envoyé au client : %s (%d octets)\n\n", messageEnvoi, ecrits);
				}			
			}
		}
	}
}

int main(int nbArgs, char *args[]){
	
	/*recupère les paramètres pour initialiser le serveur*/
	Parametre parametres;
	parametres = parametrage(nbArgs, args);
	/*--------------------------------------------------*/
	
	int socketEcoute;
	struct sockaddr_in pointDeRencontreLocal;
	socklen_t longueurAdresse;

	char messageEnvoi[LG_MESSAGE]; /* le message de la couche Application ! */
	char messageRecu[LG_MESSAGE]; /* le message de la couche Application ! */
	int ecrits, lus; /* nb d’octets ecrits et lus */


	// Crée un socket de communication
	socketEcoute = socket(PF_INET, SOCK_STREAM, 0); /* 0 indique que l’on utilisera le protocole par défaut associé à SOCK_STREAM soit TCP */
	// Teste la valeur renvoyée par l’appel système socket()
	if(socketEcoute < 0) /* échec ? */
	{
		perror("socket"); // Affiche le message d’erreur
		exit(-1); // On sort en indiquant un code erreur
	}
	printf("Socket créée avec succès ! (%d)\n", socketEcoute);
	
	
	// On prépare l’adresse d’attachement locale
	longueurAdresse = sizeof(struct sockaddr_in);
	memset(&pointDeRencontreLocal, 0x00, longueurAdresse);
	pointDeRencontreLocal.sin_family = PF_INET;
	pointDeRencontreLocal.sin_addr.s_addr = htonl(INADDR_ANY); // toutes les interfaces locales disponibles
	pointDeRencontreLocal.sin_port = htons(parametres.PORT);
	
	// On demande l’attachement local de la socket
	if((bind(socketEcoute, (struct sockaddr *)&pointDeRencontreLocal, longueurAdresse)) < 0)
	{
		perror("bind");
		exit(-2);
	}
	printf("Socket attachée avec succès !\n");
	// On fixe la taille de la file d’attente à 5 (pour les demandes de connexion non encore
	
	
	if(listen(socketEcoute, 5) < 0)
	{
		perror("listen");
		exit(-3);
	}
	printf("Socket placée en écoute passive ...\n");
	
	
	/*creation d'un pollfd pour l'acceuil de plusieur clients*, et d'un tableau de poll pour la reception des différents message*/
	struct pollfd poll_accept;
	memset(&poll_accept, 0, sizeof(poll_accept));
	poll_accept.fd = socketEcoute;
	poll_accept.events = POLLIN; 
	
	struct pollfd *poll_message = calloc(0, sizeof(struct pollfd));
	/*-------------------------------------------------------------------------------------------------------------------------*/
	
	
	memset(messageEnvoi, 0x00, LG_MESSAGE*sizeof(char));
	memset(messageRecu, 0x00, LG_MESSAGE*sizeof(char));
	printf("Attente d’une demande de connexion (quitter avec Ctrl-C)\n\n");
	
	printf("PORT : %d\n", parametres.PORT);
	printf("LARGEUR : %d\n", parametres.dimensions[0]);
	printf("HAUTEUR : %d\n", parametres.dimensions[1]);
	printf("LIMITE DE PIXELS : %d\n\n", parametres.limite);
	
	
	User *tete_liste = NULL;
	int nbClient = 0;
	int *pt_nbClient = &nbClient;
	while(1)
	{	
		/*----------------------------poll---------------------------------------------------------*/
		
		if( poll(&poll_accept, 1, 100) == 1){

			//socketDialogue = accept(socketEcoute, (struct sockaddr *)&pointDeRencontreDistant, &longueurAdresse);
			tete_liste = ajout_Client(tete_liste, socketEcoute, &longueurAdresse);
			User *parcour = tete_liste;
			while(parcour->suivant != NULL){
				parcour = parcour->suivant;
			}
			
			if (parcour->socketClient < 0)
			{
				perror("accept");
				close(parcour->socketClient);
				close(socketEcoute);
				exit(-4);
			}
			// On réceptionne les données du client et on affiche ses informations
			printf("Connection client de : %s:%d\n", inet_ntoa(parcour->sockin->sin_addr),ntohs(parcour->sockin->sin_port));
			
			nbClient +=1 ;
			poll_message[nbClient].fd = parcour->socketClient;
			poll_message[nbClient].events = POLLIN;
			
		}
		
		
		if(nbClient > 0){
			reception_message(poll_message, pt_nbClient, &tete_liste);	
		}

		/*-----------------------------------------------------------------------------------*/
	}
	close(socketEcoute);
	return 0;
}