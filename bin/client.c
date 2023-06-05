#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "relay_client.h"


int main(int argc, char *argv[]) {
    if (argc < 4) {
        perror("argc");
        return 1;
    }
    //TODO
    return 0;
}
