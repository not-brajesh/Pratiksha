```dockerfile
FROM ubuntu:24.04

RUN apt-get update && apt-get install -y --no-install-recommends \
    g++ \
    make \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY main.cpp .

RUN g++ -std=c++17 -O2 -pthread main.cpp -o hospital

ENV PORT=8080

EXPOSE 8080

CMD ["./hospital"]
```