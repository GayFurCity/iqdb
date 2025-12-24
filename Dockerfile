FROM alpine:3.23 AS builder

RUN apk --no-cache add sqlite-dev cmake git build-base

WORKDIR /iqdb
COPY . .
RUN make release

FROM alpine:3.23

RUN apk --no-cache add sqlite-libs libstdc++

COPY --from=builder /iqdb/build/release/src/iqdb /usr/local/bin/

CMD ["iqdb", "http", "0.0.0.0", "5588", "/iqdb/data.db"]
