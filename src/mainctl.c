#include "common.h"

#include "ctl.h"
#include "option.h"
#include "str.h"

#include <stdio.h>
#include <sys/socket.h>

#ifndef PACKAGE_VERSION
#define PACKAGE_VERSION "unknown"
#endif

static struct {
    char *dev;
    int version;
    struct {
        struct {
            int set;
            const char *addr;
        } add, del;
    } path;
} gt = {
    .dev = "tun0",
};

static int
gt_setup_option(int argc, char **argv)
{
    // clang-format off

    struct option path_opts[] = {
        { "add", &gt.path.add.addr, option_str },
        { "del", &gt.path.del.addr, option_str },
        {  NULL                                },
    };

    struct option opts[] = {
        { "dev",     &gt.dev,    option_str    },
        { "path",    &path_opts, option_option },
        { "version", NULL,       option_option },
        {  NULL                                },

    };

    // clang-format on

    if (option(opts, argc, argv))
        return 1;

    gt.path.add.set = option_is_set(path_opts, "add");
    gt.path.del.set = option_is_set(path_opts, "del");
    gt.version = option_is_set(opts, "version");

    return 0;
}

int
main(int argc, char **argv)
{
    if (gt_setup_option(argc, argv))
        return 1;

    if (gt.version) {
        gt_print(PACKAGE_VERSION "\n");
        return 0;
    }

    int ctl_fd = ctl_init("/run/" PACKAGE_NAME, "client");

    if (ctl_fd == -1) {
        perror("ctl_init");
        return 1;
    }

    if (ctl_connect(ctl_fd, "/run/" PACKAGE_NAME, gt.dev) == -1) {
        gt_log("couldn't connect to %s\n", gt.dev);
        return 1;
    }

    struct ctl_msg msg;

    if (gt.path.add.set) {
        msg = (struct ctl_msg){
            .type = CTL_PATH_ADD,
        };
        str_cpy(msg.path.add.addr, sizeof(msg.path.add.addr) - 1, gt.path.add.addr);
    } else if (gt.path.del.set) {
        msg = (struct ctl_msg){
            .type = CTL_PATH_DEL,
        };
        str_cpy(msg.path.del.addr, sizeof(msg.path.del.addr) - 1, gt.path.del.addr);
    }

    if (send(ctl_fd, &msg, sizeof(msg), 0) == -1) {
        perror("send");
        return 1;
    }

    struct ctl_msg reply;

    if (recv(ctl_fd, &reply, sizeof(reply), 0) == -1) {
        perror("recv");
        return 1;
    }

    switch (reply.type) {
    case CTL_REPLY:
        if (reply.reply) {
            errno = reply.reply;
            perror("error");
        }
        break;
    case CTL_UNKNOWN:
        gt_print("unknown command: %i\n", reply.unknown.type);
        break;
    default:
        gt_log("bad reply from server: %i\n", reply.type);
    }

    close(ctl_fd);

    return 0;
}
