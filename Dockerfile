FROM debian:latest

RUN set -xe; \
    apt-get -yqq update; \
    apt-get install -y cmake build-essential libz-dev libssl-dev libbz2-dev libpng-dev libusb-dev \
    ;

COPY . /xpwn
WORKDIR /xpwn

RUN mkdir builddir
WORKDIR /xpwn/builddir

RUN cmake ..
RUN make

WORKDIR /

CMD [ "sh", "-c", "/xpwn/builddir/ipsw-patch/xpwntool /in /out -iv ${IV} -k ${KEY} -decrypt" ]

ENTRYPOINT [ "/xpwn/builddir/ipsw-patch/xpwntool" ]
