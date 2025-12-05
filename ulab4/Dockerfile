FROM alpine:latest
RUN apk add --no-cache g++ make openssl-dev linux-headers
WORKDIR /app
COPY dispatcher.cpp worker.cpp ./
RUN g++ -std=c++17 -O2 -o dispatcher dispatcher.cpp -lcrypto && \
    g++ -std=c++17 -O2 -o worker worker.cpp -lcrypto