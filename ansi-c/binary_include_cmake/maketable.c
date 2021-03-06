#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>


int main(int argc, const char *argv[])
{
    if (argc != 3) return 1;

    int input = open(argv[1], O_RDONLY);
    if (input == -1) {
        fprintf(stderr, "error opening %s: %s\n", argv[1], strerror(errno));
        return 1;
    }

    int output = open(argv[2], O_WRONLY|O_TRUNC|O_CREAT, 0660);
    if (output == -1) {
        close(input);
        fprintf(stderr, "error opening %s: %s\n", argv[2], strerror(errno));
        return 1;
    }

    char buffer[10];
    while (1) {
        char c;
        int numread = read(input, &c, 1);
        if (numread == 0) break;    // EOF
        sprintf(buffer, "0x%02x, ", c);
        write(output, buffer, strlen(buffer));
    }
    char end = '\n';
    write(output, &end, 1);

    close(output);
    close(input);

    return 0;
}

