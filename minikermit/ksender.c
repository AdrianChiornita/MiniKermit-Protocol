#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "lib.h"

#define HOST "127.0.0.1"
#define PORT 10000

#define TRUE 1
#define FALSE 0

S_DATA GLOBAL;

int sequence;
unsigned int loss;

msg tr;
msg* rc;

MINI_KERMIT rcPCKG, trPCKG;

void incrementSEQ() {
    sequence = (sequence + 1) % 64;
}

MINI_KERMIT S(S_DATA SendInit) {
    MINI_KERMIT result;
    result.DATA = (DATA) malloc (sizeof(SendInit));
    
    result.SOH = 0x01;
    result.LEN = sizeof(SendInit) + 5;
    result.SEQ = sequence;
    result.TYPE = 'S';

    memcpy (result.DATA, &SendInit, sizeof(SendInit));
    
    char aux[sizeof(SendInit) + 4];
    char *p = aux;
    
    aux[0] = result.SOH;
    aux[1] = result.LEN;
    aux[2] = result.SEQ;
    aux[3] = result.TYPE;
    memcpy (p += 4*sizeof(char), result.DATA, sizeof(SendInit));

    result.CHECK  = crc16_ccitt(aux, sizeof(SendInit) + 4);

    result.MARK = SendInit.EOL; 
    return result;
}

MINI_KERMIT F(DATA fileName, int len) {
    MINI_KERMIT result;
    result.DATA = (DATA) malloc (len);
    
    result.SOH = 0x01;
    result.LEN = len + 5; 
    result.SEQ = sequence;     
    result.TYPE = 'F';

    memcpy (result.DATA, fileName, len); 

    char aux[len + 4];
    char *p = aux;
    aux[0] = result.SOH;
    aux[1] = result.LEN;
    aux[2] = result.SEQ;
    aux[3] = result.TYPE;
    memcpy (p += 4*sizeof(char), result.DATA, len);

    result.CHECK  = crc16_ccitt(aux, len + 4);
    result.MARK = GLOBAL.EOL; 
    return result;
}

MINI_KERMIT D(DATA data, int len) {
    MINI_KERMIT result;
    result.DATA = (DATA) malloc (len);
    
    result.SOH = 0x01;
    result.LEN = len + 5; 
    result.SEQ = sequence;     
    result.TYPE = 'D';
    
    memcpy (result.DATA, data, len); 

    char aux[len + 4];
    char *p = aux;

    aux[0] = result.SOH;
    aux[1] = result.LEN;
    aux[2] = result.SEQ;
    aux[3] = result.TYPE;
    memcpy (p += 4*sizeof(char), result.DATA, len);

    result.CHECK  = crc16_ccitt(aux, len + 4);
    result.MARK = GLOBAL.EOL; 
    return result;
}

MINI_KERMIT Z() {
    MINI_KERMIT result;
    
    result.SOH = 0x01;
    result.LEN = 5; 
    result.SEQ = sequence;     
    result.TYPE = 'Z';

    char aux[4];
    aux[0] = result.SOH;
    aux[1] = result.LEN;
    aux[2] = result.SEQ;
    aux[3] = result.TYPE;

    result.DATA = NULL;
    result.CHECK  = crc16_ccitt(aux, 4);
    result.MARK = GLOBAL.EOL;

    return result;
}

MINI_KERMIT B() {
    MINI_KERMIT result;
    
    result.SOH = 0x01;
    result.LEN = 5; 
    result.SEQ = sequence;     
    result.TYPE = 'B';

    char aux[4];
    aux[0] = result.SOH;
    aux[1] = result.LEN;
    aux[2] = result.SEQ;
    aux[3] = result.TYPE;

    result.DATA = NULL;
    result.CHECK  = crc16_ccitt(aux, 4);
    result.MARK = GLOBAL.EOL; 

    return result;
}

void tryToSend (char type, DATA data, int len) {

    printf("===============================\n");
    while (TRUE) {
        switch (type) {
            case 'S':
                trPCKG = S(GLOBAL);
                break;
            case 'F':
                trPCKG = F(data,len);
                break;
            case 'D':
                trPCKG = D(data,len);
                break;
            case 'Z':
                trPCKG = Z();
                break;
            case 'B':
                trPCKG = B();
                break;
            default: break;
        }

        tr = pckgToMessage(trPCKG);
        send_message(&tr);
        
        printf("[S] Send the package     : %c [%d].\n",tr.payload[3], (int) tr.payload[2]);

        rc = receive_message_timeout(GLOBAL.TIME*1000);

        while (loss != 3 && rc == NULL) {
            printf("[S] Message is not coming.\n");
            
            loss++;
            rc = NULL;
            if (loss == 3) {
                printf("[S] {ERROR} Receiver is not responding.\n");
                exit(-1);
            }

            send_message(&tr);
            printf("[S] Send the package     : %c [%d].\n",tr.payload[3], (int) tr.payload[2]);

            rc = receive_message_timeout(GLOBAL.TIME*1000);
        }
        loss = 0;
        
        rcPCKG = messageToPckg(*rc);
        printf("[S] Got message with type: %c [%d].\n" , rcPCKG.TYPE, rcPCKG.SEQ);

        if (rcPCKG.TYPE == 'Y') {
            incrementSEQ();
            
            tr = EMPTYMSG;
            rc = NULL;
            rcPCKG = EMPTYPCKG;

            break;
        } else {
            incrementSEQ();

            tr = EMPTYMSG;
            rc = NULL;
            rcPCKG = EMPTYPCKG;
        }
    }
}

int main(int argc, char** argv) {
    
    GLOBAL = SendInit(0xFA, 0x05, 0x00, 0x00, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x00,0x00);
    
    tr = EMPTYMSG;
    rc = NULL;
    trPCKG = EMPTYPCKG;
    rcPCKG = EMPTYPCKG;

    init(HOST, PORT);

    tryToSend('S',(DATA)"",0);

    for (int file = 1; file < argc; ++file) {
        FILE* RF = fopen (argv[file], "rb");
        
        if (RF == NULL) {
            printf("[S] {ERROR} : Can't open the file %s.\n", argv[file]);
            return -1;
        }

        tryToSend('F',(DATA)argv[file],strlen(argv[file]));

        unsigned int size;
        unsigned char buffer[250];

        while ((size = fread(buffer,sizeof(unsigned char),GLOBAL.MAXL,RF)) != 0) {
            if (size == -1) {
                printf("[S] {ERROR} : Can't read from the file %s.\n", argv[1]);
                return -1;
            }
            tryToSend('D',(DATA) buffer, size);
        }
        
        tryToSend('Z',(DATA)"", 0);
    }

    tryToSend('B',(DATA)"", 0);
    
    return 0;
}
