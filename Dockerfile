FROM caddy:2.10.2

COPY build/fw-images /srv

COPY Caddyfile /etc/caddy/Caddyfile
