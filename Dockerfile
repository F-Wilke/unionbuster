
FROM ubuntu:latest

# Switch to Debian archived repositories for EOL buster
RUN sed -i 's|deb.debian.org|archive.debian.org|g' /etc/apt/sources.list && \
    sed -i 's|security.debian.org|archive.debian.org|g' /etc/apt/sources.list && \
    sed -i '/deb.*buster-updates/d' /etc/apt/sources.list && \
    apt-get update && \
    apt-get install -y build-essential nano && \
    apt-get clean && rm -rf /var/lib/apt/lists/*


WORKDIR /workspace
COPY . /workspace


# COPY runsc /gvisor/bin/runsc

RUN make

# Create test files for covert channel experiments
# Create 8MB random files at pages 0, 32, 64, etc. for strided channel
RUN dd if=/dev/urandom of=/workspace/rand0.bin bs=4096 count=32768 && \
    dd if=/dev/urandom of=/workspace/rand1.bin bs=4096 count=32768

CMD ["sleep", "infinity"]


#how to build and run:
# docker build -t union-buster .
# sudo docker run --runtime=runsc -d --name gv1 union-buster:latest