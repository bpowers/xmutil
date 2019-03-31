FROM alpine:3.9 as builder

MAINTAINER Bobby Powers "bobbypowers@gmail.com"

RUN apk add --no-cache --virtual .build-deps \
	binutils \
	build-base \
	ca-certificates \
	ninja \
	gcc \
	git \
	libc-dev \
	libgcc \
	libstdc++ \
	tinyxml2-dev

WORKDIR /src

COPY . .

RUN ./configure.sh \
 && ninja -C out/Release

CMD ["nginx", "-g", "daemon off;"]
