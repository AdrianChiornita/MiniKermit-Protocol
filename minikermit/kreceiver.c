#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include "lib.h"

#define HOST "127.0.0.1"
#define PORT 10001

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

MINI_KERMIT Y() {
    MINI_KERMIT result;
    
    result.SOH = 0x01;
    result.LEN = 5;
    result.SEQ = sequence;
    result.TYPE = 'Y';

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

MINI_KERMIT YS(S_DATA SendInit) {
    MINI_KERMIT result;
    result.DATA = (DATA) malloc (sizeof(SendInit));
    
    result.SOH = 0x01;
    result.LEN = sizeof(SendInit) + 5; 
    result.SEQ = sequence;     
    result.TYPE = 'Y';

    memcpy (result.DATA, &SendInit, sizeof(SendInit)); 

    char aux[sizeof(SendInit) + 4];
    char *p = aux;

    aux[0] = result.SOH;
    aux[1] = result.LEN;
    aux[2] = result.SEQ;
    aux[3] = result.TYPE;
    memcpy (p += 4*sizeof(char), result.DATA, sizeof(SendInit));

    result.CHECK  = crc16_ccitt(aux, sizeof(SendInit) + 4);
    result.MARK = GLOBAL.EOL; 
    
    return result;
}

MINI_KERMIT N() {
    MINI_KERMIT result;
    
    result.SOH = 0x01;
    result.LEN = 5; 
    result.SEQ = sequence;     
    result.TYPE = 'N';

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

void tryToReceive (char type) {

     while (TRUE) {
        rc = receive_message_timeout(GLOBAL.TIME*1000);

        while (loss != 3 && rc == NULL) {
            printf("[R] Message is not coming.\n");

            loss++;
            rc = NULL;
            if (loss == 3) {
	            printf("[R] {ERROR} Sender   is not responding.\n");
	            exit(-1);
	        }

            rc = receive_message_timeout(GLOBAL.TIME*1000);
        }
        loss = 0;

        rcPCKG = messageToPckg(*rc);

        printf("[R] Got message with type: %c [%d].\n" , rcPCKG.TYPE, rcPCKG.SEQ);

        unsigned short crc = crc16_ccitt(rc->payload, ((int) (unsigned char) rc->payload[1]) - 1);
        
        if(crc == rcPCKG.CHECK) {

            sequence = (int) rcPCKG.SEQ;
            if (type == 'I') {
                memcpy(&GLOBAL, rcPCKG.DATA, sizeof(GLOBAL));
                trPCKG = YS(GLOBAL);
            }
            else trPCKG = Y();

            tr = pckgToMessage(trPCKG);
            send_message(&tr);

            rc = NULL;
            
            printf("[R] Send the package     : %c [%d].\n",tr.payload[3], (int) tr.payload[2]);
            incrementSEQ();

            break;
        } else {
            trPCKG = N();
            tr = pckgToMessage(trPCKG);
            send_message(&tr);

            rc = NULL;

            printf("[R] Send the package     : %c [%d].\n",tr.payload[3], (int) tr.payload[2]);
        }   
    }
    printf("-------------------------\n");
}

int main(int argc, char** argv) {
	GLOBAL = SendInit(0xFA, 0x05, 0x00, 0x00, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x00,0x00);
	
	tr = EMPTYMSG;
    rc = NULL;
    trPCKG = EMPTYPCKG;
    rcPCKG = EMPTYPCKG;

    init(HOST, PORT);
    tryToReceive('I');
    while (TRUE) {
    	tryToReceive('T');

	    if (rcPCKG.TYPE  == 'B') break;

	    char fileName[((int) rcPCKG.LEN) - 4];
        sprintf(fileName, "recv_%s", rcPCKG.DATA);

        FILE* WF = fopen(fileName,"wb");

        if (WF == NULL) {
            printf("[R] {ERROR} : Can't open the file %s.\n", fileName);
            return -1;
        }

    	while (TRUE) {
            tryToReceive('T');

		    if (rcPCKG.TYPE  == 'Z') break;

		    unsigned int size = ((int) rcPCKG.LEN) - 5;
            fwrite(rcPCKG.DATA, sizeof(char), size, WF);
		}
    }

	return 0;
}
