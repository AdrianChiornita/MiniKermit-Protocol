#ifndef LIB
#define LIB

typedef struct {
    int len;
    char payload[1400];
} msg;

typedef unsigned char* DATA;

typedef struct {
    unsigned char SOH;
    unsigned char LEN;
    unsigned char SEQ;
    char TYPE;
    DATA DATA;
    unsigned short CHECK;
    unsigned char MARK;
} MINI_KERMIT; 

typedef struct {
    unsigned char MAXL;
    unsigned char TIME;
    unsigned char NPAD;
    unsigned char PADC;
    unsigned char EOL;
    unsigned char QCTL;  
    unsigned char QBIN; 
    unsigned char CHKT; 
    unsigned char REPT; 
    unsigned char CAPA; 
    unsigned char R; 
} S_DATA;

static const msg EMPTYMSG;
static const MINI_KERMIT EMPTYPCKG;

S_DATA SendInit (unsigned char MAXL, unsigned char TIME, unsigned char NPAD, unsigned char PADC,
                 unsigned char EOL,  unsigned char QCTL, unsigned char QBIN, unsigned char CHKT,
                 unsigned char REPT, unsigned char CAPA, unsigned char R) 
{
    S_DATA SendInit;
    
    SendInit.MAXL = MAXL;
    SendInit.TIME = TIME;
    SendInit.NPAD = NPAD;
    SendInit.PADC = PADC;
    SendInit.EOL = EOL;
    SendInit.QCTL = QCTL;
    SendInit.QBIN = QBIN;
    SendInit.CHKT = CHKT;
    SendInit.REPT = REPT;
    SendInit.CAPA = CAPA;
    SendInit.R = R;

    return SendInit;
}

msg pckgToMessage (MINI_KERMIT pckg) {
    msg t;

    t.len = ((int) pckg.LEN) + 2;
    char *p = t.payload;
    
    t.payload[0] = pckg.SOH;
    t.payload[1] = pckg.LEN;
    t.payload[2] = pckg.SEQ;
    t.payload[3] = pckg.TYPE;

    if (((int) pckg.LEN) > 5) {
        memcpy (p += 4*sizeof(char), pckg.DATA, ((int) pckg.LEN) - 5);
        memcpy (p += (((int) pckg.LEN) - 5), &pckg.CHECK, sizeof(unsigned short));
    }
    else 
        memcpy (p += 4*sizeof(char), &pckg.CHECK, sizeof(unsigned short));
    
    memcpy (p += sizeof(unsigned short), &pckg.MARK, sizeof(char));

    return t;
}

MINI_KERMIT messageToPckg (msg  m) {
    MINI_KERMIT pckg;
    char* p =  m.payload;

    pckg.SOH =  m.payload[0];
    pckg.LEN =  m.payload[1];
    pckg.SEQ =  m.payload[2];
    pckg.TYPE =  m.payload[3];
    
    if (((int) pckg.LEN) > 5) {
        pckg.DATA = (DATA) malloc( ((int) pckg.LEN) - 5);
        memcpy (pckg.DATA, p += 4*sizeof(char),((int) pckg.LEN) - 5);
        
        pckg.CHECK = (((short)  m.payload[((int) pckg.LEN)]) << 8) 
                            | ( m.payload[((int) pckg.LEN) - 1] & 0xff);
        pckg.MARK =  m.payload[((int) pckg.LEN) + 1];
    } else {
        pckg.DATA = NULL;
        pckg.CHECK = (((short)  m.payload[5]) << 8) |
                                     ( m.payload[4] & 0xff);
        pckg.MARK =  m.payload[6];
    }

    return pckg;
}

void printPayloadInfo (msg  m) {
    printf("PAYLOAD: =================\n");
    printf("SOH     : %#04X \n",  m.payload[0]);
    printf("LEN     : %d\n", (int)  m.payload[1]);
    printf("SEQ     : %d\n", (int)  m.payload[2]);
    printf("TYPE    : %c \n",  m.payload[3]);

    if (((int)  m.payload[1]) > 5) {
        printf("DATA    : ");
        for (int i = 4; i <((int)  m.payload[1]) - 1; ++i) printf("%#02X ",  m.payload[i]& 0xff);
                                                          printf("\n");
        printf("CHECK   : %#02X",  m.payload[((int)  m.payload[1])]& 0xff);
        printf("%02X\n",  m.payload[((int)  m.payload[1]) - 1]& 0xff);
        printf("MARK    : %#04X \n", m.payload[((int)  m.payload[1]) + 1] & 0xff);
    }else {
        printf("CHECK   : %#02X",  m.payload[5]& 0xff);
        printf("%02X\n",  m.payload[4]& 0xff);
        printf("MARK    : %#04X \n", m.payload[6] & 0xff);
    }
}

void printPackageInfo (MINI_KERMIT pckg) {
    printf("PACKAGE: =================\n");
    printf("SOH     : %#04X \n", pckg.SOH);
    printf("LEN     : %d\n",  (int) pckg.LEN);
    printf("SEQ     : %d\n",  (int) pckg.SEQ);
    printf("TYPE    : %c\n", pckg.TYPE);
    if ((pckg.TYPE == 'Y' && (int) pckg.LEN > 5) || (pckg.TYPE == 'S')) {
        char* init[11] = {"MAXL", "TIME", "NPAD", "PADC", 
                                    "EOL ", "QCTL", "QBIN", "CHKT", "REPT", "CAPA", "R   "}; 
        for (int i = 0; i < 11; i++)
            printf("%s : 0x%04X \n",init[i], pckg.DATA[i]);
    }else
        printf("DATA    : %s\n", pckg.DATA);
    
    printf("CHECK   : %#04X \n", pckg.CHECK);
    printf("MARK    : %#04X \n", pckg.MARK);
}

void init(char* remote, int remote_port);
void set_local_port(int port);
void set_remote(char* ip, int port);
int send_message(const msg*  m);
int recv_message(msg* r);
msg* receive_message_timeout(int timeout);
unsigned short crc16_ccitt(const void *buf, int len);

#endif
