/*
 * Runs in place of a 32-bit program while 32-bit apps are turned off, so the
 * person gets a prompt instead of a bare "Exec format error".
 *
 * The kernel hands every 32-bit x86 program to this through binfmt_misc
 * (wildos-32bit-prompt.service registers it only when the running kernel was
 * started with ia32_emulation=0, so it never stands in for a program that
 * could have run). It runs as whoever started the program, often inside a
 * sandbox such as Steam's Flatpak, so it is built static (the sandbox has
 * its own libraries, or none) and the kernel opens it once at registration
 * (the F flag), which is why the sandbox's own /usr does not need it.
 *
 * It says why on stderr, then asks this user's wildos-dialogs, if there is
 * one, to show the prompt: one datagram to an abstract socket named after
 * the user. Abstract sockets belong to the network namespace, so this
 * reaches the desktop from the host and from sandboxes that share the
 * network (Steam's does); from one without network the message on stderr
 * is all there is. The dialogs service checks the sender's uid itself and
 * decides whether to show anything, so a launcher retrying in a loop can
 * send as many as it likes.
 *
 * It exits 126, what a shell returns for a program it could not run.
 */
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

/* The app's id when running in a Flatpak, so the prompt can name the app
 * rather than the program file inside it. */
static void flatpak_app(char *out, size_t size)
{
    out[0] = '\0';
    FILE *info = fopen("/.flatpak-info", "re");
    if (!info) {
        return;
    }
    char line[512];
    int application = 0;
    while (fgets(line, sizeof line, info)) {
        line[strcspn(line, "\n")] = '\0';
        if (line[0] == '[') {
            application = strcmp(line, "[Application]") == 0;
        } else if (application && strncmp(line, "name=", 5) == 0) {
            snprintf(out, size, "%s", line + 5);
            break;
        }
    }
    fclose(info);
}

int main(int argc, char **argv)
{
    const char *program = argc > 1 ? argv[1] : "This program";
    const char *name = strrchr(program, '/');
    name = name ? name + 1 : program;

    fprintf(stderr,
            "%s: this is a 32-bit program, and 32-bit apps are turned off on this computer for security.\n"
            "Turn them on in Settings > Privacy and security > Steam and 32-bit apps.\n",
            name);

    char app[256];
    flatpak_app(app, sizeof app);

    int fd = socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
    if (fd >= 0) {
        struct sockaddr_un addr;
        memset(&addr, 0, sizeof addr);
        addr.sun_family = AF_UNIX;
        int len = snprintf(addr.sun_path + 1, sizeof addr.sun_path - 1, "wildos-32bit-%u", (unsigned)getuid());
        char message[1024];
        /* "program\napp", each cut short rather than dropped. */
        int n = snprintf(message, sizeof message, "%.700s\n%.255s", program, app);
        if (n > (int)sizeof message - 1) {
            n = sizeof message - 1;
        }
        sendto(fd, message, (size_t)n, MSG_DONTWAIT | MSG_NOSIGNAL, (struct sockaddr *)&addr,
               (socklen_t)(offsetof(struct sockaddr_un, sun_path) + 1 + len));
        close(fd);
    }
    return 126;
}
