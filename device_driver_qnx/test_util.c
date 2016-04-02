/* ************************************************************
PETRA UTLITAIRE DE TEST
ACTIVATION D’ACTUATEURS ET LECTURE DE CAPTEURS DU PETRA
************************************************************ */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <termios.h>

#define CLEAR_SCN "\033[2J"
#define COLOR_W "\e[0;37m"
#define COLOR_R "\e[0;31m"
#define COLOR_G "\e[0;32m"
#define changeStateColor(etat) if(etat){printf(COLOR_G);}else{printf(COLOR_R);}

/* structures de données - bits actuateurs et capteurs */
struct  ACTUATEURS
{
    unsigned CP : 2;
    unsigned C1 : 1;
    unsigned C2 : 1;
    unsigned PV : 1;
    unsigned PA : 1;
    unsigned AA : 1;
    unsigned GA : 1;
};
struct  CAPTEURS
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

/* unions bytes et structure de bits (actuateurs/capteurs) */
union
{
    struct CAPTEURS capt ;
    unsigned char byte ;
} u_capt;
union
{
    struct ACTUATEURS act ;
    unsigned char byte ;
} u_act;

/*----------------------------------------------------------------------------*/

int main()
{
    int fd_petra;
    int continuer = 1;
    char choix = 0;
    u_act.byte = 0x00 ;

    /* accès en lecture/écriture */
    fd_petra = open ("/dev/vinders_pci", O_RDWR);
    if (fd_petra == -1)
    {
        perror("MAIN : Erreur ouverture driver");
        exit(1);
    }
    else
        printf("MAIN: driver ouvert\n");

    /* boucle d'interaction et affichage (IHM) */
    fflush(stdin);
    while (continuer)
    {
        /* saisie utilisateur */
        if (tcischars(0) > 0)
        {
            choix = getchar();
            fflush(stdin);
            switch (choix)
            {
                case '0':
                case '1':
                case '2':
                case '3':
                    u_act.act.CP = choix - '0';
                    break;
                case 'a':
                    u_act.act.C1 ^= 1;
                    break;
                case 'z':
                    u_act.act.C2 ^= 1;
                    break;
                case 'e':
                    u_act.act.PA ^= 1;
                    break;
                case 'r':
                    u_act.act.PV ^= 1;
                    break;
                case 't':
                    u_act.act.AA ^= 1;
                    break;
                case 'y':
                    u_act.act.GA ^= 1;
                    break;
                case 'q':
                case 'Q': /* quitter */
                    continuer = 0;
                    break;
            }
            choix = 0;
        }
        write(fd_petra, &u_act.byte, 1); /* mettre à jour actuateurs */
        read(fd_petra, &u_capt.byte, 1); /* actualiser état capteurs */

        /* affichage d'état - capteurs */
        printf(CLEAR_SCN);
        printf("CAPTEURS :\r\n");
        changeStateColor(u_capt.capt.L1);
        printf("L1: %d  ", u_capt.capt.L1);
        changeStateColor(u_capt.capt.L2);
        printf("L2: %d  ", u_capt.capt.L2);
        changeStateColor(u_capt.capt.T);
        printf("T: %d  ", u_capt.capt.T);
        changeStateColor(u_capt.capt.S);
        printf("S: %d\r\n", u_capt.capt.S);
        changeStateColor(u_capt.capt.CS);
        printf("CS: %d  ", u_capt.capt.CS);
        changeStateColor(u_capt.capt.AP);
        printf("AP: %d  ", u_capt.capt.AP);
        changeStateColor(u_capt.capt.PP);
        printf("PP: %d  ", u_capt.capt.PP);
        changeStateColor(u_capt.capt.DE);
        printf("DE: %d\r\n\n", u_capt.capt.DE);

        /* affichage d'état - actuateurs */
        printf(COLOR_W);
        printf("ACTUATEURS :\r\n");
        printf("CP (0 - 3): %d\r\n", u_act.act.CP);
        changeStateColor(u_act.act.C1);
        printf("C1 (a): %d  ", u_act.act.C1);
        changeStateColor(u_act.act.C2);
        printf("C2 (z): %d\r\n", u_act.act.C2);
        changeStateColor(u_act.act.PA);
        printf("PA (e): %d  ", u_act.act.PA);
        changeStateColor(u_act.act.PV);
        printf("PV (r): %d\r\n", u_act.act.PV);
        changeStateColor(u_act.act.AA);
        printf("AA (t): %d  ", u_act.act.AA);
        changeStateColor(u_act.act.GA);
        printf("GA (y): %d\r\n\n", u_act.act.GA);
        printf(COLOR_W);
        printf("Quitter (q)\n");
        sleep(1);
    }

    /* retour état initial + fermeture */
    u_act.byte = 0x00;
    write(fd_petra, &u_act.byte, 1);
    close(fd_petra);
    exit(0);
}

