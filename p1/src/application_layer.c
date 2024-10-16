// Application layer protocol implementation
#include "link_layer.h"
#include "application_layer.h"
#include <string.h>
#include <stdio.h>
void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename)
{
    LinkLayerRole roley;

    if (strcmp(role, "tx") == 0)
        roley = LlTx;
    else if (strcmp(role, "rx")==0)
        roley=LlRx;
    else{
        printf("error\n");
        return;
    }

    LinkLayer connect={
        .role=roley,
        .baudRate=baudRate,
        .nRetransmissions = nTries,
        .timeout= timeout
        };
    
    strcpy(connect.serialPort, serialPort);
    
    llopen(connect);
    
    // if (roley ==LlTx) {
    //     llwrite(&bu,5);
    // }
    // else {
    //     llwrite(&bu,5);
    // }

    

}
