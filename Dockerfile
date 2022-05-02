FROM devkitpro/devkitppc:latest

RUN git clone https://github.com/devkitPro/wut && \
 cd wut && \
 git checkout cd6b4fb45d054d53af92bc0b3685e8bd9f01445d && \
 make -j$(nproc) && \
 make install && \
 cd .. && \
 rm -rf wut && \
 git clone --recursive https://github.com/Crementif/libiosuhax && \
 cd libiosuhax && \
 make -j$(nproc) && \
 make install && \
 cd .. && \
 rm -rf libiosuhax

WORKDIR project
