/*
VINDERS Romain
Driver pour carte PCI
*/
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/neutrino.h>
#include <sys/dispatch.h>
#include <sys/resmgr.h>
#include <sys/iofunc.h>
#include <hw/pci.h>
#include <hw/pci_devices.h>
#include <hw/inout.h>
#include <fcntl.h>
#include <termios.h>
#include <time.h>
#include <signal.h>
#include <sched.h>
#include <sys/mman.h>

// constantes de configuration de périphérique
#define VENDORID   0x6666
#define DEVICEID   0x0101
#define ADDR_INDEX 3
#define OFFSET_CW  0x03
#define OFFSET_OUT 0x00
#define OFFSET_IN  0x01
#define CW_INIT    0x82

// prototypes callbacks pour fonctions POSIX
int io_open (resmgr_context_t *ctp, io_open_t *msg,  RESMGR_HANDLE_T *handle, void *extra);    
int io_read (resmgr_context_t *ctp, io_read_t *msg,  iofunc_ocb_t *ocb);
int io_write(resmgr_context_t *ctp, io_write_t *msg, iofunc_ocb_t *ocb);
int io_close(resmgr_context_t *ctp, void* reserverd, RESMGR_OCB_T *ocb);

// tables de pointeurs de fonctions - connexion et I/O
static resmgr_connect_funcs_t connect_funcs;
static resmgr_io_funcs_t      io_funcs;
// attributs IO et resource manager
static resmgr_attr_t resmgr_attr;
static iofunc_attr_t io_attr;
struct pci_dev_info  pci_info;
// handles
int   pciHandle;
void *deviceHandle;
dispatch_t *dpp;

// prototype init PCI
void init_pci();
void shutdown(int);
void handlerShutdown(int);
// gestion des options de lancement
void options (int argc, char **argv); // protoype
int  optv = 0;  // option gérée : -v (verbeux)
int  optc = 0;  // option gérée : -c (mode chaine de caractères)

// nom et descripteur
char *driver_descriptor = "/dev/vinders_pci";
char *driver_title = "VindersPCI";
uintptr_t base_addr;
size_t    base_addr_size;
uint32_t  pci_irq;
int pathID;

// -----------------------------------------------------------------------------
int main (int argc, char **argv)
{
    // variable de dispatch context
    dispatch_context_t     *ctp;
    
    // vérification des options de lancement
    options(argc, argv);
    if (optv) 
        printf ("%s: lancement...\n", driver_title);
    if (optc) 
        printf ("%s: mode chaine de caracteres actif\n", driver_title);
    
    // initialiser interface dispatch (utilisée par framework resource manager)
    if ((dpp = dispatch_create()) == NULL) 
    {
        fprintf(stderr, "%s: erreur dispatch_create: %s\n", driver_title, strerror(errno));
        exit(1);
    }
    if (optv) 
        printf ("%s: dispatch_create OK\n", driver_title);
    // initialiser attributs du resource manager
    memset (&resmgr_attr, 0, sizeof (resmgr_attr));
    resmgr_attr.nparts_max   = 1;
    resmgr_attr.msg_max_size = 2048;
    // initialiser tables de pointeurs de routines de callback
    iofunc_func_init (_RESMGR_CONNECT_NFUNCS, &connect_funcs, _RESMGR_IO_NFUNCS, &io_funcs);
    connect_funcs.open = io_open;
    io_funcs.read  = io_read;
    io_funcs.write = io_write;
    io_funcs.close_ocb = io_close;  
    // initialiser attributs de périphérique : permissions, type, propriétaire, groupe
    iofunc_attr_init(&io_attr, S_IFCHR | 0666, NULL, NULL);
    io_attr.nbytes = 1;

    // attacher nom logique au process manager
    pathID = resmgr_attach(dpp, &resmgr_attr, driver_descriptor, _FTYPE_ANY, 0, 
                           &connect_funcs, &io_funcs, &io_attr);
    if (pathID == -1) 
    {
        fprintf (stderr, "%s: erreur resmgr_attach: %s\n", driver_title, strerror(errno)); shutdown(1);
    }
    if (optv)
    {
        printf ("%s: retour resmgr_attach: %d\n", driver_title, pathID);
        printf ("%s: descripteur %s\n", driver_title, driver_descriptor);
    }
    
    // allouer structure de contexte dispatch (pour données de messages)
    ctp = dispatch_context_alloc(dpp);
    if (optv) 
        printf ("%s: dispatch_context_alloc OK\n", driver_title);
    
    // armer signal de fermeture
    struct sigaction sigact;
    sigact.sa_handler = handlerShutdown;
    sigfillset(&sigact.sa_mask); // signal d'arrêt non interruptible
    sigact.sa_flags = 0;
    if (sigaction(SIGINT, &sigact, NULL) == -1)
    {
        fprintf (stderr, "%s: erreur armement signal: %s\n", driver_title, strerror(errno)); shutdown(1);
    }
    if (optv) 
        printf ("%s: armement signal fin OK\n", driver_title);
    
    // initialisation carte PCI
    init_pci();
    
    // boucle de messages du resource manager
    while (1) 
    {
        if ((ctp = dispatch_block(ctp)) == NULL) 
        {
            fprintf (stderr, "%s: erreur dispatch_block: %s\n", driver_title, strerror(errno)); shutdown(1);
        }
        if (optv) 
            printf ("%s: dispatch block : requete\n", driver_title);
        // appel bloquant de fonction de callback associée au message
        dispatch_handler(ctp);
    }
    return 0;
}

// INITIALISATION CARTE PCI ----------------------------------------------------
void init_pci()
{
    int i=0;
    // obtention privilèges
    if (ThreadCtl(_NTO_TCTL_IO,0) == -1)
    {
        fprintf(stderr, "%s: Erreur d'obtention des privileges noyau", driver_title, strerror(errno)); exit(1);
    }
    
    // connexion au serveur PCI
    if((pciHandle = pci_attach(0)) == -1)
    {
        fprintf(stderr, "%s: Erreur de pci_attach", driver_title, strerror(errno)); exit(1);
    }
    if (optv) 
        printf ("%s: pci_attach OK\n", driver_title);
    
    // attachement driver à carte PCI
    memset(&pci_info,0,sizeof(pci_info));
    pci_info.VendorId = VENDORID;
    pci_info.DeviceId = DEVICEID;
    if ((deviceHandle = pci_attach_device(NULL, PCI_SHARE|PCI_INIT_ALL, 0, &pci_info)) == 0)
    {
        fprintf(stderr, "%s: Erreur de pci_attach_device", driver_title, strerror(errno)); exit(1);
    }
    if (optv) 
        printf ("%s: pci_attach_device OK\n", driver_title);
    
    // mapper espace mémoire et récupérer adresse de base (si existante)
    if (PCI_IS_IO(pci_info.CpuBaseAddress[ADDR_INDEX]))
    {
        pci_irq = pci_info.Irq;
        base_addr_size = pci_info.BaseAddressSize[ADDR_INDEX];
        if ((base_addr = mmap_device_io(base_addr_size, PCI_IO_ADDR(pci_info.CpuBaseAddress[ADDR_INDEX])) ) 
            == MAP_DEVICE_FAILED)
        {
            fprintf(stderr, "%s: Erreur de mapping de l'espace memoire IO : %s\n", driver_title, strerror(errno)); shutdown(1);
        }
        if (optv) 
            printf ("%s: Mapping memoire mmap_device_io OK\n", driver_title);
        PCI_IO_ADDR(pci_info.CpuBaseAddress[ADDR_INDEX]);
    }
    else
    {
        fprintf(stderr, "%s: La carte de cet ordinateur n'est pas supportee", driver_title, strerror(errno)); shutdown(1);
    }
    
    // initialisation du 8255 (control word)
    out8(base_addr + OFFSET_CW, CW_INIT);
    if (optv) 
        printf ("%s: initialisation CW\n", driver_title);
}

// FERMETURE DU DRIVER ---------------------------------------------------------
void shutdown(int returnCode)
{
    // reset des actuateurs
    out8(base_addr + OFFSET_OUT, 0x0);
    // détacher carte et libérer espace mémoire
    if (munmap_device_io(base_addr, base_addr_size) == -1)
        fprintf (stderr, "%s: Erreur demapping espace memoire : %s\n", driver_title, strerror(errno));
    if (pci_detach_device(deviceHandle) != PCI_SUCCESS)
        fprintf (stderr, "%s: Erreur detachement PCI : %s\n", driver_title, strerror(errno));
    // retirer nom logique
    if (optv) 
        printf ("%s: Arret du driver %s\n", driver_title, driver_descriptor);
    resmgr_detach(dpp, pathID, _RESMGR_DETACH_ALL);
    exit(returnCode);
}
void handlerShutdown(int sig)
{
    shutdown(0);
}

// CALLBACK OUVERTURE (open) ---------------------------------------------------
int io_open(resmgr_context_t *ctp, io_open_t *msg, RESMGR_HANDLE_T *handle, void *extra)
{
    int ret;
    if (optv) 
        printf ("%s: io_open\n", driver_title);
    // ouverture par défaut
    ThreadCtl(_NTO_TCTL_IO,0); // privileges
    ret = iofunc_open_default(ctp, msg, handle, extra);
    if (optv) 
        printf ("%s: iofunc_open_default : %d\n", driver_title, ret);
    // initialisation des actuateurs
    io_attr.nbytes = 1;
    out8(base_addr + OFFSET_OUT, 0x0);
    return ret;
}

// CALLBACK FERMETURE (close) --------------------------------------------------
int io_close(resmgr_context_t *ctp, void * reserverd, RESMGR_OCB_T *ocb)
{
    int ret;
    if (optv) 
        printf ("%s: io_close\n", driver_title);
    // reset des actuateurs
    if (!optc)
        out8(base_addr + OFFSET_OUT, 0x0);
    // fermeture par défaut
    ret = iofunc_close_ocb_default(ctp, NULL, ocb);
    if (optv) 
        printf ("%s: iofunc_close_ocb_default : %d\n", driver_title, ret);
    return ret;
}

// CALLBACK LECTURE (read) -----------------------------------------------------
int io_read (resmgr_context_t *ctp, io_read_t *msg, iofunc_ocb_t *ocb)
{
    int    ret;
    if (optv) 
        printf ("%s: io_read\n", driver_title);

    // vérifier permissions de lecture
    if ((ret = iofunc_read_verify (ctp, msg, ocb, NULL)) != EOK) 
        return ret;
    if (optv) 
        printf ("%s: iofunc_read_verify OK\n", driver_title);
    // vérifier type de message
    if ((msg->i.xtype & _IO_XTYPE_MASK) != _IO_XTYPE_NONE)
        return(ENOSYS);
    
    // lecture
    if (msg->i.nbytes > 0 && (!optc || (ocb->attr->nbytes - ocb->offset) > 0))
    {
        uint8_t* read_byte;
        char* read_char;
        if ((read_byte = (uint8_t*)malloc(1)) == NULL)
            return (ENOMEM);
        if (optc)
        {
            if ((read_char = (char*)malloc(5)) == NULL)
                return (ENOMEM);
        }

        if (optv) 
        {
            printf ("bytes a lire: %d\n", msg->i.nbytes);
            if (msg->i.nbytes > 1)
                printf ("lecture tronquee a 1 byte\n");
        }
        
        // lire valeur
        *read_byte = in8(base_addr + OFFSET_IN);
        unsigned int readVal = *read_byte;
        printf ("lecture: %u\n", readVal);
        if (optc) 
        {
            read_char[3] = '\0';
            snprintf(read_char, 5, "%u\n\0", readVal);
        }
        
        // préciser nombre de bytes + envoyer au client
        if (optc) // mode chaine de caracteres
        {
            _IO_SET_READ_NBYTES(ctp, strlen(read_char) + 1);
            if (resmgr_msgreply(ctp, read_char, strlen(read_char) + 1) == -1)
            {
                if (optc)
                    free(read_char);
                free(read_byte);
                fprintf(stderr, "%s: Erreur resmgr_msgreply : %s\n", driver_title, strerror(errno));
                return(EINVAL);
            }
        }
        else // mode byte
        {
            _IO_SET_READ_NBYTES(ctp, 1);
            if (resmgr_msgreply(ctp, read_byte, 1) == -1)
            {
                if (optc)
                    free(read_char);
                free(read_byte);
                fprintf(stderr, "%s: Erreur resmgr_msgreply : %s\n", driver_title, strerror(errno));
                return(EINVAL);
            }
        }
        if (optv) 
            printf ("%s: resmgr_msgreply OK\n", driver_title);
        
        // invalider temps d'accès (mise à jour structures POSIX + OCB)
        ocb->attr->flags |= (IOFUNC_ATTR_ATIME | IOFUNC_ATTR_DIRTY_TIME);
        // décaler prochaine lecture
        if ((msg->i.xtype & _IO_XTYPE_MASK) == _IO_XTYPE_NONE)
            ocb->offset += (ocb->attr->nbytes - ocb->offset);
        
        if (optc)
            free(read_char);
        free(read_byte);
    }
    else // rien à lire
    {
        if (optv) 
            printf ("0 bytes demandes ou lecture complete\n");
        
        // envoi vide (débloquer commande d'appel client)
        if (resmgr_msgreply(ctp, EOK, 0) == -1)
        {
            fprintf(stderr, "%s: Erreur resmgr_msgreply : %s\n", driver_title, strerror(errno));
            return(EINVAL);
        }
    }
    
    // indiquer que réponse déjà envoyée
    if (optv) 
        printf ("%s: fin io_read\n", driver_title);
    return (_RESMGR_NOREPLY);
}

// CALLBACK ECRITURE (write) ---------------------------------------------------
int io_write (resmgr_context_t *ctp, io_write_t *msg, iofunc_ocb_t *ocb)              
{                                                                                 
    int ret;
    unsigned int conv_val;
    if (optv) 
        printf ("%s: io_write\n", driver_title);
    
    // vérifier permissions d'écriture
    if ((ret = iofunc_write_verify(ctp, msg, ocb, NULL)) != EOK)
        return ret;
    if (optv) 
        printf ("%s: iofunc_write_verify OK\n", driver_title);
    // vérifier type de message
    if ((msg->i.xtype & _IO_XTYPE_MASK) != _IO_XTYPE_NONE)
        return(ENOSYS);
    
    // écriture des bytes
    if (msg->i.nbytes > 0)
    {
        uint8_t written_byte;
        char* write_buffer;
        unsigned int writtenVal;
       if ((write_buffer = (char*)malloc(1 + msg->i.nbytes)) == NULL)
        {
            return (ENOMEM);
        }
        
        // recupérer les données à écrire
        if (resmgr_msgread (ctp, write_buffer, msg->i.nbytes, sizeof (msg->i)) == -1)
        {
            free(write_buffer);
            fprintf(stderr, "%s: Erreur resmgr_msgread : %s\n", driver_title, strerror(errno));
            return (errno);
        }
        if (optv) 
        {
            if (optc)
                printf ("%s: %d bytes recus\n", driver_title, msg->i.nbytes);
            else if (msg->i.nbytes > 1)
                printf ("%s: %d bytes recus, tronques a 1\n", driver_title, msg->i.nbytes);
            else
                printf ("%s: 1 byte recu\n", driver_title);
        }
        
        // écrire les données
        if (optc) // chaine de caracteres
        {
            write_buffer[msg->i.nbytes] = '\0';
            conv_val = atoi(write_buffer);
            if (conv_val > 255)
                conv_val = 255;
            written_byte = conv_val;
        }
        else // byte
        {
            written_byte = write_buffer[0];
        }
        writtenVal = written_byte;
        printf ("valeur ecrite : %u\n", writtenVal);
        out8(base_addr + OFFSET_OUT, written_byte); // écriture
        if (optv) 
            printf ("%s: 1 byte ecrit\n", driver_title);
        
        free(write_buffer);
    }
    else
    {
        if (optv) 
            printf ("%s: 0 byte ecrit\n", driver_title);
    }
    
    // indiquer nombre de bytes écrits
    _IO_SET_WRITE_NBYTES (ctp, msg->i.nbytes);
    
    // invalider temps d'accès (mise à jour structures POSIX + OCB)
    if (msg->i.nbytes > 0)
    {
        ocb->attr->flags |= (IOFUNC_ATTR_MTIME | IOFUNC_ATTR_CTIME);
        ocb->offset += msg->i.nbytes;
    }
    if (optv) 
        printf ("%s: fin io_write\n", driver_title);
    return (_RESMGR_NPARTS (0)); // fin transaction
}    

// RECUPERATION D'OPTIONS DE LANCEMENT -----------------------------------------
void options (int argc, char **argv)
{
    int opt;
    optv = 0;
    optc = 0;
    while (optind < argc) 
    {
        while ((opt = getopt (argc, argv, "vc")) != -1) 
        {
            switch (opt) 
            {
                case 'v': optv = 1; break;
                case 'c': optc = 1; break;
            }
        }
    }
}
