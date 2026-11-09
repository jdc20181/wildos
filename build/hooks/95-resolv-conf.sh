# Lookups go through systemd-resolved (wildos-settings depends on it), which
# is what lets them be encrypted. Left alone, the build container's own
# resolv.conf would ship in place of the link to resolved, and programs
# reading it would ask that address directly. This runs last, since the
# hooks before it may still need the container's DNS.
if [ -x /usr/lib/systemd/systemd-resolved ]; then
    ln -sfn ../run/systemd/resolve/stub-resolv.conf /etc/resolv.conf
fi
