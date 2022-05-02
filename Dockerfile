FROM devkitpro/devkitppc:latest

RUN git clone --recursive https://github.com/Crementif/libiosuhax && \
 cd libiosuhax && \
 make -j$(nproc) && \
 make install && \
 cd .. && \
 rm -rf libiosuhax

WORKDIR project
