FROM panjjo/zlmediakit:latest

ENV ZLM_PUBLIC_IP=179.189.117.2

# Keep the requested /opt/media paths while using the executable shipped by the base image.
RUN mkdir -p /opt/media/bin /opt/media/conf /app/storage/recordings /run/guardia \
    && ln -s /zlmediakit/MediaServer /opt/media/bin/MediaServer

COPY conf/config.ini /opt/media/conf/config.ini
COPY conf/config.ini /opt/media/default-config/config.ini
COPY docker/guardia/entrypoint.sh /usr/local/bin/guardia-zlm-entrypoint
RUN chmod 0755 /usr/local/bin/guardia-zlm-entrypoint

VOLUME ["/opt/media/conf", "/app/storage/recordings"]

EXPOSE 18080/tcp 18082/tcp 19350/tcp
EXPOSE 10000-10100/udp 10050/udp 10055/udp

ENTRYPOINT ["/usr/local/bin/guardia-zlm-entrypoint"]
CMD ["-c", "/opt/media/conf/config.ini", "-s", "/zlmediakit/ssl.p12"]
