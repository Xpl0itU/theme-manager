FROM wiiuenv/devkitppc:latest

COPY --from=wiiuenv/libiosuhax:latest /artifacts $DEVKITPRO

WORKDIR project
