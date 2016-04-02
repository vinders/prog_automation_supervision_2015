/* 
VINDERS Romain
Interface de contrôle d'unité locale
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stddef.h>
#include <stdint.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <termios.h>
#include <time.h>
#include <signal.h>
#include <sched.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

// constantes serveur
#define IP_HOST "10.59.40.64"
#define PORT 1024

// constantes de modification de couleurs
#define CLEAR_SCN "\033[2J"
#define COLOR_W "\e[0;37m"
#define COLOR_R "\e[0;31m"
#define COLOR_G "\e[0;32m"
#define changeStateColor(etat) if(etat){printf(COLOR_G);}else{printf(COLOR_R);}

// structures de données - bits actuateurs et capteurs PETRA
struct actuators_bits_t
{
    unsigned CP : 2;
    unsigned C1 : 1;
    unsigned C2 : 1;
    unsigned PV : 1;
    unsigned PA : 1;
    unsigned AA : 1;
    unsigned GA : 1;
};
struct sensors_bits_t
{
    unsigned L1 : 1;
    unsigned L2 : 1;
    unsigned T  : 1; /* cablé H */
    unsigned S  : 1;
    unsigned CS : 1;
    unsigned AP : 1;
    unsigned PP : 1;
    unsigned DE : 1;
};

// unions bytes et structures de bits (actuateurs/capteurs)
union
{
    struct sensors_bits_t bits;
    unsigned char byte;
} sensors_u;
union
{
    struct actuators_bits_t bits;
    unsigned char byte;
} actuators_u;

// prototypes
int startServer(char*, int);
int waitClient();
void processClient();
void* threadClientListener(void*);
int sendData();
int receiveData();
void displayUnitStatus();
int sleepTime(int, int);
int setSignal(int, void (*fct));
void handlerStop(int);

// variables globales
pthread_mutex_t mutexUnit;
pthread_t listeningThread;
int clientContinue = 0;
int fdUnit = -1;
int socketServer = -1;
int socketClient = -1;

/*----------------------------------------------------------------------------*/

int main()
{
    // initialisation mutex
    if (pthread_mutex_init(&mutexUnit, NULL) )
    {
        perror("MAIN : erreur mutex init"); exit(1);
    }
    
    // accès au dispositif contrôlé en lecture/écriture
    fdUnit = open("/dev/vinders_pci", O_RDWR);
    if (fdUnit == -1)
    {
        perror("MAIN : erreur ouverture driver"); exit(1);
    }
    printf("MAIN: ouverture driver OK\n");
    
    // armer signal de fermeture
    if (setSignal(SIGINT, handlerStop) == -1)
    {
        perror("MAIN: erreur armement signal SIGINT"); exit(1);
    }
    printf("MAIN: signal de fermeture OK\n");
    
    // ouverture socket serveur
    if (startServer(IP_HOST, PORT) == -1)
        exit(1);
    printf("MAIN: ouverture socket serveur OK\n");
    
    // exécution principale serveur
    while (1)
    {
        printf("MAIN: attente client...\n");
        if (waitClient() == -1)
            perror("MAIN: erreur connexion client");
        else
            processClient();
        printf("MAIN: fin de connexion\n");
    }
    return 0;
}

/* CREATION SOCKET SERVEUR--------------------------------------------------- */
int startServer(char* ip, int port)
{
    // création socket serveur
    struct sockaddr_in socketAddress;
    if ((socketServer = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("STARTSERVER: erreur creation socket"); return -1;
    }
    // configuration serveur TCP
    memset(&socketAddress, 0, sizeof(struct sockaddr_in));
    socketAddress.sin_family = AF_INET; // domaine
    socketAddress.sin_port   = htons(port);
    socketAddress.sin_addr.s_addr = inet_addr(ip);
    if (bind(socketServer, (struct sockaddr *)&socketAddress, sizeof(struct sockaddr_in)) == -1)
    {
        perror("STARTSERVER: erreur bind socket"); return -1;
    }
    return 0;
}

/* ATTENTE CONNEXION CLIENT ------------------------------------------------- */
int waitClient()
{
    struct sockaddr_in remoteAddress;
    socklen_t remoteAddressLength = sizeof(remoteAddress);

    // attendre connexion client
    listen(socketServer, SOMAXCONN);
    memset((void*)&remoteAddress, 0, sizeof(remoteAddress));
    socketClient = accept(socketServer, (struct sockaddr *)&remoteAddress, &remoteAddressLength);
    if (socketClient == 1)
        return -1;
    return 0;
}

/* PRISE EN CHARGE DU CLIENT ------------------------------------------------ */
void processClient()
{
    unsigned char prevSensorsByte = 255;
    sensors_u.byte = 0x00;
    actuators_u.byte = 0x00;
    
    // lancement thread
    if (pthread_create(&listeningThread, NULL, threadClientListener, NULL) == -1)
    {
        perror("PROCESSCLIENT: erreur de pthread_create");
        return;
    }
    pthread_detach(listeningThread);
    
    // boucle de supervision
    clientContinue = 1;
    while (clientContinue == 1)
    {
        // actualiser état des capteurs
        pthread_mutex_lock(&mutexUnit);
        read(fdUnit, &sensors_u.byte, 1);
        if (prevSensorsByte != sensors_u.byte)
        {
            prevSensorsByte = sensors_u.byte;
            pthread_mutex_unlock(&mutexUnit);
            if (sendData() == -1)
                clientContinue = 0;
            displayUnitStatus();
        }
        else
        {
            pthread_mutex_unlock(&mutexUnit);
        }
        sleepTime(0, 50000000);
    }
}

/* THREAD D'ECOUTE DU CLIENT ------------------------------------------------ */
void* threadClientListener(void* arg)
{
    int ret;
    int prevActuatorsByte;
    while (clientContinue == 1)
    {
        // demande de mise à jour des actuateurs
        prevActuatorsByte = actuators_u.byte;
        if ((ret = receiveData()) == -1)
            clientContinue = -1;
        
        // écriture
        if (ret > 0)
        {
            pthread_mutex_lock(&mutexUnit);
            if (prevActuatorsByte != actuators_u.byte)
            {
                write(fdUnit, &actuators_u.byte, 1);
                pthread_mutex_unlock(&mutexUnit);
                displayUnitStatus();
            }
            else
            {
                pthread_mutex_unlock(&mutexUnit);
            }
        }
    }
    pthread_exit(0);
    return NULL;
}

/* AFFICHAGE CAPTEURS/ACTUATEURS -------------------------------------------- */
void displayUnitStatus()
{
    // affichage d'état - capteurs
    printf(CLEAR_SCN);
    printf("CAPTEURS :\r\n");
    changeStateColor(sensors_u.bits.L1);
    printf("L1: %d  ", sensors_u.bits.L1);
    changeStateColor(sensors_u.bits.L2);
    printf("L2: %d  ", sensors_u.bits.L2);
    changeStateColor(sensors_u.bits.T);
    printf("T: %d  ", sensors_u.bits.T);
    changeStateColor(sensors_u.bits.S);
    printf("S: %d\r\n", sensors_u.bits.S);
    changeStateColor(sensors_u.bits.CS);
    printf("CS: %d  ", sensors_u.bits.CS);
    changeStateColor(sensors_u.bits.AP);
    printf("AP: %d  ", sensors_u.bits.AP);
    changeStateColor(sensors_u.bits.PP);
    printf("PP: %d  ", sensors_u.bits.PP);
    changeStateColor(sensors_u.bits.DE);
    printf("DE: %d\r\n\n", sensors_u.bits.DE);

    // affichage d'état - actuateurs
    printf(COLOR_W);
    printf("ACTUATEURS :\r\n");
    printf("CP: %d\r\n", actuators_u.bits.CP);
    changeStateColor(actuators_u.bits.C1);
    printf("C1: %d  ", actuators_u.bits.C1);
    changeStateColor(actuators_u.bits.C2);
    printf("C2: %d\r\n", actuators_u.bits.C2);
    changeStateColor(actuators_u.bits.PA);
    printf("PA: %d  ", actuators_u.bits.PA);
    changeStateColor(actuators_u.bits.PV);
    printf("PV: %d\r\n", actuators_u.bits.PV);
    changeStateColor(actuators_u.bits.AA);
    printf("AA: %d  ", actuators_u.bits.AA);
    changeStateColor(actuators_u.bits.GA);
    printf("GA: %d\r\n", actuators_u.bits.GA);
    printf(COLOR_W);
}

/* ENVOI DE DONNEES --------------------------------------------------------- */
int sendData()
{
    // envoi de message
    int bytesSent;
    char data[] = "\0\1\1\0";
    data[0] = sensors_u.byte;
    bytesSent = send(socketClient, data, 4, 0);
    return bytesSent;
}


/* RECEPTION DE DONNEES ----------------------------------------------------- */
int receiveData()
{
    int bytesReceived;
    char* data = NULL;
    
    // obtenir taille MTU
    int mtuSize;
    int mtuSizeLength = sizeof(int);
    if (getsockopt(socketClient, IPPROTO_TCP, TCP_MAXSEG, &mtuSize, &mtuSizeLength) == -1)
        mtuSize = 4096;
    
    // lecture du message
    data = (char*)malloc(mtuSize + 1);
    if ((bytesReceived = recv(socketClient, data, mtuSize, 0)) > -1)
    {
        if (bytesReceived)
            actuators_u.byte = data[0];
        return bytesReceived;
    }
    return -1;
}

/* TEMPORISATION ------------------------------------------------------------ */
int sleepTime(int sec, int nsec)
{
    struct timespec sleepReq, sleepRem;
    sleepReq.tv_sec = sec;
    sleepReq.tv_nsec = nsec;
    return (nanosleep(&sleepReq, &sleepRem));
}

/* ARMEMENT SIGNAL ---------------------------------------------------------- */
int setSignal(int sig, void (*fct))
{
    // utiliser arguments comme paramètres
    struct sigaction sigact;
    sigact.sa_handler = fct;
    if (sig == SIGINT)
        sigfillset(&sigact.sa_mask); //signal d'arrêt non interruptible
    else
        sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = 0;

    // armement signal
    return sigaction(sig, &sigact, NULL);
}

/* HANDLERS SIGNAUX --------------------------------------------------------- */
void handlerStop(int sig)
{
    if (clientContinue == 1)
        pthread_cancel(listeningThread);
    if (socketClient > -1)
        close(socketClient);
    if (socketServer > -1)
        close(socketServer);
    if (fdUnit > -1)
        close(fdUnit);
    exit(0);
}

