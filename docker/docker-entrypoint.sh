#!/bin/bash
echo "env.sh start"
/usr/local/bin/zlmedia-ui-env.sh
echo "nginx starting"
/usr/sbin/service nginx start
echo "ZLMediaKit starting"
/usr/src/ZLMediaKit/build/bin/MediaServer --daemon --level 2