
FROM nginx:1.20.1

# Switch to Debian archived repositories for EOL buster
RUN sed -i 's|deb.debian.org|archive.debian.org|g' /etc/apt/sources.list && \
    sed -i 's|security.debian.org|archive.debian.org|g' /etc/apt/sources.list && \
    sed -i '/deb.*buster-updates/d' /etc/apt/sources.list && \
    apt-get update && \
    apt-get install -y build-essential && \
    apt-get clean && rm -rf /var/lib/apt/lists/*


WORKDIR /workspace
COPY . /workspace


RUN gcc -o /spy_on /workspace/spy_on.c && \
    gcc -o /read_page /workspace/read_page.c

CMD ["nginx", "-g", "daemon off;"]
