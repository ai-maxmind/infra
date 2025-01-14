FROM alpine:3.14
RUN apk add --no-cache \
    build-base \
    libmicrohttpd-dev \
    libmicrohttpd
WORKDIR /app
COPY . .
RUN gcc -o IdGenerator IdGenerator.c -lmicrohttpd -lpthread && \
    apk del build-base libmicrohttpd-dev
EXPOSE 8080
CMD ["./IdGenerator"]
