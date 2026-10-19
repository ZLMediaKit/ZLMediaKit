#!/bin/sh
set -eu

: "${ZLM_API_SECRET:?Configure ZLM_API_SECRET through the deployment secret manager}"
: "${ZLM_PUBLIC_IP:=179.189.117.2}"

case "$ZLM_API_SECRET" in
    *[!A-Za-z0-9._@+-]*)
        echo "ZLM_API_SECRET contains characters unsupported by the INI runtime substitution." >&2
        exit 64
        ;;
esac

case "$ZLM_PUBLIC_IP" in
    ""|*[!A-Za-z0-9.-]*)
        echo "ZLM_PUBLIC_IP must contain only an IPv4 address or DNS name." >&2
        exit 64
        ;;
esac

config_file="${ZLM_CONFIG_FILE:-/opt/media/conf/config.ini}"
if [ "$#" -ge 2 ] && [ "$1" = "-c" ]; then
    config_file="$2"
    shift 2
fi
if [ ! -r "$config_file" ]; then
    mkdir -p "$(dirname "$config_file")"
    cp /opt/media/default-config/config.ini "$config_file"
fi

umask 077
mkdir -p /run/guardia
awk -v secret="$ZLM_API_SECRET" -v public_ip="$ZLM_PUBLIC_IP" '
    /^secret=/ {
        print "secret=" secret
        replaced++
        next
    }
    /^externIP=/ {
        print "externIP=" public_ip
        extern_replaced++
        next
    }
    { print }
    END { if (replaced != 1 || extern_replaced != 1) exit 2 }
' "$config_file" > /run/guardia/config.ini

exec /opt/media/bin/MediaServer \
    -c /run/guardia/config.ini \
    -l 2 "$@"
