FROM devkitpro/devkitppc:latest

COPY --from=wiiuenv/libmocha:20220919 /artifacts $DEVKITPRO

WORKDIR /project
